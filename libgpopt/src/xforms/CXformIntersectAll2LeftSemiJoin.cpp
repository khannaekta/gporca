//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformIntersectAll2LeftSemiJoin.cpp
//
//	@doc:
//		Implement the transformation of CLogicalIntersectAll into a left semi join
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/exception.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/CColRefComputed.h"
#include "gpopt/operators/ops.h"
#include "gpopt/operators/COperator.h"
#include "gpopt/xforms/CXformIntersectAll2LeftSemiJoin.h"
#include "gpopt/xforms/CXformUtils.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"

using namespace gpmd;
using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformIntersectAll2LeftSemiJoin::CXformIntersectAll2LeftSemiJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformIntersectAll2LeftSemiJoin::CXformIntersectAll2LeftSemiJoin
	(
	IMemoryPool *memory_pool
	)
	:
	CXformExploration
		(
		 // pattern
		GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalIntersectAll(memory_pool),
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // left relational child
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)) // right relational child
					)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformIntersectAll2LeftSemiJoin::Transform
//
//	@doc:
//		Actual transformation that transforms a intersect all into a left semi
//		join over a window operation over the inputs
//
//---------------------------------------------------------------------------
void
CXformIntersectAll2LeftSemiJoin::Transform
	(
	CXformContext *pxfctxt,
	CXformResult *pxfres,
	CExpression *pexpr
	)
	const
{
	GPOS_ASSERT(NULL != pxfctxt);
	GPOS_ASSERT(NULL != pxfres);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	IMemoryPool *memory_pool = pxfctxt->Pmp();

	// TODO: we currently only handle intersect all operators that
	// have two children
	GPOS_ASSERT(2 == pexpr->Arity());

	// extract components
	CExpression *pexprLeftChild = (*pexpr)[0];
	CExpression *pexprRightChild = (*pexpr)[1];

	CLogicalIntersectAll *popIntersectAll = CLogicalIntersectAll::PopConvert(pexpr->Pop());
	ColRefArrays *pdrgpdrgpcrInput = popIntersectAll->PdrgpdrgpcrInput();

	CExpression *pexprLeftWindow = CXformUtils::PexprWindowWithRowNumber(memory_pool, pexprLeftChild, (*pdrgpdrgpcrInput)[0]);
	CExpression *pexprRightWindow = CXformUtils::PexprWindowWithRowNumber(memory_pool, pexprRightChild, (*pdrgpdrgpcrInput)[1]);

	ColRefArrays *pdrgpdrgpcrInputNew = GPOS_NEW(memory_pool) ColRefArrays(memory_pool);
	ColRefArray *pdrgpcrLeftNew = CUtils::PdrgpcrExactCopy(memory_pool, (*pdrgpdrgpcrInput)[0]);
	pdrgpcrLeftNew->Append(CXformUtils::PcrProjectElement(pexprLeftWindow, 0 /* row_number window function*/));

	ColRefArray *pdrgpcrRightNew = CUtils::PdrgpcrExactCopy(memory_pool, (*pdrgpdrgpcrInput)[1]);
	pdrgpcrRightNew->Append(CXformUtils::PcrProjectElement(pexprRightWindow, 0 /* row_number window function*/));

	pdrgpdrgpcrInputNew->Append(pdrgpcrLeftNew);
	pdrgpdrgpcrInputNew->Append(pdrgpcrRightNew);

	CExpression *pexprScCond = CUtils::PexprConjINDFCond(memory_pool, pdrgpdrgpcrInputNew);

	// assemble the new logical operator
	CExpression *pexprLSJ = GPOS_NEW(memory_pool) CExpression
										(
										memory_pool,
										GPOS_NEW(memory_pool) CLogicalLeftSemiJoin(memory_pool),
										pexprLeftWindow,
										pexprRightWindow,
										pexprScCond
										);

	// clean up
	pdrgpdrgpcrInputNew->Release();

	pxfres->Add(pexprLSJ);
}

// EOF
