//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CConstraintDisjunction.cpp
//
//	@doc:
//		Implementation of disjunction constraints
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/base/CConstraintDisjunction.h"
#include "gpopt/operators/CPredicateUtils.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CConstraintDisjunction::CConstraintDisjunction
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CConstraintDisjunction::CConstraintDisjunction
	(
	IMemoryPool *memory_pool,
	DrgPcnstr *pdrgpcnstr
	)
	:
	CConstraint(memory_pool),
	m_pdrgpcnstr(NULL)
{
	GPOS_ASSERT(NULL != pdrgpcnstr);
	m_pdrgpcnstr = PdrgpcnstrFlatten(memory_pool, pdrgpcnstr, EctDisjunction);

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
//		CConstraintDisjunction::~CConstraintDisjunction
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CConstraintDisjunction::~CConstraintDisjunction()
{
	m_pdrgpcnstr->Release();
	m_pcrsUsed->Release();
	m_phmcolconstr->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintDisjunction::FContradiction
//
//	@doc:
//		Check if this constraint is a contradiction
//
//---------------------------------------------------------------------------
BOOL
CConstraintDisjunction::FContradiction() const
{
	const ULONG length = m_pdrgpcnstr->Size();

	BOOL fContradiction = true;
	for (ULONG ul = 0; fContradiction && ul < length; ul++)
	{
		fContradiction = (*m_pdrgpcnstr)[ul]->FContradiction();
	}

	return fContradiction;
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintDisjunction::FConstraint
//
//	@doc:
//		Check if there is a constraint on the given column
//
//---------------------------------------------------------------------------
BOOL
CConstraintDisjunction::FConstraint
	(
	const CColRef *colref
	)
	const
{
	DrgPcnstr *pdrgpcnstrCol = m_phmcolconstr->Find(colref);
	return (NULL != pdrgpcnstrCol && m_pdrgpcnstr->Size() == pdrgpcnstrCol->Size());
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintDisjunction::PcnstrCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the constraint with remapped columns
//
//---------------------------------------------------------------------------
CConstraint *
CConstraintDisjunction::PcnstrCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	DrgPcnstr *pdrgpcnstr = GPOS_NEW(memory_pool) DrgPcnstr(memory_pool);
	const ULONG length = m_pdrgpcnstr->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstr = (*m_pdrgpcnstr)[ul];
		CConstraint *pcnstrCopy = pcnstr->PcnstrCopyWithRemappedColumns(memory_pool, colref_mapping, must_exist);
		pdrgpcnstr->Append(pcnstrCopy);
	}
	return GPOS_NEW(memory_pool) CConstraintDisjunction(memory_pool, pdrgpcnstr);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintDisjunction::Pcnstr
//
//	@doc:
//		Return constraint on a given column
//
//---------------------------------------------------------------------------
CConstraint *
CConstraintDisjunction::Pcnstr
	(
	IMemoryPool *memory_pool,
	const CColRef *colref
	)
{
	// all children referencing given column
	DrgPcnstr *pdrgpcnstrCol = m_phmcolconstr->Find(colref);
	if (NULL == pdrgpcnstrCol)
	{
		return NULL;
	}

	// if not all children have this col, return unbounded constraint
	const ULONG length = pdrgpcnstrCol->Size();
	if (length != m_pdrgpcnstr->Size())
	{
		return CConstraintInterval::PciUnbounded(memory_pool, colref, true /*fIncludesNull*/);
	}

	DrgPcnstr *pdrgpcnstr = GPOS_NEW(memory_pool) DrgPcnstr(memory_pool);

	for (ULONG ul = 0; ul < length; ul++)
	{
		// the part of the child that references this column
		CConstraint *pcnstrCol = (*pdrgpcnstrCol)[ul]->Pcnstr(memory_pool, colref);
		if (NULL == pcnstrCol)
		{
			pcnstrCol = CConstraintInterval::PciUnbounded(memory_pool, colref, true /*is_null*/);
		}
		if (pcnstrCol->IsConstraintUnbounded())
		{
			pdrgpcnstr->Release();
			return pcnstrCol;
		}
		pdrgpcnstr->Append(pcnstrCol);
	}

	return CConstraint::PcnstrDisjunction(memory_pool, pdrgpcnstr);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintDisjunction::Pcnstr
//
//	@doc:
//		Return constraint on a given column set
//
//---------------------------------------------------------------------------
CConstraint *
CConstraintDisjunction::Pcnstr
	(
	IMemoryPool *memory_pool,
	CColRefSet *pcrs
	)
{
	const ULONG length = m_pdrgpcnstr->Size();

	DrgPcnstr *pdrgpcnstr = GPOS_NEW(memory_pool) DrgPcnstr(memory_pool);

	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstr = (*m_pdrgpcnstr)[ul];
		if (pcnstr->PcrsUsed()->IsDisjoint(pcrs))
		{
			// a child has none of these columns... return unbounded constraint
			pdrgpcnstr->Release();
			return CConstraintInterval::PciUnbounded(memory_pool, pcrs, true /*fIncludesNull*/);
		}

		// the part of the child that references these columns
		CConstraint *pcnstrCol = pcnstr->Pcnstr(memory_pool, pcrs);

		if (NULL == pcnstrCol)
		{
			pcnstrCol = CConstraintInterval::PciUnbounded(memory_pool, pcrs, true /*fIncludesNull*/);
		}
		GPOS_ASSERT(NULL != pcnstrCol);
		pdrgpcnstr->Append(pcnstrCol);
	}

	return CConstraint::PcnstrDisjunction(memory_pool, pdrgpcnstr);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintDisjunction::PcnstrRemapForColumn
//
//	@doc:
//		Return a copy of the constraint for a different column
//
//---------------------------------------------------------------------------
CConstraint *
CConstraintDisjunction::PcnstrRemapForColumn
	(
	IMemoryPool *memory_pool,
	CColRef *colref
	)
	const
{
	return PcnstrConjDisjRemapForColumn(memory_pool, colref, m_pdrgpcnstr, false /*fConj*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CConstraintDisjunction::PexprScalar
//
//	@doc:
//		Scalar expression
//
//---------------------------------------------------------------------------
CExpression *
CConstraintDisjunction::PexprScalar
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
			m_pexprScalar = PexprScalarConjDisj(memory_pool, m_pdrgpcnstr, false /*fConj*/);
		}
	}

	return m_pexprScalar;
}

// EOF
