//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXformTest.cpp
//
//	@doc:
//		Test for CXForm
//---------------------------------------------------------------------------
#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "gpopt/base/CQueryContext.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXform.h"
#include "gpopt/xforms/xforms.h"

#include "unittest/base.h"
#include "unittest/gpopt/xforms/CXformTest.h"
#include "unittest/gpopt/xforms/CDecorrelatorTest.h"
#include "unittest/gpopt/translate/CTranslatorExprToDXLTest.h"
#include "unittest/gpopt/CTestUtils.h"


//---------------------------------------------------------------------------
//	@function:
//		CXformTest::EresUnittest
//
//	@doc:
//		Unittest driver
//
//---------------------------------------------------------------------------
GPOS_RESULT
CXformTest::EresUnittest()
{
	CUnittest rgut[] =
	{
		GPOS_UNITTEST_FUNC(CXformTest::EresUnittest_ApplyXforms),
		GPOS_UNITTEST_FUNC(CXformTest::EresUnittest_ApplyXforms_CTE),
#ifdef GPOS_DEBUG
		GPOS_UNITTEST_FUNC(CXformTest::EresUnittest_Mapping),
#endif // GPOS_DEBUG
	};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CXformTest::EresUnittest_ApplyXforms
//
//	@doc:
//		Test application of different xforms
//
//---------------------------------------------------------------------------
GPOS_RESULT
CXformTest::EresUnittest_ApplyXforms()
{
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	typedef CExpression *(*Pfpexpr)(IMemoryPool*);
	Pfpexpr rgpf[] = 
					{
					CTestUtils::PexprLogicalApplyWithOuterRef<CLogicalInnerApply>,
					CTestUtils::PexprLogicalApply<CLogicalLeftSemiApply>,
					CTestUtils::PexprLogicalApply<CLogicalLeftAntiSemiApply>,
					CTestUtils::PexprLogicalApply<CLogicalLeftAntiSemiApplyNotIn>,
					CTestUtils::PexprLogicalApplyWithOuterRef<CLogicalLeftOuterApply>,
					CTestUtils::PexprLogicalGet,
					CTestUtils::PexprLogicalExternalGet,
					CTestUtils::PexprLogicalSelect,
					CTestUtils::PexprLogicalLimit,
					CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>,
					CTestUtils::PexprLogicalJoin<CLogicalLeftOuterJoin>,
					CTestUtils::PexprLogicalJoin<CLogicalLeftSemiJoin>,
					CTestUtils::PexprLogicalJoin<CLogicalLeftAntiSemiJoin>,
					CTestUtils::PexprLogicalJoin<CLogicalLeftAntiSemiJoinNotIn>,
					CTestUtils::PexprLogicalGbAgg,
					CTestUtils::PexprLogicalGbAggOverJoin,
					CTestUtils::PexprLogicalGbAggWithSum,
					CTestUtils::PexprLogicalGbAggDedupOverInnerJoin,
					CTestUtils::PexprLogicalNAryJoin,
					CTestUtils::PexprLeftOuterJoinOnNAryJoin,
					CTestUtils::PexprLogicalProject,
					CTestUtils::PexprLogicalSequence,
					CTestUtils::PexprLogicalGetPartitioned,
					CTestUtils::PexprLogicalSelectPartitioned,
					CTestUtils::PexprLogicalDynamicGet,
					CTestUtils::PexprJoinPartitionedInner<CLogicalInnerJoin>,
					CTestUtils::PexprLogicalSelectCmpToConst,
					CTestUtils::PexprLogicalTVFTwoArgs,
					CTestUtils::PexprLogicalTVFNoArgs,
					CTestUtils::PexprLogicalInsert,
					CTestUtils::PexprLogicalDelete,
					CTestUtils::PexprLogicalUpdate,
					CTestUtils::PexprLogicalAssert,
					CTestUtils::PexprLogicalJoin<CLogicalFullOuterJoin>,
					PexprJoinTree,
					CTestUtils::PexprLogicalJoinWithPartitionedAndIndexedInnerChild,
					};

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(memory_pool, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc
					(
					memory_pool,
					&mda,
					NULL,  /* pceeval */
					CTestUtils::GetCostModel(memory_pool)
					);

	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgpf); ul++)
	{
		CWStringDynamic str(memory_pool);
		COstreamString oss(&str);

		// generate simple expression
		CExpression *pexpr = rgpf[ul](memory_pool);
		ApplyExprXforms(memory_pool, oss, pexpr);

		GPOS_TRACE(str.GetBuffer());
		pexpr->Release();
	}

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformTest::EresUnittest_ApplyXforms_CTE
//
//	@doc:
//		Test application of CTE-related xforms
//
//---------------------------------------------------------------------------
GPOS_RESULT
CXformTest::EresUnittest_ApplyXforms_CTE()
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
					NULL,  /* pceeval */
					CTestUtils::GetCostModel(memory_pool)
					);

	DrgPexpr *pdrgpexpr = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);

	// create producer
	ULONG ulCTEId = 0;
	CExpression *pexprProducer = CTestUtils::PexprLogicalCTEProducerOverSelect(memory_pool, ulCTEId);
	COptCtxt::PoctxtFromTLS()->Pcteinfo()->AddCTEProducer(pexprProducer);

	pdrgpexpr->Append(pexprProducer);

	DrgPcr *pdrgpcrProducer = CLogicalCTEProducer::PopConvert(pexprProducer->Pop())->Pdrgpcr();
	DrgPcr *pdrgpcrConsumer = CUtils::PdrgpcrCopy(memory_pool, pdrgpcrProducer);

	CExpression *pexprConsumer =
			GPOS_NEW(memory_pool) CExpression
						(
						memory_pool,
						GPOS_NEW(memory_pool) CLogicalCTEConsumer(memory_pool, ulCTEId, pdrgpcrConsumer)
						);

	pdrgpexpr->Append(pexprConsumer);
	COptCtxt::PoctxtFromTLS()->Pcteinfo()->IncrementConsumers(ulCTEId);

	pexprConsumer->AddRef();
	CExpression *pexprSelect = CTestUtils::PexprLogicalSelect(memory_pool, pexprConsumer);
	pdrgpexpr->Append(pexprSelect);

	pexprSelect->AddRef();
	CExpression *pexprAnchor =
			GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalCTEAnchor(memory_pool, ulCTEId),
					pexprSelect
					);

	pdrgpexpr->Append(pexprAnchor);

	const ULONG length = pdrgpexpr->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CWStringDynamic str(memory_pool);
		COstreamString oss(&str);

		ApplyExprXforms(memory_pool, oss, (*pdrgpexpr)[ul]);

		GPOS_TRACE(str.GetBuffer());
	}
	pdrgpexpr->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformTest::ApplyExprXforms
//
//	@doc:
//		Apply different xforms for the given expression
//
//---------------------------------------------------------------------------
void
CXformTest::ApplyExprXforms
	(
	IMemoryPool *memory_pool,
	IOstream &os,
	CExpression *pexpr
	)
{
	os << std::endl << "EXPR:" << std::endl;
	(void) pexpr->OsPrint(os);

	for (ULONG ulXformId = 0; ulXformId < CXform::ExfSentinel; ulXformId++)
	{
		CXform *pxform = CXformFactory::Pxff()->Pxf((CXform::EXformId) ulXformId);
		os << std::endl <<"XFORM " << pxform->SzId() << ":" << std::endl;

		CXformContext *pxfctxt = GPOS_NEW(memory_pool) CXformContext(memory_pool);
		CXformResult *pxfres = GPOS_NEW(memory_pool) CXformResult(memory_pool);

#ifdef GPOS_DEBUG
		if (pxform->FCheckPattern(pexpr) && CXform::FPromising(memory_pool, pxform, pexpr))
		{
			if (CXform::ExfExpandNAryJoinMinCard == pxform->Exfid())
			{
				GPOS_ASSERT(COperator::EopLogicalNAryJoin == pexpr->Pop()->Eopid());

				// derive stats on NAry join expression
				CExpressionHandle exprhdl(memory_pool);
				exprhdl.Attach(pexpr);
				exprhdl.DeriveStats(memory_pool, memory_pool, NULL /*prprel*/, NULL /*stats_ctxt*/);
			}

			pxform->Transform(pxfctxt, pxfres, pexpr);

			CExpression *pexprResult = pxfres->PexprNext();
			while (NULL != pexprResult)
			{
				GPOS_ASSERT(pexprResult->FMatchDebug(pexprResult));

				pexprResult = pxfres->PexprNext();
			}
			(void) pxfres->OsPrint(os);
		}
#endif // GPOS_DEBUG

		pxfres->Release();
		pxfctxt->Release();
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CXformTest::PexprStarJoinTree
//
//	@doc:
//		Generate a randomized star join tree
//
//---------------------------------------------------------------------------
CExpression *
CXformTest::PexprStarJoinTree
	(
	IMemoryPool *memory_pool,
	ULONG ulTabs
	)
{
	
	CExpression *pexprLeft = CTestUtils::PexprLogicalGet(memory_pool);
	
	for (ULONG ul = 1; ul < ulTabs; ul++)
	{
		CDrvdPropRelational *pdprelLeft = CDrvdPropRelational::GetRelationalProperties(pexprLeft->PdpDerive());
		CColRef *pcrLeft = pdprelLeft->PcrsOutput()->PcrAny();
	
		CExpression *pexprRight = CTestUtils::PexprLogicalGet(memory_pool);

		CDrvdPropRelational *pdprelRight = CDrvdPropRelational::GetRelationalProperties(pexprRight->PdpDerive());
		CColRef *pcrRight = pdprelRight->PcrsOutput()->PcrAny();
		
		CExpression *pexprPred = CUtils::PexprScalarEqCmp(memory_pool, pcrLeft, pcrRight);
		
		pexprLeft = CUtils::PexprLogicalJoin<CLogicalInnerJoin>(memory_pool, pexprLeft, pexprRight, pexprPred);
	}
	
	return pexprLeft;	
}
	

//---------------------------------------------------------------------------
//	@function:
//		CXformTest::PexprJoinTree
//
//	@doc:
//		Generate a randomized star join tree
//
//---------------------------------------------------------------------------
CExpression *
CXformTest::PexprJoinTree
	(
	IMemoryPool *memory_pool
	)
{
	return PexprStarJoinTree(memory_pool, 3);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CXformTest::EresUnittest_Mapping
//
//	@doc:
//		Test name -> xform mapping
//
//---------------------------------------------------------------------------
GPOS_RESULT
CXformTest::EresUnittest_Mapping()
{
	for (ULONG ul = 0; ul < CXform::ExfSentinel; ul++)
	{
		CXform::EXformId exfid = (CXform::EXformId) ul;
		CXform *pxform = CXformFactory::Pxff()->Pxf(exfid);
		CXform *pxformMapped = CXformFactory::Pxff()->Pxf(pxform->SzId());
		GPOS_ASSERT(pxform == pxformMapped);
	}

	return GPOS_OK;
}
#endif // GPOS_DEBUG


// EOF
