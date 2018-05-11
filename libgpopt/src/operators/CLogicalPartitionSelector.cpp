//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CLogicalPartitionSelector.cpp
//
//	@doc:
//		Implementation of Logical partition selector
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/base/CDistributionSpecAny.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalPartitionSelector.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalPartitionSelector::CLogicalPartitionSelector
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalPartitionSelector::CLogicalPartitionSelector
	(
	IMemoryPool *memory_pool
	)
	:
	CLogical(memory_pool),
	m_mdid(NULL),
	m_pdrgpexprFilters(NULL),
	m_pcrOid(NULL)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalPartitionSelector::CLogicalPartitionSelector
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalPartitionSelector::CLogicalPartitionSelector
	(
	IMemoryPool *memory_pool,
	IMDId *mdid,
	DrgPexpr *pdrgpexprFilters,
	CColRef *pcrOid
	)
	:
	CLogical(memory_pool),
	m_mdid(mdid),
	m_pdrgpexprFilters(pdrgpexprFilters),
	m_pcrOid(pcrOid)
{
	GPOS_ASSERT(mdid->IsValid());
	GPOS_ASSERT(NULL != pdrgpexprFilters);
	GPOS_ASSERT(0 < pdrgpexprFilters->Size());
	GPOS_ASSERT(NULL != pcrOid);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalPartitionSelector::~CLogicalPartitionSelector
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalPartitionSelector::~CLogicalPartitionSelector()
{
	CRefCount::SafeRelease(m_mdid);
	CRefCount::SafeRelease(m_pdrgpexprFilters);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalPartitionSelector::Matches
//
//	@doc:
//		Match operators
//
//---------------------------------------------------------------------------
BOOL
CLogicalPartitionSelector::Matches
	(
	COperator *pop
	)
	const
{
	if (Eopid() != pop->Eopid())
	{
		return false;
	}

	CLogicalPartitionSelector *popPartSelector = CLogicalPartitionSelector::PopConvert(pop);

	return popPartSelector->PcrOid() == m_pcrOid &&
			popPartSelector->MDId()->Equals(m_mdid) &&
			popPartSelector->m_pdrgpexprFilters->Equals(m_pdrgpexprFilters);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalPartitionSelector::HashValue
//
//	@doc:
//		Hash operator
//
//---------------------------------------------------------------------------
ULONG
CLogicalPartitionSelector::HashValue() const
{
	return gpos::CombineHashes(Eopid(), m_mdid->HashValue());
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalPartitionSelector::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalPartitionSelector::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	CColRef *pcrOid = CUtils::PcrRemap(m_pcrOid, colref_mapping, must_exist);
	DrgPexpr *pdrgpexpr = CUtils::PdrgpexprRemap(memory_pool, m_pdrgpexprFilters, colref_mapping);

	m_mdid->AddRef();

	return GPOS_NEW(memory_pool) CLogicalPartitionSelector(memory_pool, m_mdid, pdrgpexpr, pcrOid);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalPartitionSelector::PcrsDeriveOutput
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalPartitionSelector::PcrsDeriveOutput
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	CColRefSet *pcrsOutput = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	pcrsOutput->Union(exprhdl.GetRelationalProperties(0)->PcrsOutput());
	pcrsOutput->Include(m_pcrOid);

	return pcrsOutput;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalPartitionSelector::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalPartitionSelector::Maxcard
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	// pass on max card of first child
	return exprhdl.GetRelationalProperties(0)->Maxcard();
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalPartitionSelector::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalPartitionSelector::PxfsCandidates
	(
	IMemoryPool *memory_pool
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfImplementPartitionSelector);
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalPartitionSelector::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalPartitionSelector::OsPrint
	(
	IOstream &os
	)
	const
{
	os	<< SzId()
		<< ", Part Table: ";
	m_mdid->OsPrint(os);

	return os;
}

// EOF
