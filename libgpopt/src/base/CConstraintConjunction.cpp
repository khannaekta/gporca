//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CConstraintConjunction.cpp
//
//	@doc:
//		Implementation of conjunction constraints
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CConstraintConjunction.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/operators/CPredicateUtils.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CConstraintConjunction::CConstraintConjunction
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CConstraintConjunction::CConstraintConjunction
	(
	IMemoryPool *memory_pool,
	ConstraintArray *pdrgpcnstr
	)
	:
	CConstraint(memory_pool),
	m_pdrgpcnstr(NULL)
{
	GPOS_ASSERT(NULL != pdrgpcnstr);
	m_pdrgpcnstr = PdrgpcnstrFlatten(memory_pool, pdrgpcnstr, EctConjunction);

	const ULONG length = m_pdrgpcnstr->Size();
	GPOS_ASSERT(0 < length);

	m_pcrsUsed = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstr = (*m_pdrgpcnstr)[ul];
		m_pcrsUsed->Include(pcnstr->PcrsUsed());
	}

	m_phmcolconstr = Phmcolconstr(memory_pool, m_pcrsUsed, m_pdrgpcnstr);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintConjunction::~CConstraintConjunction
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CConstraintConjunction::~CConstraintConjunction()
{
	m_pdrgpcnstr->Release();
	m_pcrsUsed->Release();
	m_phmcolconstr->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintConjunction::FContradiction
//
//	@doc:
//		Check if this constraint is a contradiction
//
//---------------------------------------------------------------------------
BOOL
CConstraintConjunction::FContradiction() const
{
	const ULONG length = m_pdrgpcnstr->Size();

	BOOL fContradiction = false;
	for (ULONG ul = 0; !fContradiction && ul < length; ul++)
	{
		fContradiction = (*m_pdrgpcnstr)[ul]->FContradiction();
	}

	return fContradiction;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintConjunction::FConstraint
//
//	@doc:
//		Check if there is a constraint on the given column
//
//---------------------------------------------------------------------------
BOOL
CConstraintConjunction::FConstraint
	(
	const CColRef *colref
	)
	const
{
	ConstraintArray *pdrgpcnstrCol = m_phmcolconstr->Find(colref);
	return (NULL != pdrgpcnstrCol && 0 < pdrgpcnstrCol->Size());
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintConjunction::PcnstrCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the constraint with remapped columns. If must_exist is
//		set to true, then every column reference in this constraint must be in
//		the hashmap in order to be replace. Otherwise, some columns may not be
//		in the mapping, and hence will not be replaced
//
//---------------------------------------------------------------------------
CConstraint *
CConstraintConjunction::PcnstrCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	ConstraintArray *pdrgpcnstr = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);
	const ULONG length = m_pdrgpcnstr->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstr = (*m_pdrgpcnstr)[ul];
		CConstraint *pcnstrCopy = pcnstr->PcnstrCopyWithRemappedColumns(memory_pool, colref_mapping, must_exist);
		pdrgpcnstr->Append(pcnstrCopy);
	}
	return GPOS_NEW(memory_pool) CConstraintConjunction(memory_pool, pdrgpcnstr);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintConjunction::Pcnstr
//
//	@doc:
//		Return constraint on a given column
//
//---------------------------------------------------------------------------
CConstraint *
CConstraintConjunction::Pcnstr
	(
	IMemoryPool *memory_pool,
	const CColRef *colref
	)
{
	// all children referencing given column
	ConstraintArray *pdrgpcnstrCol = m_phmcolconstr->Find(colref);
	if (NULL == pdrgpcnstrCol)
	{
		return NULL;
	}

	ConstraintArray *pdrgpcnstr = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);

	const ULONG length = pdrgpcnstrCol->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		// the part of the child that references this column
		CConstraint *pcnstrCol = (*pdrgpcnstrCol)[ul]->Pcnstr(memory_pool, colref);
		if (NULL == pcnstrCol || pcnstrCol->IsConstraintUnbounded())
		{
			CRefCount::SafeRelease(pcnstrCol);
			continue;
		}
		pdrgpcnstr->Append(pcnstrCol);
	}

	return CConstraint::PcnstrConjunction(memory_pool, pdrgpcnstr);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintConjunction::Pcnstr
//
//	@doc:
//		Return constraint on a given column set
//
//---------------------------------------------------------------------------
CConstraint *
CConstraintConjunction::Pcnstr
	(
	IMemoryPool *memory_pool,
	CColRefSet *pcrs
	)
{
	const ULONG length = m_pdrgpcnstr->Size();

	ConstraintArray *pdrgpcnstr = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);

	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstr = (*m_pdrgpcnstr)[ul];
		if (pcnstr->PcrsUsed()->IsDisjoint(pcrs))
		{
			continue;
		}

		// the part of the child that references these columns
		CConstraint *pcnstrCol = pcnstr->Pcnstr(memory_pool, pcrs);
		if (NULL != pcnstrCol)
		{
			pdrgpcnstr->Append(pcnstrCol);
		}
	}

	return CConstraint::PcnstrConjunction(memory_pool, pdrgpcnstr);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintConjunction::PcnstrRemapForColumn
//
//	@doc:
//		Return a copy of the constraint for a different column
//
//---------------------------------------------------------------------------
CConstraint *
CConstraintConjunction::PcnstrRemapForColumn
	(
	IMemoryPool *memory_pool,
	CColRef *colref
	)
	const
{
	return PcnstrConjDisjRemapForColumn(memory_pool, colref, m_pdrgpcnstr, true /*fConj*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintConjunction::PexprScalar
//
//	@doc:
//		Scalar expression
//
//---------------------------------------------------------------------------
CExpression *
CConstraintConjunction::PexprScalar
	(
	IMemoryPool *memory_pool
	)
{
	if (NULL == m_pexprScalar)
	{
		if (FContradiction())
		{
			m_pexprScalar = CUtils::PexprScalarConstBool(memory_pool, false /*fval*/, false /*is_null*/);
		}
		else
		{
			m_pexprScalar = PexprScalarConjDisj(memory_pool, m_pdrgpcnstr, true /*fConj*/);
		}
	}

	return m_pexprScalar;
}

// EOF
