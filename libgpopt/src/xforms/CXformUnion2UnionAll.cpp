//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformUnion2UnionAll.cpp
//
//	@doc:
//		Implementation of the transformation that takes a logical union and
//		coverts it into an aggregate over a logical union all
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformUnion2UnionAll.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CXformUnion2UnionAll::CXformUnion2UnionAll
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformUnion2UnionAll::CXformUnion2UnionAll
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
						GPOS_NEW(memory_pool) CLogicalUnion(memory_pool),
						GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternMultiLeaf(memory_pool))
						)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformUnion2UnionAll::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformUnion2UnionAll::Transform
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
	CLogicalUnion *popUnion = CLogicalUnion::PopConvert(pexpr->Pop());
	ColRefArray *pdrgpcrOutput = popUnion->PdrgpcrOutput();
	ColRefArrays *pdrgpdrgpcrInput = popUnion->PdrgpdrgpcrInput();

	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	const ULONG arity = pexpr->Arity();

	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprChild = (*pexpr)[ul];
		pexprChild->AddRef();
		pdrgpexpr->Append(pexprChild);
	}

	pdrgpcrOutput->AddRef();
	pdrgpdrgpcrInput->AddRef();

	// assemble new logical operator
	CExpression *pexprUnionAll = GPOS_NEW(memory_pool) CExpression
									(
									memory_pool,
									GPOS_NEW(memory_pool) CLogicalUnionAll(memory_pool, pdrgpcrOutput, pdrgpdrgpcrInput),
									pdrgpexpr
									);

	pdrgpcrOutput->AddRef();

	CExpression *pexprProjList = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool), GPOS_NEW(memory_pool) ExpressionArray(memory_pool));

	CExpression *pexprAgg = GPOS_NEW(memory_pool) CExpression
										(
										memory_pool,
										GPOS_NEW(memory_pool) CLogicalGbAgg(memory_pool, pdrgpcrOutput, COperator::EgbaggtypeGlobal /*egbaggtype*/),
										pexprUnionAll,
										pexprProjList
										);

	// add alternative to results
	pxfres->Add(pexprAgg);
}

// EOF
