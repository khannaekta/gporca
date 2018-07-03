//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp
//
//	@filename:
//		CLogicalConstTableGet.cpp
//
//	@doc:
//		Implementation of const table access
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalConstTableGet.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefTable.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::CLogicalConstTableGet
//
//	@doc:
//		ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalConstTableGet::CLogicalConstTableGet
	(
	IMemoryPool *memory_pool
	)
	:
	CLogical(memory_pool),
	m_pdrgpcoldesc(NULL),
	m_pdrgpdrgpdatum(NULL),
	m_pdrgpcrOutput(NULL)
{
	m_fPattern = true;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::CLogicalConstTableGet
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalConstTableGet::CLogicalConstTableGet
	(
	IMemoryPool *memory_pool,
	ColumnDescrArray *pdrgpcoldesc,
	IDatumArrays *pdrgpdrgpdatum
	)
	:
	CLogical(memory_pool),
	m_pdrgpcoldesc(pdrgpcoldesc),
	m_pdrgpdrgpdatum(pdrgpdrgpdatum),
	m_pdrgpcrOutput(NULL)
{
	GPOS_ASSERT(NULL != pdrgpcoldesc);
	GPOS_ASSERT(NULL != pdrgpdrgpdatum);

	// generate a default column set for the list of column descriptors
	m_pdrgpcrOutput = PdrgpcrCreateMapping(memory_pool, pdrgpcoldesc, UlOpId());
	
#ifdef GPOS_DEBUG
	for (ULONG ul = 0; ul < pdrgpdrgpdatum->Size(); ul++)
	{
		IDatumArray *pdrgpdatum = (*pdrgpdrgpdatum)[ul];
		GPOS_ASSERT(pdrgpdatum->Size() == pdrgpcoldesc->Size());
	}
#endif
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::CLogicalConstTableGet
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalConstTableGet::CLogicalConstTableGet
	(
	IMemoryPool *memory_pool,
	ColRefArray *pdrgpcrOutput,
	IDatumArrays *pdrgpdrgpdatum
	)
	:
	CLogical(memory_pool),
	m_pdrgpcoldesc(NULL),
	m_pdrgpdrgpdatum(pdrgpdrgpdatum),
	m_pdrgpcrOutput(pdrgpcrOutput)
{
	GPOS_ASSERT(NULL != pdrgpcrOutput);
	GPOS_ASSERT(NULL != pdrgpdrgpdatum);

	// generate column descriptors for the given output columns
	m_pdrgpcoldesc = PdrgpcoldescMapping(memory_pool, pdrgpcrOutput);

#ifdef GPOS_DEBUG
	for (ULONG ul = 0; ul < pdrgpdrgpdatum->Size(); ul++)
	{
		IDatumArray *pdrgpdatum = (*pdrgpdrgpdatum)[ul];
		GPOS_ASSERT(pdrgpdatum->Size() == m_pdrgpcoldesc->Size());
	}
#endif
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::~CLogicalConstTableGet
//
//	@doc:
//		dtor
//
//---------------------------------------------------------------------------
CLogicalConstTableGet::~CLogicalConstTableGet()
{
	CRefCount::SafeRelease(m_pdrgpcoldesc);
	CRefCount::SafeRelease(m_pdrgpdrgpdatum);
	CRefCount::SafeRelease(m_pdrgpcrOutput);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalConstTableGet::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(),
								gpos::CombineHashes(
										gpos::HashPtr<ColumnDescrArray>(m_pdrgpcoldesc),
										gpos::HashPtr<IDatumArrays>(m_pdrgpdrgpdatum)));
	ulHash = gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrOutput));

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CLogicalConstTableGet::Matches
	(
	COperator *pop
	)
	const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	CLogicalConstTableGet *popCTG = CLogicalConstTableGet::PopConvert(pop);
		
	// match if column descriptors, const values and output columns are identical
	return m_pdrgpcoldesc->Equals(popCTG->Pdrgpcoldesc()) &&
			m_pdrgpdrgpdatum->Equals(popCTG->Pdrgpdrgpdatum()) &&
			m_pdrgpcrOutput->Equals(popCTG->PdrgpcrOutput());
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalConstTableGet::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	ColRefArray *colref_array = NULL;
	if (must_exist)
	{
		colref_array = CUtils::PdrgpcrRemapAndCreate(memory_pool, m_pdrgpcrOutput, colref_mapping);
	}
	else
	{
		colref_array = CUtils::PdrgpcrRemap(memory_pool, m_pdrgpcrOutput, colref_mapping, must_exist);
	}
	m_pdrgpdrgpdatum->AddRef();

	return GPOS_NEW(memory_pool) CLogicalConstTableGet(memory_pool, colref_array, m_pdrgpdrgpdatum);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::PcrsDeriveOutput
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalConstTableGet::PcrsDeriveOutput
	(
	IMemoryPool *memory_pool,
	CExpressionHandle & // exprhdl
	)
{
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrs->Include(m_pdrgpcrOutput);

	return pcrs;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalConstTableGet::Maxcard
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle & // exprhdl
	)
	const
{
	return CMaxCard(m_pdrgpdrgpdatum->Size());
}	


//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::FInputOrderSensitive
//
//	@doc:
//		Not called for leaf operators
//
//---------------------------------------------------------------------------
BOOL
CLogicalConstTableGet::FInputOrderSensitive() const
{
	GPOS_ASSERT(!"Unexpected function call of FInputOrderSensitive");
	return false;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalConstTableGet::PxfsCandidates
	(
	IMemoryPool *memory_pool
	) 
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfImplementConstTableGet);
	return xform_set;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::PdrgpcoldescMapping
//
//	@doc:
//		Construct column descriptors from column references
//
//---------------------------------------------------------------------------
ColumnDescrArray *
CLogicalConstTableGet::PdrgpcoldescMapping
	(
	IMemoryPool *memory_pool,
	ColRefArray *colref_array
	)
	const
{
	GPOS_ASSERT(NULL != colref_array);
	ColumnDescrArray *pdrgpcoldesc = GPOS_NEW(memory_pool) ColumnDescrArray(memory_pool);

	const ULONG length = colref_array->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CColRef *colref = (*colref_array)[ul];

		ULONG length = gpos::ulong_max;
		if (CColRef::EcrtTable == colref->Ecrt())
		{
			CColRefTable *pcrTable = CColRefTable::PcrConvert(colref);
			length = pcrTable->Width();
		}

		CColumnDescriptor *pcoldesc = GPOS_NEW(memory_pool) CColumnDescriptor
													(
													memory_pool,
													colref->RetrieveType(),
													colref->TypeModifier(),
													colref->Name(),
													ul + 1, //attno
													true, // IsNullable
													length
													);
		pdrgpcoldesc->Append(pcoldesc);
	}

	return pdrgpcoldesc;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalConstTableGet::PstatsDerive
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	StatsArray * // not used
	)
	const
{
	GPOS_ASSERT(Esp(exprhdl) > EspNone);
	CReqdPropRelational *prprel = CReqdPropRelational::GetReqdRelationalProps(exprhdl.Prp());
	CColRefSet *pcrs = prprel->PcrsStat();
	ULongPtrArray *col_ids = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);
	pcrs->ExtractColIds(memory_pool, col_ids);
	ULongPtrArray *pdrgpulColWidth = CUtils::Pdrgpul(memory_pool, m_pdrgpcrOutput);

	IStatistics *stats = CStatistics::MakeDummyStats
										(
										memory_pool,
										col_ids,
										pdrgpulColWidth,
										m_pdrgpdrgpdatum->Size()
										);

	// clean up
	col_ids->Release();
	pdrgpulColWidth->Release();

	return stats;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalConstTableGet::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalConstTableGet::OsPrint
	(
	IOstream &os
	)
	const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}
	else
	{
		os << SzId() << " ";
		os << "Columns: [";
		CUtils::OsPrintDrgPcr(os, m_pdrgpcrOutput);
		os << "] ";
		os << "Values: [";
		for (ULONG ulA = 0; ulA < m_pdrgpdrgpdatum->Size(); ulA++)
		{
			if (0 < ulA)
			{
				os << "; ";
			}
			os << "(";
			IDatumArray *pdrgpdatum = (*m_pdrgpdrgpdatum)[ulA];
			
			const ULONG length = pdrgpdatum->Size();
			for (ULONG ulB = 0; ulB < length; ulB++)
			{
				IDatum *datum = (*pdrgpdatum)[ulB];
				datum->OsPrint(os);

				if (ulB < length-1)
				{
					os << ", ";
				}
			}
			os << ")";
		}
		os << "]";

	}
		
	return os;
}



// EOF

