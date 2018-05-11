//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//
//	@filename:
//		CFilterCardinalityTest.h
//
//	@doc:
//		Test for filter cardinality estimation
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CFilterCardinalityTest_H
#define GPNAUCRATES_CFilterCardinalityTest_H

#include "gpos/base.h"

#include "naucrates/statistics/CPoint.h"
#include "naucrates/statistics/CBucket.h"
#include "naucrates/statistics/CHistogram.h"
#include "naucrates/statistics/CStatistics.h"
#include "naucrates/statistics/CStatsPredDisj.h"

#include "naucrates/dxl/CDXLUtils.h"

namespace gpnaucrates
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CFilterCardinalityTest
	//
	//	@doc:
	//		Static unit tests for join cardinality estimation
	//
	//---------------------------------------------------------------------------
	class CFilterCardinalityTest
	{
		// shorthand for functions for generating the disjunctive filter predicates
		typedef CStatsPred *(FnPstatspredDisj)(IMemoryPool *memory_pool);

		private:

			// triplet consisting of comparison type, double m_bytearray_value and its byte array representation
			struct SStatsCmpValElem
			{
				CStatsPred::EStatsCmpType m_stats_cmp_type; // comparison operator
				const WCHAR *m_wsz; // byte array representation
				CDouble m_value; // double m_bytearray_value
			}; // SStatsCmpValElem

			// test case for disjunctive filter evaluation
			struct SStatsFilterSTestCase
			{
				// input stats dxl file
				const CHAR *m_szInputFile;

				// output stats dxl file
				const CHAR *m_szOutputFile;

				// filter predicate generation function pointer
				FnPstatspredDisj *m_pf;
			}; // SStatsFilterSTestCase

			// helper method to iterate over an array generated filter predicates for stats evaluation
			static
			GPOS_RESULT EresUnittest_CStatistics(SStatsFilterSTestCase rgstatsdisjtc[], ULONG ulTestCases);

			// disjunction filters
			static
			CStatsPred *PstatspredDisj1(IMemoryPool *memory_pool);

			// disjunction filters
			static
			CStatsPred *PstatspredDisj2(IMemoryPool *memory_pool);

			// disjunction filters
			static
			CStatsPred *PstatspredDisj3(IMemoryPool *memory_pool);

			// disjunction filters
			static
			CStatsPred *PstatspredDisj4(IMemoryPool *memory_pool);

			// disjunction filters
			static
			CStatsPred *PstatspredDisj5(IMemoryPool *memory_pool);

			// disjunction filters
			static
			CStatsPred *PstatspredDisj6(IMemoryPool *memory_pool);

			// disjunction filters
			static
			CStatsPred *PstatspredDisj7(IMemoryPool *memory_pool);

			// disjunction filters
			static
			CStatsPred *PstatspredDisj8(IMemoryPool *memory_pool);

			// nested AND and OR predicates
			static
			CStatsPred *PstatspredNestedPredDiffCol1(IMemoryPool *memory_pool);

			// nested AND and OR predicates
			static
			CStatsPred *PstatspredNestedPredDiffCol2(IMemoryPool *memory_pool);

			// nested AND and OR predicates
			static
			CStatsPred *PstatspredNestedPredCommonCol1(IMemoryPool *memory_pool);

			// nested AND and OR predicates
			static
			CStatsPred *PstatspredNestedPredCommonCol2(IMemoryPool *memory_pool);

			// nested AND and OR predicates
			static
			CStatsPred *PstatspredNestedSharedCol(IMemoryPool *memory_pool);

			// nested AND and OR predicates
			static
			CStatsPred *PstatspredDisjOverConjSameCol1(IMemoryPool *memory_pool);

			// nested AND and OR predicates
			static
			CStatsPred *PstatspredDisjOverConjSameCol2(IMemoryPool *memory_pool);

			// nested AND and OR predicates
			static
			CStatsPred *PstatspredDisjOverConjSameCol3(IMemoryPool *memory_pool);

			// nested AND and OR predicates
			static
			CStatsPred *PstatspredDisjOverConjSameCol4(IMemoryPool *memory_pool);

			// nested AND and OR predicates
			static
			CStatsPred *PstatspredDisjOverConjDifferentCol1(IMemoryPool *memory_pool);

			static
			CStatsPred *PstatspredDisjOverConjMultipleIdenticalCols(IMemoryPool *memory_pool);

			// conjunctive predicates
			static
			CStatsPred *PstatspredConj(IMemoryPool *memory_pool);

			// generate an array of filter given a column identifier, comparison type, and array of integer point
			static
			StatsPredPtrArry *PdrgpstatspredInteger
			(
					IMemoryPool *memory_pool,
					ULONG col_id,
					CStatsPred::EStatsCmpType stats_cmp_type,
					INT *piVals,
					ULONG ulVals
			);

			// create a numeric predicate on a particular column
			static
			StatsPredPtrArry *PdrgppredfilterNumeric(IMemoryPool *memory_pool, ULONG col_id, SStatsCmpValElem statsCmpValElem);

			// create a filter on a column with null values
			static
			CStatsPred *PstatspredNullableCols(IMemoryPool *memory_pool);

			// create a point filter where the constant is null
			static
			CStatsPred *PstatspredWithNullConstant(IMemoryPool *memory_pool);

			// create a 'is not null' point filter
			static
			CStatsPred *PstatspredNotNull(IMemoryPool *memory_pool);

			// compare the derived statistics with the statistics in the outputfile
			static
			GPOS_RESULT EresUnittest_CStatisticsCompare
			(
					IMemoryPool *memory_pool,
					CMDAccessor *md_accessor,
					CStatisticsArray *pdrgpstatBefore,
					CStatsPred *pred_stats,
					const CHAR *szDXLOutput,
					BOOL fApplyTwice = false
			);

		public:

			// unittests
			static
			GPOS_RESULT EresUnittest();

			// testing select predicates
			static
			GPOS_RESULT EresUnittest_CStatisticsFilter();

			// testing nested AND / OR predicates
			static
			GPOS_RESULT EresUnittest_CStatisticsNestedPred();

			// test disjunctive filter
			static
			GPOS_RESULT EresUnittest_CStatisticsFilterDisj();

			// test conjunctive filter
			static
			GPOS_RESULT EresUnittest_CStatisticsFilterConj();

			// DXL based test on numeric data types
			static
			GPOS_RESULT EresUnittest_CStatisticsBasicsFromDXLNumeric();

			// basic statistics parsing
			static
			GPOS_RESULT EresUnittest_CStatisticsBasicsFromDXL();

			// test for accumulating cardinality in disjunctive and conjunctive predicates
			static
			GPOS_RESULT EresUnittest_CStatisticsAccumulateCard();

	}; // class CFilterCardinalityTest
}

#endif // !GPNAUCRATES_CFilterCardinalityTest_H


// EOF
