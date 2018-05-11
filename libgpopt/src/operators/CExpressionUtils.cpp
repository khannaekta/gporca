//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CExpressionUtils.cpp
//
//	@doc:
//		Utility routines for transforming expressions
//
//	@owner:
//		, 
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CColRefTable.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/exception.h"

#include "gpopt/operators/ops.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/CExpressionUtils.h"

#include "gpopt/mdcache/CMDAccessor.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDType.h"
#include "naucrates/statistics/CStatistics.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CExpressionUtils::UnnestChild
//
//	@doc:
//		Unnest a given expression's child and append unnested nodes to
//		the given expression array
//
//---------------------------------------------------------------------------
void CExpressionUtils::UnnestChild
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr, // parent node
	ULONG child_index, // child index
	BOOL fAnd, // is expression an AND node?
	BOOL fOr, // is expression an OR node?
	BOOL fHasNegatedChild, // does expression have NOT child nodes?
	DrgPexpr *pdrgpexpr // array to append results to
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(child_index < pexpr->Arity());
	GPOS_ASSERT(NULL != pdrgpexpr);

	CExpression *pexprChild = (*pexpr)[child_index];

	if ((fAnd && CPredicateUtils::FAnd(pexprChild)) ||
		(fOr && CPredicateUtils::FOr(pexprChild)))
	{
		// two cascaded AND nodes or two cascaded OR nodes, recursively
		// pull-up children
		AppendChildren(memory_pool, pexprChild, pdrgpexpr);

		return;
	}

	if (fHasNegatedChild &&
		CPredicateUtils::FNot(pexprChild) &&
		CPredicateUtils::FNot((*pexprChild)[0]))
	{
		// two cascaded Not nodes cancel each other
		CExpression *pexprNot = (*pexprChild)[0];
		pexprChild = (*pexprNot)[0];
	}
	CExpression *pexprUnnestedChild = PexprUnnest(memory_pool, pexprChild);
	pdrgpexpr->Append(pexprUnnestedChild);
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionUtils::AppendChildren
//
//	@doc:
//		Append the unnested children of given expression to given array
//
//---------------------------------------------------------------------------
void CExpressionUtils::AppendChildren
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	DrgPexpr *pdrgpexpr
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(NULL != pdrgpexpr);

	DrgPexpr *pdrgpexprChildren = PdrgpexprUnnestChildren(memory_pool, pexpr);
	CUtils::AddRefAppend<CExpression, CleanupRelease>(pdrgpexpr, pdrgpexprChildren);
	pdrgpexprChildren->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionUtils::PdrgpexprUnnestChildren
//
//	@doc:
//		Return an array of expression's children after unnesting nested
//		AND/OR/NOT subtrees
//
//---------------------------------------------------------------------------
DrgPexpr *
CExpressionUtils::PdrgpexprUnnestChildren
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr
	)
{
	// protect against stack overflow during recursion
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexpr);

	// compute flags for cases where we may have nested predicates
	BOOL fAnd = CPredicateUtils::FAnd(pexpr);
	BOOL fOr = CPredicateUtils::FOr(pexpr);
	BOOL fHasNegatedChild = CPredicateUtils::FHasNegatedChild(pexpr);

	DrgPexpr *pdrgpexpr = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	const ULONG arity = pexpr->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		UnnestChild(memory_pool, pexpr, ul, fAnd, fOr, fHasNegatedChild, pdrgpexpr);
	}

	return pdrgpexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CExpressionUtils::PexprUnnest
//
//	@doc:
//		Unnest AND/OR/NOT predicates
//
//---------------------------------------------------------------------------
CExpression *
CExpressionUtils::PexprUnnest
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr
	)
{
	// protect against stack overflow during recursion
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexpr);

	if (CPredicateUtils::FNot(pexpr))
	{
		CExpression *pexprChild = (*pexpr)[0];
		CExpression *pexprPushedNot = PexprPushNotOneLevel(memory_pool, pexprChild);

		COperator *pop = pexprPushedNot->Pop();
		DrgPexpr *pdrgpexpr = PdrgpexprUnnestChildren(memory_pool, pexprPushedNot);
		pop->AddRef();

		// clean up
		pexprPushedNot->Release();

		return GPOS_NEW(memory_pool) CExpression(memory_pool, pop, pdrgpexpr);
	}

	COperator *pop = pexpr->Pop();
	DrgPexpr *pdrgpexpr = PdrgpexprUnnestChildren(memory_pool, pexpr);
	pop->AddRef();

	return GPOS_NEW(memory_pool) CExpression(memory_pool, pop, pdrgpexpr);
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionUtils::PexprPushNotOneLevel
//
//	@doc:
// 		Push not expression one level down the given expression. For example:
// 		1. AND of expressions into an OR a negation of these expression
// 		2. OR of expressions into an AND a negation of these expression
// 		3. EXISTS into NOT EXISTS and vice versa
//      4. Else, return NOT of given expression
//---------------------------------------------------------------------------
CExpression *
CExpressionUtils::PexprPushNotOneLevel
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);

	BOOL fAnd = CPredicateUtils::FAnd(pexpr);
	BOOL fOr = CPredicateUtils::FOr(pexpr);

	if (fAnd || fOr)
	{
		COperator *popNew = NULL;

		if (fOr)
		{
			popNew = GPOS_NEW(memory_pool) CScalarBoolOp(memory_pool, CScalarBoolOp::EboolopAnd);
		}
		else
		{
			popNew = GPOS_NEW(memory_pool) CScalarBoolOp(memory_pool, CScalarBoolOp::EboolopOr);
		}

		DrgPexpr *pdrgpexpr = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
		const ULONG arity = pexpr->Arity();
		for (ULONG ul = 0; ul < arity; ul++)
		{
			CExpression *pexprChild = (*pexpr)[ul];
			pexprChild->AddRef();
			pdrgpexpr->Append(CUtils::PexprNegate(memory_pool, pexprChild));
		}

		return GPOS_NEW(memory_pool) CExpression(memory_pool, popNew, pdrgpexpr);
	}

	const COperator *pop = pexpr->Pop();
	if (COperator::EopScalarSubqueryExists == pop->Eopid())
	{
		pexpr->PdrgPexpr()->AddRef();
		return GPOS_NEW(memory_pool) CExpression
							(
							memory_pool,
							GPOS_NEW(memory_pool) CScalarSubqueryNotExists(memory_pool),
							pexpr->PdrgPexpr()
							);
	}

	if (COperator::EopScalarSubqueryNotExists == pop->Eopid())
	{
		pexpr->PdrgPexpr()->AddRef();
		return GPOS_NEW(memory_pool) CExpression
							(
							memory_pool,
							GPOS_NEW(memory_pool) CScalarSubqueryExists(memory_pool),
							pexpr->PdrgPexpr()
							);
	}

	// TODO: , Feb 4 2015, we currently only handling EXISTS/NOT EXISTS/AND/OR
	pexpr->AddRef();
	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CScalarBoolOp(memory_pool, CScalarBoolOp::EboolopNot),
					pexpr
					);
}


//---------------------------------------------------------------------------
//	@function:
//		CExpressionUtils::PexprDedupChildren
//
//	@doc:
//		Remove duplicate AND/OR children
//
//---------------------------------------------------------------------------
CExpression *
CExpressionUtils::PexprDedupChildren
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr
	)
{
	// protect against stack overflow during recursion
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexpr);

	// recursively process children
	const ULONG arity = pexpr->Arity();
	DrgPexpr *pdrgpexprChildren = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprChild = PexprDedupChildren(memory_pool, (*pexpr)[ul]);
		pdrgpexprChildren->Append(pexprChild);
	}

	if (CPredicateUtils::FAnd(pexpr) || CPredicateUtils::FOr(pexpr))
	{
		DrgPexpr *pdrgpexprNewChildren = CUtils::PdrgpexprDedup(memory_pool, pdrgpexprChildren);

		pdrgpexprChildren->Release();
		pdrgpexprChildren = pdrgpexprNewChildren;

		// Check if we end with one child, return that child
		if (1 == pdrgpexprChildren->Size())
		{
			CExpression *pexprChild = (*pdrgpexprChildren)[0];
			pexprChild->AddRef();
			pdrgpexprChildren->Release();

			return pexprChild;
		}
	}


	COperator *pop = pexpr->Pop();
	pop->AddRef();
	return GPOS_NEW(memory_pool) CExpression(memory_pool, pop, pdrgpexprChildren);
}

// EOF
