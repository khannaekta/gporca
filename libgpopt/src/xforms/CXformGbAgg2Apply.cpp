//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformGbAgg2Apply.cpp
//
//	@doc:
//		Implementation of GbAgg to Apply transform
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformGbAgg2Apply.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformGbAgg2Apply::CXformGbAgg2Apply
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformGbAgg2Apply::CXformGbAgg2Apply
	(
	IMemoryPool *memory_pool
	)
	:
	// pattern
	CXformSubqueryUnnest
		(
		GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalGbAgg(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)),	// relational child
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool))	// project list
				)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformGbAgg2Apply::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle;
//		scalar child must have subquery
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformGbAgg2Apply::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	CLogicalGbAgg *popGbAgg = CLogicalGbAgg::PopConvert(exprhdl.Pop());
	if (popGbAgg->FGlobal() && exprhdl.GetDrvdScalarProps(1)->FHasSubquery())
	{
		return CXform::ExfpHigh;
	}

	return CXform::ExfpNone;
}


// EOF

