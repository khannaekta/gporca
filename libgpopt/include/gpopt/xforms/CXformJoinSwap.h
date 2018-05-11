//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformJoinSwap.h
//
//	@doc:
//		Join swap transformation
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformJoinSwap_H
#define GPOPT_CXformJoinSwap_H

#include "gpos/base.h"

#include "gpopt/operators/ops.h"

#include "gpopt/xforms/CXformExploration.h"
#include "gpopt/xforms/CXformUtils.h"

namespace gpopt
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CXformJoinSwap
	//
	//	@doc:
	//		Join swap transformation
	//
	//---------------------------------------------------------------------------
	template<class TJoinTop, class TJoinBottom>
	class CXformJoinSwap : public CXformExploration
	{

		private:

			// private copy ctor
			CXformJoinSwap(const CXformJoinSwap &);

		public:

			// ctor
			explicit
			CXformJoinSwap(IMemoryPool *memory_pool)
                :
                CXformExploration
                (
                 // pattern
                 GPOS_NEW(memory_pool) CExpression
                        (
                         memory_pool,
                         GPOS_NEW(memory_pool) TJoinTop(memory_pool),
                         GPOS_NEW(memory_pool) CExpression  // left child is a join tree
                                (
                                 memory_pool,
                                 GPOS_NEW(memory_pool) TJoinBottom(memory_pool),
                                 GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // left child
                                 GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // right child
                                 GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)) // predicate
                                 ),
                         GPOS_NEW(memory_pool) CExpression // right child is a pattern leaf
                                (
                                 memory_pool,
                                 GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)
                                 ),
                         GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)) // top-join predicate
                         )
                )
            {}

			// dtor
			virtual
			~CXformJoinSwap() {}

			// compute xform promise for a given expression handle
			virtual
			EXformPromise Exfp
				(
				CExpressionHandle & // exprhdl
				)
				const
			{
				return CXform::ExfpHigh;
			}

			// actual transform
			void Transform
					(
					CXformContext *pxfctxt,
					CXformResult *pxfres,
					CExpression *pexpr
					)
					const
            {
                GPOS_ASSERT(NULL != pxfctxt);
                GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
                GPOS_ASSERT(FCheckPattern(pexpr));

                IMemoryPool *memory_pool = pxfctxt->Pmp();

                CExpression *pexprResult = CXformUtils::PexprSwapJoins(memory_pool, pexpr, (*pexpr)[0]);
                if (NULL == pexprResult)
                {
                    return;
                }

                pxfres->Add(pexprResult);
            }

	}; // class CXformJoinSwap

}

#endif // !GPOPT_CXformJoinSwap_H

// EOF
