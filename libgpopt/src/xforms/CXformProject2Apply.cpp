//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformProject2Apply.cpp
//
//	@doc:
//		Implementation of Project to Apply transform
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformProject2Apply.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformProject2Apply::CXformProject2Apply
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformProject2Apply::CXformProject2Apply
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
				GPOS_NEW(memory_pool) CLogicalProject(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)),	// relational child
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool))	// scalar project list
				)
		)
{}


// EOF

