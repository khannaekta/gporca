//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CLogicalLeftSemiCorrelatedApply.cpp
//
//	@doc:
//		Implementation of left semi correlated apply for EXISTS subqueries
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CLogicalLeftSemiCorrelatedApply.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiCorrelatedApply::CLogicalLeftSemiCorrelatedApply
//
//	@doc:
//		Ctor - for patterns
//
//---------------------------------------------------------------------------
CLogicalLeftSemiCorrelatedApply::CLogicalLeftSemiCorrelatedApply
	(
	IMemoryPool *memory_pool
	)
	:
	CLogicalLeftSemiApply(memory_pool)
{}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiCorrelatedApply::CLogicalLeftSemiCorrelatedApply
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalLeftSemiCorrelatedApply::CLogicalLeftSemiCorrelatedApply
	(
	IMemoryPool *memory_pool,
	ColRefArray *pdrgpcrInner,
	EOperatorId eopidOriginSubq
	)
	:
	CLogicalLeftSemiApply(memory_pool, pdrgpcrInner, eopidOriginSubq)
{}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiCorrelatedApply::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalLeftSemiCorrelatedApply::PxfsCandidates
	(
	IMemoryPool *memory_pool
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfImplementLeftSemiCorrelatedApply);

	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLeftSemiCorrelatedApply::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalLeftSemiCorrelatedApply::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	ColRefArray *pdrgpcrInner = CUtils::PdrgpcrRemap(memory_pool, m_pdrgpcrInner, colref_mapping, must_exist);

	return GPOS_NEW(memory_pool) CLogicalLeftSemiCorrelatedApply(memory_pool, pdrgpcrInner, m_eopidOriginSubq);
}


// EOF

