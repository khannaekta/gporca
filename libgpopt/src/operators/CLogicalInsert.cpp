//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalInsert.cpp
//
//	@doc:
//		Implementation of logical Insert operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/base/CPartIndexMap.h"

#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalInsert.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInsert::CLogicalInsert
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalInsert::CLogicalInsert
	(
	IMemoryPool *memory_pool
	)
	:
	CLogical(memory_pool),
	m_ptabdesc(NULL),
	m_pdrgpcrSource(NULL)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInsert::CLogicalInsert
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalInsert::CLogicalInsert
	(
	IMemoryPool *memory_pool,
	CTableDescriptor *ptabdesc,
	DrgPcr *pdrgpcrSource
	)
	:
	CLogical(memory_pool),
	m_ptabdesc(ptabdesc),
	m_pdrgpcrSource(pdrgpcrSource)

{
	GPOS_ASSERT(NULL != ptabdesc);
	GPOS_ASSERT(NULL != pdrgpcrSource);

	m_pcrsLocalUsed->Include(m_pdrgpcrSource);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInsert::~CLogicalInsert
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalInsert::~CLogicalInsert()
{
	CRefCount::SafeRelease(m_ptabdesc);
	CRefCount::SafeRelease(m_pdrgpcrSource);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInsert::Matches
//
//	@doc:
//		Match function
//
//---------------------------------------------------------------------------
BOOL
CLogicalInsert::Matches
	(
	COperator *pop
	)
	const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	CLogicalInsert *popInsert = CLogicalInsert::PopConvert(pop);

	return m_ptabdesc->MDId()->Equals(popInsert->Ptabdesc()->MDId()) &&
			m_pdrgpcrSource->Equals(popInsert->PdrgpcrSource());
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInsert::HashValue
//
//	@doc:
//		Hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalInsert::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(), m_ptabdesc->MDId()->HashValue());
	ulHash = gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrSource));

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInsert::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalInsert::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	DrgPcr *colref_array = CUtils::PdrgpcrRemap(memory_pool, m_pdrgpcrSource, colref_mapping, must_exist);
	m_ptabdesc->AddRef();

	return GPOS_NEW(memory_pool) CLogicalInsert(memory_pool, m_ptabdesc, colref_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInsert::PcrsDeriveOutput
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalInsert::PcrsDeriveOutput
	(
	IMemoryPool *memory_pool,
	CExpressionHandle & //exprhdl
	)
{
	CColRefSet *pcrsOutput = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrsOutput->Include(m_pdrgpcrSource);
	return pcrsOutput;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInsert::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalInsert::PkcDeriveKeys
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	return PkcDeriveKeysPassThru(exprhdl, 0 /* ulChild */);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInsert::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalInsert::Maxcard
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
//		CLogicalInsert::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalInsert::PxfsCandidates
	(
	IMemoryPool *memory_pool
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfInsert2DML);
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInsert::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalInsert::PstatsDerive
	(
	IMemoryPool *, // memory_pool,
	CExpressionHandle &exprhdl,
	StatsArray * // not used
	)
	const
{
	return PstatsPassThruOuter(exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInsert::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalInsert::OsPrint
	(
	IOstream &os
	)
	const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}

	os	<< SzId()
		<< " (";
	m_ptabdesc->Name().OsPrint(os);
	os << "), Source Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcrSource);
	os	<< "]";

	return os;
}

// EOF

