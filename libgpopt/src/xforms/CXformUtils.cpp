//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2012 EMC Corp.
//
//	@filename:
//		CXformUtils.cpp
//
//	@doc:
//		Implementation of xform utilities
//---------------------------------------------------------------------------


#include "gpos/base.h"
#include "gpos/error/CMessage.h"
#include "gpos/error/CMessageRepository.h"
#include "gpos/memory/CAutoMemoryPool.h"

#include "naucrates/base/CDatumInt8GPDB.h"
#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/CMDTriggerGPDB.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/md/IMDTypeInt8.h"
#include "naucrates/md/IMDTypeOid.h"
#include "naucrates/md/IMDTrigger.h"
#include "naucrates/md/IMDCheckConstraint.h"

#include "gpopt/base/CConstraintConjunction.h"
#include "gpopt/base/CConstraintNegation.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/search/CGroupExpression.h"
#include "gpopt/search/CGroupProxy.h"
#include "gpopt/xforms/CXformExploration.h"
#include "gpopt/xforms/CDecorrelator.h"
#include "gpopt/xforms/CXformUtils.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/exception.h"
#include "gpopt/engine/CHint.h"


using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::ExfpLogicalJoin2PhysicalJoin
//
//	@doc:
//		Check the applicability of logical join to physical join xform
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformUtils::ExfpLogicalJoin2PhysicalJoin
	(
	CExpressionHandle &exprhdl
	)
{
	// if scalar predicate has a subquery, we must have an
	// equivalent logical Apply expression created during exploration;
	// no need for generating a physical join
	if (exprhdl.GetDrvdScalarProps(2)->FHasSubquery())
	{
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::ExfpSemiJoin2CrossProduct
//
//	@doc:
//		Check the applicability of semi join to cross product xform
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformUtils::ExfpSemiJoin2CrossProduct
	(
	CExpressionHandle &exprhdl
	)
{
#ifdef GPOS_DEBUG
	COperator::EOperatorId op_id =  exprhdl.Pop()->Eopid();
#endif // GPOS_DEBUG
	GPOS_ASSERT(COperator::EopLogicalLeftSemiJoin == op_id ||
			COperator::EopLogicalLeftAntiSemiJoin == op_id ||
			COperator::EopLogicalLeftAntiSemiJoinNotIn == op_id);

	CColRefSet *pcrsUsed = exprhdl.GetDrvdScalarProps(2 /*child_index*/)->PcrsUsed();
	CColRefSet *pcrsOuterOutput = exprhdl.GetRelationalProperties(0 /*child_index*/)->PcrsOutput();
	if (0 == pcrsUsed->Size() || !pcrsOuterOutput->ContainsAll(pcrsUsed))
	{
		// xform is inapplicable of join predicate uses columns from join's inner child
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::ExfpExpandJoinOrder
//
//	@doc:
//		Check the applicability of N-ary join expansion
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformUtils::ExfpExpandJoinOrder
	(
	CExpressionHandle &exprhdl
	)
{
	if (exprhdl.GetDrvdScalarProps(exprhdl.Arity() - 1)->FHasSubquery() || exprhdl.HasOuterRefs())
	{
		// subqueries must be unnested before applying xform
		return CXform::ExfpNone;
	}

#ifdef GPOS_DEBUG
	CAutoMemoryPool amp;
	GPOS_ASSERT(!FJoinPredOnSingleChild(amp.Pmp(), exprhdl) &&
			"join predicates are not pushed down");
#endif // GPOS_DEBUG

	if (NULL != exprhdl.Pgexpr())
	{
		// if handle is attached to a group expression, transformation is applied
		// to the Memo and we need to check if stats are derivable on child groups
		CGroup *pgroup = exprhdl.Pgexpr()->Pgroup();
		CAutoMemoryPool amp;
		IMemoryPool *memory_pool = amp.Pmp();
		if (!pgroup->FStatsDerivable(memory_pool))
		{
			// stats must be derivable before applying xforms
			return CXform::ExfpNone;
		}

		const ULONG arity = exprhdl.Arity();
		for (ULONG ul = 0; ul < arity; ul++)
		{
			CGroup *pgroupChild = (*exprhdl.Pgexpr())[ul];
			if (!pgroupChild->FScalar() && !pgroupChild->FStatsDerivable(memory_pool))
			{
				// stats must be derivable on every child
				return CXform::ExfpNone;
			}
		}
	}

	return CXform::ExfpHigh;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::FInlinableCTE
//
//	@doc:
//		Check whether a CTE should be inlined
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FInlinableCTE
	(
	ULONG ulCTEId
	)
{
	CCTEInfo *pcteinfo = COptCtxt::PoctxtFromTLS()->Pcteinfo();
	CExpression *pexprProducer = pcteinfo->PexprCTEProducer(ulCTEId);
	GPOS_ASSERT(NULL != pexprProducer);
	CFunctionProp *pfp = CDrvdPropRelational::GetRelationalProperties(pexprProducer->PdpDerive())->Pfp();

	CPartInfo *ppartinfoCTEProducer = CDrvdPropRelational::GetRelationalProperties(pexprProducer->PdpDerive())->Ppartinfo();
	GPOS_ASSERT(NULL != ppartinfoCTEProducer);
	
	return IMDFunction::EfsVolatile > pfp->Efs() && 
			!pfp->FMasterOnly() &&
			(0 == ppartinfoCTEProducer->UlConsumers() || 1 == pcteinfo->UlConsumers(ulCTEId));
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PcrsFKey
//
//	@doc:
//		Helper to construct a foreign key by collecting columns that appear
//		in equality predicates with primary key columns;
//		return NULL if no foreign key could be constructed
//
//---------------------------------------------------------------------------
CColRefSet *
CXformUtils::PcrsFKey
	(
	IMemoryPool *memory_pool,
	DrgPexpr *pdrgpexpr, // array of scalar conjuncts
	CColRefSet *prcsOutput, // output columns of outer expression
	CColRefSet *pcrsKey // a primary key of a inner expression
	)
{
	GPOS_ASSERT(NULL != pdrgpexpr);
	GPOS_ASSERT(NULL != pcrsKey);
	GPOS_ASSERT(NULL != prcsOutput);

	 // collected columns that are part of primary key and used in equality predicates
	CColRefSet *pcrsKeyParts = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	// FK columns
	CColRefSet *pcrsFKey = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	const ULONG ulConjuncts = pdrgpexpr->Size();
	for (ULONG ul = 0; ul < ulConjuncts; ul++)
	{
		CExpression *pexprConjunct = (*pdrgpexpr)[ul];
		if (!CPredicateUtils::FPlainEquality(pexprConjunct))
		{
			continue;
		}

		CColRef *pcrFst = const_cast<CColRef*> (CScalarIdent::PopConvert((*pexprConjunct)[0]->Pop())->Pcr());
		CColRef *pcrSnd = const_cast<CColRef*> (CScalarIdent::PopConvert((*pexprConjunct)[1]->Pop())->Pcr());
		if (pcrsKey->FMember(pcrFst) && prcsOutput->FMember(pcrSnd))
		{
			pcrsKeyParts->Include(pcrFst);
			pcrsFKey->Include(pcrSnd);
		}
		else if (pcrsKey->FMember(pcrSnd) && prcsOutput->FMember(pcrFst))
		{
			pcrsFKey->Include(pcrFst);
			pcrsKeyParts->Include(pcrSnd);
		}
	}

	// check if collected key parts constitute a primary key
	if (!pcrsKeyParts->Equals(pcrsKey))
	{
		// did not succeeded in building foreign key
		pcrsFKey->Release();
		pcrsFKey = NULL;
	}
	pcrsKeyParts->Release();

	return pcrsFKey;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PcrsFKey
//
//	@doc:
//		Return a foreign key pointing from outer expression to inner
//		expression;
//		return NULL if no foreign key could be extracted
//
//---------------------------------------------------------------------------
CColRefSet *
CXformUtils::PcrsFKey
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter,
	CExpression *pexprInner,
	CExpression *pexprScalar
	)
{
	// get inner expression key
	CKeyCollection *pkc =  CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->Pkc();
	if (NULL == pkc)
	{
		// inner expression has no key
		return NULL;
	}
	// get outer expression output columns
	CColRefSet *prcsOutput = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();

	DrgPexpr *pdrgpexpr = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pexprScalar);
	CColRefSet *pcrsFKey = NULL;

	const ULONG ulKeys = pkc->Keys();
	for (ULONG ulKey = 0; ulKey < ulKeys; ulKey++)
	{
		DrgPcr *pdrgpcrKey = pkc->PdrgpcrKey(memory_pool, ulKey);

		CColRefSet *pcrsKey = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
		pcrsKey->Include(pdrgpcrKey);
		pdrgpcrKey->Release();

		// attempt to construct a foreign key based on current primary key
		pcrsFKey = PcrsFKey(memory_pool, pdrgpexpr, prcsOutput, pcrsKey);
		pcrsKey->Release();

		if (NULL != pcrsFKey)
		{
			// succeeded in finding FK
			break;
		}
	}

	pdrgpexpr->Release();

	return pcrsFKey;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprRedundantSelectForDynamicIndex
//
//	@doc:
// 		Add a redundant SELECT node on top of Dynamic (Bitmap) IndexGet to
//		to be able to use index predicate in partition elimination
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprRedundantSelectForDynamicIndex
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr // input expression is a dynamic (bitmap) IndexGet with an optional Select on top
	)
{
	GPOS_ASSERT(NULL != pexpr);

	COperator::EOperatorId op_id = pexpr->Pop()->Eopid();
	GPOS_ASSERT(COperator::EopLogicalDynamicIndexGet == op_id ||
			COperator::EopLogicalDynamicBitmapTableGet == op_id ||
			COperator::EopLogicalSelect == op_id);

	CExpression *pexprRedundantScalar = NULL;
	if (COperator::EopLogicalDynamicIndexGet == op_id || COperator::EopLogicalDynamicBitmapTableGet == op_id)
	{
		// no residual predicate, use index lookup predicate only
		pexpr->AddRef();
		// reuse index lookup predicate
		(*pexpr)[0]->AddRef();
		pexprRedundantScalar = (*pexpr)[0];

		return GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CLogicalSelect(memory_pool), pexpr, pexprRedundantScalar);
	}

	// there is a residual predicate in a SELECT node on top of DynamicIndexGet,
	// we create a conjunction of both residual predicate and index lookup predicate
	CExpression *pexprChild = (*pexpr)[0];
#ifdef GPOS_DEBUG
	COperator::EOperatorId eopidChild = pexprChild->Pop()->Eopid();
	GPOS_ASSERT(COperator::EopLogicalDynamicIndexGet == eopidChild ||
				COperator::EopLogicalDynamicBitmapTableGet == eopidChild);
#endif // GPOS_DEBUG

	CExpression *pexprIndexLookupPred = (*pexprChild)[0];
	CExpression *pexprResidualPred = (*pexpr)[1];
	pexprRedundantScalar = CPredicateUtils::PexprConjunction(memory_pool, pexprIndexLookupPred, pexprResidualPred);

	pexprChild->AddRef();

	return GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalSelect(memory_pool),
				pexprChild,
				pexprRedundantScalar
				);
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::FSwapableJoinType
//
//	@doc:
//		Check whether the given join type is swapable
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FSwapableJoinType
	(
	COperator::EOperatorId op_id
	)
{
	return (COperator::EopLogicalLeftSemiJoin == op_id ||
			COperator::EopLogicalLeftAntiSemiJoin == op_id ||
			COperator::EopLogicalLeftAntiSemiJoinNotIn == op_id ||
			COperator::EopLogicalInnerJoin == op_id);
}
#endif // GPOS_DEBUG

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprSwapJoins
//
//	@doc:
//		Compute a swap of the two given joins;
//		the function returns null if swapping cannot be done
//
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprSwapJoins
	(
	IMemoryPool *memory_pool,
	CExpression *pexprTopJoin,
	CExpression *pexprBottomJoin
	)
{
#ifdef GPOS_DEBUG
	COperator::EOperatorId eopidTop = pexprTopJoin->Pop()->Eopid();
	COperator::EOperatorId eopidBottom = pexprBottomJoin->Pop()->Eopid();
#endif // GPOS_DEBUG

	GPOS_ASSERT(FSwapableJoinType(eopidTop) && FSwapableJoinType(eopidBottom));
	GPOS_ASSERT_IMP(COperator::EopLogicalInnerJoin == eopidTop,
			COperator::EopLogicalLeftSemiJoin == eopidBottom ||
			COperator::EopLogicalLeftAntiSemiJoin == eopidBottom ||
			COperator::EopLogicalLeftAntiSemiJoinNotIn == eopidBottom);

	// get used columns by the join predicate of top join
	CColRefSet *pcrsUsed = CDrvdPropScalar::GetDrvdScalarProps((*pexprTopJoin)[2]->PdpDerive())->PcrsUsed();

	// get output columns of bottom join's children
	const CColRefSet *pcrsBottomOuter = CDrvdPropRelational::GetRelationalProperties((*pexprBottomJoin)[0]->PdpDerive())->PcrsOutput();
	const CColRefSet *pcrsBottomInner = CDrvdPropRelational::GetRelationalProperties((*pexprBottomJoin)[1]->PdpDerive())->PcrsOutput();

	BOOL fDisjointWithBottomOuter = pcrsUsed->IsDisjoint(pcrsBottomOuter);
	BOOL fDisjointWithBottomInner = pcrsUsed->IsDisjoint(pcrsBottomInner);
	if (!fDisjointWithBottomOuter && !fDisjointWithBottomInner)
	{
		// top join uses columns from both children of bottom join;
		// join swap is not possible
		return NULL;
	}

	CExpression *pexprChild = (*pexprBottomJoin)[0];
	CExpression *pexprChildOther = (*pexprBottomJoin)[1];
	if (fDisjointWithBottomOuter && !fDisjointWithBottomInner)
	{
		pexprChild = (*pexprBottomJoin)[1];
		pexprChildOther = (*pexprBottomJoin)[0];
	}

	CExpression *pexprRight = (*pexprTopJoin)[1];
	CExpression *pexprScalar = (*pexprTopJoin)[2];
	COperator *pop = pexprTopJoin->Pop();
	pop->AddRef();
	pexprChild->AddRef();
	pexprRight->AddRef();
	pexprScalar->AddRef();
	CExpression *pexprNewBottomJoin = GPOS_NEW(memory_pool) CExpression
								(
								memory_pool,
								pop,
								pexprChild,
								pexprRight,
								pexprScalar
								);

	pexprScalar = (*pexprBottomJoin)[2];
	pop = pexprBottomJoin->Pop();
	pop->AddRef();
	pexprChildOther->AddRef();
	pexprScalar->AddRef();

	// return a new expression with the two joins swapped
	return  GPOS_NEW(memory_pool) CExpression
						(
						memory_pool,
						pop,
						pexprNewBottomJoin,
						pexprChildOther,
						pexprScalar
						);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprPushGbBelowJoin
//
//	@doc:
//		Push a Gb, optionally with a having clause below the child join;
//		if push down fails, the function returns NULL expression
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprPushGbBelowJoin
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr
	)
{
	COperator::EOperatorId op_id = pexpr->Pop()->Eopid();

	GPOS_ASSERT(COperator::EopLogicalGbAgg == op_id ||
				COperator::EopLogicalGbAggDeduplicate == op_id ||
				COperator::EopLogicalSelect == op_id);

	CExpression *pexprSelect = NULL;
	CExpression *pexprGb = pexpr;
	if (COperator::EopLogicalSelect == op_id)
	{
		pexprSelect = pexpr;
		pexprGb = (*pexpr)[0];
	}

	CExpression *pexprJoin = (*pexprGb)[0];
	CExpression *pexprPrjList = (*pexprGb)[1];
	CExpression *pexprOuter = (*pexprJoin)[0];
	CExpression *pexprInner = (*pexprJoin)[1];
	CExpression *pexprScalar = (*pexprJoin)[2];
	CLogicalGbAgg *popGbAgg = CLogicalGbAgg::PopConvert(pexprGb->Pop());

	CColRefSet *pcrsOuterOutput = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();
	CColRefSet *pcrsAggOutput = CDrvdPropRelational::GetRelationalProperties(pexprGb->PdpDerive())->PcrsOutput();
	CColRefSet *pcrsUsed = CDrvdPropScalar::GetDrvdScalarProps(pexprPrjList->PdpDerive())->PcrsUsed();
	CColRefSet *pcrsFKey = PcrsFKey(memory_pool, pexprOuter, pexprInner, pexprScalar);
	
	CColRefSet *pcrsScalarFromOuter = GPOS_NEW(memory_pool) CColRefSet(memory_pool, *(CDrvdPropScalar::GetDrvdScalarProps(pexprScalar->PdpDerive())->PcrsUsed()));
	pcrsScalarFromOuter->Intersection(pcrsOuterOutput);
	
	// use minimal grouping columns if they exist, otherwise use all grouping columns
	CColRefSet *pcrsGrpCols = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	DrgPcr *colref_array = popGbAgg->PdrgpcrMinimal();
	if (NULL == colref_array)
	{
		colref_array = popGbAgg->Pdrgpcr();
	}
	pcrsGrpCols->Include(colref_array);

	BOOL fCanPush = FCanPushGbAggBelowJoin(pcrsGrpCols, pcrsOuterOutput, pcrsScalarFromOuter, pcrsAggOutput, pcrsUsed, pcrsFKey);
	
	// cleanup
	CRefCount::SafeRelease(pcrsFKey);
	pcrsScalarFromOuter->Release();

	if (!fCanPush)
	{
		pcrsGrpCols->Release();

		return NULL;
	}

	// here, we know that grouping columns include FK and all used columns by Gb
	// come only from the outer child of the join;
	// we can safely push Gb to be on top of join's outer child

	popGbAgg->AddRef();
	CLogicalGbAgg *popGbAggNew = PopGbAggPushableBelowJoin(memory_pool, popGbAgg, pcrsOuterOutput, pcrsGrpCols);
	pcrsGrpCols->Release();

	pexprOuter->AddRef();
	pexprPrjList->AddRef();
	CExpression *pexprNewGb = GPOS_NEW(memory_pool) CExpression(memory_pool, popGbAggNew, pexprOuter, pexprPrjList);

	CExpression *pexprNewOuter = pexprNewGb;
	if (NULL != pexprSelect)
	{
		// add Select node on top of Gb
		(*pexprSelect)[1]->AddRef();
		pexprNewOuter = CUtils::PexprLogicalSelect(memory_pool, pexprNewGb, (*pexprSelect)[1]);
	}

	COperator *popJoin = pexprJoin->Pop();
	popJoin->AddRef();
	pexprInner->AddRef();
	pexprScalar->AddRef();

	return GPOS_NEW(memory_pool) CExpression(memory_pool, popJoin, pexprNewOuter, pexprInner, pexprScalar);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PopGbAggPushableBelowJoin
//
//	@doc:
//		Create the Gb operator to be pushed below a join given the original Gb
//		operator, output columns of the join's outer child and grouping cols
//
//---------------------------------------------------------------------------
CLogicalGbAgg *
CXformUtils::PopGbAggPushableBelowJoin
	(
	IMemoryPool *memory_pool,
	CLogicalGbAgg *popGbAggOld,
	CColRefSet *pcrsOutputOuter,
	CColRefSet *pcrsGrpCols
	)
{
	GPOS_ASSERT(NULL != popGbAggOld);
	GPOS_ASSERT(NULL != pcrsOutputOuter);
	GPOS_ASSERT(NULL != pcrsGrpCols);

	CLogicalGbAgg *popGbAggNew = popGbAggOld;
	if (!pcrsOutputOuter->ContainsAll(pcrsGrpCols))
	{
		// we have grouping columns from both join children;
		// we can drop grouping columns from the inner join child since
		// we already have a FK in grouping columns

		popGbAggNew->Release();
		CColRefSet *pcrsGrpColsNew = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
		pcrsGrpColsNew->Include(pcrsGrpCols);
		pcrsGrpColsNew->Intersection(pcrsOutputOuter);
		if (COperator::EopLogicalGbAggDeduplicate == popGbAggOld->Eopid())
		{
			DrgPcr *pdrgpcrKeys = CLogicalGbAggDeduplicate::PopConvert(popGbAggOld)->PdrgpcrKeys();
			pdrgpcrKeys->AddRef();
			popGbAggNew = GPOS_NEW(memory_pool) CLogicalGbAggDeduplicate(memory_pool, pcrsGrpColsNew->Pdrgpcr(memory_pool), popGbAggOld->Egbaggtype(), pdrgpcrKeys);
		}
		else
		{
			popGbAggNew = GPOS_NEW(memory_pool) CLogicalGbAgg(memory_pool, pcrsGrpColsNew->Pdrgpcr(memory_pool), popGbAggOld->Egbaggtype());
		}
		pcrsGrpColsNew->Release();
	}

	return popGbAggNew;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::FCanPushGbAggBelowJoin
//
//	@doc:
//		Check if the preconditions for pushing down Group by through join are 
//		satisfied
//---------------------------------------------------------------------------
BOOL
CXformUtils::FCanPushGbAggBelowJoin
	(
	CColRefSet *pcrsGrpCols,
	CColRefSet *pcrsJoinOuterChildOutput,
	CColRefSet *pcrsJoinScalarUsedFromOuter,
	CColRefSet *pcrsGrpByOutput,
	CColRefSet *pcrsGrpByUsed,
	CColRefSet *pcrsFKey
	)
{
	BOOL fGrpByProvidesUsedColumns = pcrsGrpByOutput->ContainsAll(pcrsJoinScalarUsedFromOuter);

	BOOL fHasFK = (NULL != pcrsFKey);
	BOOL fGrpColsContainFK = (fHasFK && pcrsGrpCols->ContainsAll(pcrsFKey));
	BOOL fOutputColsContainUsedCols = pcrsJoinOuterChildOutput->ContainsAll(pcrsGrpByUsed);

	if (!fHasFK || !fGrpColsContainFK || !fOutputColsContainUsedCols || !fGrpByProvidesUsedColumns)
	{
		// GrpBy cannot be pushed through join because
		// (1) no FK exists, or
		// (2) FK exists but grouping columns do not include it, or
		// (3) Gb uses columns from both join children, or
		// (4) Gb hides columns required for the join scalar child

		return false;
	}
	
	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::FSameDatatype
//
//	@doc:
//		Check if the input columns of the outer child are the same as the
//		aligned input columns of the each of the inner children.
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FSameDatatype
	(
	DrgDrgPcr *pdrgpdrgpcrInput
	)
{
	GPOS_ASSERT(1 < pdrgpdrgpcrInput->Size());

	DrgPcr *pdrgpcrOuter = (*pdrgpdrgpcrInput)[0];

	ULONG ulColIndex = pdrgpcrOuter->Size();
	ULONG child_index = pdrgpdrgpcrInput->Size();
	for (ULONG ulColCounter = 0; ulColCounter < ulColIndex; ulColCounter++)
	{
		CColRef *pcrOuter = (*pdrgpcrOuter)[ulColCounter];

		for (ULONG ulChildCounter = 1; ulChildCounter < child_index; ulChildCounter++)
		{
			DrgPcr *pdrgpcrInnner = (*pdrgpdrgpcrInput)[ulChildCounter];
			CColRef *pcrInner = (*pdrgpcrInnner)[ulColCounter];

			GPOS_ASSERT(pcrOuter != pcrInner);

			if (pcrInner->RetrieveType() != pcrOuter->RetrieveType())
			{
				return false;
			}
		}
	}

	return true;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::ExistentialToAgg
//
//	@doc:
//		Helper for creating Agg expression equivalent to an existential subquery
//
//		Example:
//			For 'exists(select * from r where a = 10)', we produce the following:
//			New Subquery: (select count(*) as cc from r where a = 10)
//			New Scalar: cc > 0
//
//---------------------------------------------------------------------------
void
CXformUtils::ExistentialToAgg
	(
	IMemoryPool *memory_pool,
	CExpression *pexprSubquery,
	CExpression **ppexprNewSubquery, // output argument for new scalar subquery
	CExpression **ppexprNewScalar   // output argument for new scalar expression
	)
{
	GPOS_ASSERT(CUtils::FExistentialSubquery(pexprSubquery->Pop()));
	GPOS_ASSERT(NULL != ppexprNewSubquery);
	GPOS_ASSERT(NULL != ppexprNewScalar);

	COperator::EOperatorId op_id = pexprSubquery->Pop()->Eopid();
	CExpression *pexprInner = (*pexprSubquery)[0];
	IMDType::ECmpType ecmptype = IMDType::EcmptG;
	if (COperator::EopScalarSubqueryNotExists == op_id)
	{
		ecmptype = IMDType::EcmptEq;
	}

	pexprInner->AddRef();
	CExpression *pexprInnerNew = CUtils::PexprCountStar(memory_pool, pexprInner);
	const CColRef *pcrCount = CScalarProjectElement::PopConvert((*(*pexprInnerNew)[1])[0]->Pop())->Pcr();

	*ppexprNewSubquery = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarSubquery(memory_pool, pcrCount, true /*fGeneratedByExist*/, false /*fGeneratedByQuantified*/), pexprInnerNew);
	*ppexprNewScalar =
			CUtils::PexprCmpWithZero
			(
			memory_pool,
			CUtils::PexprScalarIdent(memory_pool, pcrCount),
			pcrCount->RetrieveType()->MDId(),
			ecmptype
			);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::QuantifiedToAgg
//
//	@doc:
//		Helper for creating Agg expression equivalent to a quantified subquery,
//
//
//---------------------------------------------------------------------------
void
CXformUtils::QuantifiedToAgg
	(
	IMemoryPool *memory_pool,
	CExpression *pexprSubquery,
	CExpression **ppexprNewSubquery, // output argument for new scalar subquery
	CExpression **ppexprNewScalar   // output argument for new scalar expression
	)
{
	GPOS_ASSERT(CUtils::FQuantifiedSubquery(pexprSubquery->Pop()));
	GPOS_ASSERT(NULL != ppexprNewSubquery);
	GPOS_ASSERT(NULL != ppexprNewScalar);

	if (COperator::EopScalarSubqueryAll == pexprSubquery->Pop()->Eopid())
	{
		return SubqueryAllToAgg(memory_pool, pexprSubquery, ppexprNewSubquery, ppexprNewScalar);
	}

	return SubqueryAnyToAgg(memory_pool, pexprSubquery, ppexprNewSubquery, ppexprNewScalar);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::SubqueryAnyToAgg
//
//	@doc:
//		Helper for transforming SubqueryAll into aggregate subquery,
//		we need to differentiate between two cases:
//		(1) if subquery predicate uses nullable columns, we may produce null values,
//			this is handeled by adding a null indicator to subquery column, and
//			counting the number of subquery results with null m_bytearray_value in subquery column,
//		(2) if subquery predicate does not use nullable columns, we can only produce
//			boolean values,
//
//		Examples:
//
//			- For 'b1 in (select b2 from R)', with b2 not-nullable, we produce the following:
//			* New Subquery: (select count(*) as cc from R where b1=b2))
//			* New Scalar: cc > 0
//
//			- For 'b1 in (select b2 from R)', with b2 nullable, we produce the following:
//			* New Subquery: (select Prj_cc from (select count(*), sum(null_indic) from R where b1=b2))
//			where Prj_cc is a project for column cc, defined as follows:
//				if (count(*) == 0), then cc = 0,
//				else if (count(*) == sum(null_indic)), then cc = -1,
//				else cc = count(*)
//			where (-1) indicates that subquery produced a null (this is replaced by NULL in
//			SubqueryHandler when unnesting to LeftApply)
//			* New Scalar: cc > 0
//
//
//---------------------------------------------------------------------------
void
CXformUtils::SubqueryAnyToAgg
	(
	IMemoryPool *memory_pool,
	CExpression *pexprSubquery,
	CExpression **ppexprNewSubquery, // output argument for new scalar subquery
	CExpression **ppexprNewScalar   // output argument for new scalar expression
	)
{
	GPOS_ASSERT(CUtils::FQuantifiedSubquery(pexprSubquery->Pop()));
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == pexprSubquery->Pop()->Eopid());
	GPOS_ASSERT(NULL != ppexprNewSubquery);
	GPOS_ASSERT(NULL != ppexprNewScalar);

	CExpression *pexprInner = (*pexprSubquery)[0];

	// build subquery quantified comparison
	CExpression *pexprResult = NULL;
	CSubqueryHandler sh(memory_pool, false /* fEnforceCorrelatedApply */);
	CExpression *pexprSubqPred = sh.PexprSubqueryPred(pexprInner, pexprSubquery, &pexprResult);

	const CColRef *pcrSubq = CScalarSubqueryQuantified::PopConvert(pexprSubquery->Pop())->Pcr();
	BOOL fUsesNullableCol = CUtils::FUsesNullableCol(memory_pool, pexprSubqPred, pexprResult);

	CExpression *pexprInnerNew = NULL;
	pexprInner->AddRef();
	if (fUsesNullableCol)
	{
		// add a null indicator
		CExpression *pexprNullIndicator = PexprNullIndicator(memory_pool, CUtils::PexprScalarIdent(memory_pool, pcrSubq));
		CExpression *pexprPrj = CUtils::PexprAddProjection(memory_pool, pexprResult, pexprNullIndicator);
		pexprResult = pexprPrj;

		// add disjunction with is not null check
		DrgPexpr *pdrgpexpr = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
		pdrgpexpr->Append(pexprSubqPred);
		pdrgpexpr->Append(CUtils::PexprIsNull(memory_pool, CUtils::PexprScalarIdent(memory_pool, pcrSubq)));

		pexprSubqPred = CPredicateUtils::PexprDisjunction(memory_pool, pdrgpexpr);
	}

	CExpression *pexprSelect = CUtils::PexprLogicalSelect(memory_pool, pexprResult, pexprSubqPred);
	if (fUsesNullableCol)
	{
		const CColRef *pcrNullIndicator = CScalarProjectElement::PopConvert((*(*(*pexprSelect)[0])[1])[0]->Pop())->Pcr();
		pexprInnerNew = CUtils::PexprCountStarAndSum(memory_pool, pcrNullIndicator, pexprSelect);
		const CColRef *pcrCount = CScalarProjectElement::PopConvert((*(*pexprInnerNew)[1])[0]->Pop())->Pcr();
		const CColRef *pcrSum = CScalarProjectElement::PopConvert((*(*pexprInnerNew)[1])[1]->Pop())->Pcr();

		CExpression *pexprScalarIdentCount = CUtils::PexprScalarIdent(memory_pool, pcrCount);
		CExpression *pexprCountEqZero = CUtils::PexprCmpWithZero(memory_pool, pexprScalarIdentCount, CScalarIdent::PopConvert(pexprScalarIdentCount->Pop())->MDIdType(), IMDType::EcmptEq);
		CExpression *pexprCountEqSum = CUtils::PexprScalarEqCmp(memory_pool, pcrCount, pcrSum);

		CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
		const IMDTypeInt8 *pmdtypeint8 = md_accessor->PtMDType<IMDTypeInt8>();
		IMDId *pmdidInt8 = pmdtypeint8->MDId();
		pmdidInt8->AddRef();
		pmdidInt8->AddRef();

		CExpression *pexprProjected =
			GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CScalarIf(memory_pool, pmdidInt8),
				pexprCountEqZero,
				CUtils::PexprScalarConstInt8(memory_pool, 0 /*val*/),
				GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CScalarIf(memory_pool, pmdidInt8),
					pexprCountEqSum,
					CUtils::PexprScalarConstInt8(memory_pool, -1 /*val*/),
					CUtils::PexprScalarIdent(memory_pool, pcrCount)
					)
			);
		CExpression *pexprPrj = CUtils::PexprAddProjection(memory_pool, pexprInnerNew, pexprProjected);

		const CColRef *pcrSubquery = CScalarProjectElement::PopConvert((*(*pexprPrj)[1])[0]->Pop())->Pcr();
		*ppexprNewSubquery = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarSubquery(memory_pool, pcrSubquery, false /*fGeneratedByExist*/, true /*fGeneratedByQuantified*/), pexprPrj);
		*ppexprNewScalar = CUtils::PexprCmpWithZero(memory_pool, CUtils::PexprScalarIdent(memory_pool, pcrSubquery), pcrSubquery->RetrieveType()->MDId(), IMDType::EcmptG);
	}
	else
	{
		pexprInnerNew = CUtils::PexprCountStar(memory_pool, pexprSelect);
		const CColRef *pcrCount = CScalarProjectElement::PopConvert((*(*pexprInnerNew)[1])[0]->Pop())->Pcr();

		*ppexprNewSubquery = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarSubquery(memory_pool, pcrCount, false /*fGeneratedByExist*/, true /*fGeneratedByQuantified*/), pexprInnerNew);
		*ppexprNewScalar = CUtils::PexprCmpWithZero(memory_pool, CUtils::PexprScalarIdent(memory_pool, pcrCount), pcrCount->RetrieveType()->MDId(), IMDType::EcmptG);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::SubqueryAllToAgg
//
//	@doc:
//		Helper for transforming SubqueryAll into aggregate subquery,
//		we generate aggregate expressions that compute the following values:
//		- N: number of null values returned by evaluating inner expression
//		- S: number of inner values matching outer m_bytearray_value
//		the generated subquery returns a Boolean result generated by the following
//		nested-if statement:
//
//			if (inner is empty)
//				return true
//			else if (N > 0)
//				return null
//			else if (outer m_bytearray_value is null)
//				return null
//			else if (S == 0)
//				return true
//			else
//				return false
//
//
//---------------------------------------------------------------------------
void
CXformUtils::SubqueryAllToAgg
	(
	IMemoryPool *memory_pool,
	CExpression *pexprSubquery,
	CExpression **ppexprNewSubquery, // output argument for new scalar subquery
	CExpression **ppexprNewScalar   // output argument for new scalar expression
	)
{
	GPOS_ASSERT(CUtils::FQuantifiedSubquery(pexprSubquery->Pop()));
	GPOS_ASSERT(COperator::EopScalarSubqueryAll == pexprSubquery->Pop()->Eopid());
	GPOS_ASSERT(NULL != ppexprNewSubquery);
	GPOS_ASSERT(NULL != ppexprNewScalar);

	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	CExpression *pexprInner = (*pexprSubquery)[0];
	CExpression *pexprScalarOuter = (*pexprSubquery)[1];
	CExpression *pexprSubqPred = PexprInversePred(memory_pool, pexprSubquery);

	// generate subquery test expression
	const IMDTypeInt4 *pmdtypeint4 = md_accessor->PtMDType<IMDTypeInt4>();
	IMDId *pmdidInt4 = pmdtypeint4->MDId();
	pmdidInt4->AddRef();
	CExpression *pexprSubqTest =
			GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CScalarIf(memory_pool, pmdidInt4),
				pexprSubqPred,
				CUtils::PexprScalarConstInt4(memory_pool, 1 /*val*/),
				CUtils::PexprScalarConstInt4(memory_pool, 0 /*val*/)
				);

	DrgPexpr *pdrgpexpr = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	pdrgpexpr->Append(pexprSubqTest);

	// generate null indicator for inner expression
	const CColRef *pcrSubq = CScalarSubqueryQuantified::PopConvert(pexprSubquery->Pop())->Pcr();
	CExpression *pexprInnerNullIndicator = PexprNullIndicator(memory_pool, CUtils::PexprScalarIdent(memory_pool, pcrSubq));
	pdrgpexpr->Append(pexprInnerNullIndicator);

	// add generated expression as projected nodes
	pexprInner->AddRef();
	CExpression *pexprPrj = CUtils::PexprAddProjection(memory_pool, pexprInner, pdrgpexpr);
	pdrgpexpr->Release();

	// generate a group by expression with sum(subquery-test) and sum(inner null indicator) aggreagtes
	DrgPcr *colref_array = GPOS_NEW(memory_pool) DrgPcr(memory_pool);
	CColRef *pcrSubqTest = const_cast<CColRef*>(CScalarProjectElement::PopConvert((*(*pexprPrj)[1])[0]->Pop())->Pcr());
	CColRef *pcrInnerNullTest = const_cast<CColRef *>(CScalarProjectElement::PopConvert((*(*pexprPrj)[1])[1]->Pop())->Pcr());
	colref_array->Append(pcrSubqTest);
	colref_array->Append(pcrInnerNullTest);
	CExpression *pexprGbAggSum = CUtils::PexprGbAggSum(memory_pool, pexprPrj, colref_array);
	colref_array->Release();

	// generate helper test expressions
	const CColRef *pcrSum = CScalarProjectElement::PopConvert((*(*pexprGbAggSum)[1])[0]->Pop())->Pcr();
	const CColRef *pcrSumNulls = CScalarProjectElement::PopConvert((*(*pexprGbAggSum)[1])[1]->Pop())->Pcr();
	CExpression *pexprScalarIdentSum = CUtils::PexprScalarIdent(memory_pool, pcrSum);
	CExpression *pexprScalarIdentSumNulls = CUtils::PexprScalarIdent(memory_pool, pcrSumNulls);

	CExpression *pexprSumTest = CUtils::PexprCmpWithZero(memory_pool, pexprScalarIdentSum, CScalarIdent::PopConvert(pexprScalarIdentSum->Pop())->MDIdType(), IMDType::EcmptEq);
	pexprScalarIdentSum->AddRef();
	CExpression *pexprIsInnerEmpty = CUtils::PexprIsNull(memory_pool, pexprScalarIdentSum);
	CExpression *pexprInnerHasNulls = CUtils::PexprCmpWithZero(memory_pool, pexprScalarIdentSumNulls, CScalarIdent::PopConvert(pexprScalarIdentSumNulls->Pop())->MDIdType(), IMDType::EcmptG);
	pexprScalarOuter->AddRef();
	CExpression *pexprIsOuterNull = CUtils::PexprIsNull(memory_pool, pexprScalarOuter);

	// generate the main scalar if that will produce subquery result
	const IMDTypeBool *pmdtypebool = md_accessor->PtMDType<IMDTypeBool>();
	IMDId *pmdidBool = pmdtypebool->MDId();
	pmdidBool->AddRef();
	pmdidBool->AddRef();
	pmdidBool->AddRef();
	pmdidBool->AddRef();
	pexprPrj =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CScalarIf(memory_pool, pmdidBool),
			pexprIsInnerEmpty,
			CUtils::PexprScalarConstBool(memory_pool, true /*m_bytearray_value*/), // if inner is empty, return true
			GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CScalarIf(memory_pool, pmdidBool),
					pexprInnerHasNulls,
					CUtils::PexprScalarConstBool(memory_pool, false /*m_bytearray_value*/, true /*is_null*/),	// if inner produced null values, return null
					GPOS_NEW(memory_pool) CExpression
						(
						memory_pool,
						GPOS_NEW(memory_pool) CScalarIf(memory_pool, pmdidBool),
						pexprIsOuterNull,
						CUtils::PexprScalarConstBool(memory_pool, false /*m_bytearray_value*/, true /*is_null*/), // if outer m_bytearray_value is null, return null
						GPOS_NEW(memory_pool) CExpression
							(
							memory_pool,
							GPOS_NEW(memory_pool) CScalarIf(memory_pool, pmdidBool),
							pexprSumTest,   // otherwise, test number of inner values that match outer m_bytearray_value
							CUtils::PexprScalarConstBool(memory_pool, true /*m_bytearray_value*/),  // no matches
							CUtils::PexprScalarConstBool(memory_pool, false /*m_bytearray_value*/)  // at least one match
							)
						)
					)
			);

	CExpression *pexprProjected = CUtils::PexprAddProjection(memory_pool, pexprGbAggSum, pexprPrj);

	const CColRef *pcrSubquery = CScalarProjectElement::PopConvert((*(*pexprProjected)[1])[0]->Pop())->Pcr();
	*ppexprNewSubquery = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarSubquery(memory_pool, pcrSubquery, false /*fGeneratedByExist*/, true /*fGeneratedByQuantified*/), pexprProjected);
	*ppexprNewScalar = CUtils::PexprScalarCmp(memory_pool, CUtils::PexprScalarIdent(memory_pool, pcrSubquery),CUtils::PexprScalarConstBool(memory_pool, true /*m_bytearray_value*/), IMDType::EcmptEq);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprSeparateSubqueryPreds
//
//	@doc:
//		Helper function to separate subquery predicates in a top Select node
//
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprSeparateSubqueryPreds
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr
	)
{
	COperator::EOperatorId op_id = pexpr->Pop()->Eopid();
	GPOS_ASSERT(COperator::EopLogicalInnerJoin == op_id ||
			COperator::EopLogicalNAryJoin == op_id);

	// split scalar expression into a conjunction of predicates with and without
	// subqueries
	const ULONG arity = pexpr->Arity();
	CExpression *pexprScalar = (*pexpr)[arity - 1];
	DrgPexpr *pdrgpexprConjuncts = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pexprScalar);
	DrgPexpr *pdrgpexprSQ = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	DrgPexpr *pdrgpexprNonSQ = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);

	const ULONG ulConjuncts = pdrgpexprConjuncts->Size();
	for (ULONG ul = 0; ul < ulConjuncts; ul++)
	{
		CExpression *pexprConj = (*pdrgpexprConjuncts)[ul];
		pexprConj->AddRef();

		if (CDrvdPropScalar::GetDrvdScalarProps(pexprConj->PdpDerive())->FHasSubquery())
		{
			pdrgpexprSQ->Append(pexprConj);
		}
		else
		{
			pdrgpexprNonSQ->Append(pexprConj);
		}
	}
	GPOS_ASSERT(0 < pdrgpexprSQ->Size());

	pdrgpexprConjuncts->Release();

	// build children array from logical children and a conjunction of
	// non-subquery predicates
	DrgPexpr *pdrgpexpr = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	for (ULONG ul = 0; ul < arity - 1; ul++)
	{
		CExpression *pexprChild = (*pexpr)[ul];
		pexprChild->AddRef();
		pdrgpexpr->Append(pexprChild);
	}
	pdrgpexpr->Append(CPredicateUtils::PexprConjunction(memory_pool, pdrgpexprNonSQ));

	// build a new join
	COperator *popJoin = NULL;
	if (COperator::EopLogicalInnerJoin == op_id)
	{
		popJoin = GPOS_NEW(memory_pool) CLogicalInnerJoin(memory_pool);
	}
	else
	{
		popJoin = GPOS_NEW(memory_pool) CLogicalNAryJoin(memory_pool);
	}
	CExpression *pexprJoin = GPOS_NEW(memory_pool) CExpression(memory_pool, popJoin, pdrgpexpr);

	// return a Select node with a conjunction of subquery predicates
	return CUtils::PexprLogicalSelect(memory_pool, pexprJoin, CPredicateUtils::PexprConjunction(memory_pool, pdrgpexprSQ));
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprInversePred
//
//	@doc:
//		Helper for creating the inverse of the predicate used by
//		subquery ALL
//
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprInversePred
	(
	IMemoryPool *memory_pool,
	CExpression *pexprSubquery
	)
{
	// get the scalar child of subquery
	CScalarSubqueryAll *popSqAll = CScalarSubqueryAll::PopConvert(pexprSubquery->Pop());
	CExpression *pexprScalar = (*pexprSubquery)[1];
	const CColRef *colref = popSqAll->Pcr();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	// get mdid and name of the inverse of the comparison operator used by subquery
	IMDId *mdid_op = popSqAll->MdIdOp();
	IMDId *pmdidInverseOp = md_accessor->RetrieveScOp(mdid_op)->GetInverseOpMdid();
	const CWStringConst *pstrFirst = md_accessor->RetrieveScOp(pmdidInverseOp)->Mdname().GetMDName();

	// generate a predicate for the inversion of the comparison involved in the subquery
	pexprScalar->AddRef();
	pmdidInverseOp->AddRef();

	return CUtils::PexprScalarCmp(memory_pool, pexprScalar, colref, *pstrFirst, pmdidInverseOp);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprNullIndicator
//
//	@doc:
//		Helper for creating a null indicator
//
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprNullIndicator
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr
	)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	CExpression *pexprIsNull = CUtils::PexprIsNull(memory_pool, pexpr);
	const IMDTypeInt4 *pmdtypeint4 = md_accessor->PtMDType<IMDTypeInt4>();
	IMDId *mdid = pmdtypeint4->MDId();
	mdid->AddRef();
	return GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CScalarIf(memory_pool, mdid),
			pexprIsNull,
			CUtils::PexprScalarConstInt4(memory_pool, 1 /*val*/),
			CUtils::PexprScalarConstInt4(memory_pool, 0 /*val*/)
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprLogicalPartitionSelector
//
//	@doc:
// 		Create a logical partition selector for the given table descriptor on top
//		of the given child expression. The partition selection filters use columns
//		from the given column array
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprLogicalPartitionSelector
	(
	IMemoryPool *memory_pool,
	CTableDescriptor *ptabdesc,
	DrgPcr *colref_array,
	CExpression *pexprChild
	)
{
	IMDId *rel_mdid = ptabdesc->MDId();
	rel_mdid->AddRef();

	// create an oid column
	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDTypeOid *pmdtype = md_accessor->PtMDType<IMDTypeOid>();
	CColRef *pcrOid = col_factory->PcrCreate(pmdtype, default_type_modifier);
	DrgPexpr *pdrgpexprFilters = PdrgpexprPartEqFilters(memory_pool, ptabdesc, colref_array);

	CLogicalPartitionSelector *popSelector = GPOS_NEW(memory_pool) CLogicalPartitionSelector(memory_pool, rel_mdid, pdrgpexprFilters, pcrOid);

	return GPOS_NEW(memory_pool) CExpression(memory_pool, popSelector, pexprChild);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprLogicalDMLOverProject
//
//	@doc:
// 		Create a logical DML on top of a project
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprLogicalDMLOverProject
	(
	IMemoryPool *memory_pool,
	CExpression *pexprChild,
	CLogicalDML::EDMLOperator edmlop,
	CTableDescriptor *ptabdesc,
	DrgPcr *colref_array,
	CColRef *pcrCtid,
	CColRef *pcrSegmentId
	)
{
	GPOS_ASSERT(CLogicalDML::EdmlInsert == edmlop || CLogicalDML::EdmlDelete == edmlop);
	INT val = CScalarDMLAction::EdmlactionInsert;
	if (CLogicalDML::EdmlDelete == edmlop)
	{
		val = CScalarDMLAction::EdmlactionDelete;
	}

	// new expressions to project
	IMDId *rel_mdid = ptabdesc->MDId();
	CExpression *pexprProject = NULL;
	CColRef *pcrAction = NULL;
	CColRef *pcrOid = NULL;

	if (ptabdesc->IsPartitioned())
	{
		// generate a PartitionSelector node which generates OIDs, then add a project
		// on top of that to add the action column
		CExpression *pexprSelector = PexprLogicalPartitionSelector(memory_pool, ptabdesc, colref_array, pexprChild);
		if (CUtils::FGeneratePartOid(ptabdesc->MDId()))
		{
			pcrOid = CLogicalPartitionSelector::PopConvert(pexprSelector->Pop())->PcrOid();
		}
		pexprProject = CUtils::PexprAddProjection(memory_pool, pexprSelector, CUtils::PexprScalarConstInt4(memory_pool, val));
		CExpression *pexprPrL = (*pexprProject)[1];
		pcrAction = CUtils::PcrFromProjElem((*pexprPrL)[0]);
	}
	else
	{
		DrgPexpr *pdrgpexprProjected = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
		// generate one project node with two new columns: action, oid (based on the traceflag)
		pdrgpexprProjected->Append(CUtils::PexprScalarConstInt4(memory_pool, val));

		BOOL fGeneratePartOid = CUtils::FGeneratePartOid(ptabdesc->MDId());
		if (fGeneratePartOid)
		{
			OID oidTable = CMDIdGPDB::CastMdid(rel_mdid)->OidObjectId();
			pdrgpexprProjected->Append(CUtils::PexprScalarConstOid(memory_pool, oidTable));
		}

		pexprProject = CUtils::PexprAddProjection(memory_pool, pexprChild, pdrgpexprProjected);
		pdrgpexprProjected->Release();

		CExpression *pexprPrL = (*pexprProject)[1];
		pcrAction = CUtils::PcrFromProjElem((*pexprPrL)[0]);
		if (fGeneratePartOid)
		{
			pcrOid = CUtils::PcrFromProjElem((*pexprPrL)[1]);
		}
	}

	GPOS_ASSERT(NULL != pcrAction);

	if (FTriggersExist(edmlop, ptabdesc, true /*fBefore*/))
	{
		rel_mdid->AddRef();
		pexprProject = PexprRowTrigger(memory_pool, pexprProject, edmlop, rel_mdid, true /*fBefore*/, colref_array);
	}

	if (CLogicalDML::EdmlInsert == edmlop)
	{
		// add assert for check constraints and nullness checks if needed
		COptimizerConfig *optimizer_config = COptCtxt::PoctxtFromTLS()->GetOptimizerConfig();
		if (optimizer_config->GetHint()->FEnforceConstraintsOnDML())
		{
			pexprProject = PexprAssertConstraints(memory_pool, pexprProject, ptabdesc, colref_array);
		}
	}

	CExpression *pexprDML = GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CLogicalDML(memory_pool, edmlop, ptabdesc, colref_array, GPOS_NEW(memory_pool) CBitSet(memory_pool) /*pbsModified*/, pcrAction, pcrOid, pcrCtid, pcrSegmentId, NULL /*pcrTupleOid*/),
			pexprProject
			);

	CExpression *pexprOutput = pexprDML;

	if (FTriggersExist(edmlop, ptabdesc, false /*fBefore*/))
	{
		rel_mdid->AddRef();
		pexprOutput = PexprRowTrigger(memory_pool, pexprOutput, edmlop, rel_mdid, false /*fBefore*/, colref_array);
	}

	return pexprOutput;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::FTriggersExist
//
//	@doc:
//		Check whether there are any BEFORE or AFTER row-level triggers on
//		the given table that match the given DML operation
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FTriggersExist
	(
	CLogicalDML::EDMLOperator edmlop,
	CTableDescriptor *ptabdesc,
	BOOL fBefore
	)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDRelation *pmdrel = md_accessor->RetrieveRel(ptabdesc->MDId());
	const ULONG ulTriggers = pmdrel->TriggerCount();

	for (ULONG ul = 0; ul < ulTriggers; ul++)
	{
		const IMDTrigger *pmdtrigger = md_accessor->RetrieveTrigger(pmdrel->TriggerMDidAt(ul));
		if (!pmdtrigger->IsEnabled() ||
			!pmdtrigger->ExecutesOnRowLevel() ||
			!FTriggerApplies(edmlop, pmdtrigger))
		{
			continue;
		}

		if (pmdtrigger->IsBefore() == fBefore)
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::FTriggerApplies
//
//	@doc:
//		Does the given trigger type match the given logical DML type
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FTriggerApplies
	(
	CLogicalDML::EDMLOperator edmlop,
	const IMDTrigger *pmdtrigger
	)
{
	return ((CLogicalDML::EdmlInsert == edmlop && pmdtrigger->IsInsert()) ||
			(CLogicalDML::EdmlDelete == edmlop && pmdtrigger->IsDelete()) ||
			(CLogicalDML::EdmlUpdate == edmlop && pmdtrigger->IsUpdate()));
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprRowTrigger
//
//	@doc:
//		Construct a trigger expression on top of the given expression
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprRowTrigger
	(
	IMemoryPool *memory_pool,
	CExpression *pexprChild,
	CLogicalDML::EDMLOperator edmlop,
	IMDId *rel_mdid,
	BOOL fBefore,
	DrgPcr *colref_array
	)
{
	GPOS_ASSERT(CLogicalDML::EdmlInsert == edmlop || CLogicalDML::EdmlDelete == edmlop);

	colref_array->AddRef();
	if (CLogicalDML::EdmlInsert == edmlop)
	{
		return PexprRowTrigger(memory_pool, pexprChild, edmlop, rel_mdid, fBefore, NULL /*pdrgpcrOld*/, colref_array);
	}

	return PexprRowTrigger(memory_pool, pexprChild, edmlop, rel_mdid, fBefore, colref_array, NULL /*pdrgpcrNew*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprAssertNotNull
//
//	@doc:
//		Construct an assert on top of the given expression for nullness checks
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprAssertNotNull
	(
	IMemoryPool *memory_pool,
	CExpression *pexprChild,
	CTableDescriptor *ptabdesc,
	DrgPcr *colref_array
	)
{	
	DrgPcoldesc *pdrgpcoldesc = ptabdesc->Pdrgpcoldesc();
	
	const ULONG num_cols = pdrgpcoldesc->Size();
	CColRefSet *pcrsNotNull = CDrvdPropRelational::GetRelationalProperties(pexprChild->PdpDerive())->PcrsNotNull();
	
	DrgPexpr *pdrgpexprAssertConstraints = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		CColumnDescriptor *pcoldesc = (*pdrgpcoldesc)[ul];
		if (pcoldesc->IsNullable() || pcoldesc->IsSystemColumn())
		{
			// target column is nullable or it's a system column: no need to check for NULL
			continue;
		}

		CColRef *colref = (*colref_array)[ul];

		if (pcrsNotNull->FMember(colref))
		{
			// source column not nullable: no need to check for NULL
			continue;
		}
		
		// add not null check for current column
		CExpression *pexprNotNull = CUtils::PexprIsNotNull(memory_pool, CUtils::PexprScalarIdent(memory_pool, colref));
		
		CWStringConst *pstrErrorMsg = PstrErrorMessage
										(
										memory_pool,
										gpos::CException::ExmaSQL, 
										gpos::CException::ExmiSQLNotNullViolation,  
										pcoldesc->Name().Pstr()->GetBuffer(),
										ptabdesc->Name().Pstr()->GetBuffer()
										);
		
		CExpression *pexprAssertConstraint = GPOS_NEW(memory_pool) CExpression
										(
										memory_pool,
										GPOS_NEW(memory_pool) CScalarAssertConstraint(memory_pool, pstrErrorMsg),
										pexprNotNull
										);
		
		pdrgpexprAssertConstraints->Append(pexprAssertConstraint);
	}
	
	if (0 == pdrgpexprAssertConstraints->Size())
	{
		pdrgpexprAssertConstraints->Release();
		return pexprChild;
	}
	
	CExpression *pexprAssertPredicate = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarAssertConstraintList(memory_pool), pdrgpexprAssertConstraints);

	CLogicalAssert *popAssert = 
			GPOS_NEW(memory_pool) CLogicalAssert
						(
						memory_pool,
						GPOS_NEW(memory_pool) CException(gpos::CException::ExmaSQL, gpos::CException::ExmiSQLNotNullViolation)
						);
			
	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					popAssert,
					pexprChild,
					pexprAssertPredicate
					);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprRowTrigger
//
//	@doc:
//		Construct a trigger expression on top of the given expression
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprRowTrigger
	(
	IMemoryPool *memory_pool,
	CExpression *pexprChild,
	CLogicalDML::EDMLOperator edmlop,
	IMDId *rel_mdid,
	BOOL fBefore,
	DrgPcr *pdrgpcrOld,
	DrgPcr *pdrgpcrNew
	)
{
	INT type = GPMD_TRIGGER_ROW;
	if (fBefore)
	{
		type |= GPMD_TRIGGER_BEFORE;
	}

	switch (edmlop)
	{
		case CLogicalDML::EdmlInsert:
			type |= GPMD_TRIGGER_INSERT;
			break;
		case CLogicalDML::EdmlDelete:
			type |= GPMD_TRIGGER_DELETE;
			break;
		case CLogicalDML::EdmlUpdate:
			type |= GPMD_TRIGGER_UPDATE;
			break;
		default:
			GPOS_ASSERT(!"Invalid DML operation");
	}

	return GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CLogicalRowTrigger(memory_pool, rel_mdid, type, pdrgpcrOld, pdrgpcrNew),
			pexprChild
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PdrgpexprPartFilters
//
//	@doc:
// 		Return partition filter expressions for a DML operation given a table
//		descriptor and the column references seen by this DML
//
//---------------------------------------------------------------------------
DrgPexpr *
CXformUtils::PdrgpexprPartEqFilters
	(
	IMemoryPool *memory_pool,
	CTableDescriptor *ptabdesc,
	DrgPcr *pdrgpcrSource
	)
{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != pdrgpcrSource);

	const ULongPtrArray *pdrgpulPart = ptabdesc->PdrgpulPart();

	DrgPexpr *pdrgpexpr = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);

	const ULONG ulPartKeys = pdrgpulPart->Size();
	GPOS_ASSERT(0 < ulPartKeys);
	GPOS_ASSERT(pdrgpcrSource->Size() >= ulPartKeys);

	for (ULONG ul = 0; ul < ulPartKeys; ul++)
	{
		ULONG *pulPartKey = (*pdrgpulPart)[ul];
		CColRef *colref = (*pdrgpcrSource)[*pulPartKey];
		pdrgpexpr->Append(CUtils::PexprScalarIdent(memory_pool, colref));
	}

	return pdrgpexpr;
}

//---------------------------------------------------------------------------
//      @function:
//              CXformUtils::PexprAssertConstraints
//
//      @doc:
//          Construct an assert on top of the given expression for check constraints
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprAssertConstraints
	(
	IMemoryPool *memory_pool,
	CExpression *pexprChild,
	CTableDescriptor *ptabdesc,
	DrgPcr *colref_array
	)
{
	CExpression *pexprAssertNotNull = PexprAssertNotNull(memory_pool, pexprChild, ptabdesc, colref_array);

	return PexprAssertCheckConstraints(memory_pool, pexprAssertNotNull, ptabdesc, colref_array);
}

//---------------------------------------------------------------------------
//      @function:
//              CXformUtils::PexprAssertCheckConstraints
//
//      @doc:
//          Construct an assert on top of the given expression for check constraints
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprAssertCheckConstraints
	(
	IMemoryPool *memory_pool,
	CExpression *pexprChild,
	CTableDescriptor *ptabdesc,
	DrgPcr *colref_array
	)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDRelation *pmdrel = md_accessor->RetrieveRel(ptabdesc->MDId());

	const ULONG ulCheckConstraint = pmdrel->CheckConstraintCount();
	if (0 < ulCheckConstraint)
	{
	 	DrgPexpr *pdrgpexprAssertConstraints = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	 	
		for (ULONG ul = 0; ul < ulCheckConstraint; ul++)
		{
			IMDId *pmdidCheckConstraint = pmdrel->CheckConstraintMDidAt(ul);
			const IMDCheckConstraint *pmdCheckConstraint = md_accessor->RetrieveCheckConstraints(pmdidCheckConstraint);

			// extract the check constraint expression
			CExpression *pexprCheckConstraint = pmdCheckConstraint->GetCheckConstraintExpr(memory_pool, md_accessor, colref_array);

			// A table check constraint is satisfied if and only if the specified <search condition>
			// evaluates to True or Unknown for every row of the table to which it applies.
			// Add an "is not false" expression on top to handle such scenarios
			CExpression *pexprIsNotFalse = CUtils::PexprIsNotFalse(memory_pool, pexprCheckConstraint);
			CWStringConst *pstrErrMsg = PstrErrorMessage
										(
										memory_pool,
										gpos::CException::ExmaSQL, 
										gpos::CException::ExmiSQLCheckConstraintViolation,
										pmdCheckConstraint->Mdname().GetMDName()->GetBuffer(),
										ptabdesc->Name().Pstr()->GetBuffer()
										);
			CExpression *pexprAssertConstraint = GPOS_NEW(memory_pool) CExpression
													(
													memory_pool,
													GPOS_NEW(memory_pool) CScalarAssertConstraint(memory_pool, pstrErrMsg),
													pexprIsNotFalse
													);
			
			pdrgpexprAssertConstraints->Append(pexprAssertConstraint);	
		}

	 	CExpression *pexprAssertPredicate = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarAssertConstraintList(memory_pool), pdrgpexprAssertConstraints);
	 	
		CLogicalAssert *popAssert = 
				GPOS_NEW(memory_pool) CLogicalAssert
							(
							memory_pool,
							GPOS_NEW(memory_pool) CException(gpos::CException::ExmaSQL, gpos::CException::ExmiSQLCheckConstraintViolation)
							);
		

	 	return GPOS_NEW(memory_pool) CExpression
	 						(
	 						memory_pool,
	 						popAssert,
	 						pexprChild,
	 						pexprAssertPredicate
	 						);
	}

	return pexprChild;
}

//---------------------------------------------------------------------------
//      @function:
//              CXformUtils::PexprAssertUpdateCardinality
//
//      @doc:
//          Construct an assert on top of the given expression for checking cardinality
//			of updated values during DML UPDATE
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprAssertUpdateCardinality
	(
	IMemoryPool *memory_pool,
	CExpression *pexprDMLChild,
	CExpression *pexprDML,
	CColRef *pcrCtid,
	CColRef *pcrSegmentId
	)
{
	COptCtxt *poctxt = COptCtxt::PoctxtFromTLS();
	CMDAccessor *md_accessor = poctxt->Pmda();
	
	CColRefSet *pcrsKey = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrsKey->Include(pcrSegmentId);
	pcrsKey->Include(pcrCtid);
	
	CDrvdPropRelational *pdprel = CDrvdPropRelational::GetRelationalProperties(pexprDMLChild->PdpDerive()); 
	CKeyCollection *pkc = pdprel->Pkc();
	if (NULL != pkc && pkc->FKey(pcrsKey))
	{
		// {segid, ctid} is a key: cardinality constraint is satisfied
		pcrsKey->Release();
		return pexprDML;
	}					
	
	pcrsKey->Release();
	
	// TODO:  - May 20, 2013; re-enable cardinality assert when the executor 
	// supports DML in a non-root slice
	
	GPOS_RAISE(gpopt::ExmaGPOPT, gpopt::ExmiUnsupportedNonDeterministicUpdate);

	// construct a select(Action='DEL')
	CLogicalDML *popDML = CLogicalDML::PopConvert(pexprDML->Pop());
	CExpression *pexprConstDel = CUtils::PexprScalarConstInt4(memory_pool, CLogicalDML::EdmlDelete /*val*/);
	CExpression *pexprDelPredicate = CUtils::PexprScalarCmp(memory_pool, popDML->PcrAction(), pexprConstDel, IMDType::EcmptEq);
	CExpression *pexprSelectDeleted = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CLogicalSelect(memory_pool), pexprDML, pexprDelPredicate);
	// construct a group by	
	CColumnFactory *col_factory = poctxt->Pcf();
		
	CExpression *pexprCountStar = CUtils::PexprCountStar(memory_pool);

	CScalar *pop = CScalar::PopConvert(pexprCountStar->Pop());
	const IMDType *pmdtype = md_accessor->RetrieveType(pop->MDIdType());
	CColRef *pcrProjElem = col_factory->PcrCreate(pmdtype, pop->TypeModifier());
	 
	CExpression *pexprProjElem = GPOS_NEW(memory_pool) CExpression
									(
									memory_pool,
									GPOS_NEW(memory_pool) CScalarProjectElement(memory_pool, pcrProjElem),
									pexprCountStar
									);
	DrgPexpr *pdrgpexprProjElemsCountDistinct = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	pdrgpexprProjElemsCountDistinct->Append(pexprProjElem);
	CExpression *pexprProjList = GPOS_NEW(memory_pool) CExpression
								(
								memory_pool,
								GPOS_NEW(memory_pool) CScalarProjectList(memory_pool),
								pdrgpexprProjElemsCountDistinct
								);
								
	DrgPcr *pdrgpcrGbCols = GPOS_NEW(memory_pool) DrgPcr(memory_pool);
	pdrgpcrGbCols->Append(pcrCtid);
	pdrgpcrGbCols->Append(pcrSegmentId);
	
	CExpression *pexprGbAgg = GPOS_NEW(memory_pool) CExpression
								(
								memory_pool,
								GPOS_NEW(memory_pool) CLogicalGbAgg(memory_pool, pdrgpcrGbCols, COperator::EgbaggtypeGlobal /*egbaggtype*/),
								pexprSelectDeleted,
								pexprProjList
								);
	
	// construct a predicate of the kind "count(*) == 1"
	CExpression *pexprConst1 = CUtils::PexprScalarConstInt8(memory_pool, 1 /*val*/);
	// obtain error code and error message	
	CWStringConst *pstrErrorMsg = GPOS_NEW(memory_pool) CWStringConst(memory_pool, GPOS_WSZ_LIT("Duplicate values in UPDATE statement"));

	CExpression *pexprAssertConstraint = GPOS_NEW(memory_pool) CExpression
											(
											memory_pool,
											GPOS_NEW(memory_pool) CScalarAssertConstraint(memory_pool, pstrErrorMsg),
											CUtils::PexprScalarCmp(memory_pool, pcrProjElem, pexprConst1, IMDType::EcmptEq)
											);
	
	CExpression *pexprAssertPredicate = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarAssertConstraintList(memory_pool), pexprAssertConstraint);
		
	return GPOS_NEW(memory_pool) CExpression
						(
						memory_pool,
						GPOS_NEW(memory_pool) CLogicalAssert
								(
								memory_pool,
								GPOS_NEW(memory_pool) CException(gpos::CException::ExmaSQL, gpos::CException::ExmiSQLDefault)
								),
						pexprGbAgg,
						pexprAssertPredicate
						);	
}

//---------------------------------------------------------------------------
//   @function:
//		CXformUtils::FSupportsMinAgg
//
//   @doc:
//      Check if all column types support MIN aggregate
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FSupportsMinAgg
	(
	DrgPcr *colref_array
	)
{
	const ULONG num_cols = colref_array->Size();
	
	// add the columns to project list
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		CColRef *colref = (*colref_array)[ul];
		const IMDType *pmdtype = colref->RetrieveType();
		if (!IMDId::IsValid(pmdtype->GetMdidForAggType(IMDType::EaggMin)))
		{
			return false;
		}
	}
	return true;
}


//---------------------------------------------------------------------------
//   @function:
//		CXformUtils::FSplitAggXform
//
//   @doc:
//      Check if given xform is an Agg splitting xform
//
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FSplitAggXform
	(
	CXform::EXformId exfid
	)
{
	return
		CXform::ExfSplitGbAgg == exfid ||
		CXform::ExfSplitDQA == exfid ||
		CXform::ExfSplitGbAggDedup == exfid;
}


//---------------------------------------------------------------------------
//   @function:
//		CXformUtils::FMultiStageAgg
//
//   @doc:
//      Check if given expression is a multi-stage Agg based on agg type
//		or origin xform
//
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FMultiStageAgg
	(
	CExpression *pexprAgg
	)
{
	GPOS_ASSERT(NULL != pexprAgg);
	GPOS_ASSERT(COperator::EopLogicalGbAgg == pexprAgg->Pop()->Eopid() ||
			COperator::EopLogicalGbAggDeduplicate == pexprAgg->Pop()->Eopid());

	CLogicalGbAgg *popAgg = CLogicalGbAgg::PopConvert(pexprAgg->Pop());
	if (COperator::EgbaggtypeGlobal != popAgg->Egbaggtype())
	{
		// a non-global agg is a multi-stage agg
		return true;
	}

	// check xform lineage
	BOOL fMultiStage = false;
	CGroupExpression *pgexprOrigin = pexprAgg->Pgexpr();
	while (NULL != pgexprOrigin && !fMultiStage)
	{
		fMultiStage = FSplitAggXform(pgexprOrigin->ExfidOrigin());
		pgexprOrigin = pgexprOrigin->PgexprOrigin();
	}

	return fMultiStage;
}


//---------------------------------------------------------------------------
//   @function:
//		CXformUtils::AddMinAggs
//
//   @doc:
//      Add a min(col) project element for each column in the given array to the
//		given expression array
//		
//
//---------------------------------------------------------------------------
void
CXformUtils::AddMinAggs
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CColumnFactory *col_factory,
	DrgPcr *colref_array,
	HMCrCr *phmcrcr,
	DrgPexpr *pdrgpexpr,
	DrgPcr **ppdrgpcrNew
	)
{
	GPOS_ASSERT(NULL != colref_array);
	GPOS_ASSERT(NULL != phmcrcr);
	GPOS_ASSERT(NULL != pdrgpexpr);
	GPOS_ASSERT(NULL != ppdrgpcrNew);
	
	const ULONG num_cols = colref_array->Size();
	
	// add the columns to project list
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		CColRef *colref = (*colref_array)[ul];
		
		CColRef *new_colref = phmcrcr->Find(colref);
		
		if (NULL == new_colref)
		{
			// construct min(col) aggregate
			CExpression *pexprMinAgg = CUtils::PexprMin(memory_pool, md_accessor, colref);
			CScalar *popMin = CScalar::PopConvert(pexprMinAgg->Pop());
			
			const IMDType *pmdtypeMin = md_accessor->RetrieveType(popMin->MDIdType());
			new_colref = col_factory->PcrCreate(pmdtypeMin, popMin->TypeModifier());
			CExpression *pexprProjElemMin = GPOS_NEW(memory_pool) CExpression
											(
											memory_pool,
											GPOS_NEW(memory_pool) CScalarProjectElement(memory_pool, new_colref),
											pexprMinAgg
											);
			
			pdrgpexpr->Append(pexprProjElemMin);
#ifdef GPOS_DEBUG
			BOOL result =
#endif // GPOS_DEBUG
			phmcrcr->Insert(colref, new_colref);
			GPOS_ASSERT(result);
		}
		(*ppdrgpcrNew)->Append(new_colref);
	}
}

//---------------------------------------------------------------------------
//      @function:
//              CXformUtils::FXformInArray
//
//      @doc:
//          Check if given xform id is in the given array of xforms
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FXformInArray
	(
	CXform::EXformId exfid,
	CXform::EXformId rgXforms[],
	ULONG ulXforms
	)
{
	for (ULONG ul = 0; ul < ulXforms; ul++)
	{
		if (rgXforms[ul] == exfid)
		{
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//      @function:
//              CXformUtils::FDeriveStatsBeforeXform
//
//      @doc:
//          Return true if stats derivation is needed for this xform
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FDeriveStatsBeforeXform
	(
	CXform *pxform
	)
{
	GPOS_ASSERT(NULL != pxform);

	return pxform->FExploration() &&
			CXformExploration::Pxformexp(pxform)->FNeedsStats();
}

//---------------------------------------------------------------------------
//      @function:
//              CXformUtils::FSubqueryDecorrelation
//
//      @doc:
//          Check if xform is a subquery decorrelation xform
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FSubqueryDecorrelation
	(
	CXform *pxform
	)
{
	GPOS_ASSERT(NULL != pxform);

	return pxform->FExploration() &&
			CXformExploration::Pxformexp(pxform)->FApplyDecorrelating();
}


//---------------------------------------------------------------------------
//      @function:
//              CXformUtils::FSubqueryUnnesting
//
//      @doc:
//          Check if xform is a subquery unnesting xform
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FSubqueryUnnesting
	(
	CXform *pxform
	)
{
	GPOS_ASSERT(NULL != pxform);

	return pxform->FExploration() &&
			CXformExploration::Pxformexp(pxform)->FSubqueryUnnesting();
}

//---------------------------------------------------------------------------
//      @function:
//              CXformUtils::FApplyToNextBinding
//
//      @doc:
//         Check if xform should be applied to the next binding
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FApplyToNextBinding
	(
	CXform *pxform,
	CExpression *pexprLastBinding // last extracted xform pattern
	)
{
	GPOS_ASSERT(NULL != pxform);

	if (FSubqueryDecorrelation(pxform))
	{
		// if last binding is free from Subquery or Apply operators, we do not
		// need to apply the xform further
		return CUtils::FHasSubqueryOrApply(pexprLastBinding, false /*fCheckRoot*/) ||
				CUtils::FHasCorrelatedApply(pexprLastBinding, false /*fCheckRoot*/);
	}

	// set of transformations that should be applied once
	CXform::EXformId rgXforms[] =
	{
		CXform::ExfJoinAssociativity,
		CXform::ExfExpandFullOuterJoin,
		CXform::ExfUnnestTVF,
		CXform::ExfLeftSemiJoin2CrossProduct,
		CXform::ExfLeftAntiSemiJoin2CrossProduct,
		CXform::ExfLeftAntiSemiJoinNotIn2CrossProduct,
	};

	CXform::EXformId exfid = pxform->Exfid();

	BOOL fApplyOnce =
		FSubqueryUnnesting(pxform) ||
		FXformInArray(exfid, rgXforms, GPOS_ARRAY_SIZE(rgXforms));

	return !fApplyOnce;
}

//---------------------------------------------------------------------------
//      @function:
//              CXformUtils::PstrErrorMessage
//
//      @doc:
//          Compute an error message for given exception type
//
//---------------------------------------------------------------------------
CWStringConst *
CXformUtils::PstrErrorMessage
	(
	IMemoryPool *memory_pool,
	ULONG major,
	ULONG minor,
	...
	)
{
	WCHAR wsz[1024];
	CWStringStatic str(wsz, 1024);
	
	// manufacture actual exception object
	CException exc(major, minor);
	
	// during bootstrap there's no context object otherwise, record
	// all details in the context object
	if (NULL != ITask::Self())
	{
		VA_LIST valist;
		VA_START(valist, minor);

		ELocale eloc = ITask::Self()->Locale();
		CMessage *pmsg = CMessageRepository::GetMessageRepository()->LookupMessage(exc, eloc);
		pmsg->Format(&str, valist);

		VA_END(valist);
	}	

	return GPOS_NEW(memory_pool) CWStringConst(memory_pool, str.GetBuffer());
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PdrgpcrIndexKeys
//
//	@doc:
//		Return the array of columns from the given array of columns which appear 
//		in the index key columns
//
//---------------------------------------------------------------------------
DrgPcr *
CXformUtils::PdrgpcrIndexKeys
	(
	IMemoryPool *memory_pool,
	DrgPcr *colref_array,
	const IMDIndex *pmdindex,
	const IMDRelation *pmdrel
	)
{
	return PdrgpcrIndexColumns(memory_pool, colref_array, pmdindex, pmdrel, EicKey);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PcrsIndexKeys
//
//	@doc:
//		Return the set of columns from the given array of columns which appear 
//		in the index key columns
//
//---------------------------------------------------------------------------
CColRefSet *
CXformUtils::PcrsIndexKeys
	(
	IMemoryPool *memory_pool,
	DrgPcr *colref_array,
	const IMDIndex *pmdindex,
	const IMDRelation *pmdrel
	)
{
	return PcrsIndexColumns(memory_pool, colref_array, pmdindex, pmdrel, EicKey);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PcrsIndexIncludedCols
//
//	@doc:
//		Return the set of columns from the given array of columns which appear 
//		in the index included columns
//
//---------------------------------------------------------------------------
CColRefSet *
CXformUtils::PcrsIndexIncludedCols
	(
	IMemoryPool *memory_pool,
	DrgPcr *colref_array,
	const IMDIndex *pmdindex,
	const IMDRelation *pmdrel
	)
{
	return PcrsIndexColumns(memory_pool, colref_array, pmdindex, pmdrel, EicIncluded);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PcrsIndexColumns
//
//	@doc:
//		Return the set of columns from the given array of columns which appear 
//		in the index columns of the specified type (included / key)
//
//---------------------------------------------------------------------------
CColRefSet *
CXformUtils::PcrsIndexColumns
	(
	IMemoryPool *memory_pool,
	DrgPcr *colref_array,
	const IMDIndex *pmdindex,
	const IMDRelation *pmdrel,
	EIndexCols eic
	)
{
	GPOS_ASSERT(EicKey == eic || EicIncluded == eic);
	DrgPcr *pdrgpcrIndexColumns = PdrgpcrIndexColumns(memory_pool, colref_array, pmdindex, pmdrel, eic);
	CColRefSet *pcrsCols = GPOS_NEW(memory_pool) CColRefSet(memory_pool, pdrgpcrIndexColumns);

	pdrgpcrIndexColumns->Release();
	
	return pcrsCols;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PdrgpcrIndexColumns
//
//	@doc:
//		Return the ordered list of columns from the given array of columns which  
//		appear in the index columns of the specified type (included / key)
//
//---------------------------------------------------------------------------
DrgPcr *
CXformUtils::PdrgpcrIndexColumns
	(
	IMemoryPool *memory_pool,
	DrgPcr *colref_array,
	const IMDIndex *pmdindex,
	const IMDRelation *pmdrel,
	EIndexCols eic
	)
{
	GPOS_ASSERT(EicKey == eic || EicIncluded == eic);
	
	DrgPcr *pdrgpcrIndex = GPOS_NEW(memory_pool) DrgPcr(memory_pool);

	ULONG length = pmdindex->Keys();
	if (EicIncluded == eic)
	{
		length = pmdindex->IncludedCols();
	}

	for (ULONG ul = 0; ul < length; ul++)
	{
		ULONG ulPos = gpos::ulong_max;
		if (EicIncluded == eic)
		{
			ulPos = pmdindex->IncludedColAt(ul);
		}
		else
		{
			ulPos = pmdindex->KeyAt(ul);
		}
		ULONG ulPosNonDropped = pmdrel->NonDroppedColAt(ulPos);

		GPOS_ASSERT(gpos::ulong_max != ulPosNonDropped);
		GPOS_ASSERT(ulPosNonDropped < colref_array->Size());

		CColRef *colref = (*colref_array)[ulPosNonDropped];
		pdrgpcrIndex->Append(colref);
	}

	return pdrgpcrIndex;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::FIndexApplicable
//
//	@doc:
//		Check if an index is applicable given the required, output and scalar
//		expression columns
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FIndexApplicable
	(
	IMemoryPool *memory_pool,
	const IMDIndex *pmdindex,
	const IMDRelation *pmdrel,
	DrgPcr *pdrgpcrOutput,
	CColRefSet *pcrsReqd,
	CColRefSet *pcrsScalar,
	IMDIndex::EmdindexType emdindtype
	)
{
	if (emdindtype != pmdindex->IndexType() ||
		0 == pcrsScalar->Size()) // no columns to match index against
	{
		return false;
	}
	
	BOOL fApplicable = true;
	
	CColRefSet *pcrsIncludedCols = CXformUtils::PcrsIndexIncludedCols(memory_pool, pdrgpcrOutput, pmdindex, pmdrel);
	CColRefSet *pcrsIndexCols = CXformUtils::PcrsIndexKeys(memory_pool, pdrgpcrOutput, pmdindex, pmdrel);
	if (!pcrsIncludedCols->ContainsAll(pcrsReqd) || // index is not covering
		pcrsScalar->IsDisjoint(pcrsIndexCols)) // indexing columns disjoint from the columns used in the scalar expression
	{
		fApplicable = false;
	}
	
	// clean up
	pcrsIncludedCols->Release();
	pcrsIndexCols->Release();

	return fApplicable;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprRowNumber
//
//	@doc:
//		Create an expression with "row_number" window function
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprRowNumber
	(
	IMemoryPool *memory_pool
	)
{

	OID row_number_oid = COptCtxt::PoctxtFromTLS()->GetOptimizerConfig()->GetWindowOids()->OidRowNumber();

	CScalarWindowFunc *popRowNumber = GPOS_NEW(memory_pool) CScalarWindowFunc
													(
													memory_pool,
													GPOS_NEW(memory_pool) CMDIdGPDB(row_number_oid),
													GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT8_OID),
													GPOS_NEW(memory_pool) CWStringConst(memory_pool, GPOS_WSZ_LIT("row_number")),
													CScalarWindowFunc::EwsImmediate,
													false /* is_distinct */,
													false /* is_star_arg */,
													false /* is_simple_agg */
													);

	CExpression *pexprScRowNumber = GPOS_NEW(memory_pool) CExpression(memory_pool, popRowNumber);

	return pexprScRowNumber;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprWindowWithRowNumber
//
//	@doc:
//		Create a sequence project (window) expression with a row_number
//		window function and partitioned by the given array of columns references
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprWindowWithRowNumber
	(
	IMemoryPool *memory_pool,
	CExpression *pexprWindowChild,
	DrgPcr *pdrgpcrInput
	)
{
	// partitioning information
	CDistributionSpec *pds = NULL;
	if (NULL != pdrgpcrInput)
	{
		DrgPexpr *pdrgpexprInput = CUtils::PdrgpexprScalarIdents(memory_pool, pdrgpcrInput);
		pds = GPOS_NEW(memory_pool) CDistributionSpecHashed(pdrgpexprInput, true /* fNullsCollocated */);
	}
	else
	{
		 pds = GPOS_NEW(memory_pool) CDistributionSpecSingleton(CDistributionSpecSingleton::EstMaster);
	}

	// window frames
	DrgPwf *pdrgpwf = GPOS_NEW(memory_pool) DrgPwf(memory_pool);

	// ordering information
	DrgPos *pdrgpos = GPOS_NEW(memory_pool) DrgPos(memory_pool);

	// row_number window function project list
	CExpression *pexprScWindowFunc = PexprRowNumber(memory_pool);

	// generate a new column reference
	CScalarWindowFunc *popScWindowFunc = CScalarWindowFunc::PopConvert(pexprScWindowFunc->Pop());
	const IMDType *pmdtype = COptCtxt::PoctxtFromTLS()->Pmda()->RetrieveType(popScWindowFunc->MDIdType());
	CName name(popScWindowFunc->PstrFunc());
	CColRef *colref = COptCtxt::PoctxtFromTLS()->Pcf()->PcrCreate(pmdtype, popScWindowFunc->TypeModifier(), name);

	// new project element
	CScalarProjectElement *popScPrEl = GPOS_NEW(memory_pool) CScalarProjectElement(memory_pool, colref);

	// generate a project element
	CExpression *pexprProjElem = GPOS_NEW(memory_pool) CExpression(memory_pool, popScPrEl, pexprScWindowFunc);

	// generate the project list
	CExpression *pexprProjList = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool), pexprProjElem);

	CLogicalSequenceProject *popLgSequence = GPOS_NEW(memory_pool) CLogicalSequenceProject(memory_pool, pds, pdrgpos, pdrgpwf);

	pexprWindowChild->AddRef();
	CExpression *pexprLgSequence =  GPOS_NEW(memory_pool) CExpression(memory_pool, popLgSequence, pexprWindowChild, pexprProjList);

	return pexprLgSequence;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprAssertOneRow
//
//	@doc:
//		Generate a logical Assert expression that errors out when more than
//		one row is generated
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprAssertOneRow
	(
	IMemoryPool *memory_pool,
	CExpression *pexprChild
	)
{
	GPOS_ASSERT(NULL != pexprChild);
	GPOS_ASSERT(pexprChild->Pop()->FLogical());

	CExpression *pexprSeqPrj = PexprWindowWithRowNumber(memory_pool, pexprChild, NULL /*pdrgpcrInput*/);
	CColRef *pcrRowNumber = CScalarProjectElement::PopConvert((*(*pexprSeqPrj)[1])[0]->Pop())->Pcr();
	CExpression *pexprCmp = CUtils::PexprScalarEqCmp(memory_pool, pcrRowNumber, CUtils::PexprScalarConstInt4(memory_pool, 1 /*m_bytearray_value*/));

	CWStringConst *pstrErrorMsg = PstrErrorMessage(memory_pool, gpos::CException::ExmaSQL, gpos::CException::ExmiSQLMaxOneRow);
	CExpression *pexprAssertConstraint = GPOS_NEW(memory_pool) CExpression
											(
											memory_pool,
											GPOS_NEW(memory_pool) CScalarAssertConstraint(memory_pool, pstrErrorMsg),
											pexprCmp
											);

	CExpression *pexprAssertPredicate = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarAssertConstraintList(memory_pool), pexprAssertConstraint);
		
	CLogicalAssert *popAssert =
		GPOS_NEW(memory_pool) CLogicalAssert
			(
			memory_pool,
			GPOS_NEW(memory_pool) CException(gpos::CException::ExmaSQL, gpos::CException::ExmiSQLMaxOneRow)
			);

	return GPOS_NEW(memory_pool) CExpression(memory_pool, popAssert, pexprSeqPrj, pexprAssertPredicate);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PcrProjectElement
//
//	@doc:
//		Return the colref of the n-th project element
//---------------------------------------------------------------------------
CColRef *
CXformUtils::PcrProjectElement
	(
	CExpression *pexpr,
	ULONG ulIdxProjElement
	)
{
	CExpression *pexprProjList = (*pexpr)[1];
	GPOS_ASSERT(COperator::EopScalarProjectList == pexprProjList->Pop()->Eopid());

	CExpression *pexprProjElement = (*pexprProjList)[ulIdxProjElement];
	GPOS_ASSERT(NULL != pexprProjElement);

	return CScalarProjectElement::PopConvert(pexprProjElement->Pop())->Pcr();
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::LookupHashJoinKeys
//
//	@doc:
//		Lookup hash join keys in scalar child group
//
//---------------------------------------------------------------------------
void
CXformUtils::LookupHashJoinKeys
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	DrgPexpr **ppdrgpexprOuter,
	DrgPexpr **ppdrgpexprInner
	)
{
	GPOS_ASSERT(NULL != ppdrgpexprOuter);
	GPOS_ASSERT(NULL != ppdrgpexprInner);

	*ppdrgpexprOuter = NULL;
	*ppdrgpexprInner = NULL;
	CGroupExpression *pgexprScalarOrigin = (*pexpr)[2]->Pgexpr();
	if (NULL == pgexprScalarOrigin)
	{
		return;
	}

	CColRefSet *pcrsOuterOutput = CDrvdPropRelational::GetRelationalProperties((*pexpr)[0]->PdpDerive())->PcrsOutput();
	CColRefSet *pcrsInnerOutput = CDrvdPropRelational::GetRelationalProperties((*pexpr)[1]->PdpDerive())->PcrsOutput();

	CGroup *pgroupScalar = pgexprScalarOrigin->Pgroup();
	if (NULL == pgroupScalar->PdrgpexprHashJoinKeysOuter())
	{
		// hash join keys not found
		return;
	}

	GPOS_ASSERT(NULL != pgroupScalar->PdrgpexprHashJoinKeysInner());

	// extract used columns by hash join keys
	CColRefSet *pcrsUsedOuter = CUtils::PcrsExtractColumns(memory_pool, pgroupScalar->PdrgpexprHashJoinKeysOuter());
	CColRefSet *pcrsUsedInner = CUtils::PcrsExtractColumns(memory_pool, pgroupScalar->PdrgpexprHashJoinKeysInner());

	BOOL fOuterKeysUsesOuterChild = pcrsOuterOutput->ContainsAll(pcrsUsedOuter);
	BOOL fInnerKeysUsesInnerChild = pcrsInnerOutput->ContainsAll(pcrsUsedInner);
	BOOL fInnerKeysUsesOuterChild = pcrsOuterOutput->ContainsAll(pcrsUsedInner);
	BOOL fOuterKeysUsesInnerChild = pcrsInnerOutput->ContainsAll(pcrsUsedOuter);

	if ((fOuterKeysUsesOuterChild && fInnerKeysUsesInnerChild) ||
		(fInnerKeysUsesOuterChild && fOuterKeysUsesInnerChild))
	{
		CGroupProxy gp(pgroupScalar);

		pgroupScalar->PdrgpexprHashJoinKeysOuter()->AddRef();
		pgroupScalar->PdrgpexprHashJoinKeysInner()->AddRef();

		// align hash join keys with join child
		if (fOuterKeysUsesOuterChild && fInnerKeysUsesInnerChild)
		{
			*ppdrgpexprOuter = pgroupScalar->PdrgpexprHashJoinKeysOuter();
			*ppdrgpexprInner = pgroupScalar->PdrgpexprHashJoinKeysInner();
		}
		else
		{
			GPOS_ASSERT(fInnerKeysUsesOuterChild && fOuterKeysUsesInnerChild);

			*ppdrgpexprOuter = pgroupScalar->PdrgpexprHashJoinKeysInner();
			*ppdrgpexprInner = pgroupScalar->PdrgpexprHashJoinKeysOuter();
		}
	}

	pcrsUsedOuter->Release();
	pcrsUsedInner->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::CacheHashJoinKeys
//
//	@doc:
//		Cache hash join keys on scalar child group
//
//---------------------------------------------------------------------------
void
CXformUtils::CacheHashJoinKeys
	(
	CExpression *pexpr,
	DrgPexpr *pdrgpexprOuter,
	DrgPexpr *pdrgpexprInner
	)
{
	GPOS_ASSERT(NULL != pdrgpexprOuter);
	GPOS_ASSERT(NULL != pdrgpexprInner);

	CGroupExpression *pgexprScalarOrigin = (*pexpr)[2]->Pgexpr();
	if (NULL != pgexprScalarOrigin)
	{
		CGroup *pgroupScalar = pgexprScalarOrigin->Pgroup();

		{	// scope of group proxy
			CGroupProxy gp(pgroupScalar);
			gp.SetHashJoinKeys(pdrgpexprOuter, pdrgpexprInner);
		}
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::AddCTEProducer
//
//	@doc:
//		Helper to create a CTE producer expression and add it to global
//		CTE info structure
//		Does not take ownership of pexpr
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprAddCTEProducer
	(
	IMemoryPool *memory_pool,
	ULONG ulCTEId,
	DrgPcr *colref_array,
	CExpression *pexpr
	)
{
	DrgPcr *pdrgpcrProd = CUtils::PdrgpcrCopy(memory_pool, colref_array);
	UlongColRefHashMap *colref_mapping = CUtils::PhmulcrMapping(memory_pool, colref_array, pdrgpcrProd);
	CExpression *pexprRemapped = pexpr->PexprCopyWithRemappedColumns(memory_pool, colref_mapping, true /*must_exist*/);
	colref_mapping->Release();

	CExpression *pexprProducer =
			GPOS_NEW(memory_pool) CExpression
							(
							memory_pool,
							GPOS_NEW(memory_pool) CLogicalCTEProducer(memory_pool, ulCTEId, pdrgpcrProd),
							pexprRemapped
							);

	CCTEInfo *pcteinfo = COptCtxt::PoctxtFromTLS()->Pcteinfo();
	pcteinfo->AddCTEProducer(pexprProducer);
	pexprProducer->Release();

	return pcteinfo->PexprCTEProducer(ulCTEId);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::FProcessGPDBAntiSemiHashJoin
//
//	@doc:
//		Helper to extract equality from an expression tree of the form
//		OP
//		 |--(=)
//		 |	 |-- expr1
//		 |	 +-- expr2
//		 +--exprOther
//
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FExtractEquality
	(
	CExpression *pexpr,
	CExpression **ppexprEquality, // output: extracted equality expression, set to NULL if extraction failed
	CExpression **ppexprOther // output: sibling of equality expression, set to NULL if extraction failed
	)
{
	GPOS_ASSERT(2 == pexpr->Arity());

	*ppexprEquality = NULL;
	*ppexprOther = NULL;

	CExpression *pexprLeft = (*pexpr)[0];
	CExpression *pexprRight = (*pexpr)[1];
	BOOL fEqualityOnLeft = CPredicateUtils::IsEqualityOp(pexprLeft);
	BOOL fEqualityOnRight = CPredicateUtils::IsEqualityOp(pexprRight);
	if (fEqualityOnLeft || fEqualityOnRight)
	{
		*ppexprEquality = pexprLeft;
		*ppexprOther = pexprRight;
		if (fEqualityOnRight)
		{
			*ppexprEquality = pexprRight;
			*ppexprOther = pexprLeft;
		}

		return true;
	}

	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::FProcessGPDBAntiSemiHashJoin
//
//	@doc:
//		GPDB hash join return no results if the inner side of anti-semi-join
//		produces null values, this allows simplifying join predicates of the
//		form (equality_expr IS DISTINCT FROM false) to (equality_expr) since
//		GPDB hash join operator guarantees no join results to be returned in
//		this case
//
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FProcessGPDBAntiSemiHashJoin
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	CExpression **ppexprResult // output: result expression, set to NULL if processing failed
	)
{
	GPOS_ASSERT(NULL != ppexprResult);
	GPOS_ASSERT(COperator::EopLogicalLeftAntiSemiJoin == pexpr->Pop()->Eopid() ||
				COperator::EopLogicalLeftAntiSemiJoinNotIn == pexpr->Pop()->Eopid());

	*ppexprResult = NULL;
	CExpression *pexprOuter = (*pexpr)[0];
	CExpression *pexprInner = (*pexpr)[1];
	CExpression *pexprScalar = (*pexpr)[2];

	DrgPexpr *pdrgpexpr = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pexprScalar);
	DrgPexpr *pdrgpexprNew = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	const ULONG ulPreds = pdrgpexpr->Size();
	BOOL fSimplifiedPredicate = false;
	for (ULONG ul = 0; ul < ulPreds; ul++)
	{
		CExpression *pexprPred = (*pdrgpexpr)[ul];
		if (CPredicateUtils::FIDFFalse(pexprPred))
		{
			CExpression *pexprEquality = NULL;
			CExpression *pexprFalse = NULL;
			if (FExtractEquality(pexprPred, &pexprEquality, &pexprFalse) &&  // extracted equality expression
				IMDId::EmdidGPDB == CScalarConst::PopConvert(pexprFalse->Pop())->GetDatum()->MDId()->MdidType() && // underlying system is GPDB
				CPhysicalJoin::FHashJoinCompatible(pexprEquality, pexprOuter, pexprInner) && // equality is hash-join compatible
				CUtils::FUsesNullableCol(memory_pool, pexprEquality, pexprInner)) // equality uses an inner nullable column
				{
					pexprEquality->AddRef();
					pdrgpexprNew->Append(pexprEquality);
					fSimplifiedPredicate = true;
					continue;
				}
		}
		pexprPred->AddRef();
		pdrgpexprNew->Append(pexprPred);
	}

	pdrgpexpr->Release();
	if (!fSimplifiedPredicate)
	{
		pdrgpexprNew->Release();
		return false;
	}

	pexprOuter->AddRef();
	pexprInner->AddRef();
	pexpr->Pop()->AddRef();
	*ppexprResult = GPOS_NEW(memory_pool) 		CExpression
			(
			memory_pool,
			pexpr->Pop(),
			pexprOuter,
			pexprInner,
			CPredicateUtils::PexprConjunction(memory_pool, pdrgpexprNew)
			);

	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprBuildIndexPlan
//
//	@doc:
//		Construct an expression representing a new access path using the given functors for
//		operator constructors and rewritten access path.
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprBuildIndexPlan
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CExpression *pexprGet,
	ULONG ulOriginOpId,
	DrgPexpr *pdrgpexprConds,
	CColRefSet *pcrsReqd,
	CColRefSet *pcrsScalarExpr,
	CColRefSet *outer_refs,
	const IMDIndex *pmdindex,
	const IMDRelation *pmdrel,
	BOOL fAllowPartialIndex,
	CPartConstraint *ppartcnstrIndex,
	IMDIndex::EmdindexType emdindtype,
	PDynamicIndexOpConstructor pdiopc,
	PStaticIndexOpConstructor psiopc,
	PRewrittenIndexPath prip
	)
{
	GPOS_ASSERT(NULL != pexprGet);
	GPOS_ASSERT(NULL != pdrgpexprConds);
	GPOS_ASSERT(NULL != pcrsReqd);
	GPOS_ASSERT(NULL != pcrsScalarExpr);
	GPOS_ASSERT(NULL != pmdindex);
	GPOS_ASSERT(NULL != pmdrel);

	COperator::EOperatorId op_id = pexprGet->Pop()->Eopid();
	GPOS_ASSERT(CLogical::EopLogicalGet == op_id || CLogical::EopLogicalDynamicGet == op_id);

	BOOL fDynamicGet = (COperator::EopLogicalDynamicGet == op_id);
	GPOS_ASSERT_IMP(!fDynamicGet, NULL == ppartcnstrIndex);

	CTableDescriptor *ptabdesc = NULL;
	DrgPcr *pdrgpcrOutput = NULL;
	CWStringConst *alias = NULL;
	ULONG ulPartIndex = gpos::ulong_max;
	DrgDrgPcr *pdrgpdrgpcrPart = NULL;
	BOOL fPartialIndex = pmdrel->IsPartialIndex(pmdindex->MDId());
	ULONG ulSecondaryPartIndex = gpos::ulong_max;
	CPartConstraint *ppartcnstrRel = NULL;

	if (!fAllowPartialIndex && fPartialIndex)
	{
		CRefCount::SafeRelease(ppartcnstrIndex);

		// partial indexes are not allowed
		return NULL;
	}

	if (fDynamicGet)
	{
		CLogicalDynamicGet *popDynamicGet = CLogicalDynamicGet::PopConvert(pexprGet->Pop());

		ptabdesc = popDynamicGet->Ptabdesc();
		ulPartIndex = popDynamicGet->ScanId();
		pdrgpcrOutput = popDynamicGet->PdrgpcrOutput();
		GPOS_ASSERT(NULL != pdrgpcrOutput);
		alias = GPOS_NEW(memory_pool) CWStringConst(memory_pool, popDynamicGet->Name().Pstr()->GetBuffer());
		pdrgpdrgpcrPart = popDynamicGet->PdrgpdrgpcrPart();
		ulSecondaryPartIndex = popDynamicGet->UlSecondaryScanId();
		ppartcnstrRel = popDynamicGet->PpartcnstrRel();
	}
	else
	{
		CLogicalGet *popGet = CLogicalGet::PopConvert(pexprGet->Pop());
		ptabdesc = popGet->Ptabdesc();
		pdrgpcrOutput = popGet->PdrgpcrOutput();
		GPOS_ASSERT(NULL != pdrgpcrOutput);
		alias = GPOS_NEW(memory_pool) CWStringConst(memory_pool, popGet->Name().Pstr()->GetBuffer());
	}

	if (!FIndexApplicable(memory_pool, pmdindex, pmdrel, pdrgpcrOutput, pcrsReqd, pcrsScalarExpr, emdindtype))
	{
		GPOS_DELETE(alias);
		CRefCount::SafeRelease(ppartcnstrIndex);

		return NULL;
	}

	DrgPcr *pdrgppcrIndexCols = PdrgpcrIndexKeys(memory_pool, pdrgpcrOutput, pmdindex, pmdrel);
	DrgPexpr *pdrgpexprIndex = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	DrgPexpr *pdrgpexprResidual = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	CPredicateUtils::ExtractIndexPredicates(memory_pool, md_accessor, pdrgpexprConds, pmdindex, pdrgppcrIndexCols, pdrgpexprIndex, pdrgpexprResidual, outer_refs);

	if (0 == pdrgpexprIndex->Size())
	{
		// clean up
		GPOS_DELETE(alias);
		pdrgppcrIndexCols->Release();
		pdrgpexprResidual->Release();
		pdrgpexprIndex->Release();
		CRefCount::SafeRelease(ppartcnstrIndex);

		return NULL;
	}
	GPOS_ASSERT(pdrgpexprConds->Size() == pdrgpexprResidual->Size() + pdrgpexprIndex->Size());

	ptabdesc->AddRef();
	pdrgpcrOutput->AddRef();
	// create the logical (dynamic) bitmap table get operator
	CLogical *popLogicalGet =	NULL;

	if (fDynamicGet)
	{
		pdrgpdrgpcrPart->AddRef();
		ppartcnstrRel->AddRef();
		popLogicalGet = (*pdiopc)
						(
						memory_pool,
						pmdindex,
						ptabdesc,
						ulOriginOpId,
						GPOS_NEW(memory_pool) CName(memory_pool, CName(alias)),
						ulPartIndex,
						pdrgpcrOutput,
						pdrgpdrgpcrPart,
						ulSecondaryPartIndex,
						ppartcnstrIndex,
						ppartcnstrRel
						);
	}
	else
	{
		popLogicalGet = (*psiopc)
						(
						memory_pool,
						pmdindex,
						ptabdesc,
						ulOriginOpId,
						GPOS_NEW(memory_pool) CName(memory_pool, CName(alias)),
						pdrgpcrOutput
						);
	}

	// clean up
	GPOS_DELETE(alias);
	pdrgppcrIndexCols->Release();

	CExpression *pexprIndexCond = CPredicateUtils::PexprConjunction(memory_pool, pdrgpexprIndex);
	CExpression *pexprResidualCond = CPredicateUtils::PexprConjunction(memory_pool, pdrgpexprResidual);

	return (*prip)(memory_pool, pexprIndexCond, pexprResidualCond, pmdindex, ptabdesc, popLogicalGet);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprScalarBitmapBoolOp
//
//	@doc:
//		 Helper for creating BitmapBoolOp expression
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprScalarBitmapBoolOp
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CExpression *pexprOriginalPred,
	DrgPexpr *pdrgpexpr,
	CTableDescriptor *ptabdesc,
	const IMDRelation *pmdrel,
	DrgPcr *pdrgpcrOutput,
	CColRefSet *outer_refs,
	CColRefSet *pcrsReqd,
	BOOL fConjunction,
	CExpression **ppexprRecheck,
	CExpression **ppexprResidual

	)
{	
	GPOS_ASSERT(NULL != pdrgpexpr); 
		
	const ULONG ulPredicates = pdrgpexpr->Size();
	
	if (1 == ulPredicates)
	{
		return PexprBitmap
				(
				memory_pool,
				md_accessor,
				pexprOriginalPred, 
				(*pdrgpexpr)[0], 
				ptabdesc, 
				pmdrel, 
				pdrgpcrOutput,
				outer_refs,
				pcrsReqd, 
				!fConjunction, 
				ppexprRecheck, 
				ppexprResidual
				);
	}
	
	// array of recheck predicates
	DrgPexpr *pdrgpexprRecheckNew = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	
	// array of residual predicates
	DrgPexpr *pdrgpexprResidualNew = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
			
	// array of bitmap index probe/bitmap bool op expressions
	DrgPexpr *pdrgpexprBitmap = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	
	CreateBitmapIndexProbeOps
		(
		memory_pool,
		md_accessor,
		pexprOriginalPred, 
		pdrgpexpr, 
		ptabdesc, 
		pmdrel, 
		pdrgpcrOutput,
		outer_refs,
		pcrsReqd, 
		fConjunction, 
		pdrgpexprBitmap, 
		pdrgpexprRecheckNew, 
		pdrgpexprResidualNew
		);

	GPOS_ASSERT(pdrgpexprRecheckNew->Size() == pdrgpexprBitmap->Size());

	const ULONG ulBitmapExpr = pdrgpexprBitmap->Size();

	if (0 == ulBitmapExpr || (!fConjunction && ulBitmapExpr < ulPredicates))
	{
		// no relevant bitmap indexes found,
		// or expression is a disjunction and some disjuncts don't have applicable bitmap indexes
		pdrgpexprBitmap->Release();
		pdrgpexprRecheckNew->Release();
		pdrgpexprResidualNew->Release();
		return NULL;
	}
	
	
	CExpression *pexprBitmapBoolOp = (*pdrgpexprBitmap)[0];
	pexprBitmapBoolOp->AddRef();
	IMDId *pmdidBitmap = CScalar::PopConvert(pexprBitmapBoolOp->Pop())->MDIdType();
	
	for (ULONG ul = 1; ul < ulBitmapExpr; ul++)
	{
		CExpression *pexprBitmap = (*pdrgpexprBitmap)[ul];
		pexprBitmap->AddRef();
		pmdidBitmap->AddRef();

		pexprBitmapBoolOp = PexprBitmapBoolOp(memory_pool, pmdidBitmap, pexprBitmapBoolOp, pexprBitmap, fConjunction);
	}
	

	GPOS_ASSERT(NULL != pexprBitmapBoolOp && 0 < pdrgpexprRecheckNew->Size());
		
	CExpression *pexprRecheckNew = CPredicateUtils::PexprConjDisj(memory_pool, pdrgpexprRecheckNew, fConjunction);
	if (NULL != *ppexprRecheck)
	{
		CExpression *pexprRecheckNewCombined = CPredicateUtils::PexprConjDisj(memory_pool, *ppexprRecheck, pexprRecheckNew, fConjunction);
		(*ppexprRecheck)->Release();
		pexprRecheckNew->Release();
		*ppexprRecheck = pexprRecheckNewCombined;
	}
	else 
	{
		*ppexprRecheck = pexprRecheckNew;
	}
	
	if (0 < pdrgpexprResidualNew->Size())
	{
		ComputeBitmapTableScanResidualPredicate(memory_pool, fConjunction, pexprOriginalPred, ppexprResidual, pdrgpexprResidualNew);
	}
	
	pdrgpexprResidualNew->Release();
	
	// cleanup
	pdrgpexprBitmap->Release();	

	return pexprBitmapBoolOp;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::ComputeBitmapTableScanResidualPredicate
//
//	@doc:
//		Compute the residual predicate for a bitmap table scan
//
//---------------------------------------------------------------------------
void
CXformUtils::ComputeBitmapTableScanResidualPredicate
	(
	IMemoryPool *memory_pool,
	BOOL fConjunction,
	CExpression *pexprOriginalPred,
	CExpression **ppexprResidual, // input-output argument: the residual predicate computed so-far, and resulting predicate
	DrgPexpr *pdrgpexprResidualNew
	)
{
	GPOS_ASSERT(NULL != pexprOriginalPred);
	GPOS_ASSERT(0 < pdrgpexprResidualNew->Size());
	
	if (!fConjunction)
	{
		// one of the disjuncts requires a residual predicate: we need to reevaluate the original predicate
		// for example, for index keys ik1 and ik2, the following will require re-evaluating
		// the whole predicate rather than just k < 100:
		// ik1 = 1 or (ik2=2 and k<100)
		pexprOriginalPred->AddRef();
		CRefCount::SafeRelease(*ppexprResidual);
		*ppexprResidual = pexprOriginalPred;
		return;
	}

	pdrgpexprResidualNew->AddRef();
	CExpression *pexprResidualNew = CPredicateUtils::PexprConjDisj(memory_pool, pdrgpexprResidualNew, fConjunction);
	
	if (NULL != *ppexprResidual)
	{
		CExpression *pexprResidualNewCombined = CPredicateUtils::PexprConjDisj(memory_pool, *ppexprResidual, pexprResidualNew, fConjunction);
		(*ppexprResidual)->Release();
		pexprResidualNew->Release();
		*ppexprResidual = pexprResidualNewCombined;	
	}
	else 
	{
		*ppexprResidual = pexprResidualNew;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprBitmapBoolOp
//
//	@doc:
//		Construct a bitmap bool op expression between the given bitmap access
// 		path expressions
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprBitmapBoolOp
	(
	IMemoryPool *memory_pool,
	IMDId *pmdidBitmapType, 
	CExpression *pexprLeft,
	CExpression *pexprRight,
	BOOL fConjunction
	)
{
	GPOS_ASSERT(NULL != pexprLeft);
	GPOS_ASSERT(NULL != pexprRight);
	
	CScalarBitmapBoolOp::EBitmapBoolOp ebitmapboolop = CScalarBitmapBoolOp::EbitmapboolAnd;
	
	if (!fConjunction)
	{
		ebitmapboolop = CScalarBitmapBoolOp::EbitmapboolOr;
	}
	
	return GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CScalarBitmapBoolOp(memory_pool, ebitmapboolop, pmdidBitmapType),
				pexprLeft,
				pexprRight
				);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprConditionOnBoolColumn
//
//	@doc:
// 		Creates a condition of the form col = m_bytearray_value, where col is the given column.
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprEqualityOnBoolColumn
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	BOOL value,
	CColRef *colref
	)
{
	CExpression *pexprConstBool =
			CUtils::PexprScalarConstBool(memory_pool, value, false /*is_null*/);

	const IMDTypeBool *pmdtype = md_accessor->PtMDType<IMDTypeBool>();
	IMDId *mdid_op = pmdtype->GetMdidForCmpType(IMDType::EcmptEq);
	mdid_op->AddRef();

	const CMDName mdname = md_accessor->RetrieveScOp(mdid_op)->Mdname();
	CWStringConst strOpName(mdname.GetMDName()->GetBuffer());

	return CUtils::PexprScalarCmp
					(
					memory_pool,
					colref,
					pexprConstBool,
					strOpName,
					mdid_op
					);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprConditionOnBoolColumn
//
//	@doc:
//		Construct a bitmap index path expression for the given predicate
//		out of the children of the given expression.
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprBitmapFromChildren
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CExpression *pexprOriginalPred,
	CExpression *pexprPred,
	CTableDescriptor *ptabdesc,
	const IMDRelation *pmdrel,
	DrgPcr *pdrgpcrOutput,
	CColRefSet *outer_refs,
	CColRefSet *pcrsReqd,
	BOOL fConjunction,
	CExpression **ppexprRecheck,
	CExpression **ppexprResidual
	)
{
	DrgPexpr *pdrgpexpr = NULL;

	if (fConjunction)
	{
		pdrgpexpr = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pexprPred);
	}
	else
	{
		pdrgpexpr = CPredicateUtils::PdrgpexprDisjuncts(memory_pool, pexprPred);
	}

	if (1 == pdrgpexpr->Size())
	{
		// unsupported predicate that cannot be split further into conjunctions and disjunctions
		pdrgpexpr->Release();
		return NULL;
	}

	// expression is a deeper tree: recurse further in each of the components
	CExpression *pexprResult = PexprScalarBitmapBoolOp
								(
								memory_pool,
								md_accessor,
								pexprOriginalPred,
								pdrgpexpr,
								ptabdesc,
								pmdrel,
								pdrgpcrOutput,
								outer_refs,
								pcrsReqd,
								fConjunction,
								ppexprRecheck,
								ppexprResidual
								);
	pdrgpexpr->Release();

	return pexprResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprBitmapCondToUse
//
//	@doc:
//		Returns the recheck condition to use in a bitmap index scan computed
//		out of the expression 'pexprPred' that uses the bitmap index.
// 		fBoolColumn (and fNegatedColumn) say whether the predicate is a
// 		(negated) boolean scalar identifier.
// 		Caller takes ownership of the returned expression
//
//---------------------------------------------------------------------------

CExpression *
CXformUtils::PexprBitmapCondToUse
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CExpression *pexprPred,
	BOOL fBoolColumn,
	BOOL fNegatedBoolColumn,
	CColRefSet *pcrsScalar
	)
{
	GPOS_ASSERT(!fBoolColumn || !fNegatedBoolColumn);
	if (fBoolColumn || fNegatedBoolColumn)
	{
		return
			PexprEqualityOnBoolColumn(memory_pool, md_accessor, fBoolColumn, pcrsScalar->PcrFirst());
	}
	pexprPred->AddRef();

	return pexprPred;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprBitmap
//
//	@doc:
//		Construct a bitmap index path expression for the given predicate
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprBitmap
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CExpression *pexprOriginalPred,
	CExpression *pexprPred, 
	CTableDescriptor *ptabdesc,
	const IMDRelation *pmdrel,
	DrgPcr *pdrgpcrOutput,
	CColRefSet *outer_refs,
	CColRefSet *pcrsReqd,
	BOOL fConjunction,
	CExpression **ppexprRecheck,
	CExpression **ppexprResidual
	)
{	
	GPOS_ASSERT(NULL == *ppexprRecheck);
	GPOS_ASSERT(NULL == *ppexprResidual);

	BOOL fBoolColumn = (COperator::EopScalarIdent == pexprPred->Pop()->Eopid());
	BOOL fNegatedBoolColumn = (!fBoolColumn && CPredicateUtils::FNotIdent(pexprPred));

	// if the expression is not of the form
	// <ident> = <const> or <ident> or NOT <ident>
	// then look for index access paths in its children
	if (!CPredicateUtils::FCompareIdentToConst(pexprPred) &&
		!fBoolColumn && !fNegatedBoolColumn)
	{
		// first, check if it is an index lookup predicate
		CExpression *pexprBitmapForIndexLookup = PexprBitmapForIndexLookup
												(
												memory_pool,
												md_accessor,
												pexprPred,
												ptabdesc,
												pmdrel,
												pdrgpcrOutput,
												outer_refs,
												pcrsReqd,
												ppexprRecheck
												);
		if (NULL != pexprBitmapForIndexLookup)
		{
			return pexprBitmapForIndexLookup;
		}

		CExpression *pexprBitmapFromChildren = PexprBitmapFromChildren
											(
											memory_pool,
											md_accessor,
											pexprOriginalPred,
											pexprPred,
											ptabdesc,
											pmdrel,
											pdrgpcrOutput,
											outer_refs,
											pcrsReqd,
											fConjunction,
											ppexprRecheck,
											ppexprResidual
											);
		if (NULL != pexprBitmapFromChildren)
		{
			return pexprBitmapFromChildren;
		}
	}

	// predicate is of the form col op const (or boolean col): find an applicable bitmap index
	return PexprBitmapForSelectCondition
				(
				memory_pool,
				md_accessor,
				pexprPred,
				ptabdesc,
				pmdrel,
				pdrgpcrOutput,
				pcrsReqd,
				ppexprRecheck,
				fBoolColumn,
				fNegatedBoolColumn
				);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprBitmapForSelectCondition
//
//	@doc:
//		Construct a bitmap index path expression for the given predicate coming
//		from a condition without outer references
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprBitmapForSelectCondition
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CExpression *pexprPred,
	CTableDescriptor *ptabdesc,
	const IMDRelation *pmdrel,
	DrgPcr *pdrgpcrOutput,
	CColRefSet *pcrsReqd,
	CExpression **ppexprRecheck,
	BOOL fBoolColumn,
	BOOL fNegatedBoolColumn
	)
{
	CColRefSet *pcrsScalar = CDrvdPropScalar::GetDrvdScalarProps(pexprPred->PdpDerive())->PcrsUsed();

	const ULONG ulIndexes = pmdrel->IndexCount();
	for (ULONG ul = 0; ul < ulIndexes; ul++)
	{
		const IMDIndex *pmdindex = md_accessor->RetrieveIndex(pmdrel->IndexMDidAt(ul));
		
		if (!pmdrel->IsPartialIndex(pmdindex->MDId()) && CXformUtils::FIndexApplicable
									(
									memory_pool,
									pmdindex,
									pmdrel,
									pdrgpcrOutput,
									pcrsReqd,
									pcrsScalar,
									IMDIndex::EmdindBitmap
									))
		{
			// found an applicable index
			CExpression *pexprCondToUse = PexprBitmapCondToUse
										(
										memory_pool,
										md_accessor,
										pexprPred,
										fBoolColumn,
										fNegatedBoolColumn,
										pcrsScalar
										);
			DrgPexpr *pdrgpexprScalar = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pexprCondToUse);
			DrgPcr *pdrgpcrIndexCols = PdrgpcrIndexKeys(memory_pool, pdrgpcrOutput, pmdindex, pmdrel);
			DrgPexpr *pdrgpexprIndex = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
			DrgPexpr *pdrgpexprResidual = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);

			CPredicateUtils::ExtractIndexPredicates
				(
				memory_pool,
				md_accessor,
				pdrgpexprScalar,
				pmdindex,
				pdrgpcrIndexCols,
				pdrgpexprIndex,
				pdrgpexprResidual,
				NULL  // pcrsAcceptedOuterRefs
				);
			pdrgpexprScalar->Release();
			pdrgpexprResidual->Release();
			if (0 == pdrgpexprIndex->Size())
			{
				// no usable predicate, clean up
				pdrgpcrIndexCols->Release();
				pdrgpexprIndex->Release();
				pexprCondToUse->Release();
				continue;
			}

			BOOL fCompatible =
					CPredicateUtils::FCompatiblePredicates(pdrgpexprIndex, pmdindex, pdrgpcrIndexCols, md_accessor);
			pdrgpcrIndexCols->Release();

			if (!fCompatible)
			{
				pdrgpexprIndex->Release();
				pexprCondToUse->Release();
				continue;
			}

			*ppexprRecheck = pexprCondToUse;
			CIndexDescriptor *pindexdesc = CIndexDescriptor::Pindexdesc(memory_pool, ptabdesc, pmdindex);
			pmdindex->GetIndexRetItemTypeMdid()->AddRef();
			return 	GPOS_NEW(memory_pool) CExpression
							(
							memory_pool,
							GPOS_NEW(memory_pool) CScalarBitmapIndexProbe(memory_pool, pindexdesc, pmdindex->GetIndexRetItemTypeMdid()),
							pdrgpexprIndex
							);
		}
	}

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprBitmapForIndexLookup
//
//	@doc:
//		Construct a bitmap index path expression for the given predicate coming
//		from a condition with outer references that could potentially become
//		an index lookup
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprBitmapForIndexLookup
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CExpression *pexprPred,
	CTableDescriptor *ptabdesc,
	const IMDRelation *pmdrel,
	DrgPcr *pdrgpcrOutput,
	CColRefSet *outer_refs,
	CColRefSet *pcrsReqd,
	CExpression **ppexprRecheck
	)
{
	if (NULL == outer_refs || 0 == outer_refs->Size())
	{
		return NULL;
	}

	CColRefSet *pcrsScalar = CDrvdPropScalar::GetDrvdScalarProps(pexprPred->PdpDerive())->PcrsUsed();

	const ULONG ulIndexes = pmdrel->IndexCount();
	for (ULONG ul = 0; ul < ulIndexes; ul++)
	{
		const IMDIndex *pmdindex = md_accessor->RetrieveIndex(pmdrel->IndexMDidAt(ul));

		if (pmdrel->IsPartialIndex(pmdindex->MDId()) || !CXformUtils::FIndexApplicable
									(
									memory_pool,
									pmdindex,
									pmdrel,
									pdrgpcrOutput,
									pcrsReqd,
									pcrsScalar,
									IMDIndex::EmdindBitmap
									))
		{
			// skip heterogeneous indexes and indexes that do not match the predicate
			continue;
		}
		DrgPcr *pdrgpcrIndexCols = PdrgpcrIndexKeys(memory_pool, pdrgpcrOutput, pmdindex, pmdrel);
		// attempt building index lookup predicate
		CExpression *pexprLookupPred = CPredicateUtils::PexprIndexLookup
										(
										memory_pool,
										md_accessor,
										pexprPred,
										pmdindex,
										pdrgpcrIndexCols,
										outer_refs
										);
		if (NULL != pexprLookupPred &&
				CPredicateUtils::FCompatibleIndexPredicate(pexprLookupPred, pmdindex, pdrgpcrIndexCols, md_accessor))
		{
			pexprLookupPred->AddRef();
			*ppexprRecheck = pexprLookupPred;
			CIndexDescriptor *pindexdesc = CIndexDescriptor::Pindexdesc(memory_pool, ptabdesc, pmdindex);
			pmdindex->GetIndexRetItemTypeMdid()->AddRef();
			pdrgpcrIndexCols->Release();

			return 	GPOS_NEW(memory_pool) CExpression
							(
							memory_pool,
							GPOS_NEW(memory_pool) CScalarBitmapIndexProbe(memory_pool, pindexdesc, pmdindex->GetIndexRetItemTypeMdid()),
							pexprLookupPred
							);
		}

		pdrgpcrIndexCols->Release();
	}

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::CreateBitmapIndexProbeOps
//
//	@doc:
//		Given an array of predicate expressions, construct a bitmap access path
//		expression for each predicate and accumulate it in the pdrgpexprBitmap array
//
//---------------------------------------------------------------------------
void
CXformUtils::CreateBitmapIndexProbeOps
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	CExpression *pexprOriginalPred,
	DrgPexpr *pdrgpexpr, 
	CTableDescriptor *ptabdesc,
	const IMDRelation *pmdrel,
	DrgPcr *pdrgpcrOutput,
	CColRefSet *outer_refs,
	CColRefSet *pcrsReqd,
	BOOL fConjunction,
	DrgPexpr *pdrgpexprBitmap,
	DrgPexpr *pdrgpexprRecheck,
	DrgPexpr *pdrgpexprResidual
	)
{
	GPOS_ASSERT(NULL != pdrgpexpr);
	
	const ULONG ulPredicates = pdrgpexpr->Size();

	for (ULONG ul = 0; ul < ulPredicates; ul++)
	{
		CExpression *pexprPred = (*pdrgpexpr)[ul];
		CExpression *pexprRecheck = NULL;
		CExpression *pexprResidual = NULL;
		CExpression *pexprBitmap = CXformUtils::PexprBitmap
									(
									memory_pool,
									md_accessor,
									pexprOriginalPred,
									pexprPred,
									ptabdesc,
									pmdrel,
									pdrgpcrOutput,
									outer_refs,
									pcrsReqd,
									!fConjunction,
									&pexprRecheck,
									&pexprResidual
									);
		if (NULL != pexprBitmap)
		{
			GPOS_ASSERT(NULL != pexprRecheck);

			// if possible, merge this index scan with a previous index scan on the same index
			BOOL fAddedToPredicate = fConjunction && FMergeWithPreviousBitmapIndexProbe
													(
													memory_pool,
													pexprBitmap,
													pexprRecheck,
													pdrgpexprBitmap,
													pdrgpexprRecheck
													);
			if (NULL != pexprResidual)
			{
				pdrgpexprResidual->Append(pexprResidual);
			}
			if (fAddedToPredicate)
			{
				pexprBitmap->Release();
				pexprRecheck->Release();
			}
			else
			{
				pdrgpexprRecheck->Append(pexprRecheck);
				pdrgpexprBitmap->Append(pexprBitmap);
			}
		}
		else
		{
			pexprPred->AddRef();
			pdrgpexprResidual->Append(pexprPred);
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::FHasAmbiguousType
//
//	@doc:
//		Check if expression has a scalar node with ambiguous return type
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FHasAmbiguousType
	(
	CExpression *pexpr,
	CMDAccessor *md_accessor
	)
{
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(NULL != md_accessor);

	BOOL fAmbiguous = false;
	if (pexpr->Pop()->FScalar())
	{
		CScalar *popScalar = CScalar::PopConvert(pexpr->Pop());
		switch (popScalar->Eopid())
		{
			case COperator::EopScalarAggFunc:
				fAmbiguous = CScalarAggFunc::PopConvert(popScalar)->FHasAmbiguousReturnType();
				break;

			case COperator::EopScalarProjectList:
			case COperator::EopScalarProjectElement:
			case COperator::EopScalarSwitchCase:
				break; // these operators do not have valid return type

			default:
				IMDId *mdid = popScalar->MDIdType();
				if (NULL != mdid)
				{
					// check MD type of scalar node
					fAmbiguous = md_accessor->RetrieveType(mdid)->IsAmbiguous();
				}
		}
	}

	if (!fAmbiguous)
	{
		// recursively process children
		const ULONG arity = pexpr->Arity();
		for (ULONG ul = 0; !fAmbiguous && ul < arity; ul++)
		{
			CExpression *pexprChild = (*pexpr)[ul];
			fAmbiguous = FHasAmbiguousType(pexprChild, md_accessor);
		}
	}

	return fAmbiguous;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprSelect2BitmapBoolOp
//
//	@doc:
//		Transform a Select into a Bitmap(Dynamic)TableGet over BitmapBoolOp if
//		bitmap indexes exist
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprSelect2BitmapBoolOp
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);

	// extract components
	CExpression *pexprRelational = (*pexpr)[0];
	CExpression *pexprScalar = (*pexpr)[1];
	CLogical *popGet = CLogical::PopConvert(pexprRelational->Pop());

	CTableDescriptor *ptabdesc = CLogical::PtabdescFromTableGet(popGet);
	const ULONG ulIndices = ptabdesc->IndexCount();
	if (0 == ulIndices)
	{
		return NULL;
	}

	// derive the scalar and relational properties to build set of required columns
	CColRefSet *pcrsOutput = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput();
	CColRefSet *pcrsScalarExpr = CDrvdPropScalar::GetDrvdScalarProps(pexprScalar->PdpDerive())->PcrsUsed();

	CColRefSet *pcrsReqd = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrsReqd->Include(pcrsOutput);
	pcrsReqd->Include(pcrsScalarExpr);

	CExpression *pexprResult = PexprBitmapTableGet
								(
								memory_pool,
								popGet,
								pexpr->Pop()->UlOpId(),
								ptabdesc,
								pexprScalar,
								NULL,  // outer_refs
								pcrsReqd
								);
	pcrsReqd->Release();

	return pexprResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprBitmapTableGet
//
//	@doc:
//		Transform a Select into a Bitmap(Dynamic)TableGet over BitmapBoolOp if
//		bitmap indexes exist
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprBitmapTableGet
	(
	IMemoryPool *memory_pool,
	CLogical *popGet,
	ULONG ulOriginOpId,
	CTableDescriptor *ptabdesc,
	CExpression *pexprScalar,
	CColRefSet *outer_refs,
	CColRefSet *pcrsReqd
	)
{
	GPOS_ASSERT(COperator::EopLogicalGet == popGet->Eopid() || 
			COperator::EopLogicalDynamicGet == popGet->Eopid());
	
	BOOL fDynamicGet = (COperator::EopLogicalDynamicGet == popGet->Eopid());
	
	// array of expressions in the scalar expression
	DrgPexpr *pdrgpexpr = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pexprScalar);
	GPOS_ASSERT(0 < pdrgpexpr->Size());

	// find the indexes whose included columns meet the required columns
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDRelation *pmdrel = md_accessor->RetrieveRel(ptabdesc->MDId());

	GPOS_ASSERT(0 < pdrgpexpr->Size());
	
	DrgPcr *pdrgpcrOutput = CLogical::PdrgpcrOutputFromLogicalGet(popGet);
	GPOS_ASSERT(NULL != pdrgpcrOutput);
	
	CExpression *pexprRecheck = NULL;
	CExpression *pexprResidual = NULL;
	CExpression *pexprBitmap = PexprScalarBitmapBoolOp
				(
				memory_pool,
				md_accessor,
				pexprScalar,
				pdrgpexpr,
				ptabdesc,
				pmdrel,
				pdrgpcrOutput,
				outer_refs,
				pcrsReqd,
				true, // fConjunction
				&pexprRecheck,
				&pexprResidual
				);
	CExpression *pexprResult = NULL;

	if (NULL != pexprBitmap)
	{
		GPOS_ASSERT(NULL != pexprRecheck);
		ptabdesc->AddRef();
		pdrgpcrOutput->AddRef();
		
		CName *pname = 	GPOS_NEW(memory_pool) CName(memory_pool, CName(CLogical::NameFromLogicalGet(popGet).Pstr()));

		// create a bitmap table scan on top
		CLogical *popBitmapTableGet = NULL;
		
		if (fDynamicGet)
		{
			CLogicalDynamicGet *popDynamicGet = CLogicalDynamicGet::PopConvert(popGet);
			CPartConstraint *ppartcnstr = popDynamicGet->Ppartcnstr();
			ppartcnstr->AddRef();
			ppartcnstr->AddRef();
			popDynamicGet->PdrgpdrgpcrPart()->AddRef();
			popBitmapTableGet = GPOS_NEW(memory_pool) CLogicalDynamicBitmapTableGet
								(
								memory_pool,
								ptabdesc, 
								ulOriginOpId,
								pname, 
								popDynamicGet->ScanId(),
								pdrgpcrOutput,
								popDynamicGet->PdrgpdrgpcrPart(),
								popDynamicGet->UlSecondaryScanId(),
								false, // is_partial
								ppartcnstr,
								ppartcnstr
								);
		}
		else
		{
			popBitmapTableGet = GPOS_NEW(memory_pool) CLogicalBitmapTableGet(memory_pool, ptabdesc, ulOriginOpId, pname, pdrgpcrOutput);
		}
		pexprResult = GPOS_NEW(memory_pool) CExpression
						(
						memory_pool,
						popBitmapTableGet,
						pexprRecheck,
						pexprBitmap
						);
		
		if (NULL != pexprResidual)
		{
			// add a selection on top with the residual condition
			pexprResult = GPOS_NEW(memory_pool) CExpression
							(
							memory_pool,
							GPOS_NEW(memory_pool) CLogicalSelect(memory_pool),
							pexprResult,
							pexprResidual
							);				
		}
	}
	
	// cleanup
	pdrgpexpr->Release();

	return pexprResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PdrgpdrgppartdigCandidates
//
//	@doc:
//		Find a set of partial index combinations
//
//---------------------------------------------------------------------------
DrgPdrgPpartdig *
CXformUtils::PdrgpdrgppartdigCandidates
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	DrgPexpr *pdrgpexprScalar,
	DrgDrgPcr *pdrgpdrgpcrPartKey,
	const IMDRelation *pmdrel,
	CPartConstraint *ppartcnstrRel,
	DrgPcr *pdrgpcrOutput,
	CColRefSet *pcrsReqd,
	CColRefSet *pcrsScalarExpr,
	CColRefSet *pcrsAcceptedOuterRefs
	)
{
	DrgPdrgPpartdig *pdrgpdrgppartdig = GPOS_NEW(memory_pool) DrgPdrgPpartdig(memory_pool);
	const ULONG ulIndexes = pmdrel->IndexCount();

	// currently covered parts
	CPartConstraint *ppartcnstrCovered = NULL;
	DrgPpartdig *pdrgppartdig = GPOS_NEW(memory_pool) DrgPpartdig(memory_pool);

	for (ULONG ul = 0; ul < ulIndexes; ul++)
	{
		const IMDIndex *pmdindex = md_accessor->RetrieveIndex(pmdrel->IndexMDidAt(ul));

		if (!CXformUtils::FIndexApplicable(memory_pool, pmdindex, pmdrel, pdrgpcrOutput, pcrsReqd, pcrsScalarExpr, IMDIndex::EmdindBtree /*emdindtype*/) ||
			!pmdrel->IsPartialIndex(pmdindex->MDId()))
		{
			// not a partial index (handled in another function), or index does not apply to predicate
			continue;
		}
		
		CPartConstraint *ppartcnstr = CUtils::PpartcnstrFromMDPartCnstr(memory_pool, md_accessor, pdrgpdrgpcrPartKey, pmdindex->MDPartConstraint(), pdrgpcrOutput);
		DrgPexpr *pdrgpexprIndex = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
		DrgPexpr *pdrgpexprResidual = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
		CPartConstraint *ppartcnstrNewlyCovered = PpartcnstrUpdateCovered
														(
														memory_pool,
														md_accessor,
														pdrgpexprScalar,
														ppartcnstrCovered,
														ppartcnstr,
														pdrgpcrOutput,
														pdrgpexprIndex,
														pdrgpexprResidual,
														pmdrel,
														pmdindex,
														pcrsAcceptedOuterRefs
														);

		if (NULL == ppartcnstrNewlyCovered)
		{
			ppartcnstr->Release();
			pdrgpexprResidual->Release();
			pdrgpexprIndex->Release();
			continue;
		}

		CRefCount::SafeRelease(ppartcnstrCovered);
		ppartcnstrCovered = ppartcnstrNewlyCovered;
		
		pdrgppartdig->Append(GPOS_NEW(memory_pool) SPartDynamicIndexGetInfo(pmdindex, ppartcnstr, pdrgpexprIndex, pdrgpexprResidual));
	}

	if (NULL != ppartcnstrCovered && !ppartcnstrRel->FEquivalent(ppartcnstrCovered))
	{
		pdrgpexprScalar->AddRef();
		SPartDynamicIndexGetInfo *ppartdig = PpartdigDynamicGet(memory_pool, pdrgpexprScalar, ppartcnstrCovered, ppartcnstrRel);
		if (NULL == ppartdig)
		{
			CRefCount::SafeRelease(ppartcnstrCovered);
			pdrgppartdig->Release();
			return pdrgpdrgppartdig;
		}

		pdrgppartdig->Append(ppartdig);
	}

	CRefCount::SafeRelease(ppartcnstrCovered);

	pdrgpdrgppartdig->Append(pdrgppartdig);
	return pdrgpdrgppartdig;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PpartcnstrUpdateCovered
//
//	@doc:
//		Compute the newly covered part constraint based on the old covered part
//		constraint and the given part constraint
//
//---------------------------------------------------------------------------
CPartConstraint *
CXformUtils::PpartcnstrUpdateCovered
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	DrgPexpr *pdrgpexprScalar,
	CPartConstraint *ppartcnstrCovered,
	CPartConstraint *ppartcnstr,
	DrgPcr *pdrgpcrOutput,
	DrgPexpr *pdrgpexprIndex,
	DrgPexpr *pdrgpexprResidual,
	const IMDRelation *pmdrel,
	const IMDIndex *pmdindex,
	CColRefSet *pcrsAcceptedOuterRefs
	)
{
	if (NULL == ppartcnstr->PcnstrCombined())
	{
		// unsupported constraint type: do not produce a partial index scan as we cannot reason about it
		return NULL;
	}

	if (NULL != ppartcnstrCovered && ppartcnstrCovered->FOverlap(memory_pool, ppartcnstr))
	{
		// index overlaps with already considered indexes: skip
		return NULL;
	}

	DrgPcr *pdrgpcrIndexCols = PdrgpcrIndexKeys(memory_pool, pdrgpcrOutput, pmdindex, pmdrel);
	CPredicateUtils::ExtractIndexPredicates
						(
						memory_pool,
						md_accessor,
						pdrgpexprScalar,
						pmdindex,
						pdrgpcrIndexCols,
						pdrgpexprIndex,
						pdrgpexprResidual,
						pcrsAcceptedOuterRefs
						);

	pdrgpcrIndexCols->Release();
	if (0 == pdrgpexprIndex->Size())
	{
		// no predicate could use the index: clean up
		return NULL;
	}

	return CXformUtils::PpartcnstrDisjunction(memory_pool, ppartcnstrCovered, ppartcnstr);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PpartcnstrDisjunction
//
//	@doc:
//		Compute a disjunction of two part constraints
//
//---------------------------------------------------------------------------
CPartConstraint *
CXformUtils::PpartcnstrDisjunction
	(
	IMemoryPool *memory_pool,
	CPartConstraint *ppartcnstrOld,
	CPartConstraint *ppartcnstrNew
	)
{
	GPOS_ASSERT(NULL != ppartcnstrNew);
	
	if (NULL == ppartcnstrOld)
	{
		ppartcnstrNew->AddRef();
		return ppartcnstrNew;
	}

	return CPartConstraint::PpartcnstrDisjunction(memory_pool, ppartcnstrOld, ppartcnstrNew);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PpartdigDynamicGet
//
//	@doc:
//		Create a dynamic table get candidate to cover the partitions not covered
//		by the partial index scans
//
//---------------------------------------------------------------------------
SPartDynamicIndexGetInfo *
CXformUtils::PpartdigDynamicGet
	(
	IMemoryPool *memory_pool,
	DrgPexpr *pdrgpexprScalar,
	CPartConstraint *ppartcnstrCovered,
	CPartConstraint *ppartcnstrRel
	)
{
	GPOS_ASSERT(!ppartcnstrCovered->IsConstraintUnbounded());
	CPartConstraint *ppartcnstrRest = ppartcnstrRel->PpartcnstrRemaining(memory_pool, ppartcnstrCovered);
	if (NULL == ppartcnstrRest)
	{
		return NULL;
	}

	return GPOS_NEW(memory_pool) SPartDynamicIndexGetInfo(NULL /*pmdindex*/, ppartcnstrRest, NULL /* pdrgpexprIndex */, pdrgpexprScalar);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprRemapColumns
//
//	@doc:
//		Remap the expression from the old columns to the new ones
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprRemapColumns
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	DrgPcr *pdrgpcrA,
	DrgPcr *pdrgpcrRemappedA,
	DrgPcr *pdrgpcrB,
	DrgPcr *pdrgpcrRemappedB
	)
{
	UlongColRefHashMap *colref_mapping = CUtils::PhmulcrMapping(memory_pool, pdrgpcrA, pdrgpcrRemappedA);
	GPOS_ASSERT_IMP(NULL == pdrgpcrB, NULL == pdrgpcrRemappedB);
	if (NULL != pdrgpcrB)
	{
		CUtils::AddColumnMapping(memory_pool, colref_mapping, pdrgpcrB, pdrgpcrRemappedB);
	}
	CExpression *pexprRemapped = pexpr->PexprCopyWithRemappedColumns(memory_pool, colref_mapping, true /*must_exist*/);
	colref_mapping->Release();

	return pexprRemapped;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprPartialDynamicIndexGet
//
//	@doc:
//		Create a dynamic index get plan for the given partial index
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprPartialDynamicIndexGet
	(
	IMemoryPool *memory_pool,
	CLogicalDynamicGet *popGet,
	ULONG ulOriginOpId,
	DrgPexpr *pdrgpexprIndex,
	DrgPexpr *pdrgpexprResidual,
	DrgPcr *pdrgpcrDIG,
	const IMDIndex *pmdindex,
	const IMDRelation *pmdrel,
	CPartConstraint *ppartcnstr,
	CColRefSet *pcrsAcceptedOuterRefs,
	DrgPcr *pdrgpcrOuter,
	DrgPcr *pdrgpcrNewOuter
	)
{
	GPOS_ASSERT_IMP(NULL == pdrgpcrOuter, NULL == pcrsAcceptedOuterRefs);
	GPOS_ASSERT_IMP(NULL != pdrgpcrOuter, NULL != pdrgpcrNewOuter);
	GPOS_ASSERT(NULL != pmdindex);
	GPOS_ASSERT(pmdrel->IsPartialIndex(pmdindex->MDId()));

	DrgPcr *pdrgpcrIndexCols = PdrgpcrIndexKeys(memory_pool, popGet->PdrgpcrOutput(), pmdindex, pmdrel);

	UlongColRefHashMap *colref_mapping = NULL;

	if (popGet->PdrgpcrOutput() != pdrgpcrDIG)
	{
		// columns need to be remapped
		colref_mapping = CUtils::PhmulcrMapping(memory_pool, popGet->PdrgpcrOutput(), pdrgpcrDIG);
	}

	CTableDescriptor *ptabdesc = popGet->Ptabdesc();
	ptabdesc->AddRef();

	CWStringConst strTableAliasName(memory_pool, popGet->Name().Pstr()->GetBuffer());

	DrgDrgPcr *pdrgpdrgpcrPart = NULL;
	CPartConstraint *ppartcnstrDIG = NULL;
	DrgPexpr *pdrgpexprIndexRemapped = NULL;
	DrgPexpr *pdrgpexprResidualRemapped = NULL;
	CPartConstraint *ppartcnstrRel = NULL;

	if (NULL != colref_mapping)
	{
		// if there are any outer references, add them to the mapping
		if (NULL != pcrsAcceptedOuterRefs)
		{
			ULONG ulOuterPcrs = pdrgpcrOuter->Size();
			GPOS_ASSERT(ulOuterPcrs == pdrgpcrNewOuter->Size());

			for (ULONG ul = 0; ul < ulOuterPcrs; ul++)
			{
				CColRef *pcrOld = (*pdrgpcrOuter)[ul];
				CColRef *new_colref = (*pdrgpcrNewOuter)[ul];
#ifdef GPOS_DEBUG
				BOOL fInserted =
#endif
				colref_mapping->Insert(GPOS_NEW(memory_pool) ULONG(pcrOld->Id()), new_colref);
				GPOS_ASSERT(fInserted);
			}
		}

		pdrgpdrgpcrPart = CUtils::PdrgpdrgpcrRemap(memory_pool, popGet->PdrgpdrgpcrPart(), colref_mapping, true /*must_exist*/);
		ppartcnstrDIG = ppartcnstr->PpartcnstrCopyWithRemappedColumns(memory_pool, colref_mapping, true /*must_exist*/);
		ppartcnstrRel = popGet->PpartcnstrRel()->PpartcnstrCopyWithRemappedColumns(memory_pool, colref_mapping, true /*must_exist*/);

		pdrgpexprIndexRemapped = CUtils::PdrgpexprRemap(memory_pool, pdrgpexprIndex, colref_mapping);
		pdrgpexprResidualRemapped = CUtils::PdrgpexprRemap(memory_pool, pdrgpexprResidual, colref_mapping);
	}
	else
	{
		popGet->PdrgpdrgpcrPart()->AddRef();
		ppartcnstr->AddRef();
		pdrgpexprIndex->AddRef();
		pdrgpexprResidual->AddRef();
		popGet->PpartcnstrRel()->AddRef();

		pdrgpdrgpcrPart = popGet->PdrgpdrgpcrPart();
		ppartcnstrDIG = ppartcnstr;
		pdrgpexprIndexRemapped = pdrgpexprIndex;
		pdrgpexprResidualRemapped = pdrgpexprResidual;
		ppartcnstrRel = popGet->PpartcnstrRel();
	}
	pdrgpcrDIG->AddRef();

	// create the logical index get operator
	CLogicalDynamicIndexGet *popIndexGet = GPOS_NEW(memory_pool) CLogicalDynamicIndexGet
												(
												memory_pool,
												pmdindex,
												ptabdesc,
												ulOriginOpId,
												GPOS_NEW(memory_pool) CName(memory_pool, CName(&strTableAliasName)),
												popGet->ScanId(),
												pdrgpcrDIG,
												pdrgpdrgpcrPart,
												COptCtxt::PoctxtFromTLS()->UlPartIndexNextVal(),
												ppartcnstrDIG,
												ppartcnstrRel
												);


	CExpression *pexprIndexCond = CPredicateUtils::PexprConjunction(memory_pool, pdrgpexprIndexRemapped);
	CExpression *pexprResidualCond = CPredicateUtils::PexprConjunction(memory_pool, pdrgpexprResidualRemapped);

	// cleanup
	CRefCount::SafeRelease(colref_mapping);
	pdrgpcrIndexCols->Release();

	// create the expression containing the logical index get operator
	return CUtils::PexprSafeSelect(memory_pool, GPOS_NEW(memory_pool) CExpression(memory_pool, popIndexGet, pexprIndexCond), pexprResidualCond);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::FJoinPredOnSingleChild
//
//	@doc:
//		Check if expression handle is attached to a Join expression
//		with a join predicate that uses columns from a single child
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FJoinPredOnSingleChild
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	GPOS_ASSERT(CUtils::FLogicalJoin(exprhdl.Pop()));

	const ULONG arity = exprhdl.Arity();
	if (0 == exprhdl.GetDrvdScalarProps(arity - 1)->PcrsUsed()->Size())
	{
		// no columns are used in join predicate
		return false;
	}

	// construct array of children output columns
	DrgPcrs *pdrgpcrs = GPOS_NEW(memory_pool) DrgPcrs(memory_pool);
	for (ULONG ul = 0; ul < arity - 1; ul++)
	{
		CColRefSet *pcrsOutput = exprhdl.GetRelationalProperties(ul)->PcrsOutput();
		pcrsOutput->AddRef();
		pdrgpcrs->Append(pcrsOutput);
	}

	DrgPexpr *pdrgpexprPreds = CPredicateUtils::PdrgpexprConjuncts(memory_pool, exprhdl.PexprScalarChild(arity- 1));
	const ULONG ulPreds = pdrgpexprPreds->Size();
	BOOL fPredUsesSingleChild = false;
	for (ULONG ulPred = 0; !fPredUsesSingleChild && ulPred < ulPreds; ulPred++)
	{
		CExpression *pexpr = (*pdrgpexprPreds)[ulPred];
		CColRefSet *pcrsUsed = CDrvdPropScalar::GetDrvdScalarProps(pexpr->PdpDerive())->PcrsUsed();
		for (ULONG ulChild = 0; !fPredUsesSingleChild && ulChild < arity - 1; ulChild++)
		{
			fPredUsesSingleChild = (*pdrgpcrs)[ulChild]->ContainsAll(pcrsUsed);
		}
	}
	pdrgpexprPreds->Release();
	pdrgpcrs->Release();

	return fPredUsesSingleChild;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprCTEConsumer
//
//	@doc:
//		Create a new CTE consumer for the given CTE id
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprCTEConsumer
	(
	IMemoryPool *memory_pool,
	ULONG ulCTEId,
	DrgPcr *colref_array
	)
{
	CLogicalCTEConsumer *popConsumer = GPOS_NEW(memory_pool) CLogicalCTEConsumer(memory_pool, ulCTEId, colref_array);
	COptCtxt::PoctxtFromTLS()->Pcteinfo()->IncrementConsumers(ulCTEId);

	return GPOS_NEW(memory_pool) CExpression(memory_pool, popConsumer);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PdrgpcrSubsequence
//
//	@doc:
//		Returns a new array containing the columns from the given column array 'colref_array'
//		at the positions indicated by the given ULONG array 'pdrgpulIndexesOfRefs'
//
//---------------------------------------------------------------------------
DrgPcr *
CXformUtils::PdrgpcrReorderedSubsequence
	(
	IMemoryPool *memory_pool,
	DrgPcr *colref_array,
	ULongPtrArray *pdrgpulIndexesOfRefs
	)
{
	GPOS_ASSERT(NULL != colref_array);
	GPOS_ASSERT(NULL != pdrgpulIndexesOfRefs);

	const ULONG length = pdrgpulIndexesOfRefs->Size();
	GPOS_ASSERT(length <= colref_array->Size());

	DrgPcr *pdrgpcrNewSubsequence = GPOS_NEW(memory_pool) DrgPcr(memory_pool);
	for (ULONG ul = 0; ul < length; ul++)
	{
		ULONG ulPos = *(*pdrgpulIndexesOfRefs)[ul];
		GPOS_ASSERT(ulPos < colref_array->Size());
		pdrgpcrNewSubsequence->Append((*colref_array)[ulPos]);
	}

	return pdrgpcrNewSubsequence;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::FMergeWithPreviousBitmapIndexProbe
//
//	@doc:
//		If there is already an index probe in pdrgpexprBitmap on the same
//		index as the given pexprBitmap, modify the existing index probe and
//		the corresponding recheck conditions to subsume pexprBitmap and
//		pexprRecheck respectively
//
//---------------------------------------------------------------------------
BOOL
CXformUtils::FMergeWithPreviousBitmapIndexProbe
	(
	IMemoryPool *memory_pool,
	CExpression *pexprBitmap,
	CExpression *pexprRecheck,
	DrgPexpr *pdrgpexprBitmap,
	DrgPexpr *pdrgpexprRecheck
	)
{
	if (COperator::EopScalarBitmapIndexProbe != pexprBitmap->Pop()->Eopid())
	{
		return false;
	}

	CScalarBitmapIndexProbe *popScalar = CScalarBitmapIndexProbe::PopConvert(pexprBitmap->Pop());
	IMDId *pmdIdIndex = popScalar->Pindexdesc()->MDId();
	const ULONG ulNumScalars = pdrgpexprBitmap->Size();
	for (ULONG ul = 0; ul < ulNumScalars; ul++)
	{
		CExpression *pexpr = (*pdrgpexprBitmap)[ul];
		COperator *pop = pexpr->Pop();
		if (COperator::EopScalarBitmapIndexProbe != pop->Eopid())
		{
			continue;
		}
		CScalarBitmapIndexProbe *popScalarBIP = CScalarBitmapIndexProbe::PopConvert(pop);
		if (!popScalarBIP->Pindexdesc()->MDId()->Equals(pmdIdIndex))
		{
			continue;
		}

		// create a new expression with the merged conjuncts and
		// replace the old expression in the expression array
		CExpression *pexprNew = CPredicateUtils::PexprConjunction(memory_pool, (*pexpr)[0], (*pexprBitmap)[0]);
		popScalarBIP->AddRef();
		CExpression *pexprBitmapNew = GPOS_NEW(memory_pool) CExpression(memory_pool, popScalarBIP, pexprNew);
		pdrgpexprBitmap->Replace(ul, pexprBitmapNew);

		CExpression *pexprRecheckNew =
				CPredicateUtils::PexprConjunction(memory_pool, (*pdrgpexprRecheck)[ul], pexprRecheck);
		pdrgpexprRecheck->Replace(ul, pexprRecheckNew);

		return true;
	}

	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprWinFuncAgg2ScalarAgg
//
//	@doc:
//		Converts an Agg window function into regular Agg
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprWinFuncAgg2ScalarAgg
	(
	IMemoryPool *memory_pool,
	CExpression *pexprWinFunc
	)
{
	GPOS_ASSERT(NULL != pexprWinFunc);
	GPOS_ASSERT(COperator::EopScalarWindowFunc == pexprWinFunc->Pop()->Eopid());

	DrgPexpr *pdrgpexprWinFuncArgs = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	const ULONG ulArgs = pexprWinFunc->Arity();
	for (ULONG ul = 0; ul < ulArgs; ul++)
	{
		CExpression *pexprArg = (*pexprWinFunc)[ul];
		pexprArg->AddRef();
		pdrgpexprWinFuncArgs->Append(pexprArg);
	}

	CScalarWindowFunc *popScWinFunc = CScalarWindowFunc::PopConvert(pexprWinFunc->Pop());
	IMDId *mdid_func = popScWinFunc->FuncMdId();

	mdid_func->AddRef();
	return
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			CUtils::PopAggFunc
				(
				memory_pool,
				mdid_func,
				GPOS_NEW(memory_pool) CWStringConst(memory_pool, popScWinFunc->PstrFunc()->GetBuffer()),
				popScWinFunc->IsDistinct(),
				EaggfuncstageGlobal,
				false // fSplit
				),
			pdrgpexprWinFuncArgs
		);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::MapPrjElemsWithDistinctAggs
//
//	@doc:
//		Given a project list, create a map whose key is the argument of
//		distinct Agg, and m_bytearray_value is the set of project elements that define
//		distinct Aggs on that argument,
//		non-distinct Aggs are grouped together in one set with key 'True',
//		for example,
//
//		Input: (x : count(distinct a),
//				y : sum(distinct a),
//				z : avg(distinct b),
//				w : max(c))
//
//		Output: (a --> {x,y},
//				 b --> {z},
//				 true --> {w})
//
//---------------------------------------------------------------------------
void
CXformUtils::MapPrjElemsWithDistinctAggs
	(
	IMemoryPool *memory_pool,
	CExpression *pexprPrjList,
	HMExprDrgPexpr **pphmexprdrgpexpr, // output: created map
	ULONG *pulDifferentDQAs // output: number of DQAs with different arguments
	)
{
	GPOS_ASSERT(NULL != pexprPrjList);
	GPOS_ASSERT(NULL != pphmexprdrgpexpr);
	GPOS_ASSERT(NULL != pulDifferentDQAs);

	HMExprDrgPexpr *phmexprdrgpexpr = GPOS_NEW(memory_pool) HMExprDrgPexpr(memory_pool);
	ULONG ulDifferentDQAs = 0;
	CExpression *pexprTrue = CUtils::PexprScalarConstBool(memory_pool, true /*m_bytearray_value*/);
	const ULONG arity = pexprPrjList->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprPrjEl = (*pexprPrjList)[ul];
		CExpression *pexprChild = (*pexprPrjEl)[0];
		COperator *popChild = pexprChild->Pop();
		COperator::EOperatorId eopidChild = popChild->Eopid();
		if (COperator::EopScalarAggFunc != eopidChild &&
			COperator::EopScalarWindowFunc != eopidChild)
		{
			continue;
		}

		BOOL is_distinct = false;
		if (COperator::EopScalarAggFunc == eopidChild)
		{
			is_distinct = CScalarAggFunc::PopConvert(popChild)->IsDistinct();
		}
		else
		{
			is_distinct = CScalarWindowFunc::PopConvert(popChild)->IsDistinct();
		}

		CExpression *pexprKey = NULL;
		if (is_distinct && 1 == pexprChild->Arity())
		{
			// use first argument of Distinct Agg as key
			pexprKey = (*pexprChild)[0];
		}
		else
		{
			// use constant True as key
			pexprKey = pexprTrue;
		}

		DrgPexpr *pdrgpexpr = const_cast<DrgPexpr *>(phmexprdrgpexpr->Find(pexprKey));
		BOOL fExists = (NULL != pdrgpexpr);
		if (!fExists)
		{
			// first occurrence, create a new expression array
			pdrgpexpr = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
		}
		pexprPrjEl->AddRef();
		pdrgpexpr->Append(pexprPrjEl);

		if (!fExists)
		{
			pexprKey->AddRef();
#ifdef 	GPOS_DEBUG
			BOOL fSuccess =
#endif // GPOS_DEBUG
				phmexprdrgpexpr->Insert(pexprKey, pdrgpexpr);
			GPOS_ASSERT(fSuccess);

			if (pexprKey != pexprTrue)
			{
				// first occurrence of a new DQA, increment counter
				ulDifferentDQAs++;
			}
		}
	}

	pexprTrue->Release();

	*pphmexprdrgpexpr = phmexprdrgpexpr;
	*pulDifferentDQAs = ulDifferentDQAs;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::ICmpPrjElemsArr
//
//	@doc:
//		Comparator used in sorting arrays of project elements
//		based on the column id of the first entry
//
//---------------------------------------------------------------------------
INT
CXformUtils::ICmpPrjElemsArr
	(
	const void *pvFst,
	const void *pvSnd
	)
{
	GPOS_ASSERT(NULL != pvFst);
	GPOS_ASSERT(NULL != pvSnd);

	const DrgPexpr *pdrgpexprFst = *(const DrgPexpr **) (pvFst);
	const DrgPexpr *pdrgpexprSnd = *(const DrgPexpr **) (pvSnd);

	CExpression *pexprPrjElemFst = (*pdrgpexprFst)[0];
	CExpression *pexprPrjElemSnd = (*pdrgpexprSnd)[0];
	ULONG ulIdFst = CScalarProjectElement::PopConvert(pexprPrjElemFst->Pop())->Pcr()->Id();
	ULONG ulIdSnd = CScalarProjectElement::PopConvert(pexprPrjElemSnd->Pop())->Pcr()->Id();

	if (ulIdFst < ulIdSnd)
	{
		return -1;
	}

	if (ulIdFst > ulIdSnd)
	{
		return 1;
	}

	return 0;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PdrgpdrgpexprSortedPrjElemsArray
//
//	@doc:
//		Iterate over given hash map and return array of arrays of project
//		elements sorted by the column id of the first entries
//
//---------------------------------------------------------------------------
DrgPdrgPexpr *
CXformUtils::PdrgpdrgpexprSortedPrjElemsArray
	(
	IMemoryPool *memory_pool,
	HMExprDrgPexpr *phmexprdrgpexpr
	)
{
	GPOS_ASSERT(NULL != phmexprdrgpexpr);

	DrgPdrgPexpr *pdrgpdrgpexprPrjElems = GPOS_NEW(memory_pool) DrgPdrgPexpr(memory_pool);
	HMExprDrgPexprIter hmexprdrgpexpriter(phmexprdrgpexpr);
	while (hmexprdrgpexpriter.Advance())
	{
		DrgPexpr *pdrgpexprPrjElems = const_cast<DrgPexpr *>(hmexprdrgpexpriter.Value());
		pdrgpexprPrjElems->AddRef();
		pdrgpdrgpexprPrjElems->Append(pdrgpexprPrjElems);
	}
	pdrgpdrgpexprPrjElems->Sort(ICmpPrjElemsArr);

	return pdrgpdrgpexprPrjElems;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUtils::PexprGbAggOnCTEConsumer2Join
//
//	@doc:
//		Convert GbAgg with distinct aggregates to a join expression
//
//		each leaf node of the resulting join expression is a GbAgg on a single
//		distinct aggs, we also create a GbAgg leaf for all remaining (non-distinct)
//		aggregates, for example
//
//		Input:
//			GbAgg_(count(distinct a), max(distinct a), sum(distinct b), avg(a))
//				+---CTEConsumer
//
//		Output:
//			InnerJoin
//				|--InnerJoin
//				|		|--GbAgg_(count(distinct a), max(distinct a))
//				|		|		+---CTEConsumer
//				|		+--GbAgg_(sum(distinct b))
//				|				+---CTEConsumer
//				+--GbAgg_(avg(a))
//						+---CTEConsumer
//
//---------------------------------------------------------------------------
CExpression *
CXformUtils::PexprGbAggOnCTEConsumer2Join
	(
	IMemoryPool *memory_pool,
	CExpression *pexprGbAgg
	)
{
	GPOS_ASSERT(NULL != pexprGbAgg);
	GPOS_ASSERT(COperator::EopLogicalGbAgg == pexprGbAgg->Pop()->Eopid());

	CLogicalGbAgg *popGbAgg = CLogicalGbAgg::PopConvert(pexprGbAgg->Pop());
	DrgPcr *pdrgpcrGrpCols = popGbAgg->Pdrgpcr();

	GPOS_ASSERT(popGbAgg->FGlobal());

	if (COperator::EopLogicalCTEConsumer != (*pexprGbAgg)[0]->Pop()->Eopid())
	{
		// child of GbAgg must be a CTE consumer

		return NULL;
	}

	CExpression *pexprPrjList = (*pexprGbAgg)[1];
	ULONG ulDistinctAggs = CDrvdPropScalar::GetDrvdScalarProps(pexprPrjList->PdpDerive())->UlDistinctAggs();

	if (1 == ulDistinctAggs)
	{
		// if only one distinct agg is used, return input expression
		pexprGbAgg->AddRef();
		return pexprGbAgg;
	}

	HMExprDrgPexpr *phmexprdrgpexpr = NULL;
	ULONG ulDifferentDQAs = 0;
	MapPrjElemsWithDistinctAggs(memory_pool, pexprPrjList, &phmexprdrgpexpr, &ulDifferentDQAs);
	if (1 == phmexprdrgpexpr->Size())
	{
		// if all distinct aggs use the same argument, return input expression
		phmexprdrgpexpr->Release();

		pexprGbAgg->AddRef();
		return pexprGbAgg;
	}

	CExpression *pexprCTEConsumer = (*pexprGbAgg)[0];
	CLogicalCTEConsumer *popConsumer = CLogicalCTEConsumer::PopConvert(pexprCTEConsumer->Pop());
	const ULONG ulCTEId = popConsumer->UlCTEId();
	DrgPcr *pdrgpcrConsumerOutput = popConsumer->Pdrgpcr();
	CCTEInfo *pcteinfo = COptCtxt::PoctxtFromTLS()->Pcteinfo();

	CExpression *pexprLastGbAgg = NULL;
	DrgPcr *pdrgpcrLastGrpCols = NULL;
	CExpression *pexprJoin = NULL;
	CExpression *pexprTrue = CUtils::PexprScalarConstBool(memory_pool, true /*m_bytearray_value*/);

	// iterate over map to extract sorted array of array of project elements,
	// we need to sort arrays here since hash map iteration is non-deterministic,
	// which may create non-deterministic ordering of join children leading to
	// changing the plan of the same query when run multiple times
	DrgPdrgPexpr *pdrgpdrgpexprPrjElems = PdrgpdrgpexprSortedPrjElemsArray(memory_pool, phmexprdrgpexpr);

	// counter of consumers
	ULONG ulConsumers = 0;

	const ULONG size = pdrgpdrgpexprPrjElems->Size();
	for (ULONG ulPrjElemsArr = 0; ulPrjElemsArr < size; ulPrjElemsArr++)
	{
		DrgPexpr *pdrgpexprPrjElems = (*pdrgpdrgpexprPrjElems)[ulPrjElemsArr];

		CExpression *pexprNewGbAgg = NULL;
		if (0 == ulConsumers)
		{
			// reuse input consumer
			pdrgpcrGrpCols->AddRef();
			pexprCTEConsumer->AddRef();
			pdrgpexprPrjElems->AddRef();
			pexprNewGbAgg =
				GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalGbAgg(memory_pool, pdrgpcrGrpCols, COperator::EgbaggtypeGlobal),
					pexprCTEConsumer,
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool), pdrgpexprPrjElems)
					);
		}
		else
		{
			// create a new consumer
			DrgPcr *pdrgpcrNewConsumerOutput = CUtils::PdrgpcrCopy(memory_pool, pdrgpcrConsumerOutput);
			CExpression *pexprNewConsumer =
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CLogicalCTEConsumer(memory_pool, ulCTEId, pdrgpcrNewConsumerOutput));
			pcteinfo->IncrementConsumers(ulCTEId);

			// fix Aggs arguments to use new consumer output column
			UlongColRefHashMap *colref_mapping = CUtils::PhmulcrMapping(memory_pool, pdrgpcrConsumerOutput, pdrgpcrNewConsumerOutput);
			DrgPexpr *pdrgpexprNewPrjElems = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
			const ULONG ulPrjElems = pdrgpexprPrjElems->Size();
			for (ULONG ul = 0; ul < ulPrjElems; ul++)
			{
				CExpression *pexprPrjEl = (*pdrgpexprPrjElems)[ul];

				// to match requested columns upstream, we have to re-use the same computed
				// columns that define the aggregates, we avoid creating new columns during
				// expression copy by passing must_exist as false
				CExpression *pexprNewPrjEl = pexprPrjEl->PexprCopyWithRemappedColumns(memory_pool, colref_mapping, false /*must_exist*/);
				pdrgpexprNewPrjElems->Append(pexprNewPrjEl);
			}

			// re-map grouping columns
			DrgPcr *pdrgpcrNewGrpCols = CUtils::PdrgpcrRemap(memory_pool, pdrgpcrGrpCols, colref_mapping, true /*must_exist*/);

			// create new GbAgg expression
			pexprNewGbAgg =
				GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalGbAgg(memory_pool, pdrgpcrNewGrpCols, COperator::EgbaggtypeGlobal),
					pexprNewConsumer,
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool), pdrgpexprNewPrjElems)
					);

			colref_mapping->Release();
		}

		ulConsumers++;

		DrgPcr *pdrgpcrNewGrpCols  = CLogicalGbAgg::PopConvert(pexprNewGbAgg->Pop())->Pdrgpcr();
		if (NULL != pexprLastGbAgg)
		{
			CExpression *pexprJoinCondition = NULL;
			if (0 == pdrgpcrLastGrpCols->Size())
			{
				GPOS_ASSERT(0 == pdrgpcrNewGrpCols->Size());

				pexprTrue->AddRef();
				pexprJoinCondition = pexprTrue;
			}
			else
			{
				GPOS_ASSERT(pdrgpcrLastGrpCols->Size() == pdrgpcrNewGrpCols->Size());

				pexprJoinCondition = CPredicateUtils::PexprINDFConjunction(memory_pool, pdrgpcrLastGrpCols, pdrgpcrNewGrpCols);
			}

			if (NULL == pexprJoin)
			{
				// create first join
				pexprJoin = CUtils::PexprLogicalJoin<CLogicalInnerJoin>(memory_pool, pexprLastGbAgg, pexprNewGbAgg, pexprJoinCondition);
			}
			else
			{
				// cascade joins
				pexprJoin = CUtils::PexprLogicalJoin<CLogicalInnerJoin>(memory_pool, pexprJoin, pexprNewGbAgg, pexprJoinCondition);
			}
		}

		pexprLastGbAgg = pexprNewGbAgg;
		pdrgpcrLastGrpCols = pdrgpcrNewGrpCols;
	}

	pdrgpdrgpexprPrjElems->Release();
	phmexprdrgpexpr->Release();
	pexprTrue->Release();

	return pexprJoin;
}

// EOF
