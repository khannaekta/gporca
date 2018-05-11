//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformSelect2Filter.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformSelect2Filter.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2Filter::CXformSelect2Filter
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformSelect2Filter::CXformSelect2Filter
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
						GPOS_NEW(memory_pool) CLogicalSelect(memory_pool),
						GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // relational child
						GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool))	// predicate
						)
		)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2Filter::Exfp
//
//	@doc:
//		Compute xform promise level for a given expression handle;
// 		if scalar predicate has a subquery, then we must have an
// 		equivalent logical Apply expression created during exploration;
// 		no need for generating a Filter expression here
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformSelect2Filter::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	if (exprhdl.GetDrvdScalarProps(1)->FHasSubquery())
	{
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2Filter::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformSelect2Filter::Transform
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
	CExpression *pexprRelational = (*pexpr)[0];
	CExpression *pexprScalar = (*pexpr)[1];
	
	// addref all children
	pexprRelational->AddRef();
	pexprScalar->AddRef();
	
	// assemble physical operator
	CExpression *pexprFilter = 
		GPOS_NEW(memory_pool) CExpression
					(
					memory_pool, 
					GPOS_NEW(memory_pool) CPhysicalFilter(memory_pool),
					pexprRelational,
					pexprScalar
					);
	
	// add alternative to results
	pxfres->Add(pexprFilter);
}
	

// EOF

