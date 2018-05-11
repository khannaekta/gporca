//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Inc.
//
//	@filename:
//		CHistogramTest.cpp
//
//	@doc:
//		Testing operations on histogram objects
//---------------------------------------------------------------------------

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include <stdint.h>

#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/statistics/CPoint.h"
#include "naucrates/statistics/CHistogram.h"

#include "unittest/base.h"
#include "unittest/dxl/statistics/CCardinalityTestUtils.h"
#include "unittest/dxl/statistics/CHistogramTest.h"
#include "unittest/gpopt/CTestUtils.h"

using namespace gpopt;

// unittest for statistics objects
GPOS_RESULT
CHistogramTest::EresUnittest()
{
	// tests that use shared optimization context
	CUnittest rgutSharedOptCtxt[] =
		{
		GPOS_UNITTEST_FUNC(CHistogramTest::EresUnittest_CHistogramInt4),
		GPOS_UNITTEST_FUNC(CHistogramTest::EresUnittest_CHistogramBool),
		GPOS_UNITTEST_FUNC(CHistogramTest::EresUnittest_Skew),
		GPOS_UNITTEST_FUNC(CHistogramTest::EresUnittest_CHistogramValid)
		};

	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(memory_pool, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc(memory_pool, &mda, NULL /* pceeval */, CTestUtils::GetCostModel(memory_pool));

	return CUnittest::EresExecute(rgutSharedOptCtxt, GPOS_ARRAY_SIZE(rgutSharedOptCtxt));
}

// histogram of int4
GPOS_RESULT
CHistogramTest::EresUnittest_CHistogramInt4()
{
	// create memory pool
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	// original histogram
	CHistogram *histogram = CCardinalityTestUtils::PhistExampleInt4(memory_pool);
	CCardinalityTestUtils::PrintHist(memory_pool, "histogram", histogram);

	// test edge case of MakeBucketGreaterThan
	CPoint *ppoint0 = CTestUtils::PpointInt4(memory_pool, 9);
	CHistogram *phist0 = histogram->MakeHistogramFilter(memory_pool, CStatsPred::EstatscmptG, ppoint0);
	CCardinalityTestUtils::PrintHist(memory_pool, "phist0", phist0);
	GPOS_RTL_ASSERT(phist0->Buckets() == 9);

	CPoint *point1 = CTestUtils::PpointInt4(memory_pool, 35);
	CHistogram *histogram1 = histogram->MakeHistogramFilter(memory_pool, CStatsPred::EstatscmptL, point1);
	CCardinalityTestUtils::PrintHist(memory_pool, "histogram1", histogram1);
	GPOS_RTL_ASSERT(histogram1->Buckets() == 4);

	// edge case where point is equal to upper bound
	CPoint *point2 = CTestUtils::PpointInt4(memory_pool, 50);
	CHistogram *histogram2 = histogram->MakeHistogramFilter(memory_pool, CStatsPred::EstatscmptL,point2);
	CCardinalityTestUtils::PrintHist(memory_pool, "histogram2", histogram2);
	GPOS_RTL_ASSERT(histogram2->Buckets() == 5);

	// equality check
	CPoint *point3 = CTestUtils::PpointInt4(memory_pool, 100);
	CHistogram *phist3 = histogram->MakeHistogramFilter(memory_pool, CStatsPred::EstatscmptEq, point3);
	CCardinalityTestUtils::PrintHist(memory_pool, "phist3", phist3);
	GPOS_RTL_ASSERT(phist3->Buckets() == 1);

	// normalized output after filter
	CPoint *ppoint4 = CTestUtils::PpointInt4(memory_pool, 100);
	CDouble scale_factor(0.0);
	CHistogram *phist4 = histogram->MakeHistogramFilterNormalize(memory_pool, CStatsPred::EstatscmptEq, ppoint4, &scale_factor);
	CCardinalityTestUtils::PrintHist(memory_pool, "phist4", phist4);
	GPOS_RTL_ASSERT(phist4->IsValid());

	// lasj
	CHistogram *phist5 = histogram->MakeLASJHistogram(memory_pool, CStatsPred::EstatscmptEq, histogram2);
	CCardinalityTestUtils::PrintHist(memory_pool, "phist5", phist5);
	GPOS_RTL_ASSERT(phist5->Buckets() == 5);

	// inequality check
	CHistogram *phist6 = histogram->MakeHistogramFilter(memory_pool, CStatsPred::EstatscmptNEq, point2);
	CCardinalityTestUtils::PrintHist(memory_pool, "phist6", phist6);
	GPOS_RTL_ASSERT(phist6->Buckets() == 10);

	// histogram with null fraction and remaining tuples
	CHistogram *phist7 = PhistExampleInt4Remain(memory_pool);
	CCardinalityTestUtils::PrintHist(memory_pool, "phist7", phist7);
	CPoint *ppoint5 = CTestUtils::PpointInt4(memory_pool, 20);

	// equality check, hitting remaining tuples
	CHistogram *phist8 = phist7->MakeHistogramFilter(memory_pool, CStatsPred::EstatscmptEq, point3);
	GPOS_RTL_ASSERT(fabs((phist8->GetFrequency() - 0.2).Get()) < CStatistics::Epsilon);
	GPOS_RTL_ASSERT(fabs((phist8->GetNumDistinct() - 1.0).Get()) < CStatistics::Epsilon);

	// greater than, hitting remaining tuples
	CHistogram *phist9 = phist7->MakeHistogramFilter(memory_pool, CStatsPred::EstatscmptG, point1);
	CCardinalityTestUtils::PrintHist(memory_pool, "phist9", phist9);
	GPOS_RTL_ASSERT(fabs((phist9->GetFrequency() - 0.26).Get()) < CStatistics::Epsilon);
	GPOS_RTL_ASSERT(fabs((phist9->GetNumDistinct() - 1.8).Get()) < CStatistics::Epsilon);

	// equality join, hitting remaining tuples
	CHistogram *phist10 = phist7->MakeJoinHistogram(memory_pool, CStatsPred::EstatscmptEq, phist7);
	GPOS_RTL_ASSERT(phist10->Buckets() == 5);
	GPOS_RTL_ASSERT(fabs((phist10->GetDistinctRemain() - 2.0).Get()) < CStatistics::Epsilon);
	GPOS_RTL_ASSERT(fabs((phist10->GetFreqRemain() - 0.08).Get()) < CStatistics::Epsilon);

	// clean up
	ppoint0->Release();
	point1->Release();
	point2->Release();
	point3->Release();
	ppoint4->Release();
	ppoint5->Release();
	GPOS_DELETE(histogram);
	GPOS_DELETE(phist0);
	GPOS_DELETE(histogram1);
	GPOS_DELETE(histogram2);
	GPOS_DELETE(phist3);
	GPOS_DELETE(phist4);
	GPOS_DELETE(phist5);
	GPOS_DELETE(phist6);
	GPOS_DELETE(phist7);
	GPOS_DELETE(phist8);
	GPOS_DELETE(phist9);
	GPOS_DELETE(phist10);

	return GPOS_OK;
}

// histogram on bool
GPOS_RESULT
CHistogramTest::EresUnittest_CHistogramBool()
{
	// create memory pool
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	// generate histogram of the form [false, false), [true,true)
	BucketArray *histogram_buckets = GPOS_NEW(memory_pool) BucketArray(memory_pool);
	CBucket *pbucketFalse = CCardinalityTestUtils::PbucketSingletonBoolVal(memory_pool, false, 0.1);
	CBucket *pbucketTrue = CCardinalityTestUtils::PbucketSingletonBoolVal(memory_pool, false, 0.9);
	histogram_buckets->Append(pbucketFalse);
	histogram_buckets->Append(pbucketTrue);
	CHistogram *histogram =  GPOS_NEW(memory_pool) CHistogram(histogram_buckets);

	// equality check
	CPoint *point1 = CTestUtils::PpointBool(memory_pool, false);
	CDouble scale_factor(0.0);
	CHistogram *histogram1 = histogram->MakeHistogramFilterNormalize(memory_pool, CStatsPred::EstatscmptEq, point1, &scale_factor);
	CCardinalityTestUtils::PrintHist(memory_pool, "histogram1", histogram1);
	GPOS_RTL_ASSERT(histogram1->Buckets() == 1);

	// clean up
	point1->Release();
	GPOS_DELETE(histogram);
	GPOS_DELETE(histogram1);

	return GPOS_OK;
}


// check for well-formed histogram. Expected to fail
GPOS_RESULT
CHistogramTest::EresUnittest_CHistogramValid()
{
	// create memory pool
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	BucketArray *histogram_buckets = GPOS_NEW(memory_pool) BucketArray(memory_pool);

	// generate histogram of the form [0, 10), [9, 20)
	CBucket *bucket1 = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, 0, 10, 0.1, 2.0);
	histogram_buckets->Append(bucket1);
	CBucket *bucket2 = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, 9, 20, 0.1, 2.0);
	histogram_buckets->Append(bucket2);

	// original histogram
	CHistogram *histogram =  GPOS_NEW(memory_pool) CHistogram(histogram_buckets);

	// create an auto object
	CAutoP<CHistogram> ahist;
	ahist = histogram;

	{
		CAutoTrace at(memory_pool);
		at.Os() << std::endl << "Invalid Histogram"<< std::endl;
		histogram->OsPrint(at.Os());
	}

	if(histogram->IsValid())
	{
		return GPOS_FAILED;
	}

	return GPOS_OK;
}

// generates example int histogram having tuples not covered by buckets,
// including null fraction and nDistinctRemain
CHistogram*
CHistogramTest::PhistExampleInt4Remain
	(
	IMemoryPool *memory_pool
	)
{
	// generate histogram of the form [0, 0], [10, 10], [20, 20] ...
	BucketArray *histogram_buckets = GPOS_NEW(memory_pool) BucketArray(memory_pool);
	for (ULONG idx = 0; idx < 5; idx++)
	{
		INT iLower = INT(idx * 10);
		INT iUpper = iLower;
		CDouble frequency(0.1);
		CDouble distinct(1.0);
		CBucket *bucket = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, iLower, iUpper, frequency, distinct);
		histogram_buckets->Append(bucket);
	}

	return GPOS_NEW(memory_pool) CHistogram(histogram_buckets, true, 0.1 /*null_freq*/, 2.0 /*distinct_remaining*/, 0.4 /*freq_remaining*/);
}

// basis skew test
GPOS_RESULT
CHistogramTest::EresUnittest_Skew()
{
	// create memory pool
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	CBucket *bucket1 = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, 1, 100, CDouble(0.6), CDouble(100.0));
	CBucket *bucket2 = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, 101, 200, CDouble(0.2), CDouble(100.0));
	CBucket *pbucket3 = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, 201, 300, CDouble(0.2), CDouble(100.0));
	CBucket *pbucket4 = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, 301, 400, CDouble(0.2), CDouble(100.0));
	CBucket *pbucket5 = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, 401, 500, CDouble(0.2), CDouble(100.0));
	CBucket *pbucket6 = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, 501, 600, CDouble(0.2), CDouble(100.0));
	CBucket *pbucket7 = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, 601, 700, CDouble(0.2), CDouble(100.0));
	CBucket *pbucket8 = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, 701, 800, CDouble(0.2), CDouble(100.0));

	BucketArray *pdrgppbucket1 = GPOS_NEW(memory_pool) BucketArray(memory_pool);
	pdrgppbucket1->Append(bucket1);
	pdrgppbucket1->Append(bucket2);
	pdrgppbucket1->Append(pbucket3);
	CHistogram *histogram1 =  GPOS_NEW(memory_pool) CHistogram(pdrgppbucket1);

	BucketArray *pdrgppbucket2 = GPOS_NEW(memory_pool) BucketArray(memory_pool);
	pdrgppbucket2->Append(pbucket4);
	pdrgppbucket2->Append(pbucket5);
	pdrgppbucket2->Append(pbucket6);
	pdrgppbucket2->Append(pbucket7);
	pdrgppbucket2->Append(pbucket8);
	CHistogram *histogram2 =  GPOS_NEW(memory_pool) CHistogram(pdrgppbucket2);
	GPOS_ASSERT(histogram1->GetSkew() > histogram2->GetSkew());

	{
		CAutoTrace at(memory_pool);
		histogram1->OsPrint(at.Os());
		histogram2->OsPrint(at.Os());
	}

	GPOS_DELETE(histogram1);
	GPOS_DELETE(histogram2);

	return GPOS_OK;
}

// EOF

