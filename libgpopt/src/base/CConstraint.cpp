//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CConstraint.cpp
//
//	@doc:
//		Implementation of constraints
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/common/CAutoRef.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/IColConstraintsMapper.h"
#include "gpopt/base/CColConstraintsArrayMapper.h"
#include "gpopt/base/CColConstraintsHashMapper.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CConstraint.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/base/CConstraintConjunction.h"
#include "gpopt/base/CConstraintDisjunction.h"
#include "gpopt/base/CConstraintNegation.h"
#include "gpopt/operators/CScalarIdent.h"
#include "gpopt/operators/CScalarArrayCmp.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/operators/CPredicateUtils.h"

using namespace gpopt;

// initialize constant true
BOOL CConstraint::m_fTrue(true);

// initialize constant false
BOOL CConstraint::m_fFalse(false);

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::CConstraint
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CConstraint::CConstraint
	(
	IMemoryPool *memory_pool
	)
	:
	m_phmcontain(NULL),
	m_memory_pool(memory_pool),
	m_pcrsUsed(NULL),
	m_pexprScalar(NULL)
{
	m_phmcontain = GPOS_NEW(m_memory_pool) HMConstraintContainment(m_memory_pool);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::~CConstraint
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CConstraint::~CConstraint()
{
	CRefCount::SafeRelease(m_pexprScalar);
	m_phmcontain->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrFromScalarArrayCmp
//
//	@doc:
//		Create constraint from scalar array comparison expression
//		originally generated for "scalar op ANY/ALL (array)" construct
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrFromScalarArrayCmp
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	CColRef *colref
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(CUtils::FScalarArrayCmp(pexpr));

	CScalarArrayCmp *popScArrayCmp = CScalarArrayCmp::PopConvert(pexpr->Pop());
	CScalarArrayCmp::EArrCmpType earrccmpt = popScArrayCmp->Earrcmpt();

	if ((CScalarArrayCmp::EarrcmpAny == earrccmpt  || CScalarArrayCmp::EarrcmpAll == earrccmpt) &&
		CPredicateUtils::FCompareIdentToConstArray(pexpr))
	{
		// column
#ifdef GPOS_DEBUG
		CScalarIdent *popScId = CScalarIdent::PopConvert((*pexpr)[0]->Pop());
		GPOS_ASSERT (colref == (CColRef *) popScId->Pcr());
#endif // GPOS_DEBUG

		// get comparison type
		IMDType::ECmpType cmp_type = CUtils::ParseCmpType(popScArrayCmp->MdIdOp());

		if (IMDType::EcmptOther == cmp_type )
		{
			// unsupported comparison operator for constraint derivation

			return NULL;
		}
		CExpression *pexprArray = CUtils::PexprScalarArrayChild(pexpr);

		const ULONG arity = CUtils::UlScalarArrayArity(pexprArray);

		// When array size exceeds the constraint derivation threshold,
		// don't expand it into a DNF and don't derive constraints
		COptimizerConfig *optimizer_config = COptCtxt::PoctxtFromTLS()->GetOptimizerConfig();
		ULONG array_expansion_threshold = optimizer_config->GetHint()->UlArrayExpansionThreshold();

		if (arity > array_expansion_threshold)
		{
			return NULL;
		}

		ConstraintArray *pdrgpcnstr = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);

		for (ULONG ul = 0; ul < arity; ul++)
		{
			CScalarConst *popScConst = CUtils::PScalarArrayConstChildAt(pexprArray,ul);
			CConstraintInterval *pci =  CConstraintInterval::PciIntervalFromColConstCmp(memory_pool, colref, cmp_type, popScConst);
			pdrgpcnstr->Append(pci);
		}

		if (earrccmpt == CScalarArrayCmp::EarrcmpAny)
		{
			// predicate is of the form 'A IN (1,2,3)'
			// return a disjunction of ranges {[1,1], [2,2], [3,3]}
			return GPOS_NEW(memory_pool) CConstraintDisjunction(memory_pool, pdrgpcnstr);
		}

		// predicate is of the form 'A NOT IN (1,2,3)'
		// return a conjunctive negation on {[1,1], [2,2], [3,3]}
		return GPOS_NEW(memory_pool) CConstraintConjunction(memory_pool, pdrgpcnstr);
	}

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrFromScalarExpr
//
//	@doc:
//		Create constraint from scalar expression and pass back any discovered
//		equivalence classes
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrFromScalarExpr
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	ColRefSetArray **ppdrgpcrs // output equivalence classes
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(pexpr->Pop()->FScalar());
	GPOS_ASSERT(NULL != ppdrgpcrs);
	GPOS_ASSERT(NULL == *ppdrgpcrs);

	(void) pexpr->PdpDerive();
	CDrvdPropScalar *pdpScalar = CDrvdPropScalar::GetDrvdScalarProps(pexpr->Pdp(DrvdPropArray::EptScalar));

	CColRefSet *pcrs = pdpScalar->PcrsUsed();
	ULONG num_cols = pcrs->Size();

	if (0 == num_cols)
	{
		// TODO:  - May 29, 2012: in case of an expr with no columns (e.g. 1 < 2),
		// possibly evaluate the expression, and return a "TRUE" or "FALSE" constraint
		return NULL;
	}

	if (1 == num_cols)
	{
		CColRef *colref = pcrs->PcrFirst();
		if (!CUtils::FConstrainableType(colref->RetrieveType()->MDId()))
		{
			return NULL;
		}

		CConstraint *pcnstr = NULL;
		*ppdrgpcrs = GPOS_NEW(memory_pool) ColRefSetArray(memory_pool);

		// first, try creating a single interval constraint from the expression
		pcnstr = CConstraintInterval::PciIntervalFromScalarExpr(memory_pool, pexpr, colref);
		if (NULL == pcnstr && CUtils::FScalarArrayCmp(pexpr))
		{
			// if the interval creation failed, try creating a disjunction or conjunction
			// of several interval constraints in the array case
			pcnstr = PcnstrFromScalarArrayCmp(memory_pool, pexpr, colref);
		}

		if (NULL != pcnstr)
		{
			AddColumnToEquivClasses(memory_pool, colref, ppdrgpcrs);
		}
		return pcnstr;
	}

	switch (pexpr->Pop()->Eopid())
	{
		case COperator::EopScalarBoolOp:
			return PcnstrFromScalarBoolOp(memory_pool, pexpr, ppdrgpcrs);

		case COperator::EopScalarCmp:
			return PcnstrFromScalarCmp(memory_pool, pexpr, ppdrgpcrs);

		default:
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrConjunction
//
//	@doc:
//		Create conjunction from array of constraints
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrConjunction
	(
	IMemoryPool *memory_pool,
	ConstraintArray *pdrgpcnstr
	)
{
	return PcnstrConjDisj(memory_pool, pdrgpcnstr, true /*fConj*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrDisjunction
//
//	@doc:
//		Create disjunction from array of constraints
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrDisjunction
	(
	IMemoryPool *memory_pool,
	ConstraintArray *pdrgpcnstr
	)
{
	return PcnstrConjDisj(memory_pool, pdrgpcnstr, false /*fConj*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrConjDisj
//
//	@doc:
//		Create conjunction/disjunction from array of constraints
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrConjDisj
	(
	IMemoryPool *memory_pool,
	ConstraintArray *pdrgpcnstr,
	BOOL fConj
	)
{
	GPOS_ASSERT(NULL != pdrgpcnstr);

	CConstraint *pcnstr = NULL;

	const ULONG length = pdrgpcnstr->Size();

	switch (length)
	{
		case 0:
		{
			pdrgpcnstr->Release();
			break;
		}

		case 1:
		{
			pcnstr = (*pdrgpcnstr)[0];
			pcnstr->AddRef();
			pdrgpcnstr->Release();
			break;
		}

		default:
		{
			if (fConj)
			{
				pcnstr = GPOS_NEW(memory_pool) CConstraintConjunction(memory_pool, pdrgpcnstr);
			}
			else
			{
				pcnstr = GPOS_NEW(memory_pool) CConstraintDisjunction(memory_pool, pdrgpcnstr);
			}
		}
	}

	return pcnstr;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::AddColumnToEquivClasses
//
//	@doc:
//		Add column as a new equivalence class, if it is not already in one of the
//		existing equivalence classes
//
//---------------------------------------------------------------------------
void
CConstraint::AddColumnToEquivClasses
	(
	IMemoryPool *memory_pool,
	const CColRef *colref,
	ColRefSetArray **ppdrgpcrs
	)
{
	const ULONG length = (*ppdrgpcrs)->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CColRefSet *pcrs = (**ppdrgpcrs)[ul];
		if (pcrs->FMember(colref))
		{
			return;
		}
	}

	CColRefSet *pcrsNew = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrsNew->Include(colref);

	(*ppdrgpcrs)->Append(pcrsNew);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrFromScalarCmp
//
//	@doc:
//		Create constraint from scalar comparison
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrFromScalarCmp
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	ColRefSetArray **ppdrgpcrs // output equivalence classes
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(CUtils::FScalarCmp(pexpr));
	GPOS_ASSERT(NULL != ppdrgpcrs);
	GPOS_ASSERT(NULL == *ppdrgpcrs);

	CExpression *pexprLeft = (*pexpr)[0];
	CExpression *pexprRight = (*pexpr)[1];

	// check if the scalar comparison is over scalar idents
	if (COperator::EopScalarIdent == pexprLeft->Pop()->Eopid()
		&& COperator::EopScalarIdent == pexprRight->Pop()->Eopid())
	{
		CScalarIdent *popScIdLeft = CScalarIdent::PopConvert((*pexpr)[0]->Pop());
		const CColRef *pcrLeft =  popScIdLeft->Pcr();

		CScalarIdent *popScIdRight = CScalarIdent::PopConvert((*pexpr)[1]->Pop());
		const CColRef *pcrRight =  popScIdRight->Pcr();

		if (!CUtils::FConstrainableType(pcrLeft->RetrieveType()->MDId()) ||
			!CUtils::FConstrainableType(pcrRight->RetrieveType()->MDId()))
		{
			return NULL;
		}

		*ppdrgpcrs = GPOS_NEW(memory_pool) ColRefSetArray(memory_pool);
		if (CPredicateUtils::IsEqualityOp(pexpr))
		{
			// col1 = col2
			CColRefSet *pcrsNew = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
			pcrsNew->Include(pcrLeft);
			pcrsNew->Include(pcrRight);

			(*ppdrgpcrs)->Append(pcrsNew);
		}

		// create NOT NULL constraints to both columns
		ConstraintArray *pdrgpcnstr = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);
		pdrgpcnstr->Append(CConstraintInterval::PciUnbounded(memory_pool, pcrLeft, false /*fIncludesNull*/));
		pdrgpcnstr->Append(CConstraintInterval::PciUnbounded(memory_pool, pcrRight, false /*fIncludesNull*/));
		return CConstraint::PcnstrConjunction(memory_pool, pdrgpcnstr);
	}

	// TODO: , May 28, 2012; add support for other cases besides (col cmp col)

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrFromScalarBoolOp
//
//	@doc:
//		Create constraint from scalar boolean expression
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrFromScalarBoolOp
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	ColRefSetArray **ppdrgpcrs // output equivalence classes
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(CUtils::FScalarBoolOp(pexpr));
	GPOS_ASSERT(NULL != ppdrgpcrs);
	GPOS_ASSERT(NULL == *ppdrgpcrs);

	const ULONG arity= pexpr->Arity();

	// Large IN/NOT IN lists that can not be converted into
	// CScalarArrayCmp, are expanded into its disjunctive normal form,
	// represented by a large boolean expression tree.
	// For instance constructs of the form:
	// "(expression1, expression2) scalar op ANY/ALL ((const-x1,const-y1), ... (const-xn,const-yn))"
	// Deriving constraints from this is quite expensive; hence don't
	// bother when the arity of OR exceeds the threshold
	COptimizerConfig *optimizer_config = COptCtxt::PoctxtFromTLS()->GetOptimizerConfig();
	ULONG array_expansion_threshold = optimizer_config->GetHint()->UlArrayExpansionThreshold();

	if (CPredicateUtils::FOr(pexpr) && arity > array_expansion_threshold)
	{
		return NULL;
	}

	*ppdrgpcrs = GPOS_NEW(memory_pool) ColRefSetArray(memory_pool);
	ConstraintArray *pdrgpcnstr = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);

	for (ULONG ul = 0; ul < arity; ul++)
	{
		ColRefSetArray *pdrgpcrsChild = NULL;
		CConstraint *pcnstrChild = PcnstrFromScalarExpr(memory_pool, (*pexpr)[ul], &pdrgpcrsChild);
		if (NULL == pcnstrChild || pcnstrChild->IsConstraintUnbounded())
		{
			CRefCount::SafeRelease(pcnstrChild);
			CRefCount::SafeRelease(pdrgpcrsChild);
			if (CPredicateUtils::FOr(pexpr))
			{
				pdrgpcnstr->Release();
				return NULL;
			}
			continue;
		}
		GPOS_ASSERT(NULL != pdrgpcrsChild);

		pdrgpcnstr->Append(pcnstrChild);
		ColRefSetArray *pdrgpcrsMerged = PdrgpcrsMergeFromBoolOp(memory_pool, pexpr, *ppdrgpcrs, pdrgpcrsChild);

		(*ppdrgpcrs)->Release();
		*ppdrgpcrs = pdrgpcrsMerged;
		pdrgpcrsChild->Release();
	}

	const ULONG length = pdrgpcnstr->Size();
	if (0 == length)
	{
		pdrgpcnstr->Release();
		return NULL;
	}

	if (1 == length)
	{
		CConstraint *pcnstrChild = (*pdrgpcnstr)[0];
		pcnstrChild->AddRef();
		pdrgpcnstr->Release();

		if (CPredicateUtils::FNot(pexpr))
		{
			return GPOS_NEW(memory_pool) CConstraintNegation(memory_pool, pcnstrChild);
		}

		return pcnstrChild;
	}

	// we know we have more than one child
	if (CPredicateUtils::FAnd(pexpr))
	{
		return GPOS_NEW(memory_pool) CConstraintConjunction(memory_pool, pdrgpcnstr);
	}

	if (CPredicateUtils::FOr(pexpr))
	{
		return GPOS_NEW(memory_pool) CConstraintDisjunction(memory_pool, pdrgpcnstr);
	}

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PdrgpcrsMergeFromBoolOp
//
//	@doc:
//		Merge equivalence classes coming from children of a bool op
//
//---------------------------------------------------------------------------
ColRefSetArray *
CConstraint::PdrgpcrsMergeFromBoolOp
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	ColRefSetArray *pdrgpcrsFst,
	ColRefSetArray *pdrgpcrsSnd
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(CUtils::FScalarBoolOp(pexpr));
	GPOS_ASSERT(NULL != pdrgpcrsFst);
	GPOS_ASSERT(NULL != pdrgpcrsSnd);

	if (CPredicateUtils::FAnd(pexpr))
	{
		// merge with the equivalence classes we have so far
		return CUtils::PdrgpcrsMergeEquivClasses(memory_pool, pdrgpcrsFst, pdrgpcrsSnd);
	}

	if (CPredicateUtils::FOr(pexpr))
	{
		// in case of an OR, an equivalence class must be coming from all
		// children to be part of the output
		return CUtils::PdrgpcrsIntersectEquivClasses(memory_pool, pdrgpcrsFst, pdrgpcrsSnd);
	}

	GPOS_ASSERT(CPredicateUtils::FNot(pexpr));
	return GPOS_NEW(memory_pool) ColRefSetArray(memory_pool);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PdrgpcnstrOnColumn
//
//	@doc:
//		Return a subset of the given constraints which reference the
//		given column
//
//---------------------------------------------------------------------------
ConstraintArray *
CConstraint::PdrgpcnstrOnColumn
	(
	IMemoryPool *memory_pool,
	ConstraintArray *pdrgpcnstr,
	CColRef *colref,
	BOOL fExclusive		// returned constraints must reference ONLY the given col
	)
{
	ConstraintArray *pdrgpcnstrSubset = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);

	const ULONG length = pdrgpcnstr->Size();

	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstr = (*pdrgpcnstr)[ul];
		CColRefSet *pcrs = pcnstr->PcrsUsed();

		// if the fExclusive flag is true, then colref must be the only column
		if (pcrs->FMember(colref) && (!fExclusive || 1 == pcrs->Size()))
		{
			pcnstr->AddRef();
			pdrgpcnstrSubset->Append(pcnstr);
		}
	}

	return pdrgpcnstrSubset;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PexprScalarConjDisj
//
//	@doc:
//		Construct a conjunction or disjunction scalar expression from an
//		array of constraints
//
//---------------------------------------------------------------------------
CExpression *
CConstraint::PexprScalarConjDisj
	(
	IMemoryPool *memory_pool,
	ConstraintArray *pdrgpcnstr,
	BOOL fConj
	)
	const
{
	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);

	const ULONG length = pdrgpcnstr->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CExpression *pexpr = (*pdrgpcnstr)[ul]->PexprScalar(memory_pool);
		pexpr->AddRef();
		pdrgpexpr->Append(pexpr);
	}

	if (fConj)
	{
		return CPredicateUtils::PexprConjunction(memory_pool, pdrgpexpr);
	}

	return CPredicateUtils::PexprDisjunction(memory_pool, pdrgpexpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PdrgpcnstrFlatten
//
//	@doc:
//		Flatten an array of constraints to be used as children for a conjunction
//		or disjunction. If any of these children is of the same type then use
//		its children directly instead of having multiple levels of the same type
//
//---------------------------------------------------------------------------
ConstraintArray *
CConstraint::PdrgpcnstrFlatten
	(
	IMemoryPool *memory_pool,
	ConstraintArray *pdrgpcnstr,
	EConstraintType ect
	)
	const
{
	ConstraintArray *pdrgpcnstrNew = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);

	const ULONG length = pdrgpcnstr->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstrChild = (*pdrgpcnstr)[ul];
		EConstraintType ectChild = pcnstrChild->Ect();

		if (EctConjunction == ectChild && EctConjunction == ect)
		{
			CConstraintConjunction *pcconj = (CConstraintConjunction *)pcnstrChild;
			CUtils::AddRefAppend<CConstraint, CleanupRelease>(pdrgpcnstrNew, pcconj->Pdrgpcnstr());
		}
		else if (EctDisjunction == ectChild && EctDisjunction == ect)
		{
			CConstraintDisjunction *pcdisj = (CConstraintDisjunction *)pcnstrChild;
			CUtils::AddRefAppend<CConstraint, CleanupRelease>(pdrgpcnstrNew, pcdisj->Pdrgpcnstr());
		}
		else
		{
			pcnstrChild->AddRef();
			pdrgpcnstrNew->Append(pcnstrChild);
		}
	}

	pdrgpcnstr->Release();
	return PdrgpcnstrDeduplicate(memory_pool, pdrgpcnstrNew, ect);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PdrgpcnstrDeduplicate
//
//	@doc:
//		Simplify an array of constraints to be used as children for a conjunction
//		or disjunction. If there are two or more elements that reference only one
//		particular column, these constraints are combined into one
//
//---------------------------------------------------------------------------
ConstraintArray *
CConstraint::PdrgpcnstrDeduplicate
	(
	IMemoryPool *memory_pool,
	ConstraintArray *pdrgpcnstr,
	EConstraintType ect
	)
	const
{
	ConstraintArray *pdrgpcnstrNew = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);

	CAutoRef<CColRefSet> pcrsDeduped(GPOS_NEW(memory_pool) CColRefSet(memory_pool));
	CAutoRef<IColConstraintsMapper> arccm;

	const ULONG length = pdrgpcnstr->Size();

	pdrgpcnstr->AddRef();
	if (length >= 5)
	{
		arccm = GPOS_NEW(memory_pool) CColConstraintsHashMapper(memory_pool, pdrgpcnstr);
	}
	else
	{
		arccm = GPOS_NEW(memory_pool) CColConstraintsArrayMapper(memory_pool, pdrgpcnstr);
	}

	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstrChild = (*pdrgpcnstr)[ul];
		CColRefSet *pcrs = pcnstrChild->PcrsUsed();

		// we only simplify constraints that reference a single column, otherwise
		// we add constraint as is
		if (1 < pcrs->Size())
		{
			pcnstrChild->AddRef();
			pdrgpcnstrNew->Append(pcnstrChild);
			continue;
		}

		CColRef *colref = pcrs->PcrFirst();
		if (pcrsDeduped->FMember(colref))
		{
			// current constraint has already been combined with a previous one
			continue;
		}

		ConstraintArray *pdrgpcnstrCol = arccm->PdrgPcnstrLookup(colref);

		if (1 == pdrgpcnstrCol->Size())
		{
			// if there is only one such constraint, then no simplification
			// for this column
			pdrgpcnstrCol->Release();
			pcnstrChild->AddRef();
			pdrgpcnstrNew->Append(pcnstrChild);
			continue;
		}

		CExpression *pexpr = NULL;

		if (EctConjunction == ect)
		{
			pexpr = PexprScalarConjDisj(memory_pool, pdrgpcnstrCol, true /*fConj*/);
		}
		else
		{
			GPOS_ASSERT(EctDisjunction == ect);
			pexpr = PexprScalarConjDisj(memory_pool, pdrgpcnstrCol, false /*fConj*/);
		}
		pdrgpcnstrCol->Release();
		GPOS_ASSERT(NULL != pexpr);

		CConstraint *pcnstrNew = CConstraintInterval::PciIntervalFromScalarExpr(memory_pool, pexpr, colref);
		GPOS_ASSERT(NULL != pcnstrNew);
		pexpr->Release();
		pdrgpcnstrNew->Append(pcnstrNew);
		pcrsDeduped->Include(colref);
	}

	pdrgpcnstr->Release();

	return pdrgpcnstrNew;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::Phmcolconstr
//
//	@doc:
//		Construct mapping between columns and arrays of constraints
//
//---------------------------------------------------------------------------
ColRefToConstraintArrayMap *
CConstraint::Phmcolconstr
	(
	IMemoryPool *memory_pool,
	CColRefSet *pcrs,
	ConstraintArray *pdrgpcnstr
	)
	const
{
	GPOS_ASSERT(NULL != m_pcrsUsed);

	ColRefToConstraintArrayMap *phmcolconstr = GPOS_NEW(memory_pool) ColRefToConstraintArrayMap(memory_pool);

	CColRefSetIter crsi(*pcrs);
	while (crsi.Advance())
	{
		CColRef *colref = crsi.Pcr();
		ConstraintArray *pdrgpcnstrCol = PdrgpcnstrOnColumn(memory_pool, pdrgpcnstr, colref, false /*fExclusive*/);

#ifdef GPOS_DEBUG
		BOOL fres =
#endif //GPOS_DEBUG
		phmcolconstr->Insert(colref, pdrgpcnstrCol);
		GPOS_ASSERT(fres);
	}

	return phmcolconstr;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PcnstrConjDisjRemapForColumn
//
//	@doc:
//		Return a copy of the conjunction/disjunction constraint for a different column
//
//---------------------------------------------------------------------------
CConstraint *
CConstraint::PcnstrConjDisjRemapForColumn
	(
	IMemoryPool *memory_pool,
	CColRef *colref,
	ConstraintArray *pdrgpcnstr,
	BOOL fConj
	)
	const
{
	GPOS_ASSERT(NULL != colref);

	ConstraintArray *pdrgpcnstrNew = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);

	const ULONG length = pdrgpcnstr->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		// clone child
		CConstraint *pcnstrChild = (*pdrgpcnstr)[ul]->PcnstrRemapForColumn(memory_pool, colref);
		GPOS_ASSERT(NULL != pcnstrChild);

		pdrgpcnstrNew->Append(pcnstrChild);
	}

	if (fConj)
	{
		return PcnstrConjunction(memory_pool, pdrgpcnstrNew);
	}
	return PcnstrDisjunction(memory_pool, pdrgpcnstrNew);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::Contains
//
//	@doc:
//		Does the current constraint contain the given one?
//
//---------------------------------------------------------------------------
BOOL
CConstraint::Contains
	(
	CConstraint *pcnstr
	)
{
	if (IsConstraintUnbounded())
	{
		return true;
	}

	if (NULL == pcnstr || pcnstr->IsConstraintUnbounded())
	{
		return false;
	}

	if (this == pcnstr)
	{
		// a constraint always contains itself
		return true;
	}

	// check if we have computed this containment query before
	BOOL *pfContains = m_phmcontain->Find(pcnstr);
	if (NULL != pfContains)
	{
		return *pfContains;
	}

	BOOL fContains = true;

	// for each column used by the current constraint, we have to make sure that
	// the constraint on this column contains the corresponding given constraint
	CColRefSetIter crsi(*m_pcrsUsed);
	while (fContains && crsi.Advance())
	{
		CColRef *colref = crsi.Pcr();
		CConstraint *pcnstrColThis = Pcnstr(m_memory_pool, colref);
		GPOS_ASSERT (NULL != pcnstrColThis);
		CConstraint *pcnstrColOther = pcnstr->Pcnstr(m_memory_pool, colref);

		// convert each of them to interval (if they are not already)
		CConstraintInterval *pciThis = CConstraintInterval::PciIntervalFromConstraint(m_memory_pool, pcnstrColThis, colref);
		CConstraintInterval *pciOther = CConstraintInterval::PciIntervalFromConstraint(m_memory_pool, pcnstrColOther, colref);

		fContains = pciThis->FContainsInterval(m_memory_pool, pciOther);
		pciThis->Release();
		pciOther->Release();
		pcnstrColThis->Release();
		CRefCount::SafeRelease(pcnstrColOther);
	}

	// insert containment query into the local map
#ifdef GPOS_DEBUG
	BOOL fSuccess =
#endif // GPOS_DEBUG
		m_phmcontain->Insert(pcnstr, PfVal(fContains));
	GPOS_ASSERT(fSuccess);

	return fContains;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::Equals
//
//	@doc:
//		Equality function
//
//---------------------------------------------------------------------------
BOOL
CConstraint::Equals
	(
	CConstraint *pcnstr
	)
{
	if (NULL == pcnstr || pcnstr->IsConstraintUnbounded())
	{
		return IsConstraintUnbounded();
	}

	// check for pointer equality first
	if (this == pcnstr)
	{
		return true;
	}

	return (this->Contains(pcnstr) && pcnstr->Contains(this));
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraint::PrintConjunctionDisjunction
//
//	@doc:
//		Common functionality for printing conjunctions and disjunctions
//
//---------------------------------------------------------------------------
IOstream &
CConstraint::PrintConjunctionDisjunction
	(
	IOstream &os,
	ConstraintArray *pdrgpcnstr
	)
	const
{
	EConstraintType ect = Ect();
	GPOS_ASSERT(EctConjunction == ect || EctDisjunction == ect);

	os << "(";
	const ULONG arity = pdrgpcnstr->Size();
	(*pdrgpcnstr)[0]->OsPrint(os);

	for (ULONG ul = 1; ul < arity; ul++)
	{
		if (EctConjunction == ect)
		{
			os << " AND ";
		}
		else
		{
			os << " OR ";
		}
		(*pdrgpcnstr)[ul]->OsPrint(os);
	}
	os << ")";

	return os;
}

#ifdef GPOS_DEBUG
void
CConstraint::DbgPrint() const
{
	CAutoTrace at(m_memory_pool);
	(void) this->OsPrint(at.Os());
}
#endif  // GPOS_DEBUG

// EOF
