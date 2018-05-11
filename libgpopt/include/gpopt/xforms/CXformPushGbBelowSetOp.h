//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CXformPushGbBelowSetOp.h
//
//	@doc:
//		Push grouping below set operation
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformPushGbBelowSetOp_H
#define GPOPT_CXformPushGbBelowSetOp_H

#include "gpos/base.h"
#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformExploration.h"

namespace gpopt
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CXformPushGbBelowSetOp
	//
	//	@doc:
	//		Push grouping below set operation
	//
	//---------------------------------------------------------------------------
	template<class TSetOp>
	class CXformPushGbBelowSetOp : public CXformExploration
	{

		private:

			// private copy ctor
			CXformPushGbBelowSetOp(const CXformPushGbBelowSetOp &);

		public:

			// ctor
			explicit
			CXformPushGbBelowSetOp(IMemoryPool *memory_pool)
                :
                CXformExploration
                (
                 // pattern
                 GPOS_NEW(memory_pool) CExpression
                        (
                         memory_pool,
                         GPOS_NEW(memory_pool) CLogicalGbAgg(memory_pool),
                         GPOS_NEW(memory_pool) CExpression  // left child is a set operation
                                (
                                 memory_pool,
                                 GPOS_NEW(memory_pool) TSetOp(memory_pool),
                                 GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternMultiLeaf(memory_pool))
                                 ),
                         GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool)) // project list of group-by
                         )
                )
            {}

			// dtor
			virtual
			~CXformPushGbBelowSetOp() {}

			// compute xform promise for a given expression handle
			virtual
			EXformPromise Exfp(CExpressionHandle &exprhdl) const
            {
                CLogicalGbAgg *popGbAgg = CLogicalGbAgg::PopConvert(exprhdl.Pop());
                if (popGbAgg->FGlobal())
                {
                    return ExfpHigh;
                }

                return ExfpNone;
            }

			// actual transform
			virtual
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

                CExpression *pexprSetOp = (*pexpr)[0];
                CExpression *pexprPrjList = (*pexpr)[1];
                if (0 < pexprPrjList->Arity())
                {
                    // bail-out if group-by has any aggregate functions
                    return;
                }

                CLogicalGbAgg *popGbAgg = CLogicalGbAgg::PopConvert(pexpr->Pop());
                CLogicalSetOp *popSetOp = CLogicalSetOp::PopConvert(pexprSetOp->Pop());

                DrgPcr *pdrgpcrGb = popGbAgg->Pdrgpcr();
                DrgPcr *pdrgpcrOutput = popSetOp->PdrgpcrOutput();
                CColRefSet *pcrsOutput = GPOS_NEW(memory_pool) CColRefSet(memory_pool, pdrgpcrOutput);
                DrgDrgPcr *pdrgpdrgpcrInput = popSetOp->PdrgpdrgpcrInput();
                DrgPexpr *pdrgpexprNewChildren = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
                DrgDrgPcr *pdrgpdrgpcrNewInput = GPOS_NEW(memory_pool) DrgDrgPcr(memory_pool);
                const ULONG arity = pexprSetOp->Arity();

                BOOL fNewChild = false;

                for (ULONG ulChild = 0; ulChild < arity; ulChild++)
                {
                    CExpression *pexprChild = (*pexprSetOp)[ulChild];
                    DrgPcr *pdrgpcrChild = (*pdrgpdrgpcrInput)[ulChild];
                    CColRefSet *pcrsChild =  GPOS_NEW(memory_pool) CColRefSet(memory_pool, pdrgpcrChild);

                    DrgPcr *pdrgpcrChildGb = NULL;
                    if (!pcrsChild->Equals(pcrsOutput))
                    {
                        // use column mapping in SetOp to set child grouping colums
                        UlongColRefHashMap *colref_mapping = CUtils::PhmulcrMapping(memory_pool, pdrgpcrOutput, pdrgpcrChild);
                        pdrgpcrChildGb = CUtils::PdrgpcrRemap(memory_pool, pdrgpcrGb, colref_mapping, true /*must_exist*/);
                        colref_mapping->Release();
                    }
                    else
                    {
                        // use grouping columns directly as child grouping colums
                        pdrgpcrGb->AddRef();
                        pdrgpcrChildGb = pdrgpcrGb;
                    }

                    pexprChild->AddRef();
                    pcrsChild->Release();

                    // if child of setop is already an Agg with the same grouping columns
                    // that we want to use, there is no need to add another agg on top of it
                    COperator *popChild = pexprChild->Pop();
                    if (COperator::EopLogicalGbAgg == popChild->Eopid())
                    {
                        CLogicalGbAgg *popGbAgg = CLogicalGbAgg::PopConvert(popChild);
                        if (CColRef::Equals(popGbAgg->Pdrgpcr(), pdrgpcrChildGb))
                        {
                            pdrgpexprNewChildren->Append(pexprChild);
                            pdrgpdrgpcrNewInput->Append(pdrgpcrChildGb);

                            continue;
                        }
                    }

                    fNewChild = true;
                    pexprPrjList->AddRef();
                    CExpression *pexprChildGb = CUtils::PexprLogicalGbAggGlobal(memory_pool, pdrgpcrChildGb, pexprChild, pexprPrjList);
                    pdrgpexprNewChildren->Append(pexprChildGb);

                    pdrgpcrChildGb->AddRef();
                    pdrgpdrgpcrNewInput->Append(pdrgpcrChildGb);
                }

                pcrsOutput->Release();

                if (!fNewChild)
                {
                    // all children of the union were already Aggs with the same grouping
                    // columns that we would have created. No new alternative expressions
                    pdrgpdrgpcrNewInput->Release();
                    pdrgpexprNewChildren->Release();

                    return;
                }

                pdrgpcrGb->AddRef();
                TSetOp *popSetOpNew = GPOS_NEW(memory_pool) TSetOp(memory_pool, pdrgpcrGb, pdrgpdrgpcrNewInput);
                CExpression *pexprNewSetOp = GPOS_NEW(memory_pool) CExpression(memory_pool, popSetOpNew, pdrgpexprNewChildren);

                popGbAgg->AddRef();
                pexprPrjList->AddRef();
                CExpression *pexprResult = GPOS_NEW(memory_pool) CExpression(memory_pool, popGbAgg, pexprNewSetOp, pexprPrjList); 

                pxfres->Add(pexprResult);
            }

	}; // class CXformPushGbBelowSetOp

}

#endif // !GPOPT_CXformPushGbBelowSetOp_H

// EOF
