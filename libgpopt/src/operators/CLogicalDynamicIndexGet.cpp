//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalDynamicIndexGet.cpp
//
//	@doc:
//		Implementation of index access for partitioned tables
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/common/CAutoP.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CColRefTable.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/COptCtxt.h"

#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalDynamicIndexGet.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/metadata/CPartConstraint.h"

#include "naucrates/statistics/CStatisticsUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicIndexGet::CLogicalDynamicIndexGet
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalDynamicIndexGet::CLogicalDynamicIndexGet
	(
	IMemoryPool *memory_pool
	)
	:
	CLogicalDynamicGetBase(memory_pool),
	m_pindexdesc(NULL),
	m_ulOriginOpId(gpos::ulong_max),
	m_pos(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicIndexGet::CLogicalDynamicIndexGet
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalDynamicIndexGet::CLogicalDynamicIndexGet
	(
	IMemoryPool *memory_pool,
	const IMDIndex *pmdindex,
	CTableDescriptor *ptabdesc,
	ULONG ulOriginOpId,
	const CName *pnameAlias,
	ULONG part_idx_id,
	DrgPcr *pdrgpcrOutput,
	DrgDrgPcr *pdrgpdrgpcrPart,
	ULONG ulSecondaryPartIndexId,
	CPartConstraint *ppartcnstr,
	CPartConstraint *ppartcnstrRel
	)
	:
	CLogicalDynamicGetBase(memory_pool, pnameAlias, ptabdesc, part_idx_id, pdrgpcrOutput, pdrgpdrgpcrPart, ulSecondaryPartIndexId, IsPartialIndex(ptabdesc, pmdindex), ppartcnstr, ppartcnstrRel),
	m_pindexdesc(NULL),
	m_ulOriginOpId(ulOriginOpId)
{
	GPOS_ASSERT(NULL != pmdindex);

	// create the index descriptor
	m_pindexdesc  = CIndexDescriptor::Pindexdesc(memory_pool, ptabdesc, pmdindex);

	// compute the order spec
	m_pos = PosFromIndex(m_memory_pool, pmdindex, m_pdrgpcrOutput, ptabdesc);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicIndexGet::~CLogicalDynamicIndexGet
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalDynamicIndexGet::~CLogicalDynamicIndexGet()
{
	CRefCount::SafeRelease(m_pindexdesc);
	CRefCount::SafeRelease(m_pos);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicIndexGet::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalDynamicIndexGet::HashValue() const
{
	return gpos::CombineHashes(COperator::HashValue(),
	                             gpos::CombineHashes(gpos::HashValue(&m_scan_id),
					             m_pindexdesc->MDId()->HashValue()));
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicIndexGet::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CLogicalDynamicIndexGet::Matches
	(
	COperator *pop
	)
	const
{
	return CUtils::FMatchDynamicIndex(this, pop);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicIndexGet::PcrsDeriveOuter
//
//	@doc:
//		Derive outer references
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalDynamicIndexGet::PcrsDeriveOuter
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	return PcrsDeriveOuterIndexGet(memory_pool, exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicIndexGet::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalDynamicIndexGet::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDIndex *pmdindex = md_accessor->Pmdindex(m_pindexdesc->MDId());
	CName *pnameAlias = GPOS_NEW(memory_pool) CName(memory_pool, *m_pnameAlias);

	DrgPcr *pdrgpcrOutput = NULL;
	if (must_exist)
	{
		pdrgpcrOutput = CUtils::PdrgpcrRemapAndCreate(memory_pool, m_pdrgpcrOutput, colref_mapping);
	}
	else
	{
		pdrgpcrOutput = CUtils::PdrgpcrRemap(memory_pool, m_pdrgpcrOutput, colref_mapping, must_exist);
	}

	DrgDrgPcr *pdrgpdrgpcrPart = CUtils::PdrgpdrgpcrRemap(memory_pool, m_pdrgpdrgpcrPart, colref_mapping, must_exist);
	CPartConstraint *ppartcnstr = m_part_constraint->PpartcnstrCopyWithRemappedColumns(memory_pool, colref_mapping, must_exist);
	CPartConstraint *ppartcnstrRel = m_ppartcnstrRel->PpartcnstrCopyWithRemappedColumns(memory_pool, colref_mapping, must_exist);

	m_ptabdesc->AddRef();

	return GPOS_NEW(memory_pool) CLogicalDynamicIndexGet
						(
						memory_pool,
						pmdindex,
						m_ptabdesc,
						m_ulOriginOpId,
						pnameAlias,
						m_scan_id,
						pdrgpcrOutput,
						pdrgpdrgpcrPart,
						m_ulSecondaryScanId,
						ppartcnstr,
						ppartcnstrRel
						);
}

// Checking if index is partial given the table descriptor and mdid of the index
BOOL
CLogicalDynamicIndexGet::IsPartialIndex
	(
	CTableDescriptor *ptabdesc,
	const IMDIndex *pmdindex
	)
{
	// refer to the relation on which this index is defined for index partial information
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDRelation *pmdrel = md_accessor->Pmdrel(ptabdesc->MDId());
	return pmdrel->IsPartialIndex(pmdindex->MDId());
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicIndexGet::FInputOrderSensitive
//
//	@doc:
//		Is input order sensitive
//
//---------------------------------------------------------------------------
BOOL
CLogicalDynamicIndexGet::FInputOrderSensitive() const
{
	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicIndexGet::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalDynamicIndexGet::PxfsCandidates
	(
	IMemoryPool *memory_pool
	)
const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfDynamicIndexGet2DynamicIndexScan);
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicIndexGet::PstatsDerive
//
//	@doc:
//		Load up statistics from metadata
//
//---------------------------------------------------------------------------

IStatistics *
CLogicalDynamicIndexGet::PstatsDerive
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	StatsArray *stats_ctxt
	)
	const
{
	return CStatisticsUtils::DeriveStatsForIndexGet(memory_pool, exprhdl, stats_ctxt);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicIndexGet::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalDynamicIndexGet::OsPrint
	(
	IOstream &os
	)
const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}

	os << SzId() << " ";
	// index name
	os << "  Index Name: (";
	m_pindexdesc->Name().OsPrint(os);
	// table alias name
	os <<")";
	os << ", Table Name: (";
	m_pnameAlias->OsPrint(os);
	os <<"), ";
	m_part_constraint->OsPrint(os);
	os << ", Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrOutput);
	os << "] Scan Id: " << m_scan_id << "." << m_ulSecondaryScanId;

	if (!m_part_constraint->IsConstraintUnbounded())
	{
		os << ", ";
		m_part_constraint->OsPrint(os);
	}
	
	return os;
}

// EOF

