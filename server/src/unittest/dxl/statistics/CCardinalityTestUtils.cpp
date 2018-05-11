//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal, Inc.
//
//	@filename:
//		CCardinalityTestUtils.cpp
//
//	@doc:
//		Utility functions used in the testing cardinality estimation
//---------------------------------------------------------------------------

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include "naucrates/statistics/CPoint.h"
#include "naucrates/statistics/CBucket.h"
#include "naucrates/statistics/CHistogram.h"
#include "naucrates/statistics/CStatistics.h"
#include "naucrates/statistics/CStatisticsUtils.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLDatumGeneric.h"
#include "naucrates/dxl/operators/CDXLDatumStatsDoubleMappable.h"
#include "naucrates/dxl/operators/CDXLDatumStatsLintMappable.h"

#include "unittest/dxl/statistics/CCardinalityTestUtils.h"
#include "unittest/gpopt/CTestUtils.h"


// create a bucket with closed integer bounds
CBucket *
CCardinalityTestUtils::PbucketIntegerClosedLowerBound
	(
	IMemoryPool *memory_pool,
	INT iLower,
	INT iUpper,
	CDouble frequency,
	CDouble distinct
	)
{
	CPoint *ppLower = CTestUtils::PpointInt4(memory_pool, iLower);
	CPoint *ppUpper = CTestUtils::PpointInt4(memory_pool, iUpper);

	BOOL is_upper_closed = false;
	if (ppLower->Equals(ppUpper))
	{
		is_upper_closed = true;
	}

	return GPOS_NEW(memory_pool) CBucket(ppLower, ppUpper, true /* is_lower_closed */, is_upper_closed, frequency, distinct);
}

// create an integer bucket with the provider upper/lower bound, frequency and NDV information
CBucket *
CCardinalityTestUtils::PbucketInteger
	(
	IMemoryPool *memory_pool,
	INT iLower,
	INT iUpper,
	BOOL is_lower_closed,
	BOOL is_upper_closed,
	CDouble frequency,
	CDouble distinct
	)
{
	CPoint *ppLower = CTestUtils::PpointInt4(memory_pool, iLower);
	CPoint *ppUpper = CTestUtils::PpointInt4(memory_pool, iUpper);

	return GPOS_NEW(memory_pool) CBucket(ppLower, ppUpper, is_lower_closed, is_upper_closed, frequency, distinct);
}

// create a singleton bucket containing a boolean m_bytearray_value
CBucket *
CCardinalityTestUtils::PbucketSingletonBoolVal
	(
	IMemoryPool *memory_pool,
	BOOL fValue,
	CDouble frequency
	)
{
	CPoint *ppLower = CTestUtils::PpointBool(memory_pool, fValue);

	// lower bound is also upper bound
	ppLower->AddRef();
	return GPOS_NEW(memory_pool) CBucket(ppLower, ppLower, true /* fClosedUpper */, true /* fClosedUpper */, frequency, 1.0);
}

// helper function to generate integer histogram based on the NDV and bucket information provided
CHistogram*
CCardinalityTestUtils::PhistInt4Remain
	(
	IMemoryPool *memory_pool,
	ULONG num_of_buckets,
	CDouble dNDVPerBucket,
	BOOL fNullFreq,
	CDouble num_NDV_remain
	)
{
	// generate histogram of the form [0, 100), [100, 200), [200, 300) ...
	BucketArray *histogram_buckets = GPOS_NEW(memory_pool) BucketArray(memory_pool);
	for (ULONG idx = 0; idx < num_of_buckets; idx++)
	{
		INT iLower = INT(idx * 100);
		INT iUpper = INT((idx + 1) * 100);
		CDouble frequency(0.1);
		CDouble distinct = dNDVPerBucket;
		CBucket *bucket = PbucketIntegerClosedLowerBound(memory_pool, iLower, iUpper, frequency, distinct);
		histogram_buckets->Append(bucket);
	}

	CDouble freq = CStatisticsUtils::GetFrequency(histogram_buckets);
	CDouble null_freq(0.0);
	if (fNullFreq && 1 > freq)
	{
		null_freq = 0.1;
		freq = freq + null_freq;
	}

	CDouble freq_remaining = (1 - freq);
	if (freq_remaining < CStatistics::Epsilon || num_NDV_remain < CStatistics::Epsilon)
	{
		freq_remaining = CDouble(0.0);
	}

	return GPOS_NEW(memory_pool) CHistogram(histogram_buckets, true, null_freq, num_NDV_remain, freq_remaining);
}

// helper function to generate an example int histogram
CHistogram*
CCardinalityTestUtils::PhistExampleInt4
	(
	IMemoryPool *memory_pool
	)
{
	// generate histogram of the form [0, 10), [10, 20), [20, 30) ... [80, 90)
	BucketArray *histogram_buckets = GPOS_NEW(memory_pool) BucketArray(memory_pool);
	for (ULONG idx = 0; idx < 9; idx++)
	{
		INT iLower = INT(idx * 10);
		INT iUpper = iLower + INT(10);
		CDouble frequency(0.1);
		CDouble distinct(4.0);
		CBucket *bucket = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, iLower, iUpper, frequency, distinct);
		histogram_buckets->Append(bucket);
	}

	// add an additional singleton bucket [100, 100]
	histogram_buckets->Append(CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, 100, 100, 0.1, 1.0));

	return  GPOS_NEW(memory_pool) CHistogram(histogram_buckets);
}

// helper function to generates example bool histogram
CHistogram*
CCardinalityTestUtils::PhistExampleBool
	(
	IMemoryPool *memory_pool
	)
{
	BucketArray *histogram_buckets = GPOS_NEW(memory_pool) BucketArray(memory_pool);
	CBucket *pbucketFalse = CCardinalityTestUtils::PbucketSingletonBoolVal(memory_pool, false, 0.1);
	CBucket *pbucketTrue = CCardinalityTestUtils::PbucketSingletonBoolVal(memory_pool, true, 0.2);
	histogram_buckets->Append(pbucketFalse);
	histogram_buckets->Append(pbucketTrue);
	return  GPOS_NEW(memory_pool) CHistogram(histogram_buckets);
}

// helper function to generate a point from an encoded m_bytearray_value of specific datatype
CPoint *
CCardinalityTestUtils::PpointGeneric
	(
	IMemoryPool *memory_pool,
	OID oid,
	CWStringDynamic *pstrEncodedValue,
	LINT value
	)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	IMDId *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(oid);
	IDatum *datum = CTestUtils::CreateGenericDatum(memory_pool, md_accessor, mdid, pstrEncodedValue, value);
	CPoint *point = GPOS_NEW(memory_pool) CPoint(datum);

	return point;
}

// helper function to generate a point of numeric datatype
CPoint *
CCardinalityTestUtils::PpointNumeric
	(
	IMemoryPool *memory_pool,
	CWStringDynamic *pstrEncodedValue,
	CDouble value
	)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(CMDIdGPDB::m_mdid_numeric);
	const IMDType *pmdtype = md_accessor->Pmdtype(mdid);

	ULONG ulbaSize = 0;
	BYTE *data = CDXLUtils::DecodeByteArrayFromString(memory_pool, pstrEncodedValue, &ulbaSize);

	CDXLDatumStatsDoubleMappable *datum_dxl = GPOS_NEW(memory_pool) CDXLDatumStatsDoubleMappable
											(
											memory_pool,
											mdid,
											default_type_modifier,
											pmdtype->IsPassedByValue() /*is_const_by_val*/,
											false /*is_const_null*/,
											data,
											ulbaSize,
											value
											);

	IDatum *datum = pmdtype->GetDatumForDXLDatum(memory_pool, datum_dxl);
	CPoint *point = GPOS_NEW(memory_pool) CPoint(datum);
	datum_dxl->Release();

	return point;
}

// helper function to print the bucket object
void
CCardinalityTestUtils::PrintBucket
	(
	IMemoryPool *memory_pool,
	const char *pcPrefix,
	const CBucket *bucket
	)
{
	CWStringDynamic str(memory_pool);
	COstreamString oss(&str);

	oss << pcPrefix << " = ";
	bucket->OsPrint(oss);
	oss << std::endl;
	GPOS_TRACE(str.GetBuffer());
}

// helper function to print histogram object
void
CCardinalityTestUtils::PrintHist
	(
	IMemoryPool *memory_pool,
	const char *pcPrefix,
	const CHistogram *histogram
	)
{
	CWStringDynamic str(memory_pool);
	COstreamString oss(&str);

	oss << pcPrefix << " = ";
	histogram->OsPrint(oss);
	oss << std::endl;
	GPOS_TRACE(str.GetBuffer());
}

// helper function to print the statistics object
void
CCardinalityTestUtils::PrintStats
	(
	IMemoryPool *memory_pool,
	const CStatistics *stats
	)
{
	CWStringDynamic str(memory_pool);
	COstreamString oss(&str);

	oss << "Statistics = ";
	stats->OsPrint(oss);
	oss << std::endl;
	GPOS_TRACE(str.GetBuffer());

}


// EOF
