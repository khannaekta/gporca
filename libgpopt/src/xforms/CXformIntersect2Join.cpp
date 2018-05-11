//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal Inc.
//
//	@filename:
//		CXformIntersect2Join.cpp
//
//	@doc:
//		Implement the transformation of Intersect into a Join
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/exception.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/CColRefComputed.h"
#include "gpopt/operators/ops.h"
#include "gpopt/operators/COperator.h"
#include "gpopt/xforms/CXformIntersect2Join.h"
#include "gpopt/xforms/CXformUtils.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"

using namespace gpmd;
using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformIntersect2Join::CXformIntersect2Join
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformIntersect2Join::CXformIntersect2Join
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
					GPOS_NEW(memory_pool) CLogicalIntersect(memory_pool),
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // left relational child
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)) // right relational child
					)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformIntersect2Join::Transform
//
//	@doc:
//		Actual transformation that transforms an intersect into a join
//		over a group by over the inputs
//
//---------------------------------------------------------------------------
void
CXformIntersect2Join::Transform
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

	GPOS_ASSERT(2 == pexpr->Arity());

	// extract components
	CExpression *pexprLeftChild = (*pexpr)[0];
	CExpression *pexprRightChild = (*pexpr)[1];

	CLogicalIntersect *popIntersect = CLogicalIntersect::PopConvert(pexpr->Pop());
	DrgDrgPcr *pdrgpdrgpcrInput = popIntersect->PdrgpdrgpcrInput();

	// construct group by over the left and right expressions

	CExpression *pexprProjList = GPOS_NEW(memory_pool) CExpression
											(
											memory_pool,
											GPOS_NEW(memory_pool) CScalarProjectList(memory_pool),
											GPOS_NEW(memory_pool) DrgPexpr(memory_pool)
											);
	pexprProjList->AddRef();
	pexprLeftChild->AddRef();
	pexprRightChild->AddRef();
	(*pdrgpdrgpcrInput)[0]->AddRef();
	(*pdrgpdrgpcrInput)[1]->AddRef();
	
	CExpression *pexprLeftAgg = CUtils::PexprLogicalGbAggGlobal(memory_pool, (*pdrgpdrgpcrInput)[0], pexprLeftChild, pexprProjList);
	CExpression *pexprRightAgg = CUtils::PexprLogicalGbAggGlobal(memory_pool, (*pdrgpdrgpcrInput)[1], pexprRightChild, pexprProjList);

	CExpression *pexprScCond = CUtils::PexprConjINDFCond(memory_pool, pdrgpdrgpcrInput);

	CExpression *pexprJoin = GPOS_NEW(memory_pool) CExpression
										(
										memory_pool,
										GPOS_NEW(memory_pool) CLogicalInnerJoin(memory_pool),
										pexprLeftAgg,
										pexprRightAgg,
										pexprScCond
										);


	pxfres->Add(pexprJoin);
}

// EOF
