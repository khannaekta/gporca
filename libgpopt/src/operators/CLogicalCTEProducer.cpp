//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalCTEProducer.cpp
//
//	@doc:
//		Implementation of CTE producer operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalCTEProducer.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalCTEProducer::CLogicalCTEProducer
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalCTEProducer::CLogicalCTEProducer
	(
	IMemoryPool *memory_pool
	)
	:
	CLogical(memory_pool),
	m_id(0),
	m_pdrgpcr(NULL),
	m_pcrsOutput(NULL)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalCTEProducer::CLogicalCTEProducer
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalCTEProducer::CLogicalCTEProducer
	(
	IMemoryPool *memory_pool,
	ULONG id,
	ColRefArray *colref_array
	)
	:
	CLogical(memory_pool),
	m_id(id),
	m_pdrgpcr(colref_array)
{
	GPOS_ASSERT(NULL != colref_array);

	m_pcrsOutput = GPOS_NEW(memory_pool) CColRefSet(memory_pool, m_pdrgpcr);
	GPOS_ASSERT(m_pdrgpcr->Size() == m_pcrsOutput->Size());

	m_pcrsLocalUsed->Include(m_pdrgpcr);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalCTEProducer::~CLogicalCTEProducer
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalCTEProducer::~CLogicalCTEProducer()
{
	CRefCount::SafeRelease(m_pdrgpcr);
	CRefCount::SafeRelease(m_pcrsOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalCTEProducer::PcrsDeriveOutput
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalCTEProducer::PcrsDeriveOutput
	(
	IMemoryPool *, //memory_pool,
	CExpressionHandle & //exprhdl
	)
{
	m_pcrsOutput->AddRef();
	return m_pcrsOutput;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalCTEProducer::PcrsDeriveNotNull
//
//	@doc:
//		Derive not nullable output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalCTEProducer::PcrsDeriveNotNull
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
	const
{
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool, m_pdrgpcr);
	pcrs->Intersection(exprhdl.GetRelationalProperties(0)->PcrsNotNull());

	return pcrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalCTEProducer::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalCTEProducer::PkcDeriveKeys
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
//		CLogicalCTEProducer::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalCTEProducer::Maxcard
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
//		CLogicalCTEProducer::Matches
//
//	@doc:
//		Match function
//
//---------------------------------------------------------------------------
BOOL
CLogicalCTEProducer::Matches
	(
	COperator *pop
	)
	const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	CLogicalCTEProducer *popCTEProducer = CLogicalCTEProducer::PopConvert(pop);

	return m_id == popCTEProducer->UlCTEId() &&
			m_pdrgpcr->Equals(popCTEProducer->Pdrgpcr());
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalCTEProducer::HashValue
//
//	@doc:
//		Hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalCTEProducer::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(), m_id);
	ulHash = gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcr));

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalCTEProducer::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalCTEProducer::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	ColRefArray *colref_array = CUtils::PdrgpcrRemap(memory_pool, m_pdrgpcr, colref_mapping, must_exist);

	return GPOS_NEW(memory_pool) CLogicalCTEProducer(memory_pool, m_id, colref_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalCTEProducer::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalCTEProducer::PxfsCandidates
	(
	IMemoryPool *memory_pool
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfImplementCTEProducer);
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalCTEProducer::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalCTEProducer::OsPrint
	(
	IOstream &os
	)
	const
{
	os << SzId() << " (";
	os << m_id;
	os << "), Columns: [";
	CUtils::OsPrintDrgPcr(os, m_pdrgpcr);
	os	<< "]";

	return os;
}

// EOF
