//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CXformExternalGet2ExternalScan.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformExternalGet2ExternalScan.h"

#include "gpopt/operators/ops.h"
#include "gpopt/metadata/CTableDescriptor.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformExternalGet2ExternalScan::CXformExternalGet2ExternalScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformExternalGet2ExternalScan::CXformExternalGet2ExternalScan
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
				GPOS_NEW(memory_pool) CLogicalExternalGet(memory_pool)
				)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformExternalGet2ExternalScan::Exfp
//
//	@doc:
//		Compute promise of xform
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformExternalGet2ExternalScan::Exfp
	(
	CExpressionHandle & //exprhdl
	)
	const
{
	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformExternalGet2ExternalScan::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformExternalGet2ExternalScan::Transform
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

	CLogicalExternalGet *popGet = CLogicalExternalGet::PopConvert(pexpr->Pop());
	IMemoryPool *memory_pool = pxfctxt->Pmp();

	// extract components for alternative
	CName *pname = GPOS_NEW(memory_pool) CName(memory_pool, popGet->Name());

	CTableDescriptor *ptabdesc = popGet->Ptabdesc();
	ptabdesc->AddRef();

	ColRefArray *pdrgpcrOutput = popGet->PdrgpcrOutput();
	GPOS_ASSERT(NULL != pdrgpcrOutput);

	pdrgpcrOutput->AddRef();

	// create alternative expression
	CExpression *pexprAlt =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CPhysicalExternalScan(memory_pool, pname, ptabdesc, pdrgpcrOutput)
			);

	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}

// EOF

