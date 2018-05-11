//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CXformSelect2Apply.cpp
//
//	@doc:
//		Implementation of Select to Apply transform
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformSelect2Apply.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2Apply::CXformSelect2Apply
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformSelect2Apply::CXformSelect2Apply
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
				GPOS_NEW(memory_pool) CLogicalSelect(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool)), // relational child
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool))	// predicate tree
				)
		)
{}

// EOF

