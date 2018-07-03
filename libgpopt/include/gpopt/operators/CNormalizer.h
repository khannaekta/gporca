//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2012 EMC Corp.
//
//	@filename:
//		CNormalizer.h
//
//	@doc:
//		Normalization of expression trees;
//		this currently includes predicates push down
//---------------------------------------------------------------------------
#ifndef GPOPT_CNormalizer_H
#define GPOPT_CNormalizer_H

#include "gpos/base.h"
#include "gpopt/operators/CExpression.h"

namespace gpopt
{

	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CNormalizer
	//
	//	@doc:
	//		Normalization of expression trees
	//
	//---------------------------------------------------------------------------
	class CNormalizer
	{

		private:

			// definition of push through function
			typedef void(FnPushThru)
				(
				IMemoryPool *memory_pool,
				CExpression *pexprLogical,
				CExpression *pexprConj,
				CExpression **ppexprResult
				);

			//---------------------------------------------------------------------------
			//	@struct:
			//		SPushThru
			//
			//	@doc:
			//		Mapping of a logical operator to a push through function
			//
			//---------------------------------------------------------------------------
			struct SPushThru
			{
				// logical operator id
				COperator::EOperatorId m_eopid;

				// pointer to push through function
				FnPushThru *m_pfnpt;

			}; // struct SPushThru

			// array of mappings
			static
			const SPushThru m_rgpt[];

			//  return true if second expression is a child of first expression
			static
			BOOL FChild(CExpression *pexpr, CExpression *pexprChild);

			// simplify outer joins
			static
			BOOL FSimplifySelectOnOuterJoin(IMemoryPool *memory_pool, CExpression *pexprOuterJoin, CExpression *pexprPred, CExpression **ppexprResult);

			// call normalizer recursively on expression children
			static
			CExpression *PexprRecursiveNormalize(IMemoryPool *memory_pool, CExpression *pexpr);

			// check if a scalar predicate can be pushed through a logical expression
			static
			BOOL FPushable(CExpression *pexprLogical, CExpression *pexprPred);

			// check if a scalar predicate can be pushed through the child of a sequence project expression
			static
			BOOL FPushableThruSeqPrjChild(CExpression *pexprSeqPrj, CExpression *pexprPred);

			// check if a conjunct should be pushed through expression's outer child
			static
			BOOL FPushThruOuterChild(CExpression *pexprLogical);

			// return a Select expression, if needed, with a scalar condition made of given array of conjuncts
			static
			CExpression *PexprSelect(IMemoryPool *memory_pool, CExpression *pexpr, ExpressionArray *pdrgpexpr);

			// push scalar expression through an expression with unary operator with scalar child
			static
			void PushThruUnaryWithScalarChild(IMemoryPool *memory_pool, CExpression *pexprLogical, CExpression *pexprConj, CExpression **ppexprResult);

			// push scalar expression through a sequence project expression
			static
			void PushThruSeqPrj(IMemoryPool *memory_pool, CExpression *pexprSeqPrj, CExpression *pexprConj, CExpression **ppexprResult);

			// push scalar expression through a set operation
			static
			void PushThruSetOp(IMemoryPool *memory_pool, CExpression *pexprSetOp, CExpression *pexprConj, CExpression **ppexprResult);

			// push a conjunct through a CTE anchor operator
			static
			void PushThruUnaryWithoutScalarChild(IMemoryPool *memory_pool, CExpression *pexprLogical, CExpression *pexprConj, CExpression **ppexprResult);

			// push scalar expression through a select expression
			static
			void PushThruSelect(IMemoryPool *memory_pool, CExpression *pexprSelect, CExpression *pexprConj, CExpression **ppexprResult);

			// split the given conjunct into pushable and unpushable predicates
			static
			void SplitConjunct(IMemoryPool *memory_pool, CExpression *pexpr, CExpression *pexprConj, ExpressionArray **ppdrgpexprPushable, ExpressionArray **ppdrgpexprUnpushable);

			// split the given conjunct into pushable and unpushable predicates for a sequence project expression
			static
			void SplitConjunctForSeqPrj(IMemoryPool *memory_pool, CExpression *pexprSeqPrj, CExpression *pexprConj, ExpressionArray **ppdrgpexprPushable, ExpressionArray **ppdrgpexprUnpushable);

			// push scalar expression through left outer join children
			static
			void PushThruOuterChild(IMemoryPool *memory_pool, CExpression *pexpr, CExpression *pexprConj, CExpression **ppexprResult);

			// push scalar expression through a join expression
			static
			void PushThruJoin(IMemoryPool *memory_pool, CExpression *pexprJoin, CExpression *pexprConj, CExpression **ppexprResult);

			// push scalar expression through logical expression
			static
			void PushThru(IMemoryPool *memory_pool, CExpression *pexprLogical, CExpression *pexprConj, CExpression **ppexprResult);

			// push an array of conjuncts through logical expression, and compute an array of remaining conjuncts
			static
			void PushThru
				(
				IMemoryPool *memory_pool,
				CExpression *pexprLogical,
				ExpressionArray *pdrgpexprConjuncts,
				CExpression **ppexprResult,
				ExpressionArray **ppdrgpexprRemaining
				);

			// private copy ctor
			CNormalizer(const CNormalizer &);

			// pull logical projects as far up the logical tree as possible, and
			// combine consecutive projects if possible
			static
			CExpression *PexprPullUpAndCombineProjects(IMemoryPool *memory_pool, CExpression *pexpr, BOOL *pfSuccess);

			// pull up project elements from the given projection expression that do not
			// exist in the given used columns set
			static
			CExpression *PexprPullUpProjectElements
				(
				IMemoryPool *memory_pool,
				CExpression *pexpr,
				CColRefSet *pcrsUsed,
				CColRefSet *pcrsOutput,
				ExpressionArray **ppdrgpexprPrElPullUp
				);

#ifdef GPOS_DEBUG
			// check if the columns used by the operator are a subset of its input columns
			static
			BOOL FLocalColsSubsetOfInputCols(IMemoryPool *memory_pool, CExpression *pexpr);
#endif //GPOS_DEBUG

		public:

			// main driver
			static
			CExpression *PexprNormalize(IMemoryPool *memory_pool, CExpression *pexpr);

			// normalize joins so that they are on colrefs
			static
			CExpression *PexprNormalizeJoins(IMemoryPool *memory_pool, CExpression *pexpr);

			// pull logical projects as far up the logical tree as possible, and
			// combine consecutive projects if possible
			static
			CExpression *PexprPullUpProjections(IMemoryPool *memory_pool, CExpression *pexpr);

	}; // class CNormalizer
}


#endif // !GPOPT_CNormalizer_H

// EOF
