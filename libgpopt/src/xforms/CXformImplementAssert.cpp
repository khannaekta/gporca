//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementAssert.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementAssert.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementAssert::CXformImplementAssert
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformImplementAssert::CXformImplementAssert
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
						GPOS_NEW(memory_pool) CLogicalAssert(memory_pool),
						GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // relational child
						GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool))	// predicate
						)
		)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementAssert::Exfp
//
//	@doc:
//		Compute xform promise level for a given expression handle;
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformImplementAssert::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	if(exprhdl.GetDrvdScalarProps(1)->FHasSubquery())
	{		
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementAssert::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformImplementAssert::Transform
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
	CLogicalAssert *popAssert = CLogicalAssert::PopConvert(pexpr->Pop());
	CExpression *pexprRelational = (*pexpr)[0];
	CExpression *pexprScalar = (*pexpr)[1];
	CException *pexc = popAssert->Pexc();
	
	// addref all children
	pexprRelational->AddRef();
	pexprScalar->AddRef();
	
	// assemble physical operator
	CPhysicalAssert *popPhysicalAssert = 
			GPOS_NEW(memory_pool) CPhysicalAssert
						(
						memory_pool, 
						GPOS_NEW(memory_pool) CException(pexc->Major(), pexc->Minor(), pexc->Filename(), pexc->Line())
						);
	
	CExpression *pexprAssert = 
		GPOS_NEW(memory_pool) CExpression
					(
					memory_pool, 
					popPhysicalAssert,
					pexprRelational,
					pexprScalar
					);
	
	// add alternative to results
	pxfres->Add(pexprAssert);
}
	

// EOF

