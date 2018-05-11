//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformSimplifyLeftOuterJoin.cpp
//
//	@doc:
//		Simplify Left Outer Join with constant false predicate
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformSimplifyLeftOuterJoin.h"

using namespace gpmd;
using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformSimplifyLeftOuterJoin::CXformSimplifyLeftOuterJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformSimplifyLeftOuterJoin::CXformSimplifyLeftOuterJoin
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
					GPOS_NEW(memory_pool) CLogicalLeftOuterJoin(memory_pool),
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // left child
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)),  // right child
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool))  // predicate tree
					)
		)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformSimplifyLeftOuterJoin::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformSimplifyLeftOuterJoin::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	CExpression *pexprScalar = exprhdl.PexprScalarChild(2 /*child_index*/);
	if (CUtils::FScalarConstFalse(pexprScalar))
	{
		// if LOJ predicate is False, we can replace inner child with empty table
		return CXform::ExfpHigh;
	}

	return CXform::ExfpNone;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformSimplifyLeftOuterJoin::Transform
//
//	@doc:
//		Actual transformation to simplify left outer join
//
//---------------------------------------------------------------------------
void
CXformSimplifyLeftOuterJoin::Transform
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

	// extract components
	CExpression *pexprOuter = (*pexpr)[0];
	CExpression *pexprInner = (*pexpr)[1];
	CExpression *pexprScalar = (*pexpr)[2];

	pexprOuter->AddRef();
	pexprScalar->AddRef();
	CExpression *pexprResult = NULL;

	// inner child of LOJ can be replaced with empty table
	GPOS_ASSERT(CUtils::FScalarConstFalse(pexprScalar));

	// extract output columns of inner child
	DrgPcr *colref_array = CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->PcrsOutput()->Pdrgpcr(memory_pool);

	// generate empty constant table with the same columns
	COperator *popCTG = GPOS_NEW(memory_pool) CLogicalConstTableGet(memory_pool, colref_array, GPOS_NEW(memory_pool) DrgPdrgPdatum(memory_pool));
	pexprResult =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CLogicalLeftOuterJoin(memory_pool),
			pexprOuter,
			GPOS_NEW(memory_pool) CExpression(memory_pool, popCTG),
			pexprScalar
			);

	pxfres->Add(pexprResult);
}

// EOF
