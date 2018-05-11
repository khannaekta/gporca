//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformJoinAssociativity.cpp
//
//	@doc:
//		Implementation of associativity transform for left-deep joins
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformJoinAssociativity.h"

#include "gpopt/operators/ops.h"
#include "gpopt/metadata/CTableDescriptor.h"

using namespace gpopt;
using namespace gpmd;
using namespace gpnaucrates;

#define GPOPT_MAX_JOIN_DEPTH_FOR_ASSOCIATIVITY	20
#define GPOPT_MAX_JOIN_RIGHT_CHILD_DEPTH_FOR_ASSOCIATIVITY 1

//---------------------------------------------------------------------------
//	@function:
//		CXformJoinAssociativity::CXformJoinAssociativity
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformJoinAssociativity::CXformJoinAssociativity
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
					GPOS_NEW(memory_pool) CLogicalInnerJoin(memory_pool),
					GPOS_NEW(memory_pool) CExpression  // left child is a join tree
					 	 (
					 	 memory_pool,
					 	 GPOS_NEW(memory_pool) CLogicalInnerJoin(memory_pool),
					 	 GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // left child
					 	 GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // right child
					 	 GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool)) // predicate
					 	 ),
					GPOS_NEW(memory_pool) CExpression // right child is a pattern leaf
								(
								memory_pool,
								GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)
								),
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool)) // join predicate
					)
		)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformJoinAssociativity::CreatePredicates
//
//	@doc:
//		Extract all conjuncts and divvy them up between upper and lower join
//
//---------------------------------------------------------------------------
void
CXformJoinAssociativity::CreatePredicates
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	DrgPexpr *pdrgpexprLower,
	DrgPexpr *pdrgpexprUpper
	)
	const
{
	GPOS_CHECK_ABORT;

	// bind operators
	CExpression *pexprLeft = (*pexpr)[0];
	CExpression *pexprLeftLeft = (*pexprLeft)[0];
	CExpression *pexprRight = (*pexpr)[1];
	
	DrgPexpr *pdrgpexprJoins = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);

	pexprLeft->AddRef();
	pdrgpexprJoins->Append(pexprLeft);
	
	pexpr->AddRef();
	pdrgpexprJoins->Append(pexpr);	
	
	// columns for new lower join
	CColRefSet *pcrsLower = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrsLower->Union(CDrvdPropRelational::GetRelationalProperties(pexprLeftLeft->PdpDerive())->PcrsOutput());
	pcrsLower->Union(CDrvdPropRelational::GetRelationalProperties(pexprRight->PdpDerive())->PcrsOutput());
	
	// convert current predicates into arrays of conjuncts
	DrgPexpr *pdrgpexprOrig = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	
	for (ULONG ul = 0; ul < 2; ul++)
	{
		DrgPexpr *pdrgpexprPreds = CPredicateUtils::PdrgpexprConjuncts(memory_pool, (*(*pdrgpexprJoins)[ul])[2]);
		ULONG length = pdrgpexprPreds->Size();
		for (ULONG ulConj = 0; ulConj < length; ulConj++)
		{	
			CExpression *pexprConj = (*pdrgpexprPreds)[ulConj];
			pexprConj->AddRef();
			
			pdrgpexprOrig->Append(pexprConj);
		}
		pdrgpexprPreds->Release();
	}

	// divvy up conjuncts for upper and lower join
	ULONG ulConj = pdrgpexprOrig->Size();
	for (ULONG ul = 0; ul < ulConj; ul++)
	{
		CExpression *pexprPred = (*pdrgpexprOrig)[ul];
		CColRefSet *pcrs = CDrvdPropScalar::GetDrvdScalarProps(pexprPred->PdpDerive())->PcrsUsed();
		
		pexprPred->AddRef();
		if (pcrsLower->ContainsAll(pcrs))
		{
			pdrgpexprLower->Append(pexprPred);
		}
		else 
		{
			pdrgpexprUpper->Append(pexprPred);			
		}
	}
	
	// No predicates indicate a cross join. And for that, ORCA expects
	// predicate to be a scalar const "true".
	if (pdrgpexprLower->Size() == 0)
	{
		CExpression *pexprCrossLowerJoinPred = CUtils::PexprScalarConstBool(memory_pool, true, false);
		pdrgpexprLower->Append(pexprCrossLowerJoinPred);
	}
	
	// Same for upper predicates
	if (pdrgpexprUpper->Size() == 0)
	{
		CExpression *pexprCrossUpperJoinPred = CUtils::PexprScalarConstBool(memory_pool, true, false);
		pdrgpexprUpper->Append(pexprCrossUpperJoinPred);
	}

	// clean up
	pcrsLower->Release();
	pdrgpexprOrig->Release();
	
	pdrgpexprJoins->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CXformJoinAssociativity::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformJoinAssociativity::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	if 
		(
		GPOPT_MAX_JOIN_DEPTH_FOR_ASSOCIATIVITY < exprhdl.GetRelationalProperties()->JoinDepth() ||  // disallow xform beyond max join depth
		GPOPT_MAX_JOIN_RIGHT_CHILD_DEPTH_FOR_ASSOCIATIVITY < exprhdl.GetRelationalProperties(1)->JoinDepth()  // disallow xform if input is not a left deep tree
		)
	{
		// restrict associativity to left-deep trees by prohibiting the
		// transformation when right child's join depth is above threshold
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}


//	Associativity Transform: (RS)T ==> (RT)S
//	Example:
//	Input Expression:
//	+--CLogicalInnerJoin
//		 |--CLogicalInnerJoin
//		 |  |--CLogicalGet "t1"
//		 |  |--CLogicalGet "t2"
//		 |  +--CScalarCmp (=)
//		 |     |--CScalarIdent "a" (0) ==> from t1
//		 |     +--CScalarIdent "b" (9) ==> from t2
//		 |--CLogicalGet "t3"
//		 +--CScalarCmp (=)
//				|--CScalarIdent "a" (0) ==> from t1
//				+--CScalarIdent "c" (19) ==> from t3
//
//	Output CExpression:
//	+--CLogicalInnerJoin
//		 |--CLogicalInnerJoin
//		 |  |--CLogicalGet "t1"
//		 |  |--CLogicalGet "t3"
//		 |  +--CScalarCmp (=)
//		 |     |--CScalarIdent "a" (0)  ==> from t1
//		 |     +--CScalarIdent "c" (19) ==> from t3
//		 |--CLogicalGet "t2"
//		 +--CScalarCmp (=)
//				|--CScalarIdent "a" (0) ==> from t1
//				+--CScalarIdent "b" (9) ==> from t2

void
CXformJoinAssociativity::Transform
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
	
	// create new predicates
	DrgPexpr *pdrgpexprLower = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	DrgPexpr *pdrgpexprUpper = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	CreatePredicates(memory_pool, pexpr, pdrgpexprLower, pdrgpexprUpper);
	
	GPOS_ASSERT(pdrgpexprLower->Size() > 0);
	
	//  cross join contains CScalarConst(1) as the join condition.  if the
	//  input expression is as below with cross join at top level between
	//  CLogicalInnerJoin and CLogicalGet "t3"
	//  +--CLogicalInnerJoin
	//     |--CLogicalInnerJoin
	//     |  |--CLogicalGet "t1"
	//     |  |--CLogicalGet "t2"
	//     |  +--CScalarCmp (=)
	//     |     |--CScalarIdent "a" (0)
	//     |     +--CScalarIdent "b" (9)
	//     |--CLogicalGet "t3"
	//     +--CScalarConst (1)
	//  for the above expression (lower) predicate generated for the cross join
	//  between t1 and t3 will be: CScalarConst (1) In *only* such cases, donot
	//  generate such alternative with the lower join as cross join example:
	//  +--CLogicalInnerJoin
	//     |--CLogicalInnerJoin
	//     |  |--CLogicalGet "t1"
	//     |  |--CLogicalGet "t3"
	//     |  +--CScalarConst (1)
	//     |--CLogicalGet "t2"
	//     +--CScalarCmp (=)
	//        |--CScalarIdent "a" (0)
	//        +--CScalarIdent "b" (9)

	// NOTE that we want to be careful to check that input lower join wasn't a
	// cross join to begin with, because we want to build a join in this case even
	// though a new cross join will be created.

	// check if the input lower join expression is a cross join
	BOOL fInputLeftIsCrossJoin = CUtils::FCrossJoin((*pexpr)[0]);

	// check if the output lower join would result in a cross join
	BOOL fOutputLeftIsCrossJoin = (1 == pdrgpexprLower->Size() &&
			CUtils::FScalarConstTrue((*pdrgpexprLower)[0]));

	// build a join only if it does not result in a cross join
	// unless the input itself was a cross join (see earlier comments)
	if (!fOutputLeftIsCrossJoin || fInputLeftIsCrossJoin)
	{
		// bind operators
		CExpression *pexprLeft = (*pexpr)[0];
		CExpression *pexprLeftLeft = (*pexprLeft)[0];
		CExpression *pexprLeftRight = (*pexprLeft)[1];
		CExpression *pexprRight = (*pexpr)[1];

		// add-ref all components for re-use
		pexprLeftLeft->AddRef();
		pexprRight->AddRef();
		pexprLeftRight->AddRef();
		
		// build new joins
		CExpression *pexprBottomJoin = CUtils::PexprLogicalJoin<CLogicalInnerJoin>
										(
										memory_pool,
										pexprLeftLeft,
										pexprRight,
										CPredicateUtils::PexprConjunction(memory_pool, pdrgpexprLower)
										);

		CExpression *pexprResult = CUtils::PexprLogicalJoin<CLogicalInnerJoin>
									(
									memory_pool,
									pexprBottomJoin,
									pexprLeftRight,
									CPredicateUtils::PexprConjunction(memory_pool, pdrgpexprUpper)
									);

		// add alternative to transformation result
		pxfres->Add(pexprResult);
	}
	else
	{
		pdrgpexprLower->Release();
		pdrgpexprUpper->Release();
	}
}


// EOF

