//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Software Inc.
//
//	Base class for transforming Join to Index Apply
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformJoin2IndexApplyBase_H
#define GPOPT_CXformJoin2IndexApplyBase_H

#include "gpos/base.h"
#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformJoin2IndexApply.h"

namespace gpopt
{
	using namespace gpos;

	template<class TJoin, class TApply, class TGet, BOOL fWithSelect, BOOL is_partial, IMDIndex::EmdindexType eidxtype>
	class CXformJoin2IndexApplyBase : public CXformJoin2IndexApply
	{
		private:

			// private copy ctor
			CXformJoin2IndexApplyBase(const CXformJoin2IndexApplyBase &);

			// Can transform left outer join to left outer index apply?
			// For hash distributed table, we can do outer index apply only
			// when the inner columns used in the join condition contains
			// the inner distribution key set. Master only table is ok to
			// transform to outer index apply, but random table is not.
			// Because if the inner is random distributed, there is no way
			// to redistribute outer child to match inner on the join keys.
			BOOL
			FCanLeftOuterIndexApply
				(
				IMemoryPool *memory_pool,
				CExpression *pexprInner,
				CExpression *pexprScalar
				) const
			{
				GPOS_ASSERT(m_fOuterJoin);
				TGet *popGet = TGet::PopConvert(pexprInner->Pop());
				IMDRelation::Ereldistrpolicy ereldist = popGet->Ptabdesc()->GetRelDistribution();

				if (ereldist == IMDRelation::EreldistrRandom)
					return false;
				else if (ereldist == IMDRelation::EreldistrMasterOnly)
					return true;

				// now consider hash distributed table
				CColRefSet *pcrsInnerOutput = CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->PcrsOutput();
				CColRefSet *pcrsScalarExpr = CDrvdPropScalar::GetDrvdScalarProps(pexprScalar->PdpDerive())->PcrsUsed();
				CColRefSet *pcrsInnerRefs = GPOS_NEW(memory_pool) CColRefSet(memory_pool, *pcrsScalarExpr);
				pcrsInnerRefs->Intersection(pcrsInnerOutput);

				// Distribution key set of inner GET must be subset of inner columns used in
				// the left outer join condition, but doesn't need to be equal.
				BOOL fCanOuterIndexApply = pcrsInnerRefs->ContainsAll(popGet->PcrsDist());
				pcrsInnerRefs->Release();
				if (fCanOuterIndexApply)
				{
					CColRefSet *pcrsEquivPredInner = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
					// extract array of join predicates from join condition expression
					ExpressionArray *pdrgpexpr = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pexprScalar);
					for (ULONG ul = 0; ul < pdrgpexpr->Size(); ul++)
					{
						CExpression *pexprPred = (*pdrgpexpr)[ul];
						CColRefSet *pcrsPred = CDrvdPropScalar::GetDrvdScalarProps(pexprPred->PdpDerive())->PcrsUsed();

						// if it doesn't have equi-join predicate on the distribution key,
						// we can't transform to left outer index apply, because only
						// redistribute motion is allowed for outer child of join with
						// hash distributed inner child.
						// consider R LOJ S (both distribute by a and have index on a)
						// with the predicate S.a = R.a and S.a > R.b, left outer index
						// apply is still applicable.
						if (!pcrsPred->IsDisjoint(popGet->PcrsDist()) &&
							CPredicateUtils::IsEqualityOp(pexprPred))
						{
							pcrsEquivPredInner->Include(pcrsPred);
						}
					}
					fCanOuterIndexApply = pcrsEquivPredInner->ContainsAll(popGet->PcrsDist());
					pcrsEquivPredInner->Release();
					pdrgpexpr->Release();
				}

				return fCanOuterIndexApply;
			}

		protected:

			// return the new instance of logical join operator
			// being targeted in the current xform rule, caller
			// takes the ownership and responsibility to release
			// the instance.
			virtual
			CLogicalJoin *PopLogicalJoin(IMemoryPool *memory_pool) const
			{
				return GPOS_NEW(memory_pool) TJoin(memory_pool);
			}

			// return the new instance of logical apply operator
			// that it is trying to transform to in the current
			// xform rule, caller takes the ownership and
			// responsibility to release the instance.
			virtual
			CLogicalApply *PopLogicalApply
				(
				IMemoryPool *memory_pool,
				ColRefArray *colref_array
				) const
			{
				return GPOS_NEW(memory_pool) TApply(memory_pool, colref_array, m_fOuterJoin);
			}

		public:

			// ctor
			explicit
			CXformJoin2IndexApplyBase<TJoin, TApply, TGet, fWithSelect, is_partial, eidxtype>(IMemoryPool *memory_pool)
			:
			// pattern
			CXformJoin2IndexApply
			(
				GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) TJoin(pmp),
				fPartial // only when fPartial is true, CTE producer is created and is preprocessed,
					     // where it needs the entire tree for deriving relational properties.
				?
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool)) // outer child
				:
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // outer child
					fWithSelect
					?
					GPOS_NEW(memory_pool) CExpression  // inner child with Select operator
						(
						memory_pool,
						GPOS_NEW(memory_pool) CLogicalSelect(memory_pool),
						GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) TGet(memory_pool)), // Get below Select
						GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool))  // predicate
						)
					:
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) TGet(memory_pool)), // inner child with Get operator,
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool))  // predicate tree
				)
			)
			{}

			// dtor
			virtual
			~CXformJoin2IndexApplyBase<TJoin, TApply, TGet, fWithSelect, is_partial, eidxtype>()
			{}

			// actual transform
			virtual
			void Transform(CXformContext *pxfctxt, CXformResult *pxfres, CExpression *pexpr) const
			{
				GPOS_ASSERT(NULL != pxfctxt);
				GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
				GPOS_ASSERT(FCheckPattern(pexpr));

				IMemoryPool *memory_pool = pxfctxt->Pmp();

				// extract components
				CExpression *pexprOuter = (*pexpr)[0];
				CExpression *pexprInner = (*pexpr)[1];
				CExpression *pexprScalar = (*pexpr)[2];

				CExpression *pexprGet = pexprInner;
				CExpression *pexprAllPredicates = pexprScalar;

				if (fWithSelect)
				{
					pexprGet = (*pexprInner)[0];
					pexprAllPredicates = CPredicateUtils::PexprConjunction(memory_pool, pexprScalar, (*pexprInner)[1]);
				}
				else
				{
					pexprScalar->AddRef();
				}

				if (m_fOuterJoin && !FCanLeftOuterIndexApply(memory_pool, pexprGet, pexprScalar))
				{
					// It is a left outer join, but we can't do outer index apply,
					// stop transforming and return immediately.
					CRefCount::SafeRelease(pexprAllPredicates);
					return;
				}

				CLogicalDynamicGet *popDynamicGet = NULL;
				if (COperator::EopLogicalDynamicGet == pexprGet->Pop()->Eopid())
				{
					popDynamicGet = CLogicalDynamicGet::PopConvert(pexprGet->Pop());
				}

				CTableDescriptor *ptabdescInner = TGet::PopConvert(pexprGet->Pop())->Ptabdesc();
				if (is_partial)
				{
					CreatePartialIndexApplyAlternatives
						(
						memory_pool,
						pexpr->Pop()->UlOpId(),
						pexprOuter,
						pexprInner,
						pexprAllPredicates,
						ptabdescInner,
						popDynamicGet,
						pxfres
						);
				}
				else
				{
					CreateHomogeneousIndexApplyAlternatives
						(
						memory_pool,
						pexpr->Pop()->UlOpId(),
						pexprOuter,
						pexprGet,
						pexprAllPredicates,
						ptabdescInner,
						popDynamicGet,
						pxfres,
						eidxtype
						);
				}
				CRefCount::SafeRelease(pexprAllPredicates);
			}

			// return true if xform should be applied only once
			// only when fPartial is true, CTE producer is created and is preprocessed,
			// where it needs the entire tree for deriving relational properties.
			virtual
			BOOL IsApplyOnce()
			{
				return fPartial;
			}

	}; // class CXformJoin2IndexApplyBase

}

#endif // !GPOPT_CXformJoin2IndexApplyBase_H

// EOF
