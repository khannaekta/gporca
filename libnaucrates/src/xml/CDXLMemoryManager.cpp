//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLMemoryManager.cpp
//
//	@doc:
//		Implementation of the DXL memory manager to be plugged in Xerces.
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "naucrates/dxl/xml/CDXLMemoryManager.h"

using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLMemoryManager::CDXLMemoryManager
//
//	@doc:
//		Constructs a memory manager around a given memory pool.
//
//---------------------------------------------------------------------------
CDXLMemoryManager::CDXLMemoryManager(IMemoryPool *memory_pool) : m_memory_pool(memory_pool)
{
	GPOS_ASSERT(NULL != m_memory_pool);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLMemoryManager::allocate
//
//	@doc:
//		Memory allocation.
//
//---------------------------------------------------------------------------
void *
CDXLMemoryManager::allocate(XMLSize_t xmlsize)
{
	GPOS_ASSERT(NULL != m_memory_pool);
	return GPOS_NEW_ARRAY(m_memory_pool, BYTE, xmlsize);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLMemoryManager::deallocate
//
//	@doc:
//		Memory deallocation.
//
//---------------------------------------------------------------------------
void
CDXLMemoryManager::deallocate(void *pv)
{
	GPOS_DELETE_ARRAY(reinterpret_cast<BYTE *>(pv));
}


// EOF
