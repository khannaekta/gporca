//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXformContext.h
//
//	@doc:
//		Context container passed to every application of a transformation
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformContext_H
#define GPOPT_CXformContext_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpopt/operators/CPatternTree.h"

namespace gpopt
{
	using namespace gpos;
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CXformContext
	//
	//	@doc:
	//		context container
	//
	//---------------------------------------------------------------------------
	class CXformContext : public CRefCount
	{

		private:

			// Memory pool
			IMemoryPool *m_memory_pool;

			// private copy ctor
			CXformContext(const CXformContext &);

		public:
		
			// ctor
			explicit
			CXformContext
				(
				IMemoryPool *memory_pool
				)
				: 
				m_memory_pool(memory_pool)
			{
			}

			// dtor
			~CXformContext() {}


			// accessor
			inline
			IMemoryPool *Pmp() const
			{
				return m_memory_pool;
			}

	}; // class CXformContext

}


#endif // !GPOPT_CXformContext_H

// EOF
