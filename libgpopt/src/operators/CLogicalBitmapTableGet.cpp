//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CLogicalBitmapTableGet.cpp
//
//	@doc:
//		Logical operator for table access via bitmap indexes.
//
//	@owner:
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpopt/operators/CLogicalBitmapTableGet.h"

#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/xforms/CXform.h"

#include "naucrates/statistics/CStatisticsUtils.h"

using namespace gpopt;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalBitmapTableGet::CLogicalBitmapTableGet
//
//	@doc:
//		Ctor
//		Takes ownership of ptabdesc, pnameTableAlias and pdrgpcrOutput.
//
//---------------------------------------------------------------------------
CLogicalBitmapTableGet::CLogicalBitmapTableGet
	(
	IMemoryPool *memory_pool,
	CTableDescriptor *ptabdesc,
	ULONG ulOriginOpId,
	const CName *pnameTableAlias,
	ColRefArray *pdrgpcrOutput
	)
	:
	CLogical(memory_pool),
	m_ptabdesc(ptabdesc),
	m_ulOriginOpId(ulOriginOpId),
	m_pnameTableAlias(pnameTableAlias),
	m_pdrgpcrOutput(pdrgpcrOutput)
{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != pnameTableAlias);
	GPOS_ASSERT(NULL != pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalBitmapTableGet::CLogicalBitmapTableGet
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalBitmapTableGet::CLogicalBitmapTableGet
	(
	IMemoryPool *memory_pool
	)
	:
	CLogical(memory_pool),
	m_ptabdesc(NULL),
	m_ulOriginOpId(gpos::ulong_max),
	m_pnameTableAlias(NULL),
	m_pdrgpcrOutput(NULL)
{}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalBitmapTableGet::~CLogicalBitmapTableGet
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalBitmapTableGet::~CLogicalBitmapTableGet()
{
	CRefCount::SafeRelease(m_ptabdesc);
	CRefCount::SafeRelease(m_pdrgpcrOutput);

	GPOS_DELETE(m_pnameTableAlias);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalBitmapTableGet::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalBitmapTableGet::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(), m_ptabdesc->MDId()->HashValue());
	ulHash = gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrOutput));

	return ulHash;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalBitmapTableGet::Matches
//
//	@doc:
//		Match this operator with the given one.
//
//---------------------------------------------------------------------------
BOOL
CLogicalBitmapTableGet::Matches
	(
	COperator *pop
	)
	const
{
	return CUtils::FMatchBitmapScan(this, pop);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalBitmapTableGet::PcrsDeriveOutput
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalBitmapTableGet::PcrsDeriveOutput
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &
#ifdef GPOS_DEBUG
		exprhdl
#endif
	)
{
	GPOS_ASSERT(exprhdl.Pop() == this);

	return GPOS_NEW(memory_pool) CColRefSet(memory_pool, m_pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalBitmapTableGet::PcrsDeriveOuter
//
//	@doc:
//		Derive outer references
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalBitmapTableGet::PcrsDeriveOuter
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	return PcrsDeriveOuterIndexGet(memory_pool, exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalBitmapTableGet::PpcDeriveConstraint
//
//	@doc:
//		Derive the constraint property.
//
//---------------------------------------------------------------------------
CPropConstraint *
CLogicalBitmapTableGet::PpcDeriveConstraint
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
//		CLogicalBitmapTableGet::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalBitmapTableGet::PstatsDerive
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
//		CLogicalBitmapTableGet::OsPrint
//
//	@doc:
//		Debug print of this operator
//
//---------------------------------------------------------------------------
IOstream &
CLogicalBitmapTableGet::OsPrint
	(
	IOstream &os
	)
	const
{
	os << SzId() << " ";
	os << ", Table Name: (";
	m_ptabdesc->Name().OsPrint(os);
	os <<")";
	os << ", Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrOutput);
	os << "]";

	return os;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalBitmapTableGet::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalBitmapTableGet::PopCopyWithRemappedColumns
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
	CName *pnameAlias = GPOS_NEW(memory_pool) CName(memory_pool, *m_pnameTableAlias);

	m_ptabdesc->AddRef();

	return GPOS_NEW(memory_pool) CLogicalBitmapTableGet(memory_pool, m_ptabdesc, m_ulOriginOpId, pnameAlias, pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalBitmapTableGet::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalBitmapTableGet::PxfsCandidates
	(
	IMemoryPool *memory_pool
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfImplementBitmapTableGet);

	return xform_set;
}

// EOF
