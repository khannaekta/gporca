//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CSubqueryTestUtils.cpp
//
//	@doc:
//		Implementation of test utility functions
//---------------------------------------------------------------------------

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/base/CQueryContext.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/ops.h"

#include "unittest/gpopt/CSubqueryTestUtils.h"

#include "naucrates/md/CMDIdGPDB.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::GenerateGetExpressions
//
//	@doc:
//		Helper for generating a pair of randomized Get expressions
//
//---------------------------------------------------------------------------
void
CSubqueryTestUtils::GenerateGetExpressions
	(
	IMemoryPool *memory_pool,
	CExpression **ppexprOuter,
	CExpression **ppexprInner
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != ppexprOuter);
	GPOS_ASSERT(NULL != ppexprInner);

	// outer expression
	CWStringConst strNameR(GPOS_WSZ_LIT("Rel1"));
	CMDIdGPDB *pmdidR = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1, 1);
	CTableDescriptor *ptabdescR = CTestUtils::PtabdescCreate(memory_pool, 3 /*num_cols*/, pmdidR, CName(&strNameR));
	*ppexprOuter = CTestUtils::PexprLogicalGet(memory_pool, ptabdescR, &strNameR);

	// inner expression
	CWStringConst strNameS(GPOS_WSZ_LIT("Rel2"));
	CMDIdGPDB *pmdidS = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_TEST_REL_OID2, 1, 1);
	CTableDescriptor *ptabdescS = CTestUtils::PtabdescCreate(memory_pool, 3 /*num_cols*/, pmdidS, CName(&strNameS));
	*ppexprInner = CTestUtils::PexprLogicalGet(memory_pool, ptabdescS, &strNameS);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprJoinWithAggSubquery
//
//	@doc:
//		Generate randomized join expression with a subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprJoinWithAggSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CExpression *pexprLeft = NULL;
	CExpression *pexprRight = NULL;
	GenerateGetExpressions(memory_pool, &pexprLeft, &pexprRight);

	CExpression *pexprInner = CTestUtils::PexprLogicalGet(memory_pool);
	CExpression *pexprSelect = PexprSelectWithAggSubquery(memory_pool, pexprLeft, pexprInner, fCorrelated);

	(*pexprSelect)[0]->AddRef();
	(*pexprSelect)[1]->AddRef();

	CExpression *pexpr = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CLogicalInnerJoin(memory_pool),
											  (*pexprSelect)[0],
											  pexprRight,
											  (*pexprSelect)[1]);

	pexprSelect->Release();

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAggSubquery
//
//	@doc:
//		Generate a Select expression with a subquery equality predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAggSubquery
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	// get any column
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();
	CColRef *pcrLeft =  pcrs->PcrAny();

	// generate agg subquery
	CExpression *pexprSubq = PexprSubqueryAgg(memory_pool, pexprOuter, pexprInner, fCorrelated);

	// generate equality predicate
	CExpression *pexprPredicate = CUtils::PexprScalarEqCmp(memory_pool, pcrLeft, pexprSubq);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprPredicate);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAggSubqueryConstComparison
//
//	@doc:
//		Generate a Select expression with a subquery equality predicate
//		involving constant
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAggSubqueryConstComparison
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	// generate agg subquery
	CExpression *pexprSubq = PexprSubqueryAgg(memory_pool, pexprOuter, pexprInner, fCorrelated);

	CExpression *pexprConst = CUtils::PexprScalarConstInt8(memory_pool, 0 /*val*/);

	// generate equality predicate
	CExpression *pexprPredicate = CUtils::PexprScalarEqCmp(memory_pool, pexprConst, pexprSubq);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprPredicate);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithAggSubquery
//
//	@doc:
//		Generate a Project expression with a subquery equality predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithAggSubquery
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	// generate agg subquery
	CExpression *pexprSubq = PexprSubqueryAgg(memory_pool, pexprOuter, pexprInner, fCorrelated);

	// generate a computed column
	CScalarSubquery *popSubquery = CScalarSubquery::PopConvert(pexprSubq->Pop());
	const IMDType *pmdtype = md_accessor->RetrieveType(popSubquery->MDIdType());
	CColRef *pcrComputed = col_factory->PcrCreate(pmdtype, popSubquery->TypeModifier());

	// generate a scalar project list
	CExpression *pexprPrjElem = CUtils::PexprScalarProjectElement(memory_pool, pcrComputed, pexprSubq);
	CExpression *pexprPrjList =
		GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool), pexprPrjElem);

	return CUtils::PexprLogicalProject(memory_pool, pexprOuter, pexprPrjList, false /*fNewComputedCol*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAggSubquery
//
//	@doc:
//		Generate randomized Select expression with a subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAggSubquery
	(
	IMemoryPool *memory_pool,
	 BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);

	return PexprSelectWithAggSubquery(memory_pool, pexprOuter, pexprInner, fCorrelated);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAggSubqueryConstComparison
//
//	@doc:
//		Generate randomized Select expression with a subquery predicate
//		involving constant
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAggSubqueryConstComparison
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);

	return PexprSelectWithAggSubqueryConstComparison(memory_pool, pexprOuter, pexprInner, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithAggSubquery
//
//	@doc:
//		Generate randomized Project expression with a subquery in project
//		element
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithAggSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);

	return PexprProjectWithAggSubquery(memory_pool, pexprOuter, pexprInner, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAggSubqueryOverJoin
//
//	@doc:
//		Generate a random select expression with a subquery over join predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAggSubqueryOverJoin
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	// generate a pair of get expressions
	CExpression *pexprR = NULL;
	CExpression *pexprS = NULL;
	GenerateGetExpressions(memory_pool, &pexprR, &pexprS);

	// generate outer expression
	CWStringConst strNameT(GPOS_WSZ_LIT("Rel3"));

	CMDIdGPDB *pmdidT = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_TEST_REL_OID3, 1, 1);
	CTableDescriptor *ptabdescT = CTestUtils::PtabdescCreate(memory_pool, 3 /*num_cols*/, pmdidT, CName(&strNameT));
	CExpression *pexprT = CTestUtils::PexprLogicalGet(memory_pool, ptabdescT, &strNameT);
	CColRef *pcrInner = CDrvdPropRelational::GetRelationalProperties(pexprR->PdpDerive())->PcrsOutput()->PcrAny();
	CColRef *pcrOuter = CDrvdPropRelational::GetRelationalProperties(pexprT->PdpDerive())->PcrsOutput()->PcrAny();

	CExpression *pexprPred = NULL;
	if (fCorrelated)
	{
		// generate correlation predicate
		pexprPred = CUtils::PexprScalarEqCmp(memory_pool, pcrInner, pcrOuter);
	}
	else
	{
		pexprPred = CUtils::PexprScalarConstBool(memory_pool, true /*m_bytearray_value*/);
	}

	// generate N-Ary join
	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexpr->Append(pexprR);
	pdrgpexpr->Append(pexprS);
	pdrgpexpr->Append(pexprPred);
	CExpression *pexprJoin = CTestUtils::PexprLogicalNAryJoin(memory_pool, pdrgpexpr);

	CExpression *pexprSubq =
		GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarSubquery(memory_pool, pcrInner, false /*fGeneratedByExist*/, false /*fGeneratedByQuantified*/), pexprJoin);

	CExpression *pexprPredOuter = CUtils::PexprScalarEqCmp(memory_pool, pcrOuter, pexprSubq);
	return CUtils::PexprLogicalSelect(memory_pool, pexprT, pexprPredOuter);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAnySubquery
//
//	@doc:
//		Generate randomized Select expression with Any subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAnySubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprSelectWithSubqueryQuantified(memory_pool, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAnySubqueryOverWindow
//
//	@doc:
//		Generate randomized Select expression with Any subquery predicate
//		over window operation
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAnySubqueryOverWindow
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprSelectWithSubqueryQuantifiedOverWindow(memory_pool, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithSubqueryQuantifiedOverWindow
//
//	@doc:
//		Generate randomized Select expression with quantified subquery
//		predicate over window operations
//
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithSubqueryQuantifiedOverWindow
	(
	IMemoryPool *memory_pool,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id || COperator::EopScalarSubqueryAll == op_id);

	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(memory_pool);
	CExpression *pexprInner = CTestUtils::PexprOneWindowFunction(memory_pool);
	CExpression *pexprSubqueryQuantified = PexprSubqueryQuantified(memory_pool, op_id, pexprOuter, pexprInner, fCorrelated);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprSubqueryQuantified);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAllSubquery
//
//	@doc:
//		Generate randomized Select expression with All subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAllSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprSelectWithSubqueryQuantified(memory_pool, COperator::EopScalarSubqueryAll, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAllSubqueryOverWindow
//
//	@doc:
//		Generate randomized Select expression with All subquery predicate
//		over window operation
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAllSubqueryOverWindow
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprSelectWithSubqueryQuantifiedOverWindow(memory_pool, COperator::EopScalarSubqueryAll, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAnyAggSubquery
//
//	@doc:
//		Generate randomized Select expression with Any subquery whose inner
//		expression is a GbAgg
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAnyAggSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprSelectWithQuantifiedAggSubquery(memory_pool, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithAllAggSubquery
//
//	@doc:
//		Generate randomized Select expression with All subquery whose inner
//		expression is a GbAgg
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithAllAggSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprSelectWithQuantifiedAggSubquery(memory_pool, COperator::EopScalarSubqueryAll, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithAnySubquery
//
//	@doc:
//		Generate randomized Project expression with Any subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithAnySubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);

	return PexprProjectWithSubqueryQuantified(memory_pool, pexprOuter, pexprInner, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithAllSubquery
//
//	@doc:
//		Generate randomized Project expression with All subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithAllSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);

	return PexprProjectWithSubqueryQuantified(memory_pool, pexprOuter, pexprInner, COperator::EopScalarSubqueryAll, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueriesInDifferentContexts
//
//	@doc:
//		Generate a randomized expression with subqueries in both m_bytearray_value
//		and filter contexts
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueriesInDifferentContexts
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);
	CExpression *pexprSelect = PexprSelectWithAggSubquery(memory_pool, pexprOuter, pexprInner, fCorrelated);

	CExpression *pexprGet = CTestUtils::PexprLogicalGet(memory_pool);
	return PexprProjectWithSubqueryQuantified(memory_pool, pexprSelect, pexprGet, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueriesInNullTestContext
//
//	@doc:
//		Generate a randomized expression expression with subquery in null
//		test context
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueriesInNullTestContext
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);

	// generate agg subquery
	CExpression *pexprSubq = PexprSubqueryAgg(memory_pool, pexprOuter, pexprInner, fCorrelated);

	// generate Is Not Null predicate
	CExpression *pexprPredicate = CUtils::PexprIsNotNull(memory_pool, pexprSubq);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprPredicate);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithExistsSubquery
//
//	@doc:
//		Generate randomized Select expression with Exists subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithExistsSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprSelectWithSubqueryExistential(memory_pool, COperator::EopScalarSubqueryExists, fCorrelated);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithNotExistsSubquery
//
//	@doc:
//		Generate randomized Select expression with Not Exists subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithNotExistsSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprSelectWithSubqueryExistential(memory_pool, COperator::EopScalarSubqueryNotExists, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithSubqueryDisjuncts
//
//	@doc:
//		Generate randomized select expression with subquery predicates in
//		an OR tree
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithSubqueryDisjuncts
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);

	return PexprSelectWithSubqueryBoolOp(memory_pool, pexprOuter, pexprInner, fCorrelated, CScalarBoolOp::EboolopOr);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithTrimmableExists
//
//	@doc:
//		Generate randomized Select expression with trimmable Exists
//		subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithTrimmableExists
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprSelectWithTrimmableExistentialSubquery(memory_pool, COperator::EopScalarSubqueryExists, fCorrelated);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithTrimmableNotExists
//
//	@doc:
//		Generate randomized Select expression with trimmable Not Exists
//		subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithTrimmableNotExists
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprSelectWithTrimmableExistentialSubquery(memory_pool, COperator::EopScalarSubqueryNotExists, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithExistsSubquery
//
//	@doc:
//		Generate randomized Project expression with Exists subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithExistsSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprProjectWithSubqueryExistential(memory_pool, COperator::EopScalarSubqueryExists, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithNotExistsSubquery
//
//	@doc:
//		Generate randomized Project expression with Not Exists subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithNotExistsSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprProjectWithSubqueryExistential(memory_pool, COperator::EopScalarSubqueryNotExists, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithNestedCmpSubquery
//
//	@doc:
//		Generate randomized Select expression with nested comparisons
//		involving subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithNestedCmpSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	CExpression *pexprSelectWithSubquery = PexprSelectWithAggSubquery(memory_pool, fCorrelated);

	CExpression *pexprLogical = (*pexprSelectWithSubquery)[0];
	CExpression *pexprSubqueryPred = (*pexprSelectWithSubquery)[1];

	// generate a parent equality predicate
	pexprSubqueryPred->AddRef();
	CExpression *pexprPredicate1 =
		CUtils::PexprScalarEqCmp(memory_pool, CUtils::PexprScalarConstBool(memory_pool, true /*m_bytearray_value*/), pexprSubqueryPred);

	// add another nesting level
	CExpression *pexprPredicate =
		CUtils::PexprScalarEqCmp(memory_pool, CUtils::PexprScalarConstBool(memory_pool, true /*m_bytearray_value*/), pexprPredicate1);

	pexprLogical->AddRef();
	pexprSelectWithSubquery->Release();

	return CUtils::PexprLogicalSelect(memory_pool, pexprLogical, pexprPredicate);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithCmpSubqueries
//
//	@doc:
//		Generate randomized Select expression with comparison between
//		two subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithCmpSubqueries
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);

	// generate a scalar subquery
	CExpression *pexprScalarSubquery1 = PexprSubqueryAgg(memory_pool, pexprOuter, pexprInner, fCorrelated);

	// generate get expression
	CWStringConst strNameT(GPOS_WSZ_LIT("Rel3"));

	CMDIdGPDB *pmdidT = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_TEST_REL_OID3, 1, 1);
	CTableDescriptor *ptabdescT = CTestUtils::PtabdescCreate(memory_pool, 3 /*num_cols*/, pmdidT, CName(&strNameT));
	CExpression *pexprT = CTestUtils::PexprLogicalGet(memory_pool, ptabdescT, &strNameT);

	// generate another scalar subquery
	CExpression *pexprScalarSubquery2 = PexprSubqueryAgg(memory_pool, pexprOuter, pexprT, fCorrelated);

	// generate equality predicate between both subqueries
	CExpression *pexprPredicate =
		CUtils::PexprScalarEqCmp(memory_pool, pexprScalarSubquery1, pexprScalarSubquery2);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprPredicate);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithNestedSubquery
//
//	@doc:
//		Generate randomized Select expression with nested subquery predicate
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithNestedSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	CExpression *pexprInner = PexprSelectWithAggSubquery(memory_pool, fCorrelated);
	CColRef *pcrInner = CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->PcrsOutput()->PcrAny();

	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(memory_pool);
	CColRef *pcrOuter = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput()->PcrAny();

	CExpression *pexprSubq = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarSubquery(memory_pool, pcrInner, false /*fGeneratedByExist*/, false /*fGeneratedByQuantified*/), pexprInner);

	CExpression *pexprPred = CUtils::PexprScalarEqCmp(memory_pool, pcrOuter, pexprSubq);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprPred);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithNestedQuantifiedSubqueries
//
//	@doc:
//		Generate a random select expression with nested quantified subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithNestedQuantifiedSubqueries
	(
	IMemoryPool *memory_pool,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id || COperator::EopScalarSubqueryAll == op_id);

	CWStringConst strName1(GPOS_WSZ_LIT("Rel1"));
	CWStringConst strAlias1(GPOS_WSZ_LIT("Rel1Alias"));
	CExpression *pexprOuter1 = CTestUtils::PexprLogicalGetNullable(memory_pool, GPOPT_TEST_REL_OID1, &strName1, &strAlias1);

	CWStringConst strName2(GPOS_WSZ_LIT("Rel2"));
	CWStringConst strAlias2(GPOS_WSZ_LIT("Rel2Alias"));
	CExpression *pexprOuter2 = CTestUtils::PexprLogicalGetNullable(memory_pool, GPOPT_TEST_REL_OID2, &strName2, &strAlias2);

	CWStringConst strName3(GPOS_WSZ_LIT("Rel3"));
	CWStringConst strAlias3(GPOS_WSZ_LIT("Rel3Alias"));
	CExpression *pexprOuter3 = CTestUtils::PexprLogicalGetNullable(memory_pool, GPOPT_TEST_REL_OID3, &strName3, &strAlias3);

	CWStringConst strName4(GPOS_WSZ_LIT("Rel4"));
	CWStringConst strAlias4(GPOS_WSZ_LIT("Rel4Alias"));
	CExpression *pexprInner = CTestUtils::PexprLogicalGetNullable(memory_pool, GPOPT_TEST_REL_OID4, &strName4, &strAlias4);

	CExpression *pexprSubqueryQuantified1 = PexprSubqueryQuantified(memory_pool, op_id, pexprOuter3, pexprInner, fCorrelated);
	CExpression *pexprSelect1 = CUtils::PexprLogicalSelect(memory_pool, pexprOuter3, pexprSubqueryQuantified1);
	CExpression *pexprSubqueryQuantified2 = PexprSubqueryQuantified(memory_pool, op_id, pexprOuter2, pexprSelect1, fCorrelated);
	CExpression *pexprSelect2 = CUtils::PexprLogicalSelect(memory_pool, pexprOuter2, pexprSubqueryQuantified2);
	CExpression *pexprSubqueryQuantified3 = PexprSubqueryQuantified(memory_pool, op_id, pexprOuter1, pexprSelect2, fCorrelated);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter1, pexprSubqueryQuantified3);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithNestedAnySubqueries
//
//	@doc:
//		Generate a random select expression with nested Any subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithNestedAnySubqueries
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprSelectWithNestedQuantifiedSubqueries(memory_pool, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithNestedAllSubqueries
//
//	@doc:
//		Generate a random select expression with nested All subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithNestedAllSubqueries
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprSelectWithNestedQuantifiedSubqueries(memory_pool, COperator::EopScalarSubqueryAll, fCorrelated);
}



//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWith2LevelsCorrSubquery
//
//	@doc:
//		Generate randomized select expression with 2-levels correlated subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWith2LevelsCorrSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	CExpression *pexpr = PexprSelectWithNestedSubquery(memory_pool, fCorrelated);
	if (fCorrelated)
	{
		// add a 2-level correlation
		CExpression *pexprOuterSubq = (*(*pexpr)[1])[1];
		CExpression *pexprInnerSubq = (*(*(*pexprOuterSubq)[0])[1])[1];
		CExpression *pexprInnerSelect = (*(*pexprInnerSubq)[0])[0];
		ExpressionArray *pdrgpexpr = (*pexprInnerSelect)[1]->PdrgPexpr();

		CColRef *pcrOuter = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput()->PcrAny();
		CColRef *pcrInner = CDrvdPropRelational::GetRelationalProperties(pexprInnerSelect->PdpDerive())->PcrsOutput()->PcrAny();
		CExpression *pexprPred = CUtils::PexprScalarEqCmp(memory_pool, pcrOuter, pcrInner);
		pdrgpexpr->Append(pexprPred);
	}

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithSubqueryConjuncts
//
//	@doc:
//		Generate randomized select expression with subquery predicates in
//		an AND tree
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithSubqueryConjuncts
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);

	return PexprSelectWithSubqueryBoolOp(memory_pool, pexprOuter, pexprInner, fCorrelated, CScalarBoolOp::EboolopAnd);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithSubqueries
//
//	@doc:
//		Generate randomized project expression with multiple subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithSubqueries
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);

	return PexprProjectWithSubqueries(memory_pool, pexprOuter, pexprInner, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubquery
//
//	@doc:
//		Helper for generating a randomized Select expression with correlated
//		predicates to be used for building subquery examples:
//
//			SELECT inner_column
//			FROM inner_expression
//			WHERE inner_column = 5 [AND outer_column = inner_column]
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubquery
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated // add a predicate to inner expression correlated with outer expression?
	)
{
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	// get a random column from inner expression
	CColRef *pcrInner = CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->PcrsOutput()->PcrAny();

	// generate a non-correlated predicate to be added to inner expression
	CExpression *pexprNonCorrelated = CUtils::PexprScalarEqCmp(memory_pool, pcrInner, CUtils::PexprScalarConstInt4(memory_pool, 5 /*val*/));

	// predicate for the inner expression
	CExpression *pexprPred = NULL;
	if (fCorrelated)
	{
		// get a random column from outer expression
		CColRef *pcrOuter = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput()->PcrAny();

		// generate correlated predicate
		CExpression *pexprCorrelated = CUtils::PexprScalarEqCmp(memory_pool, pcrOuter, pcrInner);

		// generate AND expression of correlated and non-correlated predicates
		ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
		pdrgpexpr->Append(pexprCorrelated);
		pdrgpexpr->Append(pexprNonCorrelated);
		pexprPred = CUtils::PexprScalarBoolOp(memory_pool, CScalarBoolOp::EboolopAnd, pdrgpexpr);
	}
	else
	{
		pexprPred = pexprNonCorrelated;
	}

	// generate a select on top of inner expression
	return CUtils::PexprLogicalSelect(memory_pool, pexprInner, pexprPred);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueryQuantified
//
//	@doc:
//		Generate a quantified subquery expression
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueryQuantified
	(
	IMemoryPool *memory_pool,
	COperator::EOperatorId op_id,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated // add a predicate to inner expression correlated with outer expression?
	)
{
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id ||
				COperator::EopScalarSubqueryAll == op_id);
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	CExpression *pexprSelect = PexprSubquery(memory_pool, pexprOuter, pexprInner, fCorrelated);

	// get random columns from inner expression
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->PcrsOutput();
	const CColRef *pcrInner = pcrs->PcrAny();

	// get random columns from outer expression
	pcrs = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();
	const CColRef *pcrOuter = pcrs->PcrAny();

	// return a quantified subquery expression
	if (COperator::EopScalarSubqueryAny == op_id)
	{
		const CWStringConst *str = GPOS_NEW(memory_pool) CWStringConst(GPOS_WSZ_LIT("="));
		return GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CScalarSubqueryAny(memory_pool, GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_EQ_OP), str, pcrInner),
			pexprSelect,
			CUtils::PexprScalarIdent(memory_pool, pcrOuter)
			);
	}

	const CWStringConst *str = GPOS_NEW(memory_pool) CWStringConst(GPOS_WSZ_LIT("<>"));
	return GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CScalarSubqueryAll(memory_pool, GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_NEQ_OP), str, pcrInner),
			pexprSelect,
			CUtils::PexprScalarIdent(memory_pool, pcrOuter)
			);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprUndecorrelatableSubquery
//
//	@doc:
//		Generate an expression with undecorrelatable quantified subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprUndecorrelatableSubquery
	(
	IMemoryPool *memory_pool,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id ||
				COperator::EopScalarSubqueryAll == op_id ||
				COperator::EopScalarSubqueryExists == op_id ||
				COperator::EopScalarSubqueryNotExists == op_id);

	CWStringConst strNameR(GPOS_WSZ_LIT("Rel1"));
	CMDIdGPDB *pmdidR = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1 /*version_major*/, 1 /*version_minor*/);
	CTableDescriptor *ptabdescR = CTestUtils::PtabdescPlain(memory_pool, 3 /*num_cols*/, pmdidR, CName(&strNameR));
	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(memory_pool, ptabdescR, &strNameR);

	// generate quantified subquery predicate
	CExpression *pexprInner = CTestUtils::PexprLogicalGet(memory_pool);
	CExpression *pexprSubquery = NULL;
	switch (op_id)
	{
		case COperator::EopScalarSubqueryAny:
		case COperator::EopScalarSubqueryAll:
			pexprSubquery = PexprSubqueryQuantified(memory_pool, op_id, pexprOuter, pexprInner, fCorrelated);
			break;

		case COperator::EopScalarSubqueryExists:
		case COperator::EopScalarSubqueryNotExists:
			pexprSubquery = PexprSubqueryExistential(memory_pool, op_id, pexprOuter, pexprInner, fCorrelated);
			break;

		default:
			GPOS_ASSERT(!"Invalid subquery type");
	}

	// generate a regular predicate
	CColRef *pcrOuter = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput()->PcrAny();
	CExpression *pexprPred = CUtils::PexprScalarEqCmp(memory_pool, pcrOuter, CUtils::PexprScalarConstInt4(memory_pool, 5 /*val*/));

	// generate OR expression of  predicates
	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexpr->Append(pexprSubquery);
	pdrgpexpr->Append(pexprPred);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, CPredicateUtils::PexprDisjunction(memory_pool, pdrgpexpr));
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprUndecorrelatableAnySubquery
//
//	@doc:
//		Generate an expression with undecorrelatable ANY subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprUndecorrelatableAnySubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprUndecorrelatableSubquery(memory_pool, COperator::EopScalarSubqueryAny, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprUndecorrelatableAllSubquery
//
//	@doc:
//		Generate an expression with undecorrelatable ALL subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprUndecorrelatableAllSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprUndecorrelatableSubquery(memory_pool, COperator::EopScalarSubqueryAll, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprUndecorrelatableExistsSubquery
//
//	@doc:
//		Generate an expression with undecorrelatable Exists subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprUndecorrelatableExistsSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprUndecorrelatableSubquery(memory_pool, COperator::EopScalarSubqueryExists, fCorrelated);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprUndecorrelatableNotExistsSubquery
//
//	@doc:
//		Generate an expression with undecorrelatable Not Exists subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprUndecorrelatableNotExistsSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	return PexprUndecorrelatableSubquery(memory_pool, COperator::EopScalarSubqueryNotExists, fCorrelated);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprUndecorrelatableScalarSubquery
//
//	@doc:
//		Generate an expression with undecorrelatable Scalar subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprUndecorrelatableScalarSubquery
	(
	IMemoryPool *memory_pool,
	BOOL fCorrelated
	)
{
	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(memory_pool);
	CExpression *pexprInner = CTestUtils::PexprLogicalGet(memory_pool);
	CExpression *pexprSelect = PexprSubquery(memory_pool, pexprOuter, pexprInner, fCorrelated);

	// get a random column from inner expression
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->PcrsOutput();
	CColRef *pcrInner =  pcrs->PcrAny();

	CExpression *pexprSubquery = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarSubquery(memory_pool, pcrInner, false /*fGeneratedByExist*/, false /*fGeneratedByQuantified*/), pexprSelect);

	CColRef *pcrOuter = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput()->PcrAny();
	CExpression *pexprPred = CUtils::PexprScalarEqCmp(memory_pool, pcrOuter, pexprSubquery);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprPred);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueryExistential
//
//	@doc:
//		Generate an EXISTS/NOT EXISTS subquery expression
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueryExistential
	(
	IMemoryPool *memory_pool,
	COperator::EOperatorId op_id,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated // add a predicate to inner expression correlated with outer expression?
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(COperator::EopScalarSubqueryExists == op_id ||
				COperator::EopScalarSubqueryNotExists == op_id);
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	CExpression *pexprSelect = PexprSubquery(memory_pool, pexprOuter, pexprInner, fCorrelated);

	// return a quantified subquery expression
	if (COperator::EopScalarSubqueryExists == op_id)
	{
		return GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarSubqueryExists(memory_pool), pexprSelect);
	}

	return GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarSubqueryNotExists(memory_pool), pexprSelect);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueryAgg
//
//	@doc:
//		Generate a randomized ScalarSubquery aggregate expression for
//		the following query:
//
//			SELECT sum(inner_column)
//			FROM inner_expression
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueryAgg
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated // add a predicate to inner expression correlated with outer expression?
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	CExpression *pexprSelect = PexprSubquery(memory_pool, pexprOuter, pexprInner, fCorrelated);

	// get a random column from inner expression
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->PcrsOutput();
	CColRef *pcrInner =  pcrs->PcrAny();

	// generate a SUM expression
	CExpression *pexprProjElem = CTestUtils::PexprPrjElemWithSum(memory_pool, pcrInner);
	CColRef *pcrComputed = CScalarProjectElement::PopConvert(pexprProjElem->Pop())->Pcr();

	// add SUM expression to a project list
	CExpression *pexprProjList = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool), pexprProjElem);

	// generate empty grouping columns list
	ColRefArray *colref_array = GPOS_NEW(memory_pool) ColRefArray(memory_pool);

	// generate a group by on top of select expression
	CExpression *pexprLogicalGbAgg = CUtils::PexprLogicalGbAggGlobal(memory_pool, colref_array, pexprSelect, pexprProjList);

	// return a subquery expression on top of group by
	return GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarSubquery(memory_pool, pcrComputed, false /*fGeneratedByExist*/, false /*fGeneratedByQuantified*/), pexprLogicalGbAgg);
}

//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithSubqueryBoolOp
//
//	@doc:
//		Generate a Select expression with a BoolOp (AND/OR) predicate tree involving
//		subqueries
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithSubqueryBoolOp
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated,
	CScalarBoolOp::EBoolOperator eboolop
	)
{
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	GPOS_ASSERT(CScalarBoolOp::EboolopAnd == eboolop || CScalarBoolOp::EboolopOr == eboolop);

	// get any two columns
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();
	CColRef *pcrLeft = pcrs->PcrAny();

	// generate agg subquery
	CExpression *pexprAggSubquery = PexprSubqueryAgg(memory_pool, pexprOuter, pexprInner, fCorrelated);

	// generate equality predicate involving a subquery
	CExpression *pexprPred1 = CUtils::PexprScalarEqCmp(memory_pool, pcrLeft, pexprAggSubquery);

	// generate a regular predicate
	CExpression *pexprPred2 = CUtils::PexprScalarEqCmp(memory_pool, pcrLeft, CUtils::PexprScalarConstInt4(memory_pool, 5 /*val*/));

	// generate ALL subquery
	CExpression *pexprGet = CTestUtils::PexprLogicalGet(memory_pool);
	CExpression *pexprSubqueryAll = PexprSubqueryQuantified(memory_pool, COperator::EopScalarSubqueryAll, pexprOuter, pexprGet, fCorrelated);

	// generate EXISTS subquery
	CExpression *pexprGet2 = CTestUtils::PexprLogicalGet(memory_pool);
	CExpression *pexprSubqueryExists = PexprSubqueryExistential(memory_pool, COperator::EopScalarSubqueryExists, pexprOuter, pexprGet2, fCorrelated);

	// generate AND expression of all predicates
	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexpr->Append(pexprPred1);
	pdrgpexpr->Append(pexprPred2);
	pdrgpexpr->Append(pexprSubqueryExists);
	pdrgpexpr->Append(pexprSubqueryAll);

	CExpression *pexprPred = CUtils::PexprScalarBoolOp(memory_pool, eboolop, pdrgpexpr);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprPred);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithSubqueries
//
//	@doc:
//		Generate a Project expression with multiple subqueries in project list
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithSubqueries
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != pexprOuter);
	GPOS_ASSERT(NULL != pexprInner);

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	// generate an array of project elements holding subquery expressions
	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);

	CColRef *pcrComputed = NULL;
	CExpression *pexprPrjElem = NULL;
	CExpression *pexprGet = NULL;

	const IMDTypeBool *pmdtypebool = md_accessor->PtMDType<IMDTypeBool>();

	// generate agg subquery
	CExpression *pexprAggSubquery = PexprSubqueryAgg(memory_pool, pexprOuter, pexprInner, fCorrelated);
	const CColRef *colref = CScalarSubquery::PopConvert(pexprAggSubquery->Pop())->Pcr();
	pcrComputed = col_factory->PcrCreate(colref->RetrieveType(), colref->TypeModifier());
	pexprPrjElem = CUtils::PexprScalarProjectElement(memory_pool, pcrComputed, pexprAggSubquery);
	pdrgpexpr->Append(pexprPrjElem);

	// generate ALL subquery
	pexprGet = CTestUtils::PexprLogicalGet(memory_pool);
	CExpression *pexprSubqueryAll = PexprSubqueryQuantified(memory_pool, COperator::EopScalarSubqueryAll, pexprOuter, pexprGet, fCorrelated);
	pcrComputed = col_factory->PcrCreate(pmdtypebool, default_type_modifier);
	pexprPrjElem = CUtils::PexprScalarProjectElement(memory_pool, pcrComputed, pexprSubqueryAll);
	pdrgpexpr->Append(pexprPrjElem);

	// generate existential subquery
	pexprGet =CTestUtils::PexprLogicalGet(memory_pool);
	CExpression *pexprSubqueryExists = PexprSubqueryExistential(memory_pool, COperator::EopScalarSubqueryExists, pexprOuter, pexprGet, fCorrelated);
	pcrComputed = col_factory->PcrCreate(pmdtypebool, default_type_modifier);
	pexprPrjElem = CUtils::PexprScalarProjectElement(memory_pool, pcrComputed, pexprSubqueryExists);
	pdrgpexpr->Append(pexprPrjElem);

	CExpression *pexprPrjList = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool), pdrgpexpr);

	return CUtils::PexprLogicalProject(memory_pool, pexprOuter, pexprPrjList, false /*fNewComputedCol*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithSubqueryQuantified
//
//	@doc:
//		Generate randomized Select expression with quantified subquery
//		predicate
//
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithSubqueryQuantified
	(
	IMemoryPool *memory_pool,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id || COperator::EopScalarSubqueryAll == op_id);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);
	CExpression *pexprSubqueryQuantified = PexprSubqueryQuantified(memory_pool, op_id, pexprOuter, pexprInner, fCorrelated);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprSubqueryQuantified);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithQuantifiedAggSubquery
//
//	@doc:
//		Generate randomized Select expression with quantified subquery
//		whose inner expression is an aggregate
//
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithQuantifiedAggSubquery
	(
	IMemoryPool *memory_pool,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id || COperator::EopScalarSubqueryAll == op_id);

	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(memory_pool);
	CExpression *pexprSubq = PexprSubqueryAgg(memory_pool, pexprOuter, CTestUtils::PexprLogicalGet(memory_pool), fCorrelated);
	CExpression *pexprGb = (*pexprSubq)[0];
	pexprGb->AddRef();
	pexprSubq->Release();

	CColRef *pcrInner = CDrvdPropRelational::GetRelationalProperties(pexprGb->PdpDerive())->PcrsOutput()->PcrAny();
	CColRef *pcrOuter = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput()->PcrAny();
	CExpression *pexprSubqueryQuantified = NULL;
	if (COperator::EopScalarSubqueryAny == op_id)
	{
		const CWStringConst *str = GPOS_NEW(memory_pool) CWStringConst(GPOS_WSZ_LIT("="));
		pexprSubqueryQuantified = GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CScalarSubqueryAny(memory_pool, GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_EQ_OP), str, pcrInner),
			pexprGb,
			CUtils::PexprScalarIdent(memory_pool, pcrOuter)
			);
	}

	const CWStringConst *str = GPOS_NEW(memory_pool) CWStringConst(GPOS_WSZ_LIT("<>"));
	pexprSubqueryQuantified = GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CScalarSubqueryAll(memory_pool, GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_NEQ_OP), str, pcrInner),
			pexprGb,
			CUtils::PexprScalarIdent(memory_pool, pcrOuter)
			);


	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprSubqueryQuantified);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithSubqueryQuantified
//
//	@doc:
//		Generate a randomized Project expression with a quantified subquery
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithSubqueryQuantified
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id || COperator::EopScalarSubqueryAll == op_id);

	CExpression *pexprSubqueryQuantified = PexprSubqueryQuantified(memory_pool, op_id, pexprOuter, pexprInner, fCorrelated);

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	// generate a computed column
	CScalarSubqueryQuantified *pop = CScalarSubqueryQuantified::PopConvert(pexprSubqueryQuantified->Pop());
	const IMDType *pmdtype = md_accessor->RetrieveType(pop->MDIdType());
	CColRef *pcrComputed = col_factory->PcrCreate(pmdtype, pop->TypeModifier());

	// generate a scalar project list
	CExpression *pexprPrjElem =  CUtils::PexprScalarProjectElement(memory_pool, pcrComputed, pexprSubqueryQuantified);
	CExpression *pexprPrjList = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool), pexprPrjElem);

	return CUtils::PexprLogicalProject(memory_pool, pexprOuter, pexprPrjList, false /*fNewComputedCol*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithSubqueryExistential
//
//	@doc:
//		Generate randomized Select expression with existential subquery
//		predicate
//
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithSubqueryExistential
	(
	IMemoryPool *memory_pool,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(COperator::EopScalarSubqueryExists == op_id || COperator::EopScalarSubqueryNotExists == op_id);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);
	CExpression *pexprSubqueryExistential = PexprSubqueryExistential(memory_pool, op_id, pexprOuter, pexprInner, fCorrelated);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprSubqueryExistential);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSelectWithTrimmableExistentialSubquery
//
//	@doc:
//		Generate randomized Select expression with existential subquery
//		predicate that can be trimmed
//
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSelectWithTrimmableExistentialSubquery
	(
	IMemoryPool *memory_pool,
	COperator::EOperatorId op_id,
	BOOL // fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(COperator::EopScalarSubqueryExists == op_id || COperator::EopScalarSubqueryNotExists == op_id);

	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(memory_pool);
	CExpression *pexprInner = CTestUtils::PexprLogicalGbAggWithSum(memory_pool);

	// remove grouping columns
	(*pexprInner)[0]->AddRef();
	(*pexprInner)[1]->AddRef();
	CExpression *pexprGbAgg =
		CUtils::PexprLogicalGbAggGlobal(memory_pool, GPOS_NEW(memory_pool) ColRefArray(memory_pool), (*pexprInner)[0], (*pexprInner)[1]);
	pexprInner->Release();

	// create existential subquery
	CExpression *pexprSubqueryExistential = NULL;
	if (COperator::EopScalarSubqueryExists == op_id)
	{
		pexprSubqueryExistential = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarSubqueryExists(memory_pool), pexprGbAgg);
	}
	else
	{
		pexprSubqueryExistential = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarSubqueryNotExists(memory_pool), pexprGbAgg);
	}

	// generate a regular predicate
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();
	CExpression *pexprEqPred = CUtils::PexprScalarEqCmp(memory_pool, pcrs->PcrAny(), CUtils::PexprScalarConstInt4(memory_pool, 5 /*val*/));

	CExpression *pexprConjunction =
		CPredicateUtils::PexprConjunction(memory_pool, pexprSubqueryExistential, pexprEqPred);
	pexprEqPred->Release();
	pexprSubqueryExistential->Release();

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprConjunction);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprProjectWithSubqueryExistential
//
//	@doc:
//		Generate randomized Project expression with existential subquery
//
//
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprProjectWithSubqueryExistential
	(
	IMemoryPool *memory_pool,
	COperator::EOperatorId op_id,
	BOOL fCorrelated
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(COperator::EopScalarSubqueryExists == op_id || COperator::EopScalarSubqueryNotExists == op_id);

	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);
	CExpression *pexprSubqueryExistential = PexprSubqueryExistential(memory_pool, op_id, pexprOuter, pexprInner, fCorrelated);

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	// generate a computed column
	CScalarSubqueryExistential *pop = CScalarSubqueryExistential::PopConvert(pexprSubqueryExistential->Pop());
	const IMDType *pmdtype = md_accessor->RetrieveType(pop->MDIdType());
	CColRef *pcrComputed = col_factory->PcrCreate(pmdtype, pop->TypeModifier());

	// generate a scalar project list
	CExpression *pexprPrjElem = CUtils::PexprScalarProjectElement(memory_pool, pcrComputed, pexprSubqueryExistential);
	CExpression *pexprPrjList = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool), pexprPrjElem);

	return CUtils::PexprLogicalProject(memory_pool, pexprOuter, pexprPrjList, false /*fNewComputedCol*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueryWithConstTableGet
//
//	@doc:
//		Generate Select expression with Any subquery predicate over a const
//		table get
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueryWithConstTableGet
	(
	IMemoryPool *memory_pool,
	COperator::EOperatorId op_id
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id ||
				COperator::EopScalarSubqueryAll == op_id);

	CWStringConst strNameR(GPOS_WSZ_LIT("Rel1"));
	CMDIdGPDB *pmdidR = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1, 1);
	CTableDescriptor *ptabdescR = CTestUtils::PtabdescCreate(memory_pool, 3 /*num_cols*/, pmdidR, CName(&strNameR));

	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(memory_pool, ptabdescR, &strNameR);
	CExpression *pexprConstTableGet = CTestUtils::PexprConstTableGet(memory_pool, 3 /* ulElements */);

	// get random columns from inner expression
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprConstTableGet->PdpDerive())->PcrsOutput();
	const CColRef *pcrInner = pcrs->PcrAny();

	// get random columns from outer expression
	pcrs = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();
	const CColRef *pcrOuter = pcrs->PcrAny();

	const CWStringConst *str = GPOS_NEW(memory_pool) CWStringConst(GPOS_WSZ_LIT("="));

	CExpression *pexprSubquery = NULL;
	if (COperator::EopScalarSubqueryAny == op_id)
	{
		// construct ANY subquery expression
		pexprSubquery = GPOS_NEW(memory_pool) CExpression
									(
									memory_pool,
									GPOS_NEW(memory_pool) CScalarSubqueryAny(memory_pool, GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_EQ_OP), str, pcrInner),
									pexprConstTableGet,
									CUtils::PexprScalarIdent(memory_pool, pcrOuter)
									);
	}
	else
	{
		// construct ALL subquery expression
		pexprSubquery = GPOS_NEW(memory_pool) CExpression
									(
									memory_pool,
									GPOS_NEW(memory_pool) CScalarSubqueryAll(memory_pool, GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_EQ_OP), str, pcrInner),
									pexprConstTableGet,
									CUtils::PexprScalarIdent(memory_pool, pcrOuter)
									);

	}

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprSubquery);
}


//---------------------------------------------------------------------------
//	@function:
//		CSubqueryTestUtils::PexprSubqueryWithDisjunction
//
//	@doc:
//		Generate Select expression with a disjunction of two Any subqueries over
//		const table get
//
//---------------------------------------------------------------------------
CExpression *
CSubqueryTestUtils::PexprSubqueryWithDisjunction
	(
	IMemoryPool *memory_pool
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CWStringConst strNameR(GPOS_WSZ_LIT("Rel1"));
	CMDIdGPDB *pmdidR = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1, 1);
	CTableDescriptor *ptabdescR = CTestUtils::PtabdescCreate(memory_pool, 3 /*num_cols*/, pmdidR, CName(&strNameR));

	CExpression *pexprOuter = CTestUtils::PexprLogicalGet(memory_pool, ptabdescR, &strNameR);
	CExpression *pexprConstTableGet = CTestUtils::PexprConstTableGet(memory_pool, 3 /* ulElements */);

	// get random columns from inner expression
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprConstTableGet->PdpDerive())->PcrsOutput();
	const CColRef *pcrInner = pcrs->PcrAny();

	// get random columns from outer expression
	pcrs = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();
	const CColRef *pcrOuter = pcrs->PcrAny();

	const CWStringConst *str = GPOS_NEW(memory_pool) CWStringConst(GPOS_WSZ_LIT("="));

	CExpression *pexprSubquery = GPOS_NEW(memory_pool) CExpression
									(
									memory_pool,
									GPOS_NEW(memory_pool) CScalarSubqueryAny(memory_pool, GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_EQ_OP), str, pcrInner),
									pexprConstTableGet,
									CUtils::PexprScalarIdent(memory_pool, pcrOuter)
									);
	pexprSubquery->AddRef();

	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexpr->Append(pexprSubquery);
	pdrgpexpr->Append(pexprSubquery);

	// generate a disjunction of the subquery with itself
	CExpression *pexprBoolOp = CUtils::PexprScalarBoolOp(memory_pool, CScalarBoolOp::EboolopOr, pdrgpexpr);

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprBoolOp);
}

// EOF
