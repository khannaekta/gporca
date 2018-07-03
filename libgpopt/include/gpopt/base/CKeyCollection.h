//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 - 2012 EMC CORP.
//
//	@filename:
//		CKeyCollection.h
//
//	@doc:
//		Encodes key sets for a relation
//---------------------------------------------------------------------------
#ifndef GPOPT_CKeyCollection_H
#define GPOPT_CKeyCollection_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "gpopt/base/CColRefSet.h"


namespace gpopt
{
	using namespace gpos;
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CKeyCollection
	//
	//	@doc:
	//		Captures sets of keys for a relation
	//
	//---------------------------------------------------------------------------
	class CKeyCollection : public CRefCount
	{

		private:
		
			// memory pool
			IMemoryPool *m_memory_pool;
		
			// array of key sets
			ColRefSetArray *m_pdrgpcrs;
			
			// private copy ctor
			CKeyCollection(const CKeyCollection &);

		public:

			// ctors
			explicit
			CKeyCollection(IMemoryPool *memory_pool);
			CKeyCollection(IMemoryPool *memory_pool, CColRefSet *pcrs);
			CKeyCollection(IMemoryPool *memory_pool, ColRefArray *colref_array);

			// dtor
			virtual 
			~CKeyCollection();
			
			// add individual set -- takes ownership
			void Add(CColRefSet *pcrs);
			
			// check if set forms a key
			BOOL FKey(const CColRefSet *pcrs, BOOL fExactMatch = true) const;

			// check if an array of columns constitutes a key
			BOOL FKey(IMemoryPool *memory_pool, const ColRefArray *colref_array) const;
			
			// trim off non-key columns
			ColRefArray *PdrgpcrTrim(IMemoryPool *memory_pool, const ColRefArray *colref_array) const;
			
			// extract a key
			ColRefArray *PdrgpcrKey(IMemoryPool *memory_pool) const;

			// extract a hashable key
			ColRefArray *PdrgpcrHashableKey(IMemoryPool *memory_pool) const;

			// extract key at given position
			ColRefArray *PdrgpcrKey(IMemoryPool *memory_pool, ULONG ul) const;

			// extract key at given position
			CColRefSet *PcrsKey(IMemoryPool *memory_pool, ULONG ul) const;
			
			// number of keys
			ULONG Keys() const
			{
				return m_pdrgpcrs->Size();
			}

			// print
			virtual
			IOstream &OsPrint(IOstream &os) const;

	}; // class CKeyCollection

 	// shorthand for printing
	inline
	IOstream &operator << (IOstream &os, CKeyCollection &kc)
	{
		return kc.OsPrint(os);
	}

}

#endif // !GPOPT_CKeyCollection_H

// EOF
