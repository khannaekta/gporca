//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		COptimizationJobsTest.cpp
//
//	@doc:
//		Test for optimization jobs
//---------------------------------------------------------------------------
#include "gpopt/engine/CEngine.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/search/CGroupProxy.h"
#include "gpopt/search/CJobFactory.h"
#include "gpopt/search/CJobGroupOptimization.h"
#include "gpopt/search/CJobGroupExpressionOptimization.h"
#include "gpopt/search/CJobGroupExploration.h"
#include "gpopt/search/CJobGroupExpressionExploration.h"
#include "gpopt/search/CJobGroupImplementation.h"
#include "gpopt/search/CJobGroupExpressionImplementation.h"
#include "gpopt/search/CJobTransformation.h"
#include "gpopt/search/CScheduler.h"
#include "gpopt/search/CSchedulerContext.h"
#include "gpopt/xforms/CXformFactory.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/search/COptimizationJobsTest.h"


//---------------------------------------------------------------------------
//	@function:
//		COptimizationJobsTest::EresUnittest
//
//	@doc:
//		Unittest for optimization jobs
//
//---------------------------------------------------------------------------
GPOS_RESULT
COptimizationJobsTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(COptimizationJobsTest::EresUnittest_StateMachine),
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		COptimizationJobsTest::EresUnittest_StateMachine
//
//	@doc:
//		Test of optimization jobs stat machine
//
//---------------------------------------------------------------------------
GPOS_RESULT
COptimizationJobsTest::EresUnittest_StateMachine()
{
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(memory_pool, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	{
		CAutoOptCtxt aoc
						(
						memory_pool,
						&mda,
						NULL,  /* pceeval */
						CTestUtils::GetCostModel(memory_pool)
						);
		CEngine eng(memory_pool);

		// generate  join expression
		CExpression *pexpr = CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>(memory_pool);

		// generate query context
		CQueryContext *pqc = CTestUtils::PqcGenerate(memory_pool, pexpr);

		// Initialize engine
		eng.Init(pqc, NULL /*search_stage_array*/);

		CGroup *pgroup = eng.PgroupRoot();
		pqc->Prpp()->AddRef();
		COptimizationContext *poc = GPOS_NEW(memory_pool) COptimizationContext
							(
							memory_pool,
							pgroup,
							pqc->Prpp(),
							GPOS_NEW(memory_pool) CReqdPropRelational(GPOS_NEW(memory_pool) CColRefSet(memory_pool)),
							GPOS_NEW(memory_pool) StatsArray(memory_pool),
							0 // ulSearchStageIndex
							);

		// optimize query
		CJobFactory jf(memory_pool, 1000 /*ulJobs*/);
		CScheduler sched(memory_pool, 1000 /*ulJobs*/, 1 /*ulWorkers*/);
		CSchedulerContext sc;
		sc.Init(memory_pool, &jf, &sched, &eng);
		CJob *pj = jf.PjCreate(CJob::EjtGroupOptimization);
		CJobGroupOptimization *pjgo = CJobGroupOptimization::PjConvert(pj);
		pjgo->Init(pgroup, NULL /*pgexprOrigin*/, poc);
		sched.Add(pjgo, NULL /*pjParent*/);
		CScheduler::Run(&sc);

#ifdef GPOS_DEBUG
		{
			CAutoTrace at(memory_pool);
			at.Os() << std::endl << "GROUP OPTIMIZATION:" << std::endl;
			(void) pjgo->OsPrint(at.Os());

			// dumping state graph
			at.Os() << std::endl;
			(void) pjgo->OsDiagramToGraphviz(memory_pool, at.Os(), GPOS_WSZ_LIT("GroupOptimizationJob"));

			CJobGroupOptimization::EState *pestate = NULL;
			ULONG size = 0;
			pjgo->Unreachable(memory_pool, &pestate, &size);
			GPOS_ASSERT(size == 1 && pestate[0] == CJobGroupOptimization::estInitialized);

			GPOS_DELETE_ARRAY(pestate);
		}

		CGroupExpression *pgexprLogical = NULL;
		CGroupExpression *pgexprPhysical = NULL;
		{
			CGroupProxy gp(pgroup);
			pgexprLogical = gp.PgexprNextLogical(NULL /*pgexpr*/);
			GPOS_ASSERT(NULL != pgexprLogical);

			pgexprPhysical = gp.PgexprSkipLogical(NULL /*pgexpr*/);
			GPOS_ASSERT(NULL != pgexprPhysical);
		}

		{
			CAutoTrace at(memory_pool);
			CJobGroupImplementation jgi;
			jgi.Init(pgroup);
			at.Os() << std::endl << "GROUP IMPLEMENTATION:" << std::endl;
			(void) jgi.OsPrint(at.Os());

			// dumping state graph
			at.Os() << std::endl;
			(void) jgi.OsDiagramToGraphviz(memory_pool, at.Os(), GPOS_WSZ_LIT("GroupImplementationJob"));

			CJobGroupImplementation::EState *pestate = NULL;
			ULONG size = 0;
			jgi.Unreachable(memory_pool, &pestate, &size);
			GPOS_ASSERT(size == 1 && pestate[0] == CJobGroupImplementation::estInitialized);

			GPOS_DELETE_ARRAY(pestate);
		}

		{
			CAutoTrace at(memory_pool);
			CJobGroupExploration jge;
			jge.Init(pgroup);
			at.Os() << std::endl << "GROUP EXPLORATION:" << std::endl;
			(void) jge.OsPrint(at.Os());

			// dumping state graph
			at.Os() << std::endl;
			(void) jge.OsDiagramToGraphviz(memory_pool, at.Os(), GPOS_WSZ_LIT("GroupExplorationJob"));

			CJobGroupExploration::EState *pestate = NULL;
			ULONG size = 0;
			jge.Unreachable(memory_pool, &pestate, &size);
			GPOS_ASSERT(size == 1 && pestate[0] == CJobGroupExploration::estInitialized);

			GPOS_DELETE_ARRAY(pestate);
		}

		{
			CAutoTrace at(memory_pool);
			CJobGroupExpressionOptimization jgeo;
			jgeo.Init(pgexprPhysical, poc, 0 /*ulOptReq*/);
			at.Os() << std::endl << "GROUP EXPRESSION OPTIMIZATION:" << std::endl;
			(void) jgeo.OsPrint(at.Os());

			// dumping state graph
			at.Os() << std::endl;
			(void) jgeo.OsDiagramToGraphviz(memory_pool, at.Os(), GPOS_WSZ_LIT("GroupExpressionOptimizationJob"));

			CJobGroupExpressionOptimization::EState *pestate = NULL;
			ULONG size = 0;
			jgeo.Unreachable(memory_pool, &pestate, &size);
			GPOS_ASSERT(size == 1 && pestate[0] == CJobGroupExpressionOptimization::estInitialized);

			GPOS_DELETE_ARRAY(pestate);
		}

		{
			CAutoTrace at(memory_pool);
			CJobGroupExpressionImplementation jgei;
			jgei.Init(pgexprLogical);
			at.Os() << std::endl << "GROUP EXPRESSION IMPLEMENTATION:" << std::endl;
			(void) jgei.OsPrint(at.Os());

			// dumping state graph
			at.Os() << std::endl;
			(void) jgei.OsDiagramToGraphviz(memory_pool, at.Os(), GPOS_WSZ_LIT("GroupExpressionImplementationJob"));

			CJobGroupExpressionImplementation::EState *pestate = NULL;
			ULONG size = 0;
			jgei.Unreachable(memory_pool, &pestate, &size);
			GPOS_ASSERT(size == 1 && pestate[0] == CJobGroupExpressionImplementation::estInitialized);

			GPOS_DELETE_ARRAY(pestate);
		}

		{
			CAutoTrace at(memory_pool);
			CJobGroupExpressionExploration jgee;
			jgee.Init(pgexprLogical);
			at.Os() << std::endl << "GROUP EXPRESSION EXPLORATION:" << std::endl;
			(void) jgee.OsPrint(at.Os());

			// dumping state graph
			at.Os() << std::endl;
			(void) jgee.OsDiagramToGraphviz(memory_pool, at.Os(), GPOS_WSZ_LIT("GroupExpressionExplorationJob"));

			CJobGroupExpressionExploration::EState *pestate = NULL;
			ULONG size = 0;
			jgee.Unreachable(memory_pool, &pestate, &size);
			GPOS_ASSERT(size == 1 && pestate[0] == CJobGroupExpressionExploration::estInitialized);

			GPOS_DELETE_ARRAY(pestate);
		}

		{
			CAutoTrace at(memory_pool);
			CXformSet *xform_set = CLogical::PopConvert(pgexprLogical->Pop())->PxfsCandidates(memory_pool);

			CXformSetIter xsi(*(xform_set));
			while (xsi.Advance())
			{
				CXform *pxform = CXformFactory::Pxff()->Pxf(xsi.TBit());
				CJobTransformation jt;
				jt.Init(pgexprLogical, pxform);
				at.Os() << std::endl << "GROUP EXPRESSION TRANSFORMATION:" << std::endl;
				(void) jt.OsPrint(at.Os());

				// dumping state graph
				at.Os() << std::endl;
				(void) jt.OsDiagramToGraphviz(memory_pool, at.Os(), GPOS_WSZ_LIT("TransformationJob"));

				CJobTransformation::EState *pestate = NULL;
				ULONG size = 0;
				jt.Unreachable(memory_pool, &pestate, &size);
				GPOS_ASSERT(size == 1 && pestate[0] == CJobTransformation::estInitialized);

				GPOS_DELETE_ARRAY(pestate);
			}

			xform_set->Release();
		}
#endif // GPOS_DEBUG

		pexpr->Release();
		poc->Release();
		GPOS_DELETE(pqc);
	}

	return GPOS_OK;
}


// EOF
