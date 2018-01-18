//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Software, Inc.
//
//	Template Class for Inner / Left Outer Index Apply
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementIndexApply_H
#define GPOPT_CXformImplementIndexApply_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
	using namespace gpos;

	template<class TLogicalIndexApply, class TPhysicalIndexNLJoin>
	class CXformImplementIndexApply : public CXformImplementation
	{

		private:

			// private copy ctor
			CXformImplementIndexApply(const CXformImplementIndexApply &);

		public:

			// ctor
			explicit
			CXformImplementIndexApply(IMemoryPool *pmp)
			:
			// pattern
			CXformImplementation
				(
				GPOS_NEW(pmp) CExpression
								(
								pmp,
								GPOS_NEW(pmp) TLogicalIndexApply(pmp),
								GPOS_NEW(pmp) CExpression(pmp, GPOS_NEW(pmp) CPatternLeaf(pmp)), // outer child
								GPOS_NEW(pmp) CExpression(pmp, GPOS_NEW(pmp) CPatternLeaf(pmp)),  // inner child
								GPOS_NEW(pmp) CExpression(pmp, GPOS_NEW(pmp) CPatternLeaf(pmp))  // predicate
								)
				)
			{}

			// dtor
			virtual
			~CXformImplementIndexApply()
			{}

			// ident accessors
			virtual
			EXformId Exfid() const = 0;

			virtual
			const CHAR *SzId() const = 0;

			// compute xform promise for a given expression handle
			virtual
			EXformPromise Exfp
				(
				CExpressionHandle & // exprhdl
				)
				const
			{
				return ExfpHigh;
			}

			// actual transform
			virtual
			void Transform(CXformContext *pxfctxt, CXformResult *pxfres, CExpression *pexpr) const
			{
				GPOS_ASSERT(NULL != pxfctxt);
				GPOS_ASSERT(FPromising(pxfctxt->Pmp(), this, pexpr));
				GPOS_ASSERT(FCheckPattern(pexpr));

				IMemoryPool *pmp = pxfctxt->Pmp();

				// extract components
				CExpression *pexprOuter = (*pexpr)[0];
				CExpression *pexprInner = (*pexpr)[1];
				CExpression *pexprScalar = (*pexpr)[2];
				DrgPcr *pdrgpcr = TLogicalIndexApply::PopConvert(pexpr->Pop())->PdrgPcrOuterRefs();
				pdrgpcr->AddRef();

				// addref all components
				pexprOuter->AddRef();
				pexprInner->AddRef();
				pexprScalar->AddRef();

				// assemble physical operator
				CExpression *pexprResult =
					GPOS_NEW(pmp) CExpression
								(
								pmp,
								GPOS_NEW(pmp) TPhysicalIndexNLJoin(pmp, pdrgpcr),
								pexprOuter,
								pexprInner,
								pexprScalar
								);

				// add alternative to results
				pxfres->Add(pexprResult);
			}

	}; // class CXformImplementIndexApply

}

#endif // !GPOPT_CXformImplementIndexApply_H

// EOF
