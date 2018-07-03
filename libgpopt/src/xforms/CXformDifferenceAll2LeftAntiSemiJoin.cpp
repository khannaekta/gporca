//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformDifferenceAll2LeftAntiSemiJoin.cpp
//
//	@doc:
//		Implementation of the transformation a logical difference all into LASJ
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/operators/ops.h"
#include "gpopt/exception.h"
#include "gpopt/xforms/CXformDifferenceAll2LeftAntiSemiJoin.h"
#include "gpopt/xforms/CXformIntersectAll2LeftSemiJoin.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CXformDifferenceAll2LeftAntiSemiJoin::CXformDifferenceAll2LeftAntiSemiJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformDifferenceAll2LeftAntiSemiJoin::CXformDifferenceAll2LeftAntiSemiJoin
	(
	IMemoryPool *memory_pool
	)
	:
	// pattern
	CXformExploration
		(
		GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalDifferenceAll(memory_pool),
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternMultiLeaf(memory_pool))
					)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformDifferenceAll2LeftAntiSemiJoin::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformDifferenceAll2LeftAntiSemiJoin::Transform
	(
	CXformContext *pxfctxt,
	CXformResult *pxfres,
	CExpression *pexpr
	)
	const
{
	GPOS_ASSERT(NULL != pxfctxt);
	GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
	GPOS_ASSERT(FCheckPattern(pexpr));

	IMemoryPool *memory_pool = pxfctxt->Pmp();

	// TODO: , Jan 8th 2013, we currently only handle difference all
	//  operators with two children
	GPOS_ASSERT(2 == pexpr->Arity());

	// extract components
	CExpression *pexprLeftChild = (*pexpr)[0];
	CExpression *pexprRightChild = (*pexpr)[1];

	CLogicalDifferenceAll *popDifferenceAll = CLogicalDifferenceAll::PopConvert(pexpr->Pop());
	ColRefArrays *pdrgpdrgpcrInput = popDifferenceAll->PdrgpdrgpcrInput();

	CExpression *pexprLeftWindow = CXformUtils::PexprWindowWithRowNumber(memory_pool, pexprLeftChild, (*pdrgpdrgpcrInput)[0]);
	CExpression *pexprRightWindow = CXformUtils::PexprWindowWithRowNumber(memory_pool, pexprRightChild, (*pdrgpdrgpcrInput)[1]);

	ColRefArrays *pdrgpdrgpcrInputNew = GPOS_NEW(memory_pool) ColRefArrays(memory_pool);
	ColRefArray *pdrgpcrLeftNew = CUtils::PdrgpcrExactCopy(memory_pool, (*pdrgpdrgpcrInput)[0]);
	pdrgpcrLeftNew->Append(CXformUtils::PcrProjectElement(pexprLeftWindow, 0 /* row_number window function*/));

	ColRefArray *pdrgpcrRightNew = CUtils::PdrgpcrExactCopy(memory_pool, (*pdrgpdrgpcrInput)[1]);
	pdrgpcrRightNew->Append(CXformUtils::PcrProjectElement(pexprRightWindow, 0 /* row_number window function*/));

	pdrgpdrgpcrInputNew->Append(pdrgpcrLeftNew);
	pdrgpdrgpcrInputNew->Append(pdrgpcrRightNew);

	// generate the scalar condition for the left anti-semi join
	CExpression *pexprScCond = CUtils::PexprConjINDFCond(memory_pool, pdrgpdrgpcrInputNew);

	// assemble the new left anti-semi join logical operator
	CExpression *pexprLASJ = GPOS_NEW(memory_pool) CExpression
										(
										memory_pool,
										GPOS_NEW(memory_pool) CLogicalLeftAntiSemiJoin(memory_pool),
										pexprLeftWindow,
										pexprRightWindow,
										pexprScCond
										);

	// clean up
	pdrgpdrgpcrInputNew->Release();

	// add alternative to results
	pxfres->Add(pexprLASJ);
}

// EOF
