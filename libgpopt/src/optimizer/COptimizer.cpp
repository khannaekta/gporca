//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		COptimizer.cpp
//
//	@doc:
//		Optimizer class implementation
//---------------------------------------------------------------------------

#include "gpos/common/CBitSet.h"
#include "gpos/error/CErrorHandlerStandard.h"
#include "gpos/io/CFileDescriptor.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/md/IMDProvider.h"

#include "naucrates/traceflags/traceflags.h"
#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/base/CQueryContext.h"
#include "gpopt/engine/CEngine.h"
#include "gpopt/engine/CEnumeratorConfig.h"
#include "gpopt/engine/CStatisticsConfig.h"
#include "gpopt/exception.h"
#include "gpopt/minidump/CMiniDumperDXL.h"
#include "gpopt/minidump/CMinidumperUtils.h"
#include "gpopt/minidump/CSerializableStackTrace.h"
#include "gpopt/minidump/CSerializableQuery.h"
#include "gpopt/minidump/CSerializablePlan.h"
#include "gpopt/minidump/CSerializableOptimizerConfig.h"
#include "gpopt/minidump/CSerializableMDAccessor.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "gpopt/translate/CTranslatorExprToDXL.h"

#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/optimizer/COptimizer.h"
#include "gpopt/cost/ICostModel.h"

#include <fstream>

using namespace gpos;
using namespace gpdxl;
using namespace gpmd;
using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		COptimizer::PrintQuery
//
//	@doc:
//		Helper function to print query expression
//
//---------------------------------------------------------------------------
void
COptimizer::PrintQuery
	(
	IMemoryPool *mp,
	CExpression *pexprTranslated,
	CQueryContext *pqc
	)
{
	CAutoTrace at(mp);
	at.Os() << std::endl << "Algebrized query: " << std::endl << *pexprTranslated;

	ExpressionArray *pdrgpexpr = COptCtxt::PoctxtFromTLS()->Pcteinfo()->PdrgPexpr(mp);
	const ULONG ulCTEs = pdrgpexpr->Size();
	if (0 < ulCTEs)
	{
		at.Os() << std::endl << "Common Table Expressions: ";
		for (ULONG ul = 0; ul < ulCTEs; ul++)
		{
			at.Os() << std::endl << *(*pdrgpexpr)[ul];
		}
	}
	pdrgpexpr->Release();

	CExpression *pexprPreprocessed = pqc->Pexpr();
	(void) pexprPreprocessed->PdpDerive();
	at.Os() << std::endl << "Algebrized preprocessed query: " << std::endl << *pexprPreprocessed;
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizer::PrintPlan
//
//	@doc:
//		Helper function to print query plan
//
//---------------------------------------------------------------------------
void
COptimizer::PrintPlan
	(
	IMemoryPool *mp,
	CExpression *pexprPlan
	)
{
	CAutoTrace at(mp);
	at.Os() << std::endl << "Physical plan: " << std::endl << *pexprPlan;
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizer::DumpSamples
//
//	@doc:
//		Helper function to dump plan samples
//
//---------------------------------------------------------------------------
void
COptimizer::DumpSamples
	(
	IMemoryPool *mp,
	CEnumeratorConfig *pec,
	ULONG ulSessionId,
	ULONG ulCmdId
	)
{
	GPOS_ASSERT(NULL != pec);

	CWStringDynamic *str = CDXLUtils::SerializeSamplePlans(mp, pec, true /*indentation*/);
	pec->DumpSamples(str, ulSessionId, ulCmdId);
	GPOS_DELETE(str);
	GPOS_CHECK_ABORT;

	pec->FitCostDistribution();
	str = CDXLUtils::SerializeCostDistr(mp, pec, true /*indentation*/);
	pec->DumpCostDistr(str, ulSessionId, ulCmdId);
	GPOS_DELETE(str);
}

//---------------------------------------------------------------------------
//	@function:
//		COptimizer::DumpQueryOrPlan
//
//	@doc:
//		Print query tree or plan tree
//
//---------------------------------------------------------------------------
void
COptimizer::PrintQueryOrPlan
	(
	IMemoryPool *mp,
	CExpression *pexpr,
	CQueryContext *pqc
	)
{
	GPOS_ASSERT(NULL != pexpr);

	if (NULL != pqc)
	{
		if (GPOS_FTRACE(EopttracePrintQuery))
		{
			PrintQuery(mp, pexpr, pqc);
		}
	}
	else
	{
		if (GPOS_FTRACE(EopttracePrintPlan))
		{
			PrintPlan(mp, pexpr);
		}
	}
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizer::PdxlnOptimize
//
//	@doc:
//		Optimize given query
//		the function is oblivious of trace flags setting/resetting which
//		must happen at the caller side if needed
//
//---------------------------------------------------------------------------
CDXLNode *
COptimizer::PdxlnOptimize
	(
	IMemoryPool *mp, 
	CMDAccessor *md_accessor,
	const CDXLNode *query,
	const DXLNodeArray *query_output_dxlnode_array, 
	const DXLNodeArray *cte_producers, 
	IConstExprEvaluator *pceeval,
	ULONG ulHosts,	// actual number of data nodes in the system
	ULONG ulSessionId,
	ULONG ulCmdId,
	SearchStageArray *search_stage_array,
	COptimizerConfig *optimizer_config,
	const CHAR *szMinidumpFileName 	// name of minidump file to be created
	)
{
	GPOS_ASSERT(NULL != md_accessor);
	GPOS_ASSERT(NULL != query);
	GPOS_ASSERT(NULL != query_output_dxlnode_array);
	GPOS_ASSERT(NULL != optimizer_config);

	BOOL fMinidump = GPOS_FTRACE(EopttraceMinidump);

	// If minidump was requested, open the minidump file and initialize
	// minidumper. (We create the minidumper object even if we're not
	// dumping, but without the Init-call, it will stay inactive.)
	CMiniDumperDXL mdmp(mp);
	CAutoP<std::wofstream> wosMinidump;
	CAutoP<COstreamBasic> osMinidump;
	if (fMinidump)
	{
		CHAR file_name[GPOS_FILE_NAME_BUF_SIZE];

		CMinidumperUtils::GenerateMinidumpFileName(file_name, GPOS_FILE_NAME_BUF_SIZE, ulSessionId, ulCmdId, szMinidumpFileName);

		// Note: std::wofstream won't throw an error on failure. The stream is merely marked as
		// failed. We could check the state, and avoid the overhead of serializing the
		// minidump if it failed, but it's hardly worth optimizing for an error case.
		wosMinidump = GPOS_NEW(mp) std::wofstream(file_name);
		osMinidump = GPOS_NEW(mp) COstreamBasic(wosMinidump.Value());

		mdmp.Init(osMinidump.Value());
	}
	CDXLNode *pdxlnPlan = NULL;
	CErrorHandlerStandard errhdl;
	GPOS_TRY_HDL(&errhdl)
	{
		CSerializableStackTrace serStack;
		CSerializableOptimizerConfig serOptConfig(mp, optimizer_config);
		CSerializableMDAccessor serMDA(md_accessor);
		CSerializableQuery serQuery(mp, query, query_output_dxlnode_array, cte_producers);

		{			
			optimizer_config->AddRef();
			if (NULL != pceeval)
			{
				pceeval->AddRef();
			}

			// install opt context in TLS
			CAutoOptCtxt aoc(mp, md_accessor, pceeval, optimizer_config);

			// translate DXL Tree -> Expr Tree
			CTranslatorDXLToExpr dxltr(mp, md_accessor);
			CExpression *pexprTranslated =	dxltr.PexprTranslateQuery(query, query_output_dxlnode_array, cte_producers);
			GPOS_CHECK_ABORT;
			gpdxl::ULongPtrArray *pdrgpul = dxltr.PdrgpulOutputColRefs();
			gpmd::MDNameArray *pdrgpmdname = dxltr.Pdrgpmdname();

			CQueryContext *pqc = CQueryContext::PqcGenerate(mp, pexprTranslated, pdrgpul, pdrgpmdname, true /*fDeriveStats*/);
			GPOS_CHECK_ABORT;

			PrintQueryOrPlan(mp, pexprTranslated, pqc);

			CWStringDynamic strTrace(mp);
			COstreamString oss(&strTrace);

			// if the number of inlinable CTEs is greater than the cutoff, then
			// disable inlining for this query
			if (!GPOS_FTRACE(EopttraceEnableCTEInlining) ||
				CUtils::UlInlinableCTEs(pexprTranslated) > optimizer_config->GetCteConf()->UlCTEInliningCutoff())
			{
				COptCtxt::PoctxtFromTLS()->Pcteinfo()->DisableInlining();
			}

			GPOS_CHECK_ABORT;
			// optimize logical expression tree into physical expression tree.
			CExpression *pexprPlan = PexprOptimize(mp, pqc, search_stage_array);
			GPOS_CHECK_ABORT;

			PrintQueryOrPlan(mp, pexprPlan);

			// translate plan into DXL
			pdxlnPlan = CreateDXLNode(mp, md_accessor, pexprPlan, pqc->PdrgPcr(), pdrgpmdname, ulHosts);
			GPOS_CHECK_ABORT;

			if (fMinidump)
			{
				CSerializablePlan serPlan(mp, pdxlnPlan, optimizer_config->GetEnumeratorCfg()->GetPlanId(), optimizer_config->GetEnumeratorCfg()->GetPlanSpaceSize());
				CMinidumperUtils::Finalize(&mdmp, true /* fSerializeErrCtxt*/);
				GPOS_CHECK_ABORT;
			}
			
			if (GPOS_FTRACE(EopttraceSamplePlans))
			{
				DumpSamples(mp, optimizer_config->GetEnumeratorCfg(), ulSessionId, ulCmdId);
				GPOS_CHECK_ABORT;
			}

			// cleanup
			pexprTranslated->Release();
			pexprPlan->Release();
			GPOS_DELETE(pqc);
		}
	}
	GPOS_CATCH_EX(ex)
	{
		if (fMinidump)
		{
			CMinidumperUtils::Finalize(&mdmp, false /* fSerializeErrCtxt*/);
			HandleExceptionAfterFinalizingMinidump(ex);
		}

		GPOS_RETHROW(ex);
	}
	GPOS_CATCH_END;

	return pdxlnPlan;
}

//---------------------------------------------------------------------------
//	@function:
//		COptimizer::HandleExceptionAfterFinalizingMinidump
//
//	@doc:
//		Handle exception after finalizing minidump
//
//---------------------------------------------------------------------------
void
COptimizer::HandleExceptionAfterFinalizingMinidump
	(
	CException &ex
	)
{
	if (NULL != ITask::Self() &&
		!ITask::Self()->GetErrCtxt()->IsPending())
	{
		// if error context has no pending exception, then minidump creation
		// might have reset the error,
		// in this case we need to raise the original exception
		GPOS_RAISE
			(
			ex.Major(),
			ex.Minor(),
			GPOS_WSZ_LIT("re-raising exception after finalizing minidump")
			);
	}

	// otherwise error is still pending, re-throw original exception
	GPOS_RETHROW(ex);
}

// This function provides an entry point to check for a plan with CTE,
// if both CTEProducer and CTEConsumer are executed on the same locality.
// If it is not the case, the plan is bogus and cannot be executed
// by the executor and an exception is raised.
//
// To be able to enter the recursive logic, the execution locality of root
// is determined before the recursive call.
void
COptimizer::CheckCTEConsistency
	(
	IMemoryPool *mp,
	CExpression *pexpr
	)
{
	UlongToUlongMap *phmulul = GPOS_NEW(mp) UlongToUlongMap(mp);
	CDrvdPropPlan *pdpplanChild = CDrvdPropPlan::Pdpplan(pexpr->PdpDerive());
	CDistributionSpec *pdsChild = pdpplanChild->Pds();

	CUtils::EExecLocalityType eelt = CUtils::ExecLocalityType(pdsChild);
	CUtils::ValidateCTEProducerConsumerLocality(mp, pexpr, eelt, phmulul);
	phmulul->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		COptimizer::PexprOptimize
//
//	@doc:
//		Optimize query in given query context
//
//---------------------------------------------------------------------------
CExpression *
COptimizer::PexprOptimize
	(
	IMemoryPool *mp,
	CQueryContext *pqc,
	SearchStageArray *search_stage_array
	)
{
	CEngine eng(mp);
	eng.Init(pqc, search_stage_array);
	eng.Optimize();

	GPOS_CHECK_ABORT;

	CExpression *pexprPlan = eng.PexprExtractPlan();
	(void) pexprPlan->PrppCompute(mp, pqc->Prpp());

	CheckCTEConsistency(mp, pexprPlan);

	GPOS_CHECK_ABORT;

	return pexprPlan;
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizer::CreateDXLNode
//
//	@doc:
//		Translate an optimizer expression into a DXL tree 
//
//---------------------------------------------------------------------------
CDXLNode *
COptimizer::CreateDXLNode
	(
	IMemoryPool *mp, 
	CMDAccessor *md_accessor, 
	CExpression *pexpr,
	ColRefArray *colref_array,
	MDNameArray *pdrgpmdname,
	ULONG ulHosts
	)
{
	GPOS_ASSERT(0 < ulHosts);
	IntPtrArray *pdrgpiHosts = GPOS_NEW(mp) IntPtrArray(mp);

	for (ULONG ul = 0; ul < ulHosts; ul++)
	{
		pdrgpiHosts->Append(GPOS_NEW(mp) INT(ul));
	}

	CTranslatorExprToDXL ptrexprtodxl(mp, md_accessor, pdrgpiHosts);
	CDXLNode *pdxlnPlan = ptrexprtodxl.PdxlnTranslate(pexpr, colref_array, pdrgpmdname);
	
	return pdxlnPlan;
}

// EOF
