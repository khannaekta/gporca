//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CLogicalDynamicBitmapTableGet.cpp
//
//	@doc:
//		Logical operator for dynamic table access via bitmap indexes.
//
//	@owner:
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpopt/operators/CLogicalDynamicBitmapTableGet.h"

#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/xforms/CXform.h"

#include "naucrates/statistics/CStatisticsUtils.h"

using namespace gpopt;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicBitmapTableGet::CLogicalDynamicBitmapTableGet
//
//	@doc:
//		Ctor
//		Takes ownership of ptabdesc, pnameTableAlias and pdrgpcrOutput.
//
//---------------------------------------------------------------------------
CLogicalDynamicBitmapTableGet::CLogicalDynamicBitmapTableGet
	(
	IMemoryPool *memory_pool,
	CTableDescriptor *ptabdesc,
	ULONG ulOriginOpId,
	const CName *pnameTableAlias,
	ULONG ulPartIndex,
	ColRefArray *pdrgpcrOutput,
	ColRefArrays *pdrgpdrgpcrPart,
	ULONG ulSecondaryPartIndexId,
	BOOL is_partial,
	CPartConstraint *ppartcnstr,
	CPartConstraint *ppartcnstrRel
	)
	:
	CLogicalDynamicGetBase
	(
	memory_pool,
	pnameTableAlias,
	ptabdesc,
	ulPartIndex,
	pdrgpcrOutput,
	pdrgpdrgpcrPart,
	ulSecondaryPartIndexId,
	is_partial,
	ppartcnstr,
	ppartcnstrRel
	),
	m_ulOriginOpId(ulOriginOpId)

{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicBitmapTableGet::CLogicalDynamicBitmapTableGet
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalDynamicBitmapTableGet::CLogicalDynamicBitmapTableGet
	(
	IMemoryPool *memory_pool
	)
	:
	CLogicalDynamicGetBase(memory_pool),
	m_ulOriginOpId(gpos::ulong_max)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicBitmapTableGet::~CLogicalDynamicBitmapTableGet
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalDynamicBitmapTableGet::~CLogicalDynamicBitmapTableGet()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicBitmapTableGet::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalDynamicBitmapTableGet::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(), m_ptabdesc->MDId()->HashValue());
	ulHash = gpos::CombineHashes(ulHash, gpos::HashValue(&m_scan_id));
	ulHash = gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrOutput));

	return ulHash;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicBitmapTableGet::Matches
//
//	@doc:
//		Match this operator with the given one.
//
//---------------------------------------------------------------------------
BOOL
CLogicalDynamicBitmapTableGet::Matches
	(
	COperator *pop
	)
	const
{
	return CUtils::FMatchDynamicBitmapScan(this, pop);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicBitmapTableGet::PpcDeriveConstraint
//
//	@doc:
//		Derive the constraint property.
//
//---------------------------------------------------------------------------
CPropConstraint *
CLogicalDynamicBitmapTableGet::PpcDeriveConstraint
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
	const
{
	return PpcDeriveConstraintFromTableWithPredicates(memory_pool, exprhdl, m_ptabdesc, m_pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicBitmapTableGet::PcrsDeriveOuter
//
//	@doc:
//		Derive outer references
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalDynamicBitmapTableGet::PcrsDeriveOuter
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	return PcrsDeriveOuterIndexGet(memory_pool, exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicBitmapTableGet::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalDynamicBitmapTableGet::PstatsDerive
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	StatsArray *stats_ctxt
	)
	const
{
	return CStatisticsUtils::DeriveStatsForBitmapTableGet(memory_pool, exprhdl, stats_ctxt);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicBitmapTableGet::OsPrint
//
//	@doc:
//		Debug print of this operator
//
//---------------------------------------------------------------------------
IOstream &
CLogicalDynamicBitmapTableGet::OsPrint
	(
	IOstream &os
	)
	const
{
	os << SzId() << " ";
	os << ", Table Name: (";
	m_ptabdesc->Name().OsPrint(os);
	os <<") Scan Id: " << m_scan_id;
	os << ", Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrOutput);
	os << "]";

	return os;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicBitmapTableGet::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalDynamicBitmapTableGet::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	ColRefArray *pdrgpcrOutput = NULL;
	if (must_exist)
	{
		pdrgpcrOutput = CUtils::PdrgpcrRemapAndCreate(memory_pool, m_pdrgpcrOutput, colref_mapping);
	}
	else
	{
		pdrgpcrOutput = CUtils::PdrgpcrRemap(memory_pool, m_pdrgpcrOutput, colref_mapping, must_exist);
	}
	CName *pnameAlias = GPOS_NEW(memory_pool) CName(memory_pool, *m_pnameAlias);

	m_ptabdesc->AddRef();

	ColRefArrays *pdrgpdrgpcrPart = CUtils::PdrgpdrgpcrRemap(memory_pool, m_pdrgpdrgpcrPart, colref_mapping, must_exist);
	CPartConstraint *ppartcnstr = m_part_constraint->PpartcnstrCopyWithRemappedColumns(memory_pool, colref_mapping, must_exist);
	CPartConstraint *ppartcnstrRel = m_ppartcnstrRel->PpartcnstrCopyWithRemappedColumns(memory_pool, colref_mapping, must_exist);

	return GPOS_NEW(memory_pool) CLogicalDynamicBitmapTableGet
					(
					memory_pool,
					m_ptabdesc,
					m_ulOriginOpId,
					pnameAlias,
					m_scan_id,
					pdrgpcrOutput,
					pdrgpdrgpcrPart,
					m_ulSecondaryScanId,
					m_is_partial,
					ppartcnstr,
					ppartcnstrRel
					);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalDynamicBitmapTableGet::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalDynamicBitmapTableGet::PxfsCandidates
	(
	IMemoryPool *memory_pool
	)
const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfImplementDynamicBitmapTableGet);

	return xform_set;
}

// EOF
