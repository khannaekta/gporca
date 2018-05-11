//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software Inc.
//
//	@filename:
//		CXformRemoveSubqDistinct.cpp
//
//	@doc:
//		Implementation of the transform that removes distinct clause from subquery
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/ops.h"
#include "gpopt/operators/COperator.h"
#include "gpopt/xforms/CXformUtils.h"
#include "gpopt/xforms/CXformRemoveSubqDistinct.h"

using namespace gpopt;

CXformRemoveSubqDistinct::CXformRemoveSubqDistinct
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
			GPOS_NEW(memory_pool) CLogicalSelect(memory_pool),
			GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // relational child
			GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool))	// predicate tree
			)
	)
{}

CXform::EXformPromise
CXformRemoveSubqDistinct::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	// consider this transformation only if subqueries exist
	if (!exprhdl.GetDrvdScalarProps(1)->FHasSubquery())
	{
		return CXform::ExfpNone;
	}

	CExpression *pexprScalar = exprhdl.PexprScalarChild(1);
	COperator *pop = pexprScalar->Pop();
	if (CUtils::FQuantifiedSubquery(pop) || CUtils::FExistentialSubquery(pop))
	{
		return CXform::ExfpHigh;
	}

	return CXform::ExfpNone;
}

// For quantified subqueries (IN / NOT IN), the following transformation will be applied:
// CLogicalSelect
// |--CLogicalGet "foo"
// +--CScalarSubqueryAny(=)["c" (9)]
//    |--CLogicalGbAgg( Global ) Grp Cols: ["c" (9)]
//    |  |--CLogicalGet "bar"
//    |  +--CScalarProjectList
//    +--CScalarIdent "a" (0)
//
// will produce
//
// CLogicalSelect
// |--CLogicalGet "foo"
// +--CScalarSubqueryAny(=)["c" (9)]
//    |--CLogicalGet "bar"
//    +--CScalarIdent "a" (0)
//
// For existential subqueries (EXISTS / NOT EXISTS), the following transformation will be applied:
// CLogicalSelect
// |--CLogicalGet "foo"
// +--CScalarSubqueryExists
//    +--CLogicalGbAgg( Global ) Grp Cols: ["c" (9)]
//       |--CLogicalGet "bar"
//       +--CScalarProjectList
//
// will produce
//
// CLogicalSelect
// |--CLogicalGet "foo"
// +--CScalarSubqueryExists
//    +--CLogicalGet "bar"
//
void
CXformRemoveSubqDistinct::Transform
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
	CExpression *pexprScalar = (*pexpr)[1];
	CExpression *pexprGbAgg = (*pexprScalar)[0];

	if (COperator::EopLogicalGbAgg == pexprGbAgg->Pop()->Eopid())
	{
		CExpression *pexprGbAggProjectList = (*pexprGbAgg)[1];
		// only consider removing distinct when there is no aggregation functions
		if (0 == pexprGbAggProjectList->Arity())
		{
			CExpression *pexprNewScalar = NULL;
			CExpression *pexprRelChild = (*pexprGbAgg)[0];
			pexprRelChild->AddRef();

			COperator *pop = pexprScalar->Pop();
			pop->AddRef();
			if (CUtils::FExistentialSubquery(pop))
			{
				// EXIST/NOT EXIST scalar subquery
				pexprNewScalar = GPOS_NEW(memory_pool) CExpression(memory_pool, pop, pexprRelChild);
			}
			else
			{
				// IN/NOT IN scalar subquery
				CExpression *pexprScalarIdent = (*pexprScalar)[1];
				pexprScalarIdent->AddRef();
				pexprNewScalar = GPOS_NEW(memory_pool) CExpression(memory_pool, pop, pexprRelChild, pexprScalarIdent);
			}

			pexpr->Pop()->AddRef(); // logical select operator
			(*pexpr)[0]->AddRef(); // relational child of logical select

			// new logical select expression
			CExpression *ppexprNew = GPOS_NEW(memory_pool) CExpression(memory_pool, pexpr->Pop(), (*pexpr)[0], pexprNewScalar);
			pxfres->Add(ppexprNew);
		}
	}
}

// EOF
