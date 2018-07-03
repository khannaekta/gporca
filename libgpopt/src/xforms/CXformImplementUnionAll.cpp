//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementUnionAll.cpp
//
//	@doc:
//		Implementation of union all operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/exception.h"
#include "gpopt/xforms/CXformImplementUnionAll.h"
#include "gpopt/xforms/CXformUtils.h"
#include "gpopt/operators/CPhysicalUnionAll.h"
#include "gpopt/operators/CPhysicalUnionAllFactory.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementUnionAll::CXformImplementUnionAll
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformImplementUnionAll::CXformImplementUnionAll
	(
	IMemoryPool *memory_pool
	)
	:
	// pattern
	CXformImplementation
		(
		GPOS_NEW(memory_pool) CExpression
						(
						memory_pool,
						GPOS_NEW(memory_pool) CLogicalUnionAll(memory_pool),
						GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternMultiLeaf(memory_pool))
						)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformImplementUnionAll::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformImplementUnionAll::Transform
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

	// extract components
	CLogicalUnionAll *popUnionAll = CLogicalUnionAll::PopConvert(pexpr->Pop());
	CPhysicalUnionAllFactory factory(popUnionAll);

	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	const ULONG arity = pexpr->Arity();

	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprChild = (*pexpr)[ul];
		pexprChild->AddRef();
		pdrgpexpr->Append(pexprChild);
	}

	CPhysicalUnionAll *popPhysicalSerialUnionAll = factory.PopPhysicalUnionAll(memory_pool, false);

	// assemble serial union physical operator
	CExpression *pexprSerialUnionAll =
		GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					popPhysicalSerialUnionAll,
					pdrgpexpr
					);

	// add serial union alternative to results
	pxfres->Add(pexprSerialUnionAll);

	// parallel union alternative to the result if the GUC is on
	BOOL fParallel = GPOS_FTRACE(EopttraceEnableParallelAppend);

	if(fParallel)
	{
		CPhysicalUnionAll *popPhysicalParallelUnionAll = factory.PopPhysicalUnionAll(memory_pool, true);

		pdrgpexpr->AddRef();

		// assemble physical parallel operator
		CExpression *pexprParallelUnionAll =
		GPOS_NEW(memory_pool) CExpression
		(
		 memory_pool,
		 popPhysicalParallelUnionAll,
		 pdrgpexpr
		 );

		// add parallel union alternative to results
		pxfres->Add(pexprParallelUnionAll);
	}
}

// EOF

