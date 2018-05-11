//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalInnerApply.cpp
//
//	@doc:
//		Implementation of inner apply operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CLogicalInnerApply.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerApply::CLogicalInnerApply
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalInnerApply::CLogicalInnerApply
	(
	IMemoryPool *memory_pool
	)
	:
	CLogicalApply(memory_pool)
{
	GPOS_ASSERT(NULL != memory_pool);

	m_fPattern = true;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerApply::CLogicalInnerApply
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CLogicalInnerApply::CLogicalInnerApply
	(
	IMemoryPool *memory_pool,
	DrgPcr *pdrgpcrInner,
	EOperatorId eopidOriginSubq
	)
	:
	CLogicalApply(memory_pool, pdrgpcrInner, eopidOriginSubq)
{
	GPOS_ASSERT(0 < pdrgpcrInner->Size());
}



//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerApply::~CLogicalInnerApply
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalInnerApply::~CLogicalInnerApply()
{}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerApply::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalInnerApply::Maxcard
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	return CLogical::Maxcard(exprhdl, 2 /*ulScalarIndex*/, MaxcardDef(exprhdl));
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerApply::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalInnerApply::PxfsCandidates
	(
	IMemoryPool *memory_pool
	) 
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	
	(void) xform_set->ExchangeSet(CXform::ExfInnerApply2InnerJoin);
	(void) xform_set->ExchangeSet(CXform::ExfInnerApply2InnerJoinNoCorrelations);
	(void) xform_set->ExchangeSet(CXform::ExfInnerApplyWithOuterKey2InnerJoin);
	
	return xform_set;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalInnerApply::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalInnerApply::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	DrgPcr *pdrgpcrInner = CUtils::PdrgpcrRemap(memory_pool, m_pdrgpcrInner, colref_mapping, must_exist);

	return GPOS_NEW(memory_pool) CLogicalInnerApply(memory_pool, pdrgpcrInner, m_eopidOriginSubq);
}

// EOF

