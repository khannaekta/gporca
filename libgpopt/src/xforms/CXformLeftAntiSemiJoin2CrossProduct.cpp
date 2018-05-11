//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformLeftAntiSemiJoin2CrossProduct.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/ops.h"
#include "gpopt/operators/CNormalizer.h"
#include "gpopt/xforms/CXformLeftAntiSemiJoin2CrossProduct.h"
#include "gpopt/xforms/CXformUtils.h"



using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformLeftAntiSemiJoin2CrossProduct::CXformLeftAntiSemiJoin2CrossProduct
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CXformLeftAntiSemiJoin2CrossProduct::CXformLeftAntiSemiJoin2CrossProduct
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
					GPOS_NEW(memory_pool) CLogicalLeftAntiSemiJoin(memory_pool),
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool)), // left child is a tree since we may need to push predicates down
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // right child
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool))  // predicate is a tree since we may need to do clean-up of scalar expression
					)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformLeftAntiSemiJoin2CrossProduct::CXformLeftAntiSemiJoin2CrossProduct
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CXformLeftAntiSemiJoin2CrossProduct::CXformLeftAntiSemiJoin2CrossProduct
	(
	CExpression *pexprPattern
	)
	:
	CXformExploration(pexprPattern)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformLeftAntiSemiJoin2CrossProduct::Exfp
//
//	@doc:
//		 Compute xform promise for a given expression handle
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformLeftAntiSemiJoin2CrossProduct::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	return CXformUtils::ExfpSemiJoin2CrossProduct(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformLeftAntiSemiJoin2CrossProduct::Transform
//
//	@doc:
//		Anti semi join whose join predicate does not use columns from
//		join's inner child is equivalent to a Cross Product between outer
//		child (after pushing negated join predicate), and one tuple from
//		inner child
//
//
//---------------------------------------------------------------------------
void
CXformLeftAntiSemiJoin2CrossProduct::Transform
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

	// extract components
	CExpression *pexprOuter = (*pexpr)[0];
	CExpression *pexprInner = (*pexpr)[1];
	CExpression *pexprScalar = (*pexpr)[2];
	pexprOuter->AddRef();
	pexprInner->AddRef();
	pexprScalar->AddRef();

	CExpression *pexprNegatedScalar = CUtils::PexprNegate(memory_pool, pexprScalar);

	// create a (limit 1) on top of inner child
	CExpression *pexprLimitOffset = CUtils::PexprScalarConstInt8(memory_pool, 0 /*val*/);
	CExpression *pexprLimitCount = CUtils::PexprScalarConstInt8(memory_pool, 1 /*val*/);
	COrderSpec *pos = GPOS_NEW(memory_pool) COrderSpec(memory_pool);
	CLogicalLimit *popLimit =
			GPOS_NEW(memory_pool) CLogicalLimit(memory_pool, pos, true /*fGlobal*/, true /*fHasCount*/, false /*fNonRemovableLimit*/);
	CExpression *pexprLimit = GPOS_NEW(memory_pool) CExpression(memory_pool, popLimit, pexprInner, pexprLimitOffset, pexprLimitCount);

	// create cross product
	CExpression *pexprJoin = CUtils::PexprLogicalJoin<CLogicalInnerJoin>(memory_pool, pexprOuter, pexprLimit, pexprNegatedScalar);
	CExpression *pexprNormalized = CNormalizer::PexprNormalize(memory_pool, pexprJoin);
	pexprJoin->Release();

	pxfres->Add(pexprNormalized);
}


// EOF
