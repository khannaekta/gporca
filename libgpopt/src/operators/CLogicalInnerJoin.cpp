//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CLogicalInnerJoin.cpp
//
//	@doc:
//		Implementation of inner join operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerJoin.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/operators/CPredicateUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerJoin::CLogicalInnerJoin
//
//	@doc:
//		ctor
//		Note: 04/09/2009 - ; so far inner join doesn't have any specific
//			members, hence, no need for a separate pattern ctor
//
//---------------------------------------------------------------------------
CLogicalInnerJoin::CLogicalInnerJoin
	(
	IMemoryPool *memory_pool
	)
	:
	CLogicalJoin(memory_pool)
{
	GPOS_ASSERT(NULL != memory_pool);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerJoin::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalInnerJoin::Maxcard
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	return CLogical::Maxcard(exprhdl, 2 /*ulScalarIndex*/, MaxcardDef(exprhdl));
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerJoin::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalInnerJoin::PxfsCandidates
	(
	IMemoryPool *memory_pool
	) 
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoin2NLJoin);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoin2HashJoin);
	(void) xform_set->ExchangeSet(CXform::ExfSubqJoin2Apply);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoin2IndexGetApply);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoin2DynamicIndexGetApply);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoin2PartialDynamicIndexGetApply);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoin2BitmapIndexGetApply);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoinWithInnerSelect2IndexGetApply);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoinWithInnerSelect2DynamicIndexGetApply);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoinWithInnerSelect2PartialDynamicIndexGetApply);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoin2DynamicBitmapIndexGetApply);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoinWithInnerSelect2BitmapIndexGetApply);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoinWithInnerSelect2DynamicBitmapIndexGetApply);

	(void) xform_set->ExchangeSet(CXform::ExfJoinCommutativity);
	(void) xform_set->ExchangeSet(CXform::ExfJoinAssociativity);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoinSemiJoinSwap);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoinAntiSemiJoinSwap);
	(void) xform_set->ExchangeSet(CXform::ExfInnerJoinAntiSemiJoinNotInSwap);
	
	return xform_set;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerJoin::FFewerConj
//
//	@doc:
//		Compare two innerJoin group expressions, test whether the first one
//		has less join predicates than the second one. This is used to
//		prioritize innerJoin with less predicates for stats derivation
//
//---------------------------------------------------------------------------
BOOL
CLogicalInnerJoin::FFewerConj
	(
	IMemoryPool *memory_pool,
	CGroupExpression *pgexprFst,
	CGroupExpression *pgexprSnd
	)
{
	if (NULL == pgexprFst || NULL == pgexprSnd)
	{
		return false;
	}

	if (COperator::EopLogicalInnerJoin != pgexprFst->Pop()->Eopid() ||
		COperator::EopLogicalInnerJoin != pgexprSnd->Pop()->Eopid())
	{
		return false;
	}

	// third child must be the group for join conditions
	CGroup *pgroupScalarFst = (*pgexprFst)[2];
	CGroup *pgroupScalarSnd = (*pgexprSnd)[2];
	GPOS_ASSERT(pgroupScalarFst->FScalar());
	GPOS_ASSERT(pgroupScalarSnd->FScalar());

	DrgPexpr *pdrgpexprConjFst = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pgroupScalarFst->PexprScalar());
	DrgPexpr *pdrgpexprConjSnd = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pgroupScalarSnd->PexprScalar());

	ULONG ulConjFst = pdrgpexprConjFst->Size();
	ULONG ulConjSnd = pdrgpexprConjSnd->Size();

	pdrgpexprConjFst->Release();
	pdrgpexprConjSnd->Release();

	return ulConjFst < ulConjSnd;
}

// EOF

