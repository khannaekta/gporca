//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CLogicalLeftSemiCorrelatedApplyIn.cpp
//
//	@doc:
//		Implementation of left semi correlated apply with IN semantics
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftSemiCorrelatedApplyIn.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiCorrelatedApplyIn::CLogicalLeftSemiCorrelatedApplyIn
//
//	@doc:
//		Ctor - for patterns
//
//---------------------------------------------------------------------------
CLogicalLeftSemiCorrelatedApplyIn::CLogicalLeftSemiCorrelatedApplyIn
	(
	IMemoryPool *memory_pool
	)
	:
	CLogicalLeftSemiApplyIn(memory_pool)
{}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiCorrelatedApplyIn::CLogicalLeftSemiCorrelatedApplyIn
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalLeftSemiCorrelatedApplyIn::CLogicalLeftSemiCorrelatedApplyIn
	(
	IMemoryPool *memory_pool,
	ColRefArray *pdrgpcrInner,
	EOperatorId eopidOriginSubq
	)
	:
	CLogicalLeftSemiApplyIn(memory_pool, pdrgpcrInner, eopidOriginSubq)
{}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiCorrelatedApplyIn::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalLeftSemiCorrelatedApplyIn::PxfsCandidates
	(
	IMemoryPool *memory_pool
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfImplementLeftSemiCorrelatedApplyIn);

	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiCorrelatedApplyIn::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalLeftSemiCorrelatedApplyIn::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	ColRefArray *pdrgpcrInner = CUtils::PdrgpcrRemap(memory_pool, m_pdrgpcrInner, colref_mapping, must_exist);

	return GPOS_NEW(memory_pool) CLogicalLeftSemiCorrelatedApplyIn(memory_pool, pdrgpcrInner, m_eopidOriginSubq);
}


// EOF

