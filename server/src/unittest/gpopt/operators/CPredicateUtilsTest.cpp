//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CPredicateUtilsTest.cpp
//
//	@doc:
//		Test for predicate utilities
//---------------------------------------------------------------------------
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/operators/CExpressionPreprocessor.h"
#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CPredicateUtils.h"

#include "unittest/base.h"
#include "unittest/gpopt/operators/CPredicateUtilsTest.h"
#include "unittest/gpopt/CTestUtils.h"

#include "naucrates/md/CMDIdGPDB.h"


//---------------------------------------------------------------------------
//	@function:
//		CPredicateUtilsTest::EresUnittest
//
//	@doc:
//		Unittest for predicate utilities
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPredicateUtilsTest::EresUnittest()
{

	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CPredicateUtilsTest::EresUnittest_Conjunctions),
		GPOS_UNITTEST_FUNC(CPredicateUtilsTest::EresUnittest_Disjunctions),
		GPOS_UNITTEST_FUNC(CPredicateUtilsTest::EresUnittest_PlainEqualities),
		GPOS_UNITTEST_FUNC(CPredicateUtilsTest::EresUnittest_Implication),
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CPredicateUtilsTest::EresUnittest_Conjunctions
//
//	@doc:
//		Test extraction and construction of conjuncts
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPredicateUtilsTest::EresUnittest_Conjunctions()
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

	// build conjunction
	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	const ULONG ulConjs = 3;
	for (ULONG ul = 0; ul < ulConjs; ul++)
	{
		pdrgpexpr->Append(CUtils::PexprScalarConstBool(memory_pool, true /*fValue*/));
	}
	CExpression *pexprConjunction = CUtils::PexprScalarBoolOp(memory_pool, CScalarBoolOp::EboolopAnd, pdrgpexpr);
	
	// break into conjuncts
	ExpressionArray *pdrgpexprExtract = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pexprConjunction);
	GPOS_ASSERT(pdrgpexprExtract->Size() == ulConjs);
	
	// collapse into single conjunct
	CExpression *pexpr = CPredicateUtils::PexprConjunction(memory_pool, pdrgpexprExtract);
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(CUtils::FScalarConstTrue(pexpr));
	pexpr->Release();
	
	// collapse empty input array to conjunct
	CExpression *pexprSingleton = CPredicateUtils::PexprConjunction(memory_pool, NULL /*pdrgpexpr*/);
	GPOS_ASSERT(NULL != pexprSingleton);
	pexprSingleton->Release();

	pexprConjunction->Release();

	// conjunction on scalar comparisons
	CExpression *pexprGet = CTestUtils::PexprLogicalGet(memory_pool);
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprGet->PdpDerive())->PcrsOutput();
	CColRef *pcr1 = pcrs->PcrAny();
	CColRef *pcr2 = pcrs->PcrFirst();
	CExpression *pexprCmp1 = CUtils::PexprScalarCmp(memory_pool, pcr1, pcr2, IMDType::EcmptEq);
	CExpression *pexprCmp2 = CUtils::PexprScalarCmp(memory_pool, pcr1, CUtils::PexprScalarConstInt4(memory_pool, 1 /*val*/), IMDType::EcmptEq);

	CExpression *pexprConj = CPredicateUtils::PexprConjunction(memory_pool, pexprCmp1, pexprCmp2);
	pdrgpexprExtract = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pexprConj);
	GPOS_ASSERT(2 == pdrgpexprExtract->Size());
	pdrgpexprExtract->Release();

	pexprCmp1->Release();
	pexprCmp2->Release();
	pexprConj->Release();
	pexprGet->Release();

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CPredicateUtilsTest::EresUnittest_Disjunctions
//
//	@doc:
//		Test extraction and construction of disjuncts
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPredicateUtilsTest::EresUnittest_Disjunctions()
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

	// build disjunction
	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	const ULONG ulDisjs = 3;
	for (ULONG ul = 0; ul < ulDisjs; ul++)
	{
		pdrgpexpr->Append(CUtils::PexprScalarConstBool(memory_pool, false /*fValue*/));
	}
	CExpression *pexprDisjunction = CUtils::PexprScalarBoolOp(memory_pool, CScalarBoolOp::EboolopOr, pdrgpexpr);

	// break into disjuncts
	ExpressionArray *pdrgpexprExtract = CPredicateUtils::PdrgpexprDisjuncts(memory_pool, pexprDisjunction);
	GPOS_ASSERT(pdrgpexprExtract->Size() == ulDisjs);

	// collapse into single disjunct
	CExpression *pexpr = CPredicateUtils::PexprDisjunction(memory_pool, pdrgpexprExtract);
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(CUtils::FScalarConstFalse(pexpr));
	pexpr->Release();

	// collapse empty input array to disjunct
	CExpression *pexprSingleton = CPredicateUtils::PexprDisjunction(memory_pool, NULL /*pdrgpexpr*/);
	GPOS_ASSERT(NULL != pexprSingleton);
	pexprSingleton->Release();

	pexprDisjunction->Release();

	// disjunction on scalar comparisons
	CExpression *pexprGet = CTestUtils::PexprLogicalGet(memory_pool);
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprGet->PdpDerive())->PcrsOutput();
	CColRefSetIter crsi(*pcrs);

#ifdef GPOS_DEBUG
	BOOL fAdvance =
#endif
	crsi.Advance();
	GPOS_ASSERT(fAdvance);
	CColRef *pcr1 = crsi.Pcr();

#ifdef GPOS_DEBUG
	fAdvance =
#endif
	crsi.Advance();
	GPOS_ASSERT(fAdvance);
	CColRef *pcr2 = crsi.Pcr();

#ifdef GPOS_DEBUG
	fAdvance =
#endif
	crsi.Advance();
	GPOS_ASSERT(fAdvance);
	CColRef *pcr3 = crsi.Pcr();

	CExpression *pexprCmp1 = CUtils::PexprScalarCmp(memory_pool, pcr1, pcr2, IMDType::EcmptEq);
	CExpression *pexprCmp2 = CUtils::PexprScalarCmp(memory_pool, pcr1, CUtils::PexprScalarConstInt4(memory_pool, 1 /*val*/), IMDType::EcmptEq);

	{
		CExpression *pexprDisj = CPredicateUtils::PexprDisjunction(memory_pool, pexprCmp1, pexprCmp2);
		pdrgpexprExtract = CPredicateUtils::PdrgpexprDisjuncts(memory_pool, pexprDisj);
		GPOS_ASSERT(2 == pdrgpexprExtract->Size());
		pdrgpexprExtract->Release();
		pexprDisj->Release();
	}


	{
		ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
		CExpression *pexprCmp3 = CUtils::PexprScalarCmp(memory_pool, pcr2, pcr1, IMDType::EcmptG);
		CExpression *pexprCmp4 = CUtils::PexprScalarCmp(memory_pool, CUtils::PexprScalarConstInt4(memory_pool, 200 /*val*/), pcr3, IMDType::EcmptL);
		pexprCmp1->AddRef();
		pexprCmp2->AddRef();

		pdrgpexpr->Append(pexprCmp3);
		pdrgpexpr->Append(pexprCmp4);
		pdrgpexpr->Append(pexprCmp1);
		pdrgpexpr->Append(pexprCmp2);

		CExpression *pexprDisj = CPredicateUtils::PexprDisjunction(memory_pool, pdrgpexpr);
		pdrgpexprExtract = CPredicateUtils::PdrgpexprDisjuncts(memory_pool, pexprDisj);
		GPOS_ASSERT(4 == pdrgpexprExtract->Size());
		pdrgpexprExtract->Release();
		pexprDisj->Release();
	}

	pexprCmp1->Release();
	pexprCmp2->Release();
	pexprGet->Release();

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CPredicateUtilsTest::EresUnittest_PlainEqualities
//
//	@doc:
//		Test the extraction of equality predicates between scalar identifiers
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPredicateUtilsTest::EresUnittest_PlainEqualities()
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

	CExpression *pexprLeft = CTestUtils::PexprLogicalGet(memory_pool);
	CExpression *pexprRight = CTestUtils::PexprLogicalGet(memory_pool);

	ExpressionArray *pdrgpexprOriginal = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);

	CColRefSet *pcrsLeft = CDrvdPropRelational::GetRelationalProperties(pexprLeft->PdpDerive())->PcrsOutput();
	CColRefSet *pcrsRight = CDrvdPropRelational::GetRelationalProperties(pexprRight->PdpDerive())->PcrsOutput();

	CColRef *pcrLeft = pcrsLeft->PcrAny();
	CColRef *pcrRight = pcrsRight->PcrAny();

	// generate an equality predicate between two column reference
	CExpression *pexprScIdentEquality =
		CUtils::PexprScalarEqCmp(memory_pool, pcrLeft, pcrRight);

	pexprScIdentEquality->AddRef();
	pdrgpexprOriginal->Append(pexprScIdentEquality);

	// generate a non-equality predicate between two column reference
	CExpression *pexprScIdentInequality =
		CUtils::PexprScalarCmp(memory_pool, pcrLeft, pcrRight, CWStringConst(GPOS_WSZ_LIT("<")), GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_LT_OP));

	pexprScIdentInequality->AddRef();
	pdrgpexprOriginal->Append(pexprScIdentInequality);

	// generate an equality predicate between a column reference and a constant m_bytearray_value
	CExpression *pexprScalarConstInt4 = CUtils::PexprScalarConstInt4(memory_pool, 10 /*fValue*/);
	CExpression *pexprScIdentConstEquality = CUtils::PexprScalarEqCmp(memory_pool, pexprScalarConstInt4, pcrRight);

	pdrgpexprOriginal->Append(pexprScIdentConstEquality);

	GPOS_ASSERT(3 == pdrgpexprOriginal->Size());

	ExpressionArray *pdrgpexprResult = CPredicateUtils::PdrgpexprPlainEqualities(memory_pool, pdrgpexprOriginal);

	GPOS_ASSERT(1 == pdrgpexprResult->Size());

	// clean up
	pdrgpexprOriginal->Release();
	pdrgpexprResult->Release();
	pexprLeft->Release();
	pexprRight->Release();
	pexprScIdentEquality->Release();
	pexprScIdentInequality->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CPredicateUtilsTest::EresUnittest_Implication
//
//	@doc:
//		Test removal of implied predicates
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPredicateUtilsTest::EresUnittest_Implication()
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

	// generate a two cascaded joins
	CWStringConst strName1(GPOS_WSZ_LIT("Rel1"));
	CMDIdGPDB *pmdid1 = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1, 1);
	CTableDescriptor *ptabdesc1 = CTestUtils::PtabdescCreate(memory_pool, 3, pmdid1, CName(&strName1));
	CWStringConst strAlias1(GPOS_WSZ_LIT("Rel1"));
	CExpression *pexprRel1 = CTestUtils::PexprLogicalGet(memory_pool, ptabdesc1, &strAlias1);

	CWStringConst strName2(GPOS_WSZ_LIT("Rel2"));
	CMDIdGPDB *pmdid2 = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_TEST_REL_OID2, 1, 1);
	CTableDescriptor *ptabdesc2 = CTestUtils::PtabdescCreate(memory_pool, 3, pmdid2, CName(&strName2));
	CWStringConst strAlias2(GPOS_WSZ_LIT("Rel2"));
	CExpression *pexprRel2 = CTestUtils::PexprLogicalGet(memory_pool, ptabdesc2, &strAlias2);

	CWStringConst strName3(GPOS_WSZ_LIT("Rel3"));
	CMDIdGPDB *pmdid3 = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_TEST_REL_OID3, 1, 1);
	CTableDescriptor *ptabdesc3 = CTestUtils::PtabdescCreate(memory_pool, 3, pmdid3, CName(&strName3));
	CWStringConst strAlias3(GPOS_WSZ_LIT("Rel3"));
	CExpression *pexprRel3 = CTestUtils::PexprLogicalGet(memory_pool, ptabdesc3, &strAlias3);

	CExpression *pexprJoin1 = CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>(memory_pool, pexprRel1, pexprRel2);
	CExpression *pexprJoin2 = CTestUtils::PexprLogicalJoin<CLogicalInnerJoin>(memory_pool, pexprJoin1, pexprRel3);

	{
		CAutoTrace at(memory_pool);
		at.Os() << "Original expression:" << std::endl << *pexprJoin2 <<std::endl;
	}

	// imply new predicates by deriving constraints
	CExpression *pexprConstraints = CExpressionPreprocessor::PexprAddPredicatesFromConstraints(memory_pool, pexprJoin2);

	{
		CAutoTrace at(memory_pool);
		at.Os() << "Expression with implied predicates:" << std::endl << *pexprConstraints <<std::endl;;
	}

	// minimize join predicates by removing implied conjuncts
	CExpressionHandle exprhdl(memory_pool);
	exprhdl.Attach(pexprConstraints);
	CExpression *pexprMinimizedPred = CPredicateUtils::PexprRemoveImpliedConjuncts(memory_pool, (*pexprConstraints)[2], exprhdl);

	{
		CAutoTrace at(memory_pool);
		at.Os() << "Minimized join predicate:" << std::endl << *pexprMinimizedPred <<std::endl;
	}

	ExpressionArray *pdrgpexprOriginalConjuncts = CPredicateUtils::PdrgpexprConjuncts(memory_pool,  (*pexprConstraints)[2]);
	ExpressionArray *pdrgpexprNewConjuncts = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pexprMinimizedPred);

	GPOS_ASSERT(pdrgpexprNewConjuncts->Size() < pdrgpexprOriginalConjuncts->Size());

	// clean up
	pdrgpexprOriginalConjuncts->Release();
	pdrgpexprNewConjuncts->Release();
	pexprJoin2->Release();
	pexprConstraints->Release();
	pexprMinimizedPred->Release();

	return GPOS_OK;
}
// EOF
