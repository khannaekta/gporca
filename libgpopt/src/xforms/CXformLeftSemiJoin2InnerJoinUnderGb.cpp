//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CXformLeftSemiJoin2InnerJoinUnderGb.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"

#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CColRefTable.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformLeftSemiJoin2InnerJoinUnderGb.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CXformLeftSemiJoin2InnerJoinUnderGb::CXformLeftSemiJoin2InnerJoinUnderGb
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CXformLeftSemiJoin2InnerJoinUnderGb::CXformLeftSemiJoin2InnerJoinUnderGb
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
					GPOS_NEW(memory_pool) CLogicalLeftSemiJoin(memory_pool),
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // left child
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // right child
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool))  // predicate
					)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformLeftSemiJoin2InnerJoinUnderGb::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle;
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformLeftSemiJoin2InnerJoinUnderGb::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	CColRefSet *pcrsInnerOutput = exprhdl.GetRelationalProperties(1)->PcrsOutput();
	CExpression *pexprScalar = exprhdl.PexprScalarChild(2);
	CAutoMemoryPool amp;
	if (exprhdl.HasOuterRefs() ||
		NULL == exprhdl.GetRelationalProperties(0)->Pkc() ||
		exprhdl.GetDrvdScalarProps(2)->FHasSubquery() ||
		CPredicateUtils::FSimpleEqualityUsingCols(amp.Pmp(), pexprScalar, pcrsInnerOutput))
	{
		return ExfpNone;
	}

	return ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLeftSemiJoin2InnerJoinUnderGb::Transform
//
//	@doc:
//		actual transformation
//
//---------------------------------------------------------------------------
void
CXformLeftSemiJoin2InnerJoinUnderGb::Transform
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

	ColRefArray *pdrgpcrKeys = NULL;
	ColRefArray *pdrgpcrGrouping = CUtils::PdrgpcrGroupingKey(memory_pool, pexprOuter, &pdrgpcrKeys);
	GPOS_ASSERT(NULL != pdrgpcrKeys);

	CExpression *pexprInnerJoin =
		CUtils::PexprLogicalJoin<CLogicalInnerJoin>(memory_pool, pexprOuter, pexprInner, pexprScalar);

	CExpression *pexprGb =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CLogicalGbAggDeduplicate(memory_pool, pdrgpcrGrouping, COperator::EgbaggtypeGlobal  /*egbaggtype*/, pdrgpcrKeys),
			pexprInnerJoin,
			GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool))
			);

	pxfres->Add(pexprGb);
}

// EOF
