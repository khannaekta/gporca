//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal, Inc.
//
//	@filename:
//		CCardinalityTestUtils.h
//
//	@doc:
//		Utility functions used in the testing cardinality estimation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CCardinalityTestUtils_H
#define GPNAUCRATES_CCardinalityTestUtils_H

#include "naucrates/statistics/CPoint.h"
#include "naucrates/statistics/CBucket.h"
#include "naucrates/statistics/CHistogram.h"
#include "naucrates/statistics/CStatistics.h"
#include "naucrates/statistics/CStatsPredDisj.h"


namespace gpnaucrates
{

	//---------------------------------------------------------------------------
	//	@class:
	//		CCardinalityTestUtils
	//
	//	@doc:
	//		Static utility functions used in the testing cardinality estimation
	//
	//---------------------------------------------------------------------------
	class CCardinalityTestUtils
	{
		public:

			// create a bucket with integer bounds, and lower bound is closed
			static
			CBucket *PbucketIntegerClosedLowerBound(IMemoryPool *memory_pool, INT iLower, INT iUpper, CDouble, CDouble);

			// create a singleton bucket containing a boolean m_bytearray_value
			static
			CBucket *PbucketSingletonBoolVal(IMemoryPool *memory_pool, BOOL fValue, CDouble frequency);

			// create an integer bucket with the provider upper/lower bound, frequency and NDV information
			static
			CBucket *PbucketInteger
				(
				IMemoryPool *memory_pool,
				INT iLower,
				INT iUpper,
				BOOL is_lower_closed,
				BOOL is_upper_closed,
				CDouble frequency,
				CDouble distinct
				);

			// helper function to generate integer histogram based on the NDV and bucket information provided
			static
			CHistogram* PhistInt4Remain
				(
				IMemoryPool *memory_pool,
				ULONG num_of_buckets,
				CDouble dNDVPerBucket,
				BOOL fNullFreq,
				CDouble num_NDV_remain
				);

			// helper function to generate an example integer histogram
			static
			CHistogram* PhistExampleInt4(IMemoryPool *memory_pool);

			// helper function to generate an example boolean histogram
			static
			CHistogram* PhistExampleBool(IMemoryPool *memory_pool);

			// helper function to generate a point from an encoded m_bytearray_value of specific datatype
			static
			CPoint *PpointGeneric(IMemoryPool *memory_pool, OID oid, CWStringDynamic *pstrValueEncoded, LINT value);

			// helper function to generate a point of numeric datatype
			static
			CPoint *PpointNumeric(IMemoryPool *memory_pool, CWStringDynamic *pstrEncodedValue, CDouble value);

			// helper method to print statistics object
			static
			void PrintStats(IMemoryPool *memory_pool, const CStatistics *stats);

			// helper method to print histogram object
			static
			void PrintHist(IMemoryPool *memory_pool, const char *pcPrefix, const CHistogram *histogram);

			// helper method to print bucket object
			static
			void PrintBucket(IMemoryPool *memory_pool, const char *pcPrefix, const CBucket *bucket);

	}; // class CCardinalityTestUtils
}

#endif // !GPNAUCRATES_CCardinalityTestUtils_H


// EOF
