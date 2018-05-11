//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementTVFNoArgs.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementTVFNoArgs.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CXformImplementTVFNoArgs::CXformImplementTVFNoArgs
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformImplementTVFNoArgs::CXformImplementTVFNoArgs
	(
	IMemoryPool *memory_pool
	)
	:
	CXformImplementTVF
		(
		 // pattern
		GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalTVF(memory_pool)
				)
		)
{}

// EOF
