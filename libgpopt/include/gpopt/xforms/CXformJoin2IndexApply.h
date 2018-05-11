//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal, Inc.
//
//	Transform Inner/Outer Join to Index Apply
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformJoin2IndexApply_H
#define GPOPT_CXformJoin2IndexApply_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformExploration.h"
#include "gpopt/xforms/CXformUtils.h"

namespace gpopt
{
	using namespace gpos;

	// fwd declaration
	class CLogicalDynamicGet;

	class CXformJoin2IndexApply : public CXformExploration
	{

		private:

			// private copy ctor
			CXformJoin2IndexApply(const CXformJoin2IndexApply &);

			// helper to add IndexApply expression to given xform results container
			// for homogeneous b-tree indexes
			void CreateHomogeneousBtreeIndexApplyAlternatives
				(
				IMemoryPool *memory_pool,
				ULONG ulOriginOpId,
				CExpression *pexprOuter,
				CExpression *pexprInner,
				CExpression *pexprScalar,
				CTableDescriptor *ptabdescInner,
				CLogicalDynamicGet *popDynamicGet,
				CColRefSet *pcrsScalarExpr,
				CColRefSet *outer_refs,
				CColRefSet *pcrsReqd,
				ULONG ulIndices,
				CXformResult *pxfres
				) const;

			// helper to add IndexApply expression to given xform results container
			// for homogeneous b-tree indexes
			void CreateAlternativesForBtreeIndex
				(
				IMemoryPool *memory_pool,
				ULONG ulOriginOpId,
				CExpression *pexprOuter,
				CExpression *pexprInner,
				CMDAccessor *md_accessor,
				DrgPexpr *pdrgpexprConjuncts,
				CColRefSet *pcrsScalarExpr,
				CColRefSet *outer_refs,
				CColRefSet *pcrsReqd,
				const IMDRelation *pmdrel,
				const IMDIndex *pmdindex,
				CPartConstraint *ppartcnstrIndex,
				CXformResult *pxfres
				) const;

			// helper to add IndexApply expression to given xform results container
			// for homogeneous bitmap indexes
			void CreateHomogeneousBitmapIndexApplyAlternatives
				(
				IMemoryPool *memory_pool,
				ULONG ulOriginOpId,
				CExpression *pexprOuter,
				CExpression *pexprInner,
				CExpression *pexprScalar,
				CTableDescriptor *ptabdescInner,
				CColRefSet *outer_refs,
				CColRefSet *pcrsReqd,
				CXformResult *pxfres
				) const;

			// based on the inner and the scalar expression, it computes scalar expression
			// columns, outer references and required columns
			void ComputeColumnSets
				(
				IMemoryPool *memory_pool,
				CExpression *pexprInner,
				CExpression *pexprScalar,
				CColRefSet **ppcrsScalarExpr,
				CColRefSet **ppcrsOuterRefs,
				CColRefSet **ppcrsReqd
				) const;

			// create an index apply plan when applicable
			void CreatePartialIndexApplyPlan
					(
					IMemoryPool *memory_pool,
					ULONG ulOriginOpId,
					CExpression *pexprOuter,
					CExpression *pexprScalar,
					CColRefSet *outer_refs,
					CLogicalDynamicGet *popDynamicGet,
					DrgPpartdig *pdrgppartdig,
					const IMDRelation *pmdrel,
					CXformResult *pxfres
					) const;

			// create an join with a CTE consumer on the inner branch, with the given
			// partition constraint
			CExpression *PexprJoinOverCTEConsumer
				(
				IMemoryPool *memory_pool,
				ULONG ulOriginOpId,
				CLogicalDynamicGet *popDynamicGet,
				ULONG ulCTEId,
				CExpression *pexprScalar,
				DrgPcr *pdrgpcrDynamicGet,
				CPartConstraint *ppartcnstr,
				DrgPcr *pdrgpcrOuter,
				DrgPcr *pdrgpcrOuterNew
				) const;

			// create an index apply with a CTE consumer on the outer branch
			// and a dynamic get on the inner one
			CExpression *PexprIndexApplyOverCTEConsumer
				(
				IMemoryPool *memory_pool,
				ULONG ulOriginOpId,
				CLogicalDynamicGet *popDynamicGet,
				DrgPexpr *pdrgpexprIndex,
				DrgPexpr *pdrgpexprResidual,
				DrgPcr *pdrgpcrIndexGet,
				const IMDIndex *pmdindex,
				const IMDRelation *pmdrel,
				BOOL fFirst,
				ULONG ulCTEId,
				CPartConstraint *ppartcnstr,
				CColRefSet *outer_refs,
				DrgPcr *pdrgpcrOuter,
				DrgPcr *pdrgpcrOuterNew,
				DrgPcr *pdrgpcrOuterRefsInScan,
				ULongPtrArray *pdrgpulIndexesOfRefsInScan
				) const;

			// create a union-all with the given children
			CExpression *PexprConstructUnionAll
				(
				IMemoryPool *memory_pool,
				DrgPcr *pdrgpcrLeftSchema,
				DrgPcr *pdrgpcrRightSchema,
				CExpression *pexprLeftChild,
				CExpression *pexprRightChild,
				ULONG scan_id
				) const;

			//	construct a CTE Anchor over the given UnionAll and adds it to the given
			//	Xform result
			void AddUnionPlanForPartialIndexes
				(
				IMemoryPool *memory_pool,
				CLogicalDynamicGet *popDynamicGet,
				ULONG ulCTEId,
				CExpression *pexprUnion,
				CExpression *pexprScalar,
				CXformResult *pxfres
				) const;

			// check whether distribution key and the index key are matched.
			// always returns true for master only table.
			BOOL FMatchDistKeyAndIndexKey
				(
				const IMDRelation *pmdrel,
				const IMDIndex *pmdindex
				) const;

		protected:

			// is the logical join that is being transformed an outer join?
			BOOL m_fOuterJoin;

			// helper to add IndexApply expression to given xform results container
			// for homogeneous indexes
			virtual
			void CreateHomogeneousIndexApplyAlternatives
				(
				IMemoryPool *memory_pool,
				ULONG ulOriginOpId,
				CExpression *pexprOuter,
				CExpression *pexprInner,
				CExpression *pexprScalar,
				CTableDescriptor *PtabdescInner,
				CLogicalDynamicGet *popDynamicGet,
				CXformResult *pxfres,
				gpmd::IMDIndex::EmdindexType emdtype
				) const;

			// helper to add IndexApply expression to given xform results container
			// for partial indexes
			virtual
			void CreatePartialIndexApplyAlternatives
				(
				IMemoryPool *memory_pool,
				ULONG ulOriginOpId,
				CExpression *pexprOuter,
				CExpression *pexprInner,
				CExpression *pexprScalar,
				CTableDescriptor *PtabdescInner,
				CLogicalDynamicGet *popDynamicGet,
				CXformResult *pxfres
				) const;

			// return the new instance of logical join operator
			// being targeted in the current xform rule, caller
			// takes the ownership and responsibility to release
			// the instance.
			virtual
			CLogicalJoin *PopLogicalJoin(IMemoryPool *memory_pool) const = 0;

			// return the new instance of logical apply operator
			// that it is trying to transform to in the current
			// xform rule, caller takes the ownership and
			// responsibility to release the instance.
			virtual
			CLogicalApply *PopLogicalApply
				(
				IMemoryPool *memory_pool,
				DrgPcr *pdrgpcrOuterRefs
				) const = 0;

		public:

			// ctor
			explicit
			CXformJoin2IndexApply
				(
				CExpression *pexprPattern
				)
				:
				CXformExploration(pexprPattern)
			{
				m_fOuterJoin = (COperator::EopLogicalLeftOuterJoin == pexprPattern->Pop()->Eopid());
			}

			// dtor
			virtual
			~CXformJoin2IndexApply()
			{}

			// compute xform promise for a given expression handle
			virtual
			EXformPromise Exfp(CExpressionHandle &exprhdl) const;

	}; // class CXformJoin2IndexApply

}

#endif // !GPOPT_CXformJoin2IndexApply_H

// EOF
