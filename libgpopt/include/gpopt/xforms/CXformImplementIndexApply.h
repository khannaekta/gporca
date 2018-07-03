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

	class CXformImplementIndexApply : public CXformImplementation
	{

		private:

			// private copy ctor
			CXformImplementIndexApply(const CXformImplementIndexApply &);

		public:

			// ctor
			explicit
			CXformImplementIndexApply(IMemoryPool *memory_pool)
			:
			// pattern
			CXformImplementation
				(
				GPOS_NEW(memory_pool) CExpression
								(
								memory_pool,
								GPOS_NEW(memory_pool) CLogicalIndexApply(memory_pool),
								GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // outer child
								GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)),  // inner child
								GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool))  // predicate
								)
				)
			{}

			// dtor
			virtual
			~CXformImplementIndexApply()
			{}

			// ident accessors
			virtual
			EXformId Exfid() const
			{
				return ExfImplementIndexApply;
			}

			virtual
			const CHAR *SzId() const
			{
				return "CXformImplementIndexApply";
			}

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

				IMemoryPool *memory_pool = pxfctxt->Pmp();

				// extract components
				CExpression *pexprOuter = (*pexpr)[0];
				CExpression *pexprInner = (*pexpr)[1];
				CExpression *pexprScalar = (*pexpr)[2];
				ColRefArray *colref_array = CLogicalIndexApply::PopConvert(pexpr->Pop())->PdrgPcrOuterRefs();
				colref_array->AddRef();

				// addref all components
				pexprOuter->AddRef();
				pexprInner->AddRef();
				pexprScalar->AddRef();

				// assemble physical operator
				CPhysicalNLJoin *pop = NULL;

				if (CLogicalIndexApply::PopConvert(pexpr->Pop())->FouterJoin())
					pop = GPOS_NEW(memory_pool) CPhysicalLeftOuterIndexNLJoin(memory_pool, colref_array);
				else
					pop = GPOS_NEW(memory_pool) CPhysicalInnerIndexNLJoin(memory_pool, colref_array);

				CExpression *pexprResult =
						GPOS_NEW(memory_pool) CExpression
								(
								memory_pool,
								pop,
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
