//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CLogical.cpp
//
//	@doc:
//		Implementation of base class of logical operators
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "naucrates/md/IMDIndex.h"
#include "naucrates/md/IMDColumn.h"
#include "naucrates/md/IMDCheckConstraint.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CConstraintConjunction.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/base/CDrvdPropRelational.h"
#include "gpopt/base/CReqdPropRelational.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/base/CPartIndexMap.h"
#include "gpopt/base/COptCtxt.h"

#include "gpopt/operators/CLogical.h"
#include "gpopt/operators/CLogicalApply.h"
#include "gpopt/operators/CLogicalBitmapTableGet.h"
#include "gpopt/operators/CLogicalDynamicBitmapTableGet.h"
#include "gpopt/operators/CLogicalGet.h"
#include "gpopt/operators/CLogicalDynamicGet.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/CScalarIdent.h"

#include "gpopt/optimizer/COptimizerConfig.h"

#include "naucrates/statistics/CStatistics.h"
#include "naucrates/statistics/CStatisticsUtils.h"

using namespace gpnaucrates;
using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CLogical::CLogical
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogical::CLogical
	(
	IMemoryPool *memory_pool
	)
	:
	COperator(memory_pool)
{
	GPOS_ASSERT(NULL != memory_pool);
	m_pcrsLocalUsed = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::~CLogical
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogical::~CLogical()
{
	m_pcrsLocalUsed->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PdrgpcrCreateMapping
//
//	@doc:
//		Create output column mapping given a list of column descriptors and
//		a pointer to the operator creating that column
//
//---------------------------------------------------------------------------
ColRefArray *
CLogical::PdrgpcrCreateMapping
	(
	IMemoryPool *memory_pool,
	const ColumnDescrArray *pdrgpcoldesc,
	ULONG ulOpSourceId
	)
	const
{
	// get column factory from optimizer context object
	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	
	ULONG num_cols = pdrgpcoldesc->Size();
	
	ColRefArray *colref_array = GPOS_NEW(memory_pool) ColRefArray(memory_pool, num_cols);
	for(ULONG ul = 0; ul < num_cols; ul++)
	{
		CColumnDescriptor *pcoldesc = (*pdrgpcoldesc)[ul];
		CName name(memory_pool, pcoldesc->Name());
		
		CColRef *colref = col_factory->PcrCreate(pcoldesc, name, ulOpSourceId);
		colref_array->Append(colref);
	}
	
	return colref_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PdrgpdrgpcrCreatePartCols
//
//	@doc:
//		Initialize array of partition columns from the array with their indexes
//
//---------------------------------------------------------------------------
ColRefArrays *
CLogical::PdrgpdrgpcrCreatePartCols
	(
	IMemoryPool *memory_pool,
	ColRefArray *colref_array,
	const ULongPtrArray *pdrgpulPart
	)
{
	GPOS_ASSERT(NULL != colref_array && "Output columns cannot be NULL");
	GPOS_ASSERT(NULL != pdrgpulPart);
	
	ColRefArrays *pdrgpdrgpcrPart = GPOS_NEW(memory_pool) ColRefArrays(memory_pool);
	
	const ULONG ulPartCols = pdrgpulPart->Size();
	GPOS_ASSERT(0 < ulPartCols);
	
	for (ULONG ul = 0; ul < ulPartCols; ul++)
	{
		ULONG ulCol = *((*pdrgpulPart)[ul]);
		
		CColRef *colref = (*colref_array)[ulCol];
		ColRefArray * pdrgpcrCurr = GPOS_NEW(memory_pool) ColRefArray(memory_pool);
		pdrgpcrCurr->Append(colref);
		pdrgpdrgpcrPart->Append(pdrgpcrCurr);
	}
	
	return pdrgpdrgpcrPart;
}


//		Compute an order spec based on an index

COrderSpec *
CLogical::PosFromIndex
	(
	IMemoryPool *memory_pool,
	const IMDIndex *pmdindex,
	ColRefArray *colref_array,
	const CTableDescriptor *ptabdesc
	)
{
	//
	// compute the order spec after getting the current position of the index key
	// from the table descriptor. Index keys are relative to the
	// relation. So consider a case where we had 20 columns in a table. We
	// create an index that covers col # 20 as one of its keys. Then we drop
	// columns 10 through 15. Now the index key still points to col #20 but the
	// column ref list in colref_array will only have 15 elements in it.
	//

	COrderSpec *pos = GPOS_NEW(memory_pool) COrderSpec(memory_pool);
	const ULONG ulLenKeys = pmdindex->Keys();

	// get relation from the metadata accessor using metadata id
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDRelation *pmdrel = md_accessor->RetrieveRel(ptabdesc->MDId());

	for (ULONG  ul = 0; ul < ulLenKeys; ul++)
	{
		// This is the postion of the index key column relative to the relation
		const ULONG ulPosRel = pmdindex->KeyAt(ul);

		// get the column and it's attno from the relation
		const IMDColumn *pmdcol = pmdrel->GetMdCol(ulPosRel);
		INT attno = pmdcol->AttrNum();

		// get the position of the index key column relative to the table descriptor
		const ULONG ulPosTabDesc = ptabdesc->GetAttributePosition(attno);
		CColRef *colref = (*colref_array)[ulPosTabDesc];

		IMDId *mdid = colref->RetrieveType()->GetMdidForCmpType(IMDType::EcmptL);
		mdid->AddRef();
	
		// TODO:  March 27th 2012; we hard-code NULL treatment
		// need to revisit
		pos->Append(mdid, colref, COrderSpec::EntLast);
	}
	
	return pos;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PcrsDeriveOutputPassThru
//
//	@doc:
//		Common case of output derivation for unary operators or operators
//		that pass through the schema of only the outer child
//
//---------------------------------------------------------------------------
CColRefSet *
CLogical::PcrsDeriveOutputPassThru
	(
	CExpressionHandle &exprhdl
	)
{
	// may have additional children that are ignored, e.g., scalar children
	GPOS_ASSERT(1 <= exprhdl.Arity());
	
	CColRefSet *pcrs = exprhdl.GetRelationalProperties(0)->PcrsOutput();
	pcrs->AddRef();
	
	return pcrs;
}
	

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PcrsDeriveNotNullPassThruOuter
//
//	@doc:
//		Common case of deriving not null columns by passing through
//		not null columns from the outer child
//
//---------------------------------------------------------------------------
CColRefSet *
CLogical::PcrsDeriveNotNullPassThruOuter
	(
	CExpressionHandle &exprhdl
	)
{
	// may have additional children that are ignored, e.g., scalar children
	GPOS_ASSERT(1 <= exprhdl.Arity());

	CColRefSet *pcrs = exprhdl.GetRelationalProperties(0)->PcrsNotNull();
	pcrs->AddRef();

	return pcrs;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogical::PcrsDeriveOutputCombineLogical
//
//	@doc:
//		Common case of output derivation by combining the schemas of all
//		children
//
//---------------------------------------------------------------------------
CColRefSet *
CLogical::PcrsDeriveOutputCombineLogical
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	// union columns from the first N-1 children
	ULONG arity = exprhdl.Arity();
	for (ULONG ul = 0; ul < arity - 1; ul++)
	{
		CColRefSet *pcrsChild = exprhdl.GetRelationalProperties(ul)->PcrsOutput();
		GPOS_ASSERT(pcrs->IsDisjoint(pcrsChild) && "Input columns are not disjoint");

		pcrs->Union(pcrsChild);
	}

	return pcrs;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogical::PcrsDeriveNotNullCombineLogical
//
//	@doc:
//		Common case of combining not null columns from all logical
//		children
//
//---------------------------------------------------------------------------
CColRefSet *
CLogical::PcrsDeriveNotNullCombineLogical
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	// union not nullable columns from the first N-1 children
	ULONG arity = exprhdl.Arity();
	for (ULONG ul = 0; ul < arity - 1; ul++)
	{
		CColRefSet *pcrsChild = exprhdl.GetRelationalProperties(ul)->PcrsNotNull();
		GPOS_ASSERT(pcrs->IsDisjoint(pcrsChild) && "Input columns are not disjoint");

		pcrs->Union(pcrsChild);
	}
 
	return pcrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PpartinfoPassThruOuter
//
//	@doc:
//		Common case of common case of passing through partition consumer array
//
//---------------------------------------------------------------------------
CPartInfo *
CLogical::PpartinfoPassThruOuter
	(
	CExpressionHandle &exprhdl
	)
{
	CPartInfo *ppartinfo = exprhdl.GetRelationalProperties(0 /*child_index*/)->Ppartinfo();
	GPOS_ASSERT(NULL != ppartinfo);
	ppartinfo->AddRef();
	return ppartinfo;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogical::PkcCombineKeys
//
//	@doc:
//		Common case of combining keys from first n - 1 children
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogical::PkcCombineKeys
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	const ULONG arity = exprhdl.Arity();
	for (ULONG ul = 0; ul < arity - 1; ul++)
	{
		CKeyCollection *pkc = exprhdl.GetRelationalProperties(ul)->Pkc();
		if (NULL == pkc)
		{
			// if a child has no key, the operator has no key
			pcrs->Release();
			return NULL;
		}

		ColRefArray *colref_array = pkc->PdrgpcrKey(memory_pool);
		pcrs->Include(colref_array);
		colref_array->Release();
	}

	return GPOS_NEW(memory_pool) CKeyCollection(memory_pool, pcrs);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PkcKeysBaseTable
//
//	@doc:
//		Helper function for computing the keys in a base table
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogical::PkcKeysBaseTable
	(
	IMemoryPool *memory_pool,
	const BitSetArray *pdrgpbsKeys,
	const ColRefArray *pdrgpcrOutput
	)
{
	const ULONG ulKeys = pdrgpbsKeys->Size();
	
	if (0 == ulKeys)
	{
		return NULL;
	}

	CKeyCollection *pkc = GPOS_NEW(memory_pool) CKeyCollection(memory_pool);
	
	for (ULONG ul = 0; ul < ulKeys; ul++)
	{
		CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

		CBitSet *pbs = (*pdrgpbsKeys)[ul];
		CBitSetIter bsiter(*pbs);
		
		while (bsiter.Advance())
		{
			pcrs->Include((*pdrgpcrOutput)[bsiter.Bit()]);
		}

		pkc->Add(pcrs);
	}

	return pkc;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PpartinfoDeriveCombine
//
//	@doc:
//		Common case of combining partition info of all logical children
//
//---------------------------------------------------------------------------
CPartInfo *
CLogical::PpartinfoDeriveCombine
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	const ULONG arity = exprhdl.Arity();
	GPOS_ASSERT(0 < arity);

	CPartInfo *ppartinfo = GPOS_NEW(memory_pool) CPartInfo(memory_pool);
	
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CPartInfo *ppartinfoChild = NULL;
		if (exprhdl.FScalarChild(ul))
		{
			ppartinfoChild = exprhdl.GetDrvdScalarProps(ul)->Ppartinfo();
		}
		else
		{
			ppartinfoChild = exprhdl.GetRelationalProperties(ul)->Ppartinfo();
		}
		GPOS_ASSERT(NULL != ppartinfoChild);
		CPartInfo *ppartinfoCombined = CPartInfo::PpartinfoCombine(memory_pool, ppartinfo, ppartinfoChild);
		ppartinfo->Release();
		ppartinfo = ppartinfoCombined;
	}

	return ppartinfo;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PcrsDeriveOuter
//
//	@doc:
//		Derive outer references
//
//---------------------------------------------------------------------------
CColRefSet *
CLogical::PcrsDeriveOuter
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsUsedAdditional
	)
{
	ULONG arity = exprhdl.Arity();
	CColRefSet *outer_refs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	// collect output columns from relational children
	// and used columns from scalar children
	CColRefSet *pcrsOutput = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	CColRefSet *pcrsUsed = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	for (ULONG i = 0; i < arity; i++)
	{
		if (exprhdl.FScalarChild(i))
		{
			CDrvdPropScalar *pdpscalar = exprhdl.GetDrvdScalarProps(i);
			pcrsUsed->Union(pdpscalar->PcrsUsed());
		}
		else
		{
			CDrvdPropRelational *pdprel = exprhdl.GetRelationalProperties(i);
			pcrsOutput->Union(pdprel->PcrsOutput());

			// add outer references from relational children
			outer_refs->Union(pdprel->PcrsOuter());
		}
	}

	if (NULL != pcrsUsedAdditional)
	{
		pcrsUsed->Include(pcrsUsedAdditional);
	}

	// outer references are columns used by scalar child
	// but are not included in the output columns of relational children
	outer_refs->Union(pcrsUsed);
	outer_refs->Exclude(pcrsOutput);

	pcrsOutput->Release();
	pcrsUsed->Release();
	return outer_refs;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PcrsDeriveOuterIndexGet
//
//	@doc:
//		Derive outer references for index get and dynamic index get operators
//
//---------------------------------------------------------------------------
CColRefSet *
CLogical::PcrsDeriveOuterIndexGet
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	ULONG arity = exprhdl.Arity();
	CColRefSet *outer_refs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	CColRefSet *pcrsOutput = PcrsDeriveOutput(memory_pool, exprhdl);
	
	CColRefSet *pcrsUsed = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	for (ULONG i = 0; i < arity; i++)
	{
		GPOS_ASSERT(exprhdl.FScalarChild(i));
		CDrvdPropScalar *pdpscalar = exprhdl.GetDrvdScalarProps(i);
		pcrsUsed->Union(pdpscalar->PcrsUsed());
	}

	// outer references are columns used by scalar children
	// but are not included in the output columns of relational children
	outer_refs->Union(pcrsUsed);
	outer_refs->Exclude(pcrsOutput);

	pcrsOutput->Release();
	pcrsUsed->Release();
	return outer_refs;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PcrsDeriveCorrelatedApply
//
//	@doc:
//		Derive columns from the inner child of a correlated-apply expression
//		that can be used above the apply expression
//
//---------------------------------------------------------------------------
CColRefSet *
CLogical::PcrsDeriveCorrelatedApply
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
	const
{
	GPOS_ASSERT(this == exprhdl.Pop());

	ULONG arity = exprhdl.Arity();
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	if (CUtils::FCorrelatedApply(exprhdl.Pop()))
	{
		// add inner columns of correlated-apply expression
		pcrs->Include(CLogicalApply::PopConvert(exprhdl.Pop())->PdrgPcrInner());
	}

	// combine correlated-apply columns from logical children
	for (ULONG ul = 0; ul < arity; ul++)
	{
		if (!exprhdl.FScalarChild(ul))
		{
			CDrvdPropRelational *pdprel = exprhdl.GetRelationalProperties(ul);
			pcrs->Union(pdprel->PcrsCorrelatedApply());
		}
	}

	return pcrs;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogical::PkcDeriveKeysPassThru
//
//	@doc:
//		Addref and return keys of n-th child
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogical::PkcDeriveKeysPassThru
	(
	CExpressionHandle &exprhdl,
	ULONG ulChild
	)
{	
	CKeyCollection *pkcLeft = exprhdl.GetRelationalProperties(ulChild)->Pkc();
	
	// key collection may be NULL
	if (NULL != pkcLeft)
	{
		pkcLeft->AddRef();
	}
	
	return pkcLeft;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogical::PkcDeriveKeys
//
//	@doc:
//		Derive key collections
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogical::PkcDeriveKeys
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle & // exprhdl
	)
	const
{
	// no keys found by default
	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PpcDeriveConstraintFromPredicates
//
//	@doc:
//		Derive constraint property when expression has relational children and
//		scalar children (predicates)
//
//---------------------------------------------------------------------------
CPropConstraint *
CLogical::PpcDeriveConstraintFromPredicates
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	ColRefSetArray *pdrgpcrs = GPOS_NEW(memory_pool) ColRefSetArray(memory_pool);

	ConstraintArray *pdrgpcnstr = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);

	// collect constraint properties from relational children
	// and predicates from scalar children
	const ULONG arity = exprhdl.Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		if (exprhdl.FScalarChild(ul))
		{
			CExpression *pexprScalar = exprhdl.PexprScalarChild(ul);

			// make sure it is a predicate... boolop, cmp, nulltest
			if (NULL == pexprScalar || !CUtils::FPredicate(pexprScalar))
			{
				continue;
			}

			ColRefSetArray *pdrgpcrsChild = NULL;
			CConstraint *pcnstr = CConstraint::PcnstrFromScalarExpr(memory_pool, pexprScalar, &pdrgpcrsChild);
			if (NULL != pcnstr)
			{
				pdrgpcnstr->Append(pcnstr);

				// merge with the equivalence classes we have so far
				ColRefSetArray *pdrgpcrsMerged = CUtils::PdrgpcrsMergeEquivClasses(memory_pool, pdrgpcrs, pdrgpcrsChild);
				pdrgpcrs->Release();
				pdrgpcrs = pdrgpcrsMerged;
			}
			CRefCount::SafeRelease(pdrgpcrsChild);
		}
		else
		{
			CDrvdPropRelational *pdprel = exprhdl.GetRelationalProperties(ul);
			CPropConstraint *ppc = pdprel->Ppc();

			// equivalence classes coming from child
			ColRefSetArray *pdrgpcrsChild = ppc->PdrgpcrsEquivClasses();

			// merge with the equivalence classes we have so far
			ColRefSetArray *pdrgpcrsMerged = CUtils::PdrgpcrsMergeEquivClasses(memory_pool, pdrgpcrs, pdrgpcrsChild);
			pdrgpcrs->Release();
			pdrgpcrs = pdrgpcrsMerged;

			// constraint coming from child
			CConstraint *pcnstr = ppc->Pcnstr();
			if (NULL != pcnstr)
			{
				pcnstr->AddRef();
				pdrgpcnstr->Append(pcnstr);
			}
		}
	}

	CConstraint *pcnstrNew = CConstraint::PcnstrConjunction(memory_pool, pdrgpcnstr);

	return GPOS_NEW(memory_pool) CPropConstraint(memory_pool, pdrgpcrs, pcnstrNew);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PpcDeriveConstraintFromTable
//
//	@doc:
//		Derive constraint property from a table/index get
//
//---------------------------------------------------------------------------
CPropConstraint *
CLogical::PpcDeriveConstraintFromTable
	(
	IMemoryPool *memory_pool,
	const CTableDescriptor *ptabdesc,
	const ColRefArray *pdrgpcrOutput
	)
{
	ColRefSetArray *pdrgpcrs = GPOS_NEW(memory_pool) ColRefSetArray(memory_pool);

	ConstraintArray *pdrgpcnstr = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);

	const ColumnDescrArray *pdrgpcoldesc = ptabdesc->Pdrgpcoldesc();
	const ULONG num_cols = pdrgpcoldesc->Size();

	ColRefArray *pdrgpcrNonSystem = GPOS_NEW(memory_pool) ColRefArray(memory_pool);

	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		CColumnDescriptor *pcoldesc = (*pdrgpcoldesc)[ul];
		CColRef *colref = (*pdrgpcrOutput)[ul];
		// we are only interested in non-system columns that are defined as
		// being NOT NULL

		if (pcoldesc->IsSystemColumn())
		{
			continue;
		}

		pdrgpcrNonSystem->Append(colref);

		if (pcoldesc->IsNullable())
		{
			continue;
		}

		// add a "not null" constraint and an equivalence class
		CConstraint * pcnstr = CConstraintInterval::PciUnbounded(memory_pool, colref, false /*fIncludesNull*/);

		if (pcnstr == NULL)
		{
			continue;
		}
		pdrgpcnstr->Append(pcnstr);

		CColRefSet *pcrsEquiv = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
		pcrsEquiv->Include(colref);
		pdrgpcrs->Append(pcrsEquiv);
	}

	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDRelation *pmdrel = md_accessor->RetrieveRel(ptabdesc->MDId());

	const ULONG ulCheckConstraint = pmdrel->CheckConstraintCount();
	for (ULONG ul = 0; ul < ulCheckConstraint; ul++)
	{
		IMDId *pmdidCheckConstraint = pmdrel->CheckConstraintMDidAt(ul);

		const IMDCheckConstraint *pmdCheckConstraint = md_accessor->RetrieveCheckConstraints(pmdidCheckConstraint);

		// extract the check constraint expression
		CExpression *pexprCheckConstraint = pmdCheckConstraint->GetCheckConstraintExpr(memory_pool, md_accessor, pdrgpcrNonSystem);
		GPOS_ASSERT(NULL != pexprCheckConstraint);
		GPOS_ASSERT(CUtils::FPredicate(pexprCheckConstraint));

		ColRefSetArray *pdrgpcrsChild = NULL;
		CConstraint *pcnstr = CConstraint::PcnstrFromScalarExpr(memory_pool, pexprCheckConstraint, &pdrgpcrsChild);
		if (NULL != pcnstr)
		{
			pdrgpcnstr->Append(pcnstr);

			// merge with the equivalence classes we have so far
			ColRefSetArray *pdrgpcrsMerged = CUtils::PdrgpcrsMergeEquivClasses(memory_pool, pdrgpcrs, pdrgpcrsChild);
			pdrgpcrs->Release();
			pdrgpcrs = pdrgpcrsMerged;
		}
		CRefCount::SafeRelease(pdrgpcrsChild);
		pexprCheckConstraint->Release();
	}

	pdrgpcrNonSystem->Release();

	return GPOS_NEW(memory_pool) CPropConstraint(memory_pool, pdrgpcrs, CConstraint::PcnstrConjunction(memory_pool, pdrgpcnstr));
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PpcDeriveConstraintFromTableWithPredicates
//
//	@doc:
//		Derive constraint property from a table/index get with predicates
//
//---------------------------------------------------------------------------
CPropConstraint *
CLogical::PpcDeriveConstraintFromTableWithPredicates
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	const CTableDescriptor *ptabdesc,
	const ColRefArray *pdrgpcrOutput
	)
{
	ConstraintArray *pdrgpcnstr = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);
	CPropConstraint *ppcTable = PpcDeriveConstraintFromTable(memory_pool, ptabdesc, pdrgpcrOutput);
	CConstraint *pcnstrTable = ppcTable->Pcnstr();
	if (NULL != pcnstrTable)
	{
		pcnstrTable->AddRef();
		pdrgpcnstr->Append(pcnstrTable);
	}
	ColRefSetArray *pdrgpcrsEquivClassesTable = ppcTable->PdrgpcrsEquivClasses();

	CPropConstraint *ppcnstrCond = PpcDeriveConstraintFromPredicates(memory_pool, exprhdl);
	CConstraint *pcnstrCond = ppcnstrCond->Pcnstr();

	if (NULL != pcnstrCond)
	{
		pcnstrCond->AddRef();
		pdrgpcnstr->Append(pcnstrCond);
	}
	else if (NULL == pcnstrTable)
	{
		ppcTable->Release();
		pdrgpcnstr->Release();

		return ppcnstrCond;
	}

	ColRefSetArray *pdrgpcrsCond = ppcnstrCond->PdrgpcrsEquivClasses();
	ColRefSetArray *pdrgpcrs = CUtils::PdrgpcrsMergeEquivClasses(memory_pool, pdrgpcrsEquivClassesTable, pdrgpcrsCond);
	CPropConstraint *ppc = GPOS_NEW(memory_pool) CPropConstraint(memory_pool, pdrgpcrs, CConstraint::PcnstrConjunction(memory_pool, pdrgpcnstr));

	ppcnstrCond->Release();
	ppcTable->Release();

	return ppc;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PpcDeriveConstraintPassThru
//
//	@doc:
//		Shorthand to addref and pass through constraint from a given child
//
//---------------------------------------------------------------------------
CPropConstraint *
CLogical::PpcDeriveConstraintPassThru
	(
	CExpressionHandle &exprhdl,
	ULONG ulChild
	)
{
	// return constraint property of child
	CPropConstraint *ppc = exprhdl.GetRelationalProperties(ulChild)->Ppc();
	if (NULL != ppc)
	{
		ppc->AddRef();
	}
	return ppc;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PpcDeriveConstraintRestrict
//
//	@doc:
//		Derive constraint property only on the given columns
//
//---------------------------------------------------------------------------
CPropConstraint *
CLogical::PpcDeriveConstraintRestrict
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsOutput
	)
{
	// constraint property from relational child
	CPropConstraint *ppc = exprhdl.GetRelationalProperties(0)->Ppc();
	ColRefSetArray *pdrgpcrs = ppc->PdrgpcrsEquivClasses();

	// construct new array of equivalence classes
	ColRefSetArray *pdrgpcrsNew = GPOS_NEW(memory_pool) ColRefSetArray(memory_pool);

	const ULONG length = pdrgpcrs->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CColRefSet *pcrsEquiv = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
		pcrsEquiv->Include((*pdrgpcrs)[ul]);
		pcrsEquiv->Intersection(pcrsOutput);

		if (0 < pcrsEquiv->Size())
		{
			pdrgpcrsNew->Append(pcrsEquiv);
		}
		else
		{
			pcrsEquiv->Release();
		}
	}

	CConstraint *pcnstrChild = ppc->Pcnstr();
	if (NULL == pcnstrChild)
	{
		return GPOS_NEW(memory_pool) CPropConstraint(memory_pool, pdrgpcrsNew, NULL);
	}

	ConstraintArray *pdrgpcnstr = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);

	// include only constraints on given columns
	CColRefSetIter crsi(*pcrsOutput);
	while (crsi.Advance())
	{
		CColRef *colref = crsi.Pcr();
		CConstraint *pcnstrCol = pcnstrChild->Pcnstr(memory_pool, colref);
		if (NULL == pcnstrCol)
		{
			continue;
		}

		if (pcnstrCol->IsConstraintUnbounded())
		{
			pcnstrCol->Release();
			continue;
		}

		pdrgpcnstr->Append(pcnstrCol);
	}

	CConstraint *pcnstr = CConstraint::PcnstrConjunction(memory_pool, pdrgpcnstr);

	return GPOS_NEW(memory_pool) CPropConstraint(memory_pool, pdrgpcrsNew, pcnstr);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PfpDerive
//
//	@doc:
//		Derive function properties
//
//---------------------------------------------------------------------------
CFunctionProp *
CLogical::PfpDerive
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
	const
{
	IMDFunction::EFuncStbl efs = EfsDeriveFromChildren(exprhdl, IMDFunction::EfsImmutable);

	return GPOS_NEW(memory_pool) CFunctionProp(efs, IMDFunction::EfdaNoSQL, exprhdl.FChildrenHaveVolatileFuncScan(), false /*fScan*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PfpDeriveFromScalar
//
//	@doc:
//		Derive function properties using data access property of scalar child
//
//---------------------------------------------------------------------------
CFunctionProp *
CLogical::PfpDeriveFromScalar
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	ULONG ulScalarIndex
	)
{
	GPOS_CHECK_ABORT;
	GPOS_ASSERT(ulScalarIndex == exprhdl.Arity() - 1);
	GPOS_ASSERT(exprhdl.FScalarChild(ulScalarIndex));

	// collect stability from all children
	IMDFunction::EFuncStbl efs = EfsDeriveFromChildren(exprhdl, IMDFunction::EfsImmutable);

	// get data access from scalar child
	CFunctionProp *pfp = exprhdl.PfpChild(ulScalarIndex);
	GPOS_ASSERT(NULL != pfp);
	IMDFunction::EFuncDataAcc efda = pfp->Efda();

	return GPOS_NEW(memory_pool) CFunctionProp(efs, efda, exprhdl.FChildrenHaveVolatileFuncScan(), false /*fScan*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogical::Maxcard
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle & // exprhdl
	)
	const
{
	// unbounded by default
	return CMaxCard();
}
	

//---------------------------------------------------------------------------
//	@function:
//		CLogical::JoinDepth
//
//	@doc:
//		Derive join depth
//
//---------------------------------------------------------------------------
ULONG
CLogical::JoinDepth
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	const ULONG arity = exprhdl.Arity();

	// sum-up join depth of all relational children
	ULONG ulDepth = 0;
	for (ULONG ul = 0; ul < arity; ul++)
	{
		if (!exprhdl.FScalarChild(ul))
		{
			ulDepth = ulDepth + exprhdl.GetRelationalProperties(ul)->JoinDepth();
		}
	}

	return ulDepth;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::MaxcardDef
//
//	@doc:
//		Default max card for join and apply operators
//
//---------------------------------------------------------------------------
CMaxCard
CLogical::MaxcardDef
	(
	CExpressionHandle &exprhdl
	)
{
	const ULONG arity = exprhdl.Arity();

	CMaxCard maxcard = exprhdl.GetRelationalProperties(0)->Maxcard();
	for (ULONG ul = 1; ul < arity - 1; ul++)
	{
		if (!exprhdl.FScalarChild(ul))
		{
			maxcard *= exprhdl.GetRelationalProperties(ul)->Maxcard();
		}
	}

	return maxcard;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::Maxcard
//
//	@doc:
//		Derive max card given scalar child and constraint property. If a
//		contradiction is detected then return maxcard of zero, otherwise
//		use the given default maxcard
//
//---------------------------------------------------------------------------
CMaxCard
CLogical::Maxcard
	(
	CExpressionHandle &exprhdl,
	ULONG ulScalarIndex,
	CMaxCard maxcard
	)
{
	// in case of a false condition (when the operator is not Full / Left Outer Join) or a contradiction, maxcard should be zero
	CExpression *pexprScalar = exprhdl.PexprScalarChild(ulScalarIndex);

	if (NULL != pexprScalar &&
		( (CUtils::FScalarConstFalse(pexprScalar) &&
				(COperator::EopLogicalFullOuterJoin != exprhdl.Pop()->Eopid() &&
						COperator::EopLogicalLeftOuterJoin != exprhdl.Pop()->Eopid()))
		|| CDrvdPropRelational::GetRelationalProperties(exprhdl.Pdp())->Ppc()->FContradiction()))
	{
		return CMaxCard(0 /*ull*/);
	}

	return maxcard;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PcrsReqdChildStats
//
//	@doc:
//		Helper for compute required stat columns of the n-th child
//
//---------------------------------------------------------------------------
CColRefSet *
CLogical::PcrsReqdChildStats
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsInput,
	CColRefSet *pcrsUsed, // columns used by scalar child(ren)
	ULONG child_index
	)
{
	GPOS_ASSERT(child_index < exprhdl.Arity() - 1);
	GPOS_CHECK_ABORT;

	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrs->Union(pcrsInput);
	pcrs->Union(pcrsUsed);

	// intersect with the output columns of relational child
	pcrs->Intersection(exprhdl.GetRelationalProperties(child_index)->PcrsOutput());

	return pcrs;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogical::PcrsStatsPassThru
//
//	@doc:
//		Helper for common case of passing through required stat columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogical::PcrsStatsPassThru
	(
	CColRefSet *pcrsInput
	)
{
	GPOS_ASSERT(NULL != pcrsInput);
	GPOS_CHECK_ABORT;

	pcrsInput->AddRef();
	return pcrsInput;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogical::PstatsPassThruOuter
//
//	@doc:
//		Helper for common case of passing through derived stats
//
//---------------------------------------------------------------------------
IStatistics *
CLogical::PstatsPassThruOuter
	(
	CExpressionHandle &exprhdl
	)
{
	GPOS_CHECK_ABORT;

	IStatistics *stats = exprhdl.Pstats(0);
	stats->AddRef();

	return stats;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PstatsBaseTable
//
//	@doc:
//		Helper for deriving statistics on a base table
//
//---------------------------------------------------------------------------
IStatistics *
CLogical::PstatsBaseTable
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	CTableDescriptor *ptabdesc,
	CColRefSet *pcrsHistExtra   // additional columns required for stats, not required by parent
	)
{
	CReqdPropRelational *prprel = CReqdPropRelational::GetReqdRelationalProps(exprhdl.Prp());
	CColRefSet *pcrsHist = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrsHist->Include(prprel->PcrsStat());
	if (NULL != pcrsHistExtra)
	{
		pcrsHist->Include(pcrsHistExtra);
	}

	CDrvdPropRelational *pdprel = exprhdl.GetRelationalProperties();
	CColRefSet *pcrsOutput = pdprel->PcrsOutput();
	CColRefSet *pcrsWidth = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrsWidth->Include(pcrsOutput);
	pcrsWidth->Exclude(pcrsHist);

	const COptCtxt *poctxt = COptCtxt::PoctxtFromTLS();
	CMDAccessor *md_accessor = poctxt->Pmda();
	CStatisticsConfig *stats_config = poctxt->GetOptimizerConfig()->GetStatsConf();

	IStatistics *stats = md_accessor->Pstats
								(
								memory_pool,
								ptabdesc->MDId(),
								pcrsHist,
								pcrsWidth,
								stats_config
								);

	// clean up
	pcrsWidth->Release();
	pcrsHist->Release();

	return stats;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogical::PstatsDeriveDummy
//
//	@doc:
//		Derive dummy statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogical::PstatsDeriveDummy
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	CDouble rows
	)
	const
{
	GPOS_CHECK_ABORT;

	// return a dummy stats object that has a histogram for every
	// required-stats column
	GPOS_ASSERT(Esp(exprhdl) > EspNone);
	CReqdPropRelational *prprel = CReqdPropRelational::GetReqdRelationalProps(exprhdl.Prp());
	CColRefSet *pcrs = prprel->PcrsStat();
	ULongPtrArray *col_ids = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
	pcrs->ExtractColIds(memory_pool, col_ids);

	IStatistics *stats = CStatistics::MakeDummyStats(memory_pool, col_ids, rows);

	// clean up
	col_ids->Release();

	return stats;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PexprPartPred
//
//	@doc:
//		Compute partition predicate to pass down to n-th child
//
//---------------------------------------------------------------------------
CExpression *
CLogical::PexprPartPred
	(
	IMemoryPool *, //memory_pool,
	CExpressionHandle &, //exprhdl,
	CExpression *, //pexprInput,
	ULONG //child_index
	)
	const
{
	GPOS_CHECK_ABORT;

	// the default behavior is to never pass down any partition predicates
	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogical::PdpCreate
//
//	@doc:
//		Create base container of derived properties
//
//---------------------------------------------------------------------------
DrvdPropArray *
CLogical::PdpCreate
	(
	IMemoryPool *memory_pool
	)
	const
{
	return GPOS_NEW(memory_pool) CDrvdPropRelational();
}


//---------------------------------------------------------------------------
//	@function:
//		CLogical::PrpCreate
//
//	@doc:
//		Create base container of required properties
//
//---------------------------------------------------------------------------
CReqdProp *
CLogical::PrpCreate
	(
	IMemoryPool *memory_pool
	)
	const
{
	return GPOS_NEW(memory_pool) CReqdPropRelational();
}


//---------------------------------------------------------------------------
//	@function:
//		CLogical::PtabdescFromTableGet
//
//	@doc:
//		Returns the table descriptor for (Dynamic)(BitmapTable)Get operators
//
//---------------------------------------------------------------------------
CTableDescriptor *
CLogical::PtabdescFromTableGet
	(
	COperator *pop
	)
{
	GPOS_ASSERT(NULL != pop);
	switch (pop->Eopid())
	{
		case CLogical::EopLogicalGet:
			return CLogicalGet::PopConvert(pop)->Ptabdesc();
		case CLogical::EopLogicalDynamicGet:
			return CLogicalDynamicGet::PopConvert(pop)->Ptabdesc();
		case CLogical::EopLogicalBitmapTableGet:
			return CLogicalBitmapTableGet::PopConvert(pop)->Ptabdesc();
		case CLogical::EopLogicalDynamicBitmapTableGet:
			return CLogicalDynamicBitmapTableGet::PopConvert(pop)->Ptabdesc();
		default:
			GPOS_ASSERT(false && "Unsupported operator in CLogical::PtabdescFromTableGet");
			return NULL;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PdrgpcrOutputFromLogicalGet
//
//	@doc:
//		Extract the output columns from a logical get or dynamic get operator
//
//---------------------------------------------------------------------------
ColRefArray *
CLogical::PdrgpcrOutputFromLogicalGet
	(
	CLogical *pop
	)
{
	GPOS_ASSERT(NULL != pop);
	GPOS_ASSERT(COperator::EopLogicalGet == pop->Eopid() || COperator::EopLogicalDynamicGet == pop->Eopid());
	
	if (COperator::EopLogicalGet == pop->Eopid())
	{
		return CLogicalGet::PopConvert(pop)->PdrgpcrOutput();
	}
		
	return CLogicalDynamicGet::PopConvert(pop)->PdrgpcrOutput();
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::NameFromLogicalGet
//
//	@doc:
//		Extract the name from a logical get or dynamic get operator
//
//---------------------------------------------------------------------------
const CName &
CLogical::NameFromLogicalGet
	(
	CLogical *pop
	)
{
	GPOS_ASSERT(NULL != pop);
	GPOS_ASSERT(COperator::EopLogicalGet == pop->Eopid() || COperator::EopLogicalDynamicGet == pop->Eopid());
	
	if (COperator::EopLogicalGet == pop->Eopid())
	{
		return CLogicalGet::PopConvert(pop)->Name();
	}
		
	return CLogicalDynamicGet::PopConvert(pop)->Name();
}

//---------------------------------------------------------------------------
//	@function:
//		CLogical::PcrsDist
//
//	@doc:
//		Return the set of distribution columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogical::PcrsDist
	(
	IMemoryPool *memory_pool,
	const CTableDescriptor *ptabdesc,
	const ColRefArray *pdrgpcrOutput
	)
{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != pdrgpcrOutput);

	const ColumnDescrArray *pdrgpcoldesc = ptabdesc->Pdrgpcoldesc();
	const ColumnDescrArray *pdrgpcoldescDist = ptabdesc->PdrgpcoldescDist();
	GPOS_ASSERT(NULL != pdrgpcoldesc);
	GPOS_ASSERT(NULL != pdrgpcoldescDist);
	GPOS_ASSERT(pdrgpcrOutput->Size() == pdrgpcoldesc->Size());


	// mapping base table columns to corresponding column references
	HMICr *phmicr = GPOS_NEW(memory_pool) HMICr(memory_pool);
	const ULONG num_cols = pdrgpcoldesc->Size();
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		CColumnDescriptor *pcoldesc = (*pdrgpcoldesc)[ul];
		CColRef *colref = (*pdrgpcrOutput)[ul];
		phmicr->Insert(GPOS_NEW(memory_pool) INT(pcoldesc->AttrNum()), colref);
	}

	CColRefSet *pcrsDist = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	const ULONG ulDistCols = pdrgpcoldescDist->Size();
	for (ULONG ul2 = 0; ul2 < ulDistCols; ul2++)
	{
		CColumnDescriptor *pcoldesc = (*pdrgpcoldescDist)[ul2];
		const INT attno = pcoldesc->AttrNum();
		CColRef *pcrMapped = phmicr->Find(&attno);
		GPOS_ASSERT(NULL != pcrMapped);
		pcrsDist->Include(pcrMapped);
	}

	// clean up
	phmicr->Release();

	return pcrsDist;
}

// EOF

