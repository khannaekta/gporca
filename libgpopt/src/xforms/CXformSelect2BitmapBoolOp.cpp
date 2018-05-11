//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CXformSelect2BitmapBoolOp.cpp
//
//	@doc:
//		Transform select over table into a bitmap table get over bitmap bool op
//
//	@owner:
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformSelect2BitmapBoolOp.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpmd;
using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2BitmapBoolOp::CXformSelect2BitmapBoolOp
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformSelect2BitmapBoolOp::CXformSelect2BitmapBoolOp
	(
	IMemoryPool *memory_pool
	)
	:
	CXformExploration
		(
		GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalSelect(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CLogicalGet(memory_pool)),  // logical child
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool))  // predicate tree
				)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2BitmapBoolOp::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformSelect2BitmapBoolOp::Exfp
	(
	CExpressionHandle &  // exprhdl
	)
	const
{
	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2BitmapBoolOp::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformSelect2BitmapBoolOp::Transform
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

	CExpression *pexprResult = CXformUtils::PexprSelect2BitmapBoolOp(pxfctxt->Pmp(), pexpr);

	if (NULL != pexprResult)
	{
		pxfres->Add(pexprResult);
	}
}

// EOF
