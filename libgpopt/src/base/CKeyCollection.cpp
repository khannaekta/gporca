//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CKeyCollection.cpp
//
//	@doc:
//		Implementation of key collections
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/base/CUtils.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::CKeyCollection
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CKeyCollection::CKeyCollection
	(
	IMemoryPool *memory_pool
	)
	:
	m_memory_pool(memory_pool),
	m_pdrgpcrs(NULL)
{
	GPOS_ASSERT(NULL != memory_pool);
	
	m_pdrgpcrs = GPOS_NEW(memory_pool) DrgPcrs(memory_pool);
}


//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::CKeyCollection
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CKeyCollection::CKeyCollection
	(
	IMemoryPool *memory_pool,
	CColRefSet *pcrs
	)
	:
	m_memory_pool(memory_pool),
	m_pdrgpcrs(NULL)
{
	GPOS_ASSERT(NULL != pcrs && 0 < pcrs->Size());
	
	m_pdrgpcrs = GPOS_NEW(memory_pool) DrgPcrs(memory_pool);

	// we own the set
	Add(pcrs);
}
	

//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::CKeyCollection
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CKeyCollection::CKeyCollection
	(
	IMemoryPool *memory_pool,
	DrgPcr *colref_array
	)
	:
	m_memory_pool(memory_pool),
	m_pdrgpcrs(NULL)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != colref_array && 0 < colref_array->Size());
	
	m_pdrgpcrs = GPOS_NEW(memory_pool) DrgPcrs(memory_pool);
	
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrs->Include(colref_array);
	Add(pcrs);

	// we own the array
	colref_array->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::~CKeyCollection
//
//	@doc:
//		dtor
//
//---------------------------------------------------------------------------
CKeyCollection::~CKeyCollection()
{
	m_pdrgpcrs->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::Add
//
//	@doc:
//		Add key set to collection; takes ownership
//
//---------------------------------------------------------------------------
void
CKeyCollection::Add
	(
	CColRefSet *pcrs
	)
{
	GPOS_ASSERT(!FKey(pcrs) && "no duplicates allowed");
	
	m_pdrgpcrs->Append(pcrs);
}


//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::FKey
//
//	@doc:
//		Check if set constitutes key
//
//---------------------------------------------------------------------------
BOOL
CKeyCollection::FKey
	(
	const CColRefSet *pcrs,
	BOOL fExactMatch // true: match keys exactly,
					//  false: match keys by inclusion
	)
	const
{
	const ULONG ulSets = m_pdrgpcrs->Size();
	for (ULONG ul = 0; ul < ulSets; ul++)
	{
		if (fExactMatch)
		{
			// accept only exact matches
			if (pcrs->Equals((*m_pdrgpcrs)[ul]))
			{
				return true;
			}
		}
		else
		{
			// if given column set includes a key, then it is also a key
			if (pcrs->ContainsAll((*m_pdrgpcrs)[ul]))
			{
				return true;
			}
		}
	}
	
	return false;
}



//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::FKey
//
//	@doc:
//		Check if array constitutes key
//
//---------------------------------------------------------------------------
BOOL
CKeyCollection::FKey
	(
	IMemoryPool *memory_pool,
	const DrgPcr *colref_array
	)
	const
{
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrs->Include(colref_array);
	
	BOOL fKey = FKey(pcrs);
	pcrs->Release();
	
	return fKey;
}


//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::PdrgpcrTrim
//
//	@doc:
//		Return first subsumed key as column array
//
//---------------------------------------------------------------------------
DrgPcr *
CKeyCollection::PdrgpcrTrim
	(
	IMemoryPool *memory_pool,
	const DrgPcr *colref_array
	)
	const
{
	DrgPcr *pdrgpcrTrim = NULL;
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrs->Include(colref_array);

	const ULONG ulSets = m_pdrgpcrs->Size();
	for(ULONG ul = 0; ul < ulSets; ul++)
	{
		CColRefSet *pcrsKey = (*m_pdrgpcrs)[ul];
		if (pcrs->ContainsAll(pcrsKey))
		{
			pdrgpcrTrim = pcrsKey->Pdrgpcr(memory_pool);
			break;
		}
	}
	pcrs->Release();

	return pdrgpcrTrim;
}	

//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::PdrgpcrKey
//
//	@doc:
//		Extract a key
//
//---------------------------------------------------------------------------
DrgPcr *
CKeyCollection::PdrgpcrKey
	(
	IMemoryPool *memory_pool
	)
	const
{
	if (0 == m_pdrgpcrs->Size())
	{
		return NULL;
	}

	GPOS_ASSERT(NULL != (*m_pdrgpcrs)[0]);

	DrgPcr *colref_array = (*m_pdrgpcrs)[0]->Pdrgpcr(memory_pool);
	return colref_array;
}


//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::PdrgpcrHashableKey
//
//	@doc:
//		Extract a hashable key
//
//---------------------------------------------------------------------------
DrgPcr *
CKeyCollection::PdrgpcrHashableKey
	(
	IMemoryPool *memory_pool
	)
	const
{
	const ULONG ulSets = m_pdrgpcrs->Size();
	for(ULONG ul = 0; ul < ulSets; ul++)
	{
		DrgPcr *pdrgpcrKey = (*m_pdrgpcrs)[ul]->Pdrgpcr(memory_pool);
		if (CUtils::IsHashable(pdrgpcrKey))
		{
			return pdrgpcrKey;
		}
		pdrgpcrKey->Release();
	}

	// no hashable key is found
	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::PdrgpcrKey
//
//	@doc:
//		Extract the key at a position
//
//---------------------------------------------------------------------------
DrgPcr *
CKeyCollection::PdrgpcrKey
	(
	IMemoryPool *memory_pool,
	ULONG ulIndex
	)
	const
{
	if (0 == m_pdrgpcrs->Size())
	{
		return NULL;
	}
	
	GPOS_ASSERT(NULL != (*m_pdrgpcrs)[ulIndex]);
	
	DrgPcr *colref_array = (*m_pdrgpcrs)[ulIndex]->Pdrgpcr(memory_pool);
	return colref_array;
}


//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::PcrsKey
//
//	@doc:
//		Extract key at given position
//
//---------------------------------------------------------------------------
CColRefSet *
CKeyCollection::PcrsKey
	(
	IMemoryPool *memory_pool,
	ULONG ulIndex
	)
	const
{
	if (0 == m_pdrgpcrs->Size())
	{
		return NULL;
	}

	GPOS_ASSERT(NULL != (*m_pdrgpcrs)[ulIndex]);

	CColRefSet *pcrsKey = (*m_pdrgpcrs)[ulIndex];
	return GPOS_NEW(memory_pool) CColRefSet(memory_pool, *pcrsKey);
}


//---------------------------------------------------------------------------
//	@function:
//		CKeyCollection::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CKeyCollection::OsPrint
	(
	IOstream &os
	)
	const
{
	os << " Keys: (";

	const ULONG ulSets = m_pdrgpcrs->Size();
	for(ULONG ul = 0; ul < ulSets; ul++)
	{
		if (0 < ul)
		{
			os << ", ";
		}

		GPOS_ASSERT(NULL != (*m_pdrgpcrs)[ul]);
		os << "[" << (*(*m_pdrgpcrs)[ul]) << "]";
	}
	
	return os << ")";
}


// EOF

