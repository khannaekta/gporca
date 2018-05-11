//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CStatisticsTest.cpp
//
//	@doc:
//		Tests for CPoint
//---------------------------------------------------------------------------

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include <stdint.h>

#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "naucrates/statistics/CPoint.h"
#include "naucrates/statistics/CBucket.h"
#include "naucrates/statistics/CHistogram.h"
#include "naucrates/statistics/CStatistics.h"
#include "naucrates/statistics/CStatisticsUtils.h"
#include "naucrates/statistics/CStatisticsUtils.h"
#include "naucrates/statistics/CLimitStatsProcessor.h"
#include "naucrates/statistics/CGroupByStatsProcessor.h"
#include "naucrates/statistics/CUnionAllStatsProcessor.h"
#include "naucrates/statistics/CFilterStatsProcessor.h"

#include "naucrates/base/CDatumGenericGPDB.h"
#include "naucrates/base/CDatumInt4GPDB.h"
#include "naucrates/base/CDatumBoolGPDB.h"

#include "gpopt/base/CQueryContext.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CScalarProjectElement.h"

#include "naucrates/dxl/CDXLUtils.h"

#include "unittest/base.h"
#include "unittest/dxl/statistics/CCardinalityTestUtils.h"
#include "unittest/dxl/statistics/CStatisticsTest.h"
#include "unittest/gpopt/CTestUtils.h"

#include "naucrates/md/IMDType.h"
#include "naucrates/md/CMDTypeGenericGPDB.h"

using namespace gpopt;

const CHAR *
szQuerySelect = "../data/dxl/statistics/SelectQuery.xml";
const CHAR *
szPlanSelect = "../data/dxl/statistics/SelectPlan.xml";

// unittest for statistics objects
GPOS_RESULT
CStatisticsTest::EresUnittest()
{
	// tests that use shared optimization context
	CUnittest rgutSharedOptCtxt[] =
		{
		GPOS_UNITTEST_FUNC(CStatisticsTest::EresUnittest_CStatisticsBasic),
		GPOS_UNITTEST_FUNC(CStatisticsTest::EresUnittest_UnionAll),
		// TODO,  Mar 18 2013 temporarily disabling the test
		// GPOS_UNITTEST_FUNC(CStatisticsTest::EresUnittest_CStatisticsSelectDerivation),
		};

	// tests that use separate optimization contexts
	CUnittest rgutSeparateOptCtxt[] =
		{
		GPOS_UNITTEST_FUNC(CStatisticsTest::EresUnittest_GbAggWithRepeatedGbCols),
		};

	// run tests with shared optimization context first
	GPOS_RESULT eres = GPOS_FAILED;
	{
		CAutoMemoryPool amp;
		IMemoryPool *memory_pool = amp.Pmp();

		// setup a file-based provider
		CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
		pmdp->AddRef();
		CMDAccessor mda(memory_pool, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

		// install opt context in TLS
		CAutoOptCtxt aoc
						(
						memory_pool,
						&mda,
						NULL /* pceeval */,
						CTestUtils::GetCostModel(memory_pool)
						);

		eres = CUnittest::EresExecute(rgutSharedOptCtxt, GPOS_ARRAY_SIZE(rgutSharedOptCtxt));

		if (GPOS_FAILED == eres)
		{
			return eres;
		}
	}

	// run tests with separate optimization contexts
	return CUnittest::EresExecute(rgutSeparateOptCtxt, GPOS_ARRAY_SIZE(rgutSeparateOptCtxt));
}

// testing statistical operations on Union All;
GPOS_RESULT
CStatisticsTest::EresUnittest_UnionAll()
{
	// create memory pool
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	SStatsUnionAllSTestCase rgstatsunionalltc[] =
	{
		{"../data/dxl/statistics/UnionAll-Input-1.xml", "../data/dxl/statistics/UnionAll-Output-1.xml"},
	};

	const ULONG ulTestCases = GPOS_ARRAY_SIZE(rgstatsunionalltc);
	for (ULONG i = 0; i < ulTestCases; i++)
	{
		SStatsUnionAllSTestCase elem = rgstatsunionalltc[i];

		// read input/output DXL file
		CHAR *szDXLInput = CDXLUtils::Read(memory_pool, elem.m_szInputFile);
		CHAR *szDXLOutput = CDXLUtils::Read(memory_pool, elem.m_szOutputFile);

		GPOS_CHECK_ABORT;

		// parse the input statistics objects
		DXLStatsDerivedRelArray *dxl_derived_rel_stats_array = CDXLUtils::ParseDXLToStatsDerivedRelArray(memory_pool, szDXLInput, NULL);
		CStatisticsArray *pdrgpstatBefore = CDXLUtils::ParseDXLToOptimizerStatisticObjArray(memory_pool, md_accessor, dxl_derived_rel_stats_array);
		dxl_derived_rel_stats_array->Release();

		GPOS_ASSERT(NULL != pdrgpstatBefore);
		GPOS_ASSERT(2 == pdrgpstatBefore->Size());
		CStatistics *pstats1 = (*pdrgpstatBefore)[0];
		CStatistics *pstats2 = (*pdrgpstatBefore)[1];

		GPOS_CHECK_ABORT;

		ULongPtrArray *pdrgpulColIdOutput = Pdrgpul(memory_pool, 1);
		ULongPtrArray *pdrgpulColIdInput1 = Pdrgpul(memory_pool, 1);
		ULongPtrArray *pdrgpulColIdInput2 = Pdrgpul(memory_pool, 2);

		CStatistics *pstatsOutput = CUnionAllStatsProcessor::CreateStatsForUnionAll(memory_pool, pstats1, pstats2, pdrgpulColIdOutput, pdrgpulColIdInput1, pdrgpulColIdInput2);

		GPOS_ASSERT(NULL != pstatsOutput);

		CStatisticsArray *pdrgpstatOutput = GPOS_NEW(memory_pool) CStatisticsArray(memory_pool);
		pdrgpstatOutput->Append(pstatsOutput);

		// serialize and compare against expected stats
		CWStringDynamic *pstrOutput = CDXLUtils::SerializeStatistics
													(
													memory_pool,
													md_accessor,
													pdrgpstatOutput,
													true /*serialize_header_footer*/,
													true /*indentation*/
													);
		CWStringDynamic dstrExpected(memory_pool);
		dstrExpected.AppendFormat(GPOS_WSZ_LIT("%s"), szDXLOutput);

		GPOS_RESULT eres = GPOS_OK;
		CWStringDynamic str(memory_pool);
		COstreamString oss(&str);

		// compare the two dxls
		if (!pstrOutput->Equals(&dstrExpected))
		{
			oss << "Output does not match expected DXL document" << std::endl;
			oss << "Actual: " << std::endl;
			oss << pstrOutput->GetBuffer() << std::endl;
			oss << "Expected: " << std::endl;
			oss << dstrExpected.GetBuffer() << std::endl;
			GPOS_TRACE(str.GetBuffer());
			
			eres = GPOS_FAILED;
		}


		// clean up
		pdrgpstatBefore->Release();
		pdrgpstatOutput->Release();

		GPOS_DELETE_ARRAY(szDXLInput);
		GPOS_DELETE_ARRAY(szDXLOutput);
		GPOS_DELETE(pstrOutput);

		if (GPOS_FAILED == eres)
		{
			return eres;
		}
	}

	return GPOS_OK;
}

// gbAgg test when grouping on repeated columns
GPOS_RESULT
CStatisticsTest::EresUnittest_GbAggWithRepeatedGbCols()
{
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(memory_pool, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc
					(
					memory_pool,
					&mda,
					NULL /* pceeval */,
					CTestUtils::GetCostModel(memory_pool)
					);

	CExpression *pexpr = CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>(memory_pool);
	CDrvdPropRelational *pdprel = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive());
	CColRefSet *colrefs = pdprel->PcrsOutput();

	// create first GbAgg expression: GbAgg on top of given expression
	DrgPcr *pdrgpcr1 = GPOS_NEW(memory_pool) DrgPcr(memory_pool);
	pdrgpcr1->Append(colrefs->PcrFirst());
	CExpression *pexprGbAgg1 =
		CUtils::PexprLogicalGbAggGlobal(memory_pool, pdrgpcr1, pexpr, GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool)));

	// create second GbAgg expression: GbAgg with repeated base column on top of given expression
	DrgPcr *pdrgpcr2 = GPOS_NEW(memory_pool) DrgPcr(memory_pool);
	pdrgpcr2->Append(colrefs->PcrFirst());
	pdrgpcr2->Append(colrefs->PcrFirst());
	pexpr->AddRef();
	CExpression *pexprGbAgg2 =
			CUtils::PexprLogicalGbAggGlobal(memory_pool, pdrgpcr2, pexpr, GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool)));

	// create third GbAgg expression: GbAgg with a repeated projected base column on top of given expression
	pexpr->AddRef();
	CExpression *pexprPrj = CUtils::PexprAddProjection(memory_pool, pexpr, CUtils::PexprScalarIdent(memory_pool, colrefs->PcrFirst()));
	CColRef *pcrComputed = CScalarProjectElement::PopConvert((*(*pexprPrj)[1])[0]->Pop())->Pcr();
	DrgPcr *pdrgpcr3 = GPOS_NEW(memory_pool) DrgPcr(memory_pool);
	pdrgpcr3->Append(colrefs->PcrFirst());
	pdrgpcr3->Append(pcrComputed);
	CExpression *pexprGbAgg3 =
			CUtils::PexprLogicalGbAggGlobal(memory_pool, pdrgpcr3, pexprPrj, GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool)));

	// derive stats on different GbAgg expressions
	CReqdPropRelational *prprel = GPOS_NEW(memory_pool) CReqdPropRelational(GPOS_NEW(memory_pool) CColRefSet(memory_pool));
	(void) pexprGbAgg1->PstatsDerive(prprel, NULL /* stats_ctxt */);
	(void) pexprGbAgg2->PstatsDerive(prprel, NULL /* stats_ctxt */);
	(void) pexprGbAgg3->PstatsDerive(prprel, NULL /* stats_ctxt */);

	BOOL fRows1EqualRows2 = (pexprGbAgg1->Pstats()->Rows() == pexprGbAgg2->Pstats()->Rows());
	BOOL fRows2EqualRows3 = (pexprGbAgg2->Pstats()->Rows() == pexprGbAgg3->Pstats()->Rows());

	{
		CAutoTrace at(memory_pool);
		at.Os() << std::endl << "pexprGbAgg1:" <<  std::endl << *pexprGbAgg1 << std::endl;
		at.Os() << std::endl << "pexprGbAgg2:" <<  std::endl << *pexprGbAgg2 << std::endl;
		at.Os() << std::endl << "pexprGbAgg3:" <<  std::endl << *pexprGbAgg3 << std::endl;
	}

	// cleanup
	pexprGbAgg1->Release();
	pexprGbAgg2->Release();
	pexprGbAgg3->Release();
	prprel->Release();

	if (fRows1EqualRows2 && fRows2EqualRows3)
	{
		return GPOS_OK;
	}

	return GPOS_FAILED;
}

// generates example int histogram corresponding to dimension table
CHistogram*
CStatisticsTest::PhistExampleInt4Dim
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
		CDouble distinct(10.0);
		CBucket *bucket = CCardinalityTestUtils::PbucketIntegerClosedLowerBound(memory_pool, iLower, iUpper, frequency, distinct);
		histogram_buckets->Append(bucket);
	}

	return  GPOS_NEW(memory_pool) CHistogram(histogram_buckets);
}

// create a table descriptor with two columns having the given names.
CTableDescriptor *
CStatisticsTest::PtabdescTwoColumnSource
	(
	IMemoryPool *memory_pool,
	const CName &nameTable,
	const IMDTypeInt4 *pmdtype,
	const CWStringConst &strColA,
	const CWStringConst &strColB
	)
{
	CTableDescriptor *ptabdesc = GPOS_NEW(memory_pool) CTableDescriptor
									(
									memory_pool,
									GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1, 1),
									nameTable,
									false, // convert_hash_to_random
									IMDRelation::EreldistrRandom,
									IMDRelation::ErelstorageHeap,
									0  // ulExecuteAsUser
									);

	for (ULONG i = 0; i < 2; i++)
	{
		// create a shallow constant string to embed in a name
		const CWStringConst *str_name = &strColA;
		if (0 < i)
		{
			str_name = &strColB;
		}
		CName nameColumn(str_name);

		CColumnDescriptor *pcoldesc = GPOS_NEW(memory_pool) CColumnDescriptor
											(
											memory_pool,
											pmdtype,
											default_type_modifier,
											nameColumn,
											i + 1,
											false /*is_nullable*/
											);
		ptabdesc->AddColumn(pcoldesc);
	}

	return ptabdesc;
}

// basic statistics test
GPOS_RESULT
CStatisticsTest::EresUnittest_CStatisticsBasic()
{
	// create memory pool
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

	const IMDTypeInt4 *pmdtypeint4 = COptCtxt::PoctxtFromTLS()->Pmda()->PtMDType<IMDTypeInt4>();

	CWStringConst strRelAlias(GPOS_WSZ_LIT("Rel1"));
	CWStringConst strColA(GPOS_WSZ_LIT("a"));
	CWStringConst strColB(GPOS_WSZ_LIT("b"));
	CTableDescriptor *ptabdesc =
			PtabdescTwoColumnSource(memory_pool, CName(&strRelAlias), pmdtypeint4, strColA, strColB);
	CExpression *pexprGet = CTestUtils::PexprLogicalGet(memory_pool, ptabdesc, &strRelAlias);

	if (NULL == col_factory->LookupColRef(1 /*id*/))
	{
		// create column references for grouping columns
		(void) col_factory->PcrCreate
				(
				pmdtypeint4,
				default_type_modifier,
				0 /* attno */,
				false /*IsNullable*/,
				1 /* id */,
				CName(&strColA),
				pexprGet->Pop()->UlOpId()
				);
	}

	if (NULL == col_factory->LookupColRef(2 /*id*/))
	{
		(void) col_factory->PcrCreate
				(
				pmdtypeint4,
				default_type_modifier,
				1 /* attno */,
				false /*IsNullable*/,
				2 /* id */,
				CName(&strColB),
				pexprGet->Pop()->UlOpId()
				);
	}

	// create hash map from colid -> histogram
	UlongHistogramHashMap *col_histogram_mapping = GPOS_NEW(memory_pool) UlongHistogramHashMap(memory_pool);

	// generate bool histogram for column 1
	col_histogram_mapping->Insert(GPOS_NEW(memory_pool) ULONG(1), CCardinalityTestUtils::PhistExampleBool(memory_pool));

	// generate int histogram for column 2
	col_histogram_mapping->Insert(GPOS_NEW(memory_pool) ULONG(2), CCardinalityTestUtils::PhistExampleInt4(memory_pool));

	// array capturing columns for which width information is available
	UlongDoubleHashMap *col_id_width_mapping = GPOS_NEW(memory_pool) UlongDoubleHashMap(memory_pool);

	// width for boolean
	col_id_width_mapping->Insert(GPOS_NEW(memory_pool) ULONG(1), GPOS_NEW(memory_pool) CDouble(1.0));

	// width for int
	col_id_width_mapping->Insert(GPOS_NEW(memory_pool) ULONG(2), GPOS_NEW(memory_pool) CDouble(4.0));

	CStatistics *stats = GPOS_NEW(memory_pool) CStatistics(memory_pool, col_histogram_mapping, col_id_width_mapping, 1000.0 /* rows */, false /* is_empty */);
	stats->Rows();

	GPOS_TRACE(GPOS_WSZ_LIT("stats"));

	// before stats
	CCardinalityTestUtils::PrintStats(memory_pool, stats);

	// create a filter: column 1: [25,45), column 2: [true, true)
	StatsPredPtrArry *pdrgpstatspred = Pdrgpstatspred1(memory_pool);

	CStatsPredConj *pred_stats = GPOS_NEW(memory_pool) CStatsPredConj(pdrgpstatspred);
	CStatistics *pstats1 = CFilterStatsProcessor::MakeStatsFilter(memory_pool, stats, pred_stats, true /* do_cap_NDVs */);
	pstats1->Rows();

	GPOS_TRACE(GPOS_WSZ_LIT("pstats1 after filter"));

	// after stats
	CCardinalityTestUtils::PrintStats(memory_pool, pstats1);

	// create another statistics structure with a single int4 column with id 10
	UlongHistogramHashMap *phmulhist2 = GPOS_NEW(memory_pool) UlongHistogramHashMap(memory_pool);
	phmulhist2->Insert(GPOS_NEW(memory_pool) ULONG(10), PhistExampleInt4Dim(memory_pool));

	UlongDoubleHashMap *phmuldoubleWidth2 = GPOS_NEW(memory_pool) UlongDoubleHashMap(memory_pool);
	phmuldoubleWidth2->Insert(GPOS_NEW(memory_pool) ULONG(10), GPOS_NEW(memory_pool) CDouble(4.0));

	CStatistics *pstats2 = GPOS_NEW(memory_pool) CStatistics(memory_pool, phmulhist2, phmuldoubleWidth2, 100.0 /* rows */, false /* is_empty */);

	GPOS_TRACE(GPOS_WSZ_LIT("pstats2"));
	CCardinalityTestUtils::PrintStats(memory_pool, pstats2);

	// join stats with pstats2
	CStatsPredJoin *pstatspredjoin = GPOS_NEW(memory_pool) CStatsPredJoin(2, CStatsPred::EstatscmptEq, 10);
	StatsPredJoinArray *join_preds_stats = GPOS_NEW(memory_pool) StatsPredJoinArray(memory_pool);
	join_preds_stats->Append(pstatspredjoin);
	CStatistics *pstats3 = stats->CalcInnerJoinStats(memory_pool, pstats2, join_preds_stats);

	GPOS_TRACE(GPOS_WSZ_LIT("pstats3 = stats JOIN pstats2 on (col2 = col10)"));
	// after stats
	CCardinalityTestUtils::PrintStats(memory_pool, pstats3);

	// group by stats on columns 1 and 2
	ULongPtrArray *GCs = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
	GCs->Append(GPOS_NEW(memory_pool) ULONG(1));
	GCs->Append(GPOS_NEW(memory_pool) ULONG(2));

	ULongPtrArray *aggs = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
	CStatistics *pstats4 = CGroupByStatsProcessor::CalcGroupByStats(memory_pool, stats, GCs, aggs, NULL /*keys*/);

	GPOS_TRACE(GPOS_WSZ_LIT("pstats4 = stats group by"));
	CCardinalityTestUtils::PrintStats(memory_pool, pstats4);

	// LASJ stats
	CStatistics *pstats5 = stats->CalcLASJoinStats(memory_pool, pstats2, join_preds_stats, true /* DoIgnoreLASJHistComputation */);

	GPOS_TRACE(GPOS_WSZ_LIT("pstats5 = stats LASJ pstats2 on (col2 = col10)"));
	CCardinalityTestUtils::PrintStats(memory_pool, pstats5);

	// union all
	ULongPtrArray *col_ids = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
	col_ids->Append(GPOS_NEW(memory_pool) ULONG(1));
	col_ids->Append(GPOS_NEW(memory_pool) ULONG(2));
	col_ids->AddRef();
	col_ids->AddRef();
	col_ids->AddRef();

	CStatistics *pstats6 = CUnionAllStatsProcessor::CreateStatsForUnionAll(memory_pool, stats, stats, col_ids, col_ids, col_ids);

	GPOS_TRACE(GPOS_WSZ_LIT("pstats6 = pstats1 union all pstats1"));
	CCardinalityTestUtils::PrintStats(memory_pool, pstats6);

	CStatistics *pstats7 = CLimitStatsProcessor::CalcLimitStats(memory_pool, stats, CDouble(4.0));

	GPOS_TRACE(GPOS_WSZ_LIT("pstats7 = stats limit 4"));
	CCardinalityTestUtils::PrintStats(memory_pool, pstats7);

	stats->Release();
	pstats1->Release();
	pstats2->Release();
	pstats3->Release();
	pstats4->Release();
	pstats5->Release();
	pstats6->Release();
	pstats7->Release();
	pred_stats->Release();
	join_preds_stats->Release();
	GCs->Release();
	aggs->Release();
	col_ids->Release();
	pexprGet->Release();

	return GPOS_OK;
}

// create a filter clause
StatsPredPtrArry *
CStatisticsTest::Pdrgpstatspred1
	(
	IMemoryPool *memory_pool
	)
{
	StatsPredPtrArry *pdrgpstatspred = GPOS_NEW(memory_pool) StatsPredPtrArry(memory_pool);

	// col1 = true
	StatsFilterBool(memory_pool, 1, true, pdrgpstatspred);

	// col2 >= 25 and col2 < 35
	StatsFilterInt4(memory_pool, 2, 25, 35, pdrgpstatspred);

	return pdrgpstatspred;
}

// create a filter clause
StatsPredPtrArry *
CStatisticsTest::Pdrgpstatspred2
	(
	IMemoryPool *memory_pool
	)
{
	// contain for filters
	StatsPredPtrArry *pdrgpstatspred = GPOS_NEW(memory_pool) StatsPredPtrArry(memory_pool);

	// create int4 filter column 2: [5,15)::int4
	StatsFilterInt4(memory_pool, 2, 5, 15, pdrgpstatspred);

	// create numeric filter column 3: [1.0, 2.0)::numeric
	CWStringDynamic *pstrLowerNumeric = GPOS_NEW(memory_pool) CWStringDynamic(memory_pool, GPOS_WSZ_LIT("AAAACgAAAQABAA=="));
	CWStringDynamic *pstrUpperNumeric = GPOS_NEW(memory_pool) CWStringDynamic(memory_pool, GPOS_WSZ_LIT("AAAACgAAAQACAA=="));

	StatsFilterNumeric(memory_pool, 3, pstrLowerNumeric, pstrUpperNumeric, CDouble(1.0), CDouble(2.0), pdrgpstatspred);

	GPOS_DELETE(pstrLowerNumeric);
	GPOS_DELETE(pstrUpperNumeric);

	// create a date filter column 4: ['01-01-2012', '01-21-2012')::date
	CWStringDynamic *pstrLowerDate = GPOS_NEW(memory_pool) CWStringDynamic(memory_pool, GPOS_WSZ_LIT("HxEAAA=="));
	CWStringDynamic *pstrUpperDate = GPOS_NEW(memory_pool) CWStringDynamic(memory_pool, GPOS_WSZ_LIT("LREAAA=="));
	LINT lLowerDate = LINT(4383) * LINT(INT64_C(86400000000)); // microseconds per day
	LINT lUpperDate = LINT(4397) * LINT(INT64_C(86400000000)); // microseconds per day
	StatsFilterGeneric(memory_pool, 4, GPDB_DATE, pstrLowerDate, pstrUpperDate, lLowerDate, lUpperDate, pdrgpstatspred);

	GPOS_DELETE(pstrLowerDate);
	GPOS_DELETE(pstrUpperDate);

	// create timestamp filter column 5: ['01-01-2012 00:01:00', '01-01-2012 10:00:00')::timestamp
	CWStringDynamic *pstrLowerTS = GPOS_NEW(memory_pool) CWStringDynamic(memory_pool, GPOS_WSZ_LIT("ACcI7mpYAQA="));
	CWStringDynamic *pstrUpperTS = GPOS_NEW(memory_pool) CWStringDynamic(memory_pool, GPOS_WSZ_LIT("AAg5THNYAQA="));
	LINT lLowerTS = LINT(INT64_C(378691260000000)); // microseconds
	LINT lUpperTS = LINT(INT64_C(378727200000000)); // microseconds

	StatsFilterGeneric(memory_pool, 5, GPDB_TIMESTAMP, pstrLowerTS, pstrUpperTS, lLowerTS, lUpperTS, pdrgpstatspred);

	GPOS_DELETE(pstrLowerTS);
	GPOS_DELETE(pstrUpperTS);

	return pdrgpstatspred;
}

// create a stats filter on integer range
void
CStatisticsTest::StatsFilterInt4
	(
	IMemoryPool *memory_pool,
	ULONG col_id,
	INT iLower,
	INT iUpper,
	StatsPredPtrArry *pdrgpstatspred
	)
{
	CStatsPredPoint *pstatspred1 = GPOS_NEW(memory_pool) CStatsPredPoint
												(
												col_id,
												CStatsPred::EstatscmptGEq,
												CTestUtils::PpointInt4(memory_pool, iLower)
												);

	CStatsPredPoint *pstatspred2 = GPOS_NEW(memory_pool) CStatsPredPoint
												(
												col_id,
												CStatsPred::EstatscmptL,
												CTestUtils::PpointInt4(memory_pool, iUpper)
												);

	pdrgpstatspred->Append(pstatspred1);
	pdrgpstatspred->Append(pstatspred2);
}

// create a stats filter on boolean
void
CStatisticsTest::StatsFilterBool
	(
	IMemoryPool *memory_pool,
	ULONG col_id,
	BOOL fValue,
	StatsPredPtrArry *pdrgpstatspred
	)
{
	CStatsPredPoint *pstatspred1 = GPOS_NEW(memory_pool) CStatsPredPoint
												(
												col_id,
												CStatsPred::EstatscmptEq,
												CTestUtils::PpointBool(memory_pool, fValue)
												);

	pdrgpstatspred->Append(pstatspred1);
}

// create a stats filter on numeric types
void
CStatisticsTest::StatsFilterNumeric
	(
	IMemoryPool *memory_pool,
	ULONG col_id,
	CWStringDynamic *pstrLowerEncoded,
	CWStringDynamic *pstrUpperEncoded,
	CDouble dValLower,
	CDouble dValUpper,
	StatsPredPtrArry *pdrgpstatspred
	)
{
	CStatsPredPoint *pstatspred1 = GPOS_NEW(memory_pool) CStatsPredPoint
												(
												col_id,
												CStatsPred::EstatscmptGEq,
												CCardinalityTestUtils::PpointNumeric(memory_pool, pstrLowerEncoded, dValLower)
												);

	CStatsPredPoint *pstatspred2 = GPOS_NEW(memory_pool) CStatsPredPoint
												(
												col_id,
												CStatsPred::EstatscmptL,
												CCardinalityTestUtils::PpointNumeric(memory_pool, pstrUpperEncoded, dValUpper)
												);

	pdrgpstatspred->Append(pstatspred1);
	pdrgpstatspred->Append(pstatspred2);
}

// create a stats filter on other types
void
CStatisticsTest::StatsFilterGeneric
	(
	IMemoryPool *memory_pool,
	ULONG col_id,
	OID oid,
	CWStringDynamic *pstrLowerEncoded,
	CWStringDynamic *pstrUpperEncoded,
	LINT lValueLower,
	LINT lValueUpper,
	StatsPredPtrArry *pdrgpstatspred
	)
{
	CStatsPredPoint *pstatspred1 = GPOS_NEW(memory_pool) CStatsPredPoint
												(
												col_id,
												CStatsPred::EstatscmptGEq,
												CCardinalityTestUtils::PpointGeneric(memory_pool, oid, pstrLowerEncoded, lValueLower)
												);

	CStatsPredPoint *pstatspred2 = GPOS_NEW(memory_pool) CStatsPredPoint
												(
												col_id,
												CStatsPred::EstatscmptL,
												CCardinalityTestUtils::PpointGeneric(memory_pool, oid, pstrUpperEncoded, lValueUpper)
												);

	pdrgpstatspred->Append(pstatspred1);
	pdrgpstatspred->Append(pstatspred2);
}

// derivation over select query
GPOS_RESULT
CStatisticsTest::EresUnittest_CStatisticsSelectDerivation()
{
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	return
		CTestUtils::EresTranslate
			(
			memory_pool,
			szQuerySelect,
			szPlanSelect,
			true // ignore mismatch in output dxl due to column id differences
			);
}

// EOF
