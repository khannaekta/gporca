//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CExpressionFactorizer.h
//
//	@doc:
//		Utility functions for expression factorization.
//
//	@owner:
//		, 
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CExpressionFactorizer_H
#define GPOPT_CExpressionFactorizer_H

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/operators/COperator.h"
#include "gpopt/operators/CExpression.h"

namespace gpopt
{

	using namespace gpos;

	// fwd declarations
	class ExprMap;

	//---------------------------------------------------------------------------
	//	@class:
	//		CExpressionFactorizer
	//
	//	@doc:
	//		Expression factorization routines.
	//
	//---------------------------------------------------------------------------
	class CExpressionFactorizer
	{
		private:
			// map expression to a count, used in factorization
			typedef CHashMap<CExpression, ULONG, CExpression::HashValue, CUtils::Equals,
						CleanupRelease<CExpression>, CleanupDelete<ULONG> > ExprMap;

			// map operators to an array of expression arrays, corresponding to
			// a disjunction of expressions on columns created by that operator
			typedef CHashMap<ULONG, ExpressionArrays, gpos::HashValue<ULONG>,
					gpos::Equals<ULONG>, CleanupDelete<ULONG>,
					CleanupRelease<ExpressionArrays> > SourceToArrayPosMap;

			// iterator for map of operator to disjunctive form representation
			typedef CHashMapIter<ULONG, ExpressionArrays, gpos::HashValue<ULONG>,
					gpos::Equals<ULONG>, CleanupDelete<ULONG>,
					CleanupRelease<ExpressionArrays> > SourceToArrayPosMapIter;

			// map columns to an array of expression arrays, corresponding to
			// a disjunction of expressions using that column
			typedef CHashMap<CColRef, ExpressionArrays, gpos::HashPtr<CColRef>,
					gpos::EqualPtr<CColRef>, CleanupNULL<CColRef>,
					CleanupRelease<ExpressionArrays> > ColumnToArrayPosMap;

			// iterator for map of column to disjunctive form representation
			typedef CHashMapIter<CColRef, ExpressionArrays, gpos::HashPtr<CColRef>,
					gpos::EqualPtr<CColRef>, CleanupNULL<CColRef>,
					CleanupRelease<ExpressionArrays> > ColumnToArrayPosMapIter;

			typedef CExpression *(*PexprProcessDisj)
					(
					IMemoryPool *memory_pool,
					CExpression *pexpr,
					CExpression *pexprLowestLogicalAncestor
					);

			// helper for determining if given expression factor should be added to factors array
			static
			void AddFactor
				(
				IMemoryPool *memory_pool,
				CExpression *pexpr,
				ExpressionArray *pdrgpexprFactors,
				ExpressionArray *pdrgpexprResidual,
				ExprMap *pexprmap,
				ULONG ulDisjuncts
				);

			// helper for building a factors map
			static
			ExprMap *PexprmapFactors(IMemoryPool *memory_pool, CExpression *pexpr);

			// factorize common expressions in Or tree
			static
			CExpression *PexprFactorizeDisj
				(
				IMemoryPool *memory_pool,
				CExpression *pexpr,
				CExpression *  // pexprLowestLogicalAncestor
				);

			// visitor-like function to process descendents that are OR operators
			static
			CExpression *PexprProcessDisjDescendents
				(
				IMemoryPool *memory_pool,
				CExpression *pexpr,
				CExpression *pexprLowestLogicalAncestor,
				PexprProcessDisj pexprorfun
				);

			// discover common factors in scalar expression
			static
			CExpression *PexprDiscoverFactors(IMemoryPool *memory_pool, CExpression *pexpr);

			// if the given expression is a non volatile scalar expression using table
			// columns created by the same operator
			// return true and set ulOpSourceId to the id of the operator that created
			// that column,
			// or if the given expression is a non volatile scalar expression using
			// only one computed column
			// return true and set ppcrComputedColumn to that computed column,
			// otherwise return false
			static
			BOOL FOpSourceIdOrComputedColumn
				(
				CExpression *pexpr,
				ULONG *ulOpSourceId,
				CColRef **ppcrComputedColumn
				);

			// if the given expression is a scalar that can be pushed, it returns
			// the set of used columns
			static
			CColRefSet* PcrsUsedByPushableScalar(CExpression *pexpr);

			// find the array of expression arrays corresponding to the given
			// operator source id in the given source to array position map
			// or construct a new array and add it to the map
			static
			ExpressionArrays *PdrgPdrgpexprDisjunctArrayForSourceId
				(
				IMemoryPool *memory_pool,
				SourceToArrayPosMap *psrc2array,
				BOOL fAllowNewSources,
				ULONG ulOpSourceId
				);

			// find the array of expression arrays corresponding to the given
			// column in the given column to array position map
			// or construct a new array and add it to the map
			static
			ExpressionArrays *PdrgPdrgpexprDisjunctArrayForColumn
				(
				IMemoryPool *memory_pool,
				ColumnToArrayPosMap *pcol2array,
				BOOL fAllowNewSources,
				CColRef *colref
				);

			// if the given expression is a table column to constant comparison,
			// record it in the map
			static
			void StoreBaseOpToColumnExpr
				(
				IMemoryPool *memory_pool,
				CExpression *pexpr,
				SourceToArrayPosMap *psrc2array,
				ColumnToArrayPosMap *pcol2array,
				const CColRefSet *pcrsProducedByChildren,
				BOOL fAllowNewSources,
				ULONG ulPosition
				);

			// construct a filter based on the expressions from 'pdrgpdrgpexpr'
			// and add to the array 'pdrgpexprPrefilters'
			static
			void AddInferredFiltersFromArray
				(
				IMemoryPool *memory_pool,
				const ExpressionArrays *pdrgpdrgpexpr,
				ULONG ulDisjChildrenLength,
				ExpressionArray *pdrgpexprPrefilters
				);

			// create a conjunction of the given expression and inferred filters constructed out
			// of the given maps
			static
			CExpression * PexprAddInferredFilters
							(
							IMemoryPool *memory_pool,
							CExpression *pexpr,
							SourceToArrayPosMap *psrc2array,
							ColumnToArrayPosMap *pcol2array
							);

			//	returns the set of columns produced by the scalar children of the given
			//	expression
			static
			CColRefSet *PcrsColumnsProducedByChildren(IMemoryPool *memory_pool, CExpression *pexpr);

			// compute disjunctive pre-filters that can be pushed to the column creators
			static
			CExpression *PexprExtractInferredFiltersFromDisj
				(
				IMemoryPool *memory_pool,
				CExpression *pexpr,
				CExpression *pexprLowestLogicalAncestor
				);

		public:
			// factorize common expressions
			static
			CExpression *PexprFactorize(IMemoryPool *memory_pool, CExpression *pexpr);

			// compute disjunctive pre-filters that can be pushed to the column creators
			static
			CExpression *PexprExtractInferredFilters(IMemoryPool *memory_pool, CExpression *pexpr);
	};
}

#endif // ! GPOPT_CExpressionFactorizer_H

// EOF
