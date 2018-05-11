//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CRangeTest.h
//
//	@doc:
//		Test for ranges
//---------------------------------------------------------------------------
#ifndef GPOPT_CRangeTest_H
#define GPOPT_CRangeTest_H

#include "gpos/base.h"

#include "gpopt/base/CRange.h"

#include "unittest/gpopt/CTestUtils.h"

namespace gpopt
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CRangeTest
	//
	//	@doc:
	//		Static unit tests for ranges
	//
	//---------------------------------------------------------------------------
	class CRangeTest
	{
		typedef IDatum * (*PfPdatum)(IMemoryPool *memory_pool, INT i);

		private:

			static
			GPOS_RESULT EresInitAndCheckRanges
						(
						IMemoryPool *memory_pool,
						IMDId *mdid,
						PfPdatum pf
						);

			static
			void TestRangeRelationship
					(
					IMemoryPool *memory_pool,
					CRange *prange1,
					CRange *prange2,
					CRange *prange3,
					CRange *prange4,
					CRange *prange5
					);

			static
			void PrintRange
					(
					IMemoryPool *memory_pool,
					CColRef *colref,
					CRange *prange
					);

			// int2 datum
			static
			IDatum *CreateInt2Datum(IMemoryPool *memory_pool, INT i);

			// int4 datum
			static
			IDatum *CreateInt4Datum(IMemoryPool *memory_pool, INT i);

			// int8 datum
			static
			IDatum *CreateInt8Datum(IMemoryPool *memory_pool, INT li);

		public:

			// unittests
			static GPOS_RESULT EresUnittest();
			static GPOS_RESULT EresUnittest_CRangeInt2();
			static GPOS_RESULT EresUnittest_CRangeInt4();
			static GPOS_RESULT EresUnittest_CRangeInt8();
			static GPOS_RESULT EresUnittest_CRangeFromScalar();

	}; // class CRangeTest
}

#endif // !GPOPT_CRangeTest_H


// EOF
