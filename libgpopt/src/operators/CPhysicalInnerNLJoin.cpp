//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CPhysicalInnerNLJoin.cpp
//
//	@doc:
//		Implementation of inner nested-loops join operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CDistributionSpecReplicated.h"
#include "gpopt/base/CDistributionSpecNonSingleton.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CCastUtils.h"

#include "gpopt/base/CUtils.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPredicateUtils.h"

#include "gpopt/operators/CPhysicalInnerNLJoin.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalInnerNLJoin::CPhysicalInnerNLJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalInnerNLJoin::CPhysicalInnerNLJoin
	(
	IMemoryPool *pmp
	)
	:
	CPhysicalNLJoin(pmp)
{
	// Inner NLJ creates two distribution requests for children:
	// (0) Outer child is requested for ANY distribution, and inner child is requested for a Replicated (or a matching) distribution
	// (1) Outer child is requested for Replicated distribution, and inner child is requested for Non-Singleton

	SetDistrRequests(2);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalInnerNLJoin::~CPhysicalInnerNLJoin
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalInnerNLJoin::~CPhysicalInnerNLJoin()
{}



//---------------------------------------------------------------------------
//	@function:
//		CPhysicalInnerNLJoin::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child;
//		this function creates two distribution requests:
//
//		(0) Outer child is requested for ANY distribution, and inner child is
//		  requested for a Replicated (or a matching) distribution,
//		  this request is created by calling CPhysicalJoin::PdsRequired()
//
//		(1) Outer child is requested for Replicated distribution, and inner child
//		  is requested for Non-Singleton (or Singleton if outer delivered Universal distribution)
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalInnerNLJoin::PdsRequired
	(
	IMemoryPool *pmp,
	CExpressionHandle &exprhdl,
	CDistributionSpec *pdsRequired,
	ULONG ulChildIndex,
	DrgPdp *pdrgpdpCtxt,
	ULONG  ulOptReq
	)
	const
{
	GPOS_ASSERT(2 > ulChildIndex);
	GPOS_ASSERT(ulOptReq < UlDistrRequests());

	// if expression has to execute on master then we need a gather
	if (exprhdl.FMasterOnly())
	{
		return PdsEnforceMaster(pmp, exprhdl, pdsRequired, ulChildIndex);
	}

	if (exprhdl.FHasOuterRefs())
	{
		if (CDistributionSpec::EdtSingleton == pdsRequired->Edt() ||
			CDistributionSpec::EdtReplicated == pdsRequired->Edt())
		{
			return PdsPassThru(pmp, exprhdl, pdsRequired, ulChildIndex);
		}
		return GPOS_NEW(pmp) CDistributionSpecReplicated();
	}

	if (GPOS_FTRACE(EopttraceDisableReplicateInnerNLJOuterChild) || 0 == ulOptReq)
	{
		if (1 == ulChildIndex)
		{
			// compute a matching distribution based on derived distribution of outer child
			CDistributionSpec *pdsOuter = CDrvdPropPlan::Pdpplan((*pdrgpdpCtxt)[0])->Pds();
			if(CDistributionSpec::EdtHashed == pdsOuter->Edt())
			{
				// require inner child to have matching hashed distribution
				CExpression *pexprScPredicate = exprhdl.PexprScalarChild(2);
				DrgPexpr *pdrgpexpr = CPredicateUtils::PdrgpexprConjuncts(pmp, pexprScPredicate);

				DrgPexpr *pdrgpexprMatching = GPOS_NEW(pmp) DrgPexpr(pmp);
				CDistributionSpecHashed *pdshashed = CDistributionSpecHashed::PdsConvert(pdsOuter);
				DrgPexpr *pdrgpexprHashed = pdshashed->Pdrgpexpr();
				const ULONG ulSize = pdrgpexprHashed->UlLength();

				BOOL fSuccess = true;
				for (ULONG ul = 0; fSuccess && ul < ulSize; ul++)
				{
					CExpression *pexpr = (*pdrgpexprHashed)[ul];
					// get matching expression from predicate for the corresponding outer child
					// to create CDistributionSpecHashed for inner child
					CExpression *pexprMatching = PexprMatchEqualitySide(pexpr, pdrgpexpr);
					fSuccess = (NULL != pexprMatching);
					if (fSuccess)
					{
						pexprMatching->AddRef();
						pdrgpexprMatching->Append(pexprMatching);
					}
				}
				pdrgpexpr->Release();

				if (fSuccess)
				{
					GPOS_ASSERT(pdrgpexprMatching->UlLength() == pdrgpexprHashed->UlLength());

					// create a matching hashed distribution request
					BOOL fNullsColocated = pdshashed->FNullsColocated();
					CDistributionSpecHashed *pdshashedEquiv = GPOS_NEW(pmp) CDistributionSpecHashed(pdrgpexprMatching, fNullsColocated);

					return pdshashedEquiv;
				}
				pdrgpexprMatching->Release();
			}
		}
		return CPhysicalJoin::PdsRequired(pmp, exprhdl, pdsRequired, ulChildIndex, pdrgpdpCtxt, ulOptReq);
	}
	GPOS_ASSERT(1 == ulOptReq);

	if (0 == ulChildIndex)
	{
		return GPOS_NEW(pmp) CDistributionSpecReplicated();
	}

	// compute a matching distribution based on derived distribution of outer child
	CDistributionSpec *pdsOuter = CDrvdPropPlan::Pdpplan((*pdrgpdpCtxt)[0])->Pds();
	if (CDistributionSpec::EdtUniversal == pdsOuter->Edt())
	{
		// first child is universal, request second child to execute on the master to avoid duplicates
		return GPOS_NEW(pmp) CDistributionSpecSingleton(CDistributionSpecSingleton::EstMaster);
	}

	return GPOS_NEW(pmp) CDistributionSpecNonSingleton();
}

// search the given array of predicates for an equality predicate
// that has one side equal to the given expression,
// if found, return the other side of equality, otherwise return NULL
CExpression *
CPhysicalInnerNLJoin::PexprMatchEqualitySide
(
	CExpression *pexprToMatch,
	DrgPexpr *pdrgpexpr // array of predicates to inspect
)
{
	GPOS_ASSERT(NULL != pexprToMatch);
	GPOS_ASSERT(NULL != pdrgpexpr);

	CExpression *pexprMatching = NULL;
	const ULONG ulSize = pdrgpexpr->UlLength();
	for (ULONG ul = 0; ul < ulSize; ul++)
	{
		CExpression *pexprPred = (*pdrgpexpr)[ul];
		if (!CPredicateUtils::FEquality(pexprPred))
		{
			continue;
		}

		// extract equality sides
		CExpression *pexprPredOuter = (*pexprPred)[0];
		CExpression *pexprPredInner = (*pexprPred)[1];

		IMDId *pmdidTypeOuter = CScalar::PopConvert(pexprPredOuter->Pop())->PmdidType();
		IMDId *pmdidTypeInner = CScalar::PopConvert(pexprPredInner->Pop())->PmdidType();
		if (!pmdidTypeOuter->FEquals(pmdidTypeInner))
		{
			// only consider equality of identical types
			continue;
		}

		pexprToMatch = CCastUtils::PexprWithoutBinaryCoercibleCasts(pexprToMatch);
		if (CUtils::FEqual(CCastUtils::PexprWithoutBinaryCoercibleCasts(pexprPredOuter), pexprToMatch))
		{
			pexprMatching = pexprPredInner;
			break;
		}

		if (CUtils::FEqual(CCastUtils::PexprWithoutBinaryCoercibleCasts(pexprPredInner), pexprToMatch))
		{
			pexprMatching = pexprPredOuter;
			break;
		}
	}

	return pexprMatching;
}

// EOF

