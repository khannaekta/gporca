//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementRowTrigger.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementRowTrigger.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementRowTrigger::CXformImplementRowTrigger
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformImplementRowTrigger::CXformImplementRowTrigger
	(
	IMemoryPool *memory_pool
	)
	:
	CXformImplementation
		(
		 // pattern
		GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalRowTrigger(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool))
				)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformImplementRowTrigger::Exfp
//
//	@doc:
//		Compute promise of xform
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformImplementRowTrigger::Exfp
	(
	CExpressionHandle & // exprhdl
	)
	const
{
	return CXform::ExfpHigh;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementRowTrigger::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformImplementRowTrigger::Transform
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

	CLogicalRowTrigger *popRowTrigger = CLogicalRowTrigger::PopConvert(pexpr->Pop());
	IMemoryPool *memory_pool = pxfctxt->Pmp();

	// extract components for alternative
	IMDId *rel_mdid = popRowTrigger->GetRelMdId();
	rel_mdid->AddRef();

	INT type = popRowTrigger->GetType();

	ColRefArray *pdrgpcrOld = popRowTrigger->PdrgpcrOld();
	if (NULL != pdrgpcrOld)
	{
		pdrgpcrOld->AddRef();
	}

	ColRefArray *pdrgpcrNew = popRowTrigger->PdrgpcrNew();
	if (NULL != pdrgpcrNew)
	{
		pdrgpcrNew->AddRef();
	}

	// child of RowTrigger operator
	CExpression *pexprChild = (*pexpr)[0];
	pexprChild->AddRef();

	// create physical RowTrigger
	CExpression *pexprAlt =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CPhysicalRowTrigger(memory_pool, rel_mdid, type, pdrgpcrOld, pdrgpcrNew),
			pexprChild
			);
	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}

// EOF
