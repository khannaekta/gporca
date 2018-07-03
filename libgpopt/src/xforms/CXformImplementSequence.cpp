//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXformImplementSequence.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementSequence.h"

#include "gpopt/operators/ops.h"
#include "gpopt/metadata/CTableDescriptor.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementSequence::CXformImplementSequence
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformImplementSequence::CXformImplementSequence
	(
	IMemoryPool *memory_pool
	)
	:
	CXformImplementation
		(
		 // pattern
		GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalSequence(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternMultiLeaf(memory_pool))
				)
		)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementSequence::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformImplementSequence::Transform
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

	ExpressionArray *pdrgpexpr = pexpr->PdrgPexpr();
	pdrgpexpr->AddRef();
	
	// create alternative expression
	CExpression *pexprAlt = 
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CPhysicalSequence(memory_pool),
			pdrgpexpr
			);
	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}


// EOF

