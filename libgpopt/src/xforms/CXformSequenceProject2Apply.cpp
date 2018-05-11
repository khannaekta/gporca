//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformSequenceProject2Apply.cpp
//
//	@doc:
//		Implementation of Sequence Project to Apply transform
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformSequenceProject2Apply.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformSequenceProject2Apply::CXformSequenceProject2Apply
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformSequenceProject2Apply::CXformSequenceProject2Apply
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
				GPOS_NEW(memory_pool) CLogicalSequenceProject(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)),	// relational child
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool))	// project list
				)
		)
{}

// EOF

