//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CMiniDumperDXLTest.cpp
//
//	@doc:
//		Test for DXL-based minidumps
//---------------------------------------------------------------------------
#include "gpos/io/COstreamString.h"
#include "gpos/io/COstreamFile.h"
#include "gpos/task/CAutoTraceFlag.h"

#include "naucrates/base/CQueryToDXLResult.h"
#include "naucrates/dxl/CDXLUtils.h"

#include "gpopt/base/CQueryContext.h"
#include "gpopt/engine/CEngine.h"
#include "gpopt/engine/CEnumeratorConfig.h"
#include "gpopt/engine/CStatisticsConfig.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/minidump/CDXLMinidump.h"
#include "gpopt/minidump/CMiniDumperDXL.h"
#include "gpopt/minidump/CMinidumperUtils.h"
#include "gpopt/minidump/CSerializableQuery.h"
#include "gpopt/minidump/CSerializableMDAccessor.h"
#include "gpopt/minidump/CSerializablePlan.h"
#include "gpopt/minidump/CSerializableStackTrace.h"
#include "gpopt/minidump/CSerializableOptimizerConfig.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "gpopt/translate/CTranslatorExprToDXL.h"

#include "unittest/base.h"
#include "unittest/gpopt/minidump/CMiniDumperDXLTest.h"
#include "unittest/gpopt/translate/CTranslatorExprToDXLTest.h"
#include "unittest/gpopt/CTestUtils.h"

#include <fstream>

static
const CHAR *szQueryFile= "../data/dxl/minidump/Query.xml";

//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperDXLTest::EresUnittest
//
//	@doc:
//		Unittest for DXL minidumps
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMiniDumperDXLTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CMiniDumperDXLTest::EresUnittest_Basic),
		GPOS_UNITTEST_FUNC(CMiniDumperDXLTest::EresUnittest_Load),
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}



//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperDXLTest::EresUnittest_Basic
//
//	@doc:
//		Test minidumps in case of an exception
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMiniDumperDXLTest::EresUnittest_Basic()
{
	CAutoMemoryPool amp(CAutoMemoryPool::ElcNone);
	IMemoryPool *memory_pool = amp.Pmp();

	CWStringDynamic minidumpstr(memory_pool);
	COstreamString oss(&minidumpstr);
	CMiniDumperDXL mdrs(memory_pool);
	mdrs.Init(&oss);
	
	CHAR file_name[GPOS_FILE_NAME_BUF_SIZE];

	GPOS_TRY
	{
		CSerializableStackTrace serStackTrace;
		
		// read the dxl document
		CHAR *szQueryDXL = CDXLUtils::Read(memory_pool, szQueryFile);

		// parse the DXL query tree from the given DXL document
		CQueryToDXLResult *ptroutput = 
				CDXLUtils::ParseQueryToQueryDXLTree(memory_pool, szQueryDXL, NULL);
		GPOS_CHECK_ABORT;

		CSerializableQuery serQuery(memory_pool, ptroutput->CreateDXLNode(), ptroutput->GetOutputColumnsDXLArray(), ptroutput->GetCTEProducerDXLArray());
		
		// setup a file-based provider
		CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
		pmdp->AddRef();

		// we need to use an auto pointer for the cache here to ensure
		// deleting memory of cached objects when we throw
		CAutoP<CMDAccessor::MDCache> apcache;
		apcache = CCacheFactory::CreateCache<gpopt::IMDCacheObject*, gpopt::CMDKey*>
					(
					true, // fUnique
					0 /* unlimited cache quota */,
					CMDKey::UlHashMDKey,
					CMDKey::FEqualMDKey
					);

		CMDAccessor::MDCache *pcache = apcache.Value();

		CMDAccessor mda(memory_pool, pcache, CTestUtils::m_sysidDefault, pmdp);

		CSerializableMDAccessor serMDA(&mda);
		
		CAutoTraceFlag atfPrintQuery(EopttracePrintQuery, true);
		CAutoTraceFlag atfPrintPlan(EopttracePrintPlan, true);
		CAutoTraceFlag atfTest(EtraceTest, true);

		COptimizerConfig *optimizer_config = GPOS_NEW(memory_pool) COptimizerConfig
												(
												CEnumeratorConfig::GetEnumeratorCfg(memory_pool, 0 /*plan_id*/),
												CStatisticsConfig::PstatsconfDefault(memory_pool),
												CCTEConfig::PcteconfDefault(memory_pool),
												ICostModel::PcmDefault(memory_pool),
												CHint::PhintDefault(memory_pool),
												CWindowOids::GetWindowOids(memory_pool)
												);

		// setup opt ctx
		CAutoOptCtxt aoc
						(
						memory_pool,
						&mda,
						NULL,  /* pceeval */
						CTestUtils::GetCostModel(memory_pool)
						);

		// translate DXL Tree -> Expr Tree
		CTranslatorDXLToExpr *pdxltr = GPOS_NEW(memory_pool) CTranslatorDXLToExpr(memory_pool, &mda);
		CExpression *pexprTranslated =	pdxltr->PexprTranslateQuery
													(
													ptroutput->CreateDXLNode(),
													ptroutput->GetOutputColumnsDXLArray(),
													ptroutput->GetCTEProducerDXLArray()
													);
		
		gpdxl::ULongPtrArray *pdrgul = pdxltr->PdrgpulOutputColRefs();
		gpmd::MDNameArray *pdrgpmdname = pdxltr->Pdrgpmdname();

		ULONG ulSegments = GPOPT_TEST_SEGMENTS;
		CQueryContext *pqc = CQueryContext::PqcGenerate(memory_pool, pexprTranslated, pdrgul, pdrgpmdname, true /*fDeriveStats*/);

		// optimize logical expression tree into physical expression tree.

		CEngine eng(memory_pool);

		CSerializableOptimizerConfig serOptConfig(memory_pool, optimizer_config);
		
		eng.Init(pqc, NULL /*search_stage_array*/);
		eng.Optimize();
		
		CExpression *pexprPlan = eng.PexprExtractPlan();
		(void) pexprPlan->PrppCompute(memory_pool, pqc->Prpp());

		// translate plan into DXL
		IntPtrArray *pdrgpiSegments = GPOS_NEW(memory_pool) IntPtrArray(memory_pool);


		GPOS_ASSERT(0 < ulSegments);

		for (ULONG ul = 0; ul < ulSegments; ul++)
		{
			pdrgpiSegments->Append(GPOS_NEW(memory_pool) INT(ul));
		}

		CTranslatorExprToDXL ptrexprtodxl(memory_pool, &mda, pdrgpiSegments);
		CDXLNode *pdxlnPlan = ptrexprtodxl.PdxlnTranslate(pexprPlan, pqc->PdrgPcr(), pqc->Pdrgpmdname());
		GPOS_ASSERT(NULL != pdxlnPlan);
		
		CSerializablePlan serPlan(memory_pool, pdxlnPlan, optimizer_config->GetEnumeratorCfg()->GetPlanId(), optimizer_config->GetEnumeratorCfg()->GetPlanSpaceSize());
		GPOS_CHECK_ABORT;

		// simulate an exception 
		GPOS_OOM_CHECK(NULL);
	}	
	GPOS_CATCH_EX(ex)
	{
		// unless we're simulating faults, the exception must be OOM
		GPOS_ASSERT_IMP
			(
			!GPOS_FTRACE(EtraceSimulateAbort) && !GPOS_FTRACE(EtraceSimulateIOError) && !IWorker::m_enforce_time_slices,
			CException::ExmaSystem == ex.Major() && CException::ExmiOOM == ex.Minor()
			);
		
		mdrs.Finalize();

		GPOS_RESET_EX;

		CWStringDynamic str(memory_pool);
		COstreamString oss(&str);
		oss << std::endl << "Minidump" << std::endl;
		oss << minidumpstr.GetBuffer();
		oss << std::endl;
		
		// dump the same to a temp file
		ULONG ulSessionId = 1;
		ULONG ulCommandId = 1;

		CMinidumperUtils::GenerateMinidumpFileName(file_name, GPOS_FILE_NAME_BUF_SIZE, ulSessionId, ulCommandId, NULL /*szMinidumpFileName*/);

		std::wofstream osMinidump(file_name);
		osMinidump << minidumpstr.GetBuffer();

		oss << "Minidump file: " << file_name << std::endl;

		GPOS_TRACE(str.GetBuffer());
	}
	GPOS_CATCH_END;
	
	// TODO:  - Feb 11, 2013; enable after fixing problems with serializing
	// XML special characters (OPT-2996)
//	// try to load minidump file
//	CDXLMinidump *pdxlmd = CMinidumperUtils::PdxlmdLoad(memory_pool, file_name);
//	GPOS_ASSERT(NULL != pdxlmd);
//	delete pdxlmd;

		
	// delete temp file
	ioutils::Unlink(file_name);
	
	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMiniDumperDXLTest::EresUnittest_Load
//
//	@doc:
//		Load a minidump file
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMiniDumperDXLTest::EresUnittest_Load()
{
	CAutoMemoryPool amp(CAutoMemoryPool::ElcExc);
	IMemoryPool *memory_pool = amp.Pmp();
	
	const CHAR *rgszMinidumps[] =
	{
		 "../data/dxl/minidump/Minidump.xml",
	};
	ULONG ulTestCounter = 0;

	GPOS_RESULT eres =
			CTestUtils::EresRunMinidumps
						(
						memory_pool,
						rgszMinidumps,
						1, // ulTests
						&ulTestCounter,
						1, // ulSessionId
						1,  // ulCmdId
						false, // fMatchPlans
						false // fTestSpacePruning
						);
	return eres;

}
// EOF
