//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformDynamicGet2DynamicTableScan.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformDynamicGet2DynamicTableScan.h"

#include "gpopt/operators/ops.h"
#include "gpopt/metadata/CTableDescriptor.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicGet2DynamicTableScan::CXformDynamicGet2DynamicTableScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformDynamicGet2DynamicTableScan::CXformDynamicGet2DynamicTableScan
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
				GPOS_NEW(memory_pool) CLogicalDynamicGet(memory_pool)
				)
		)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicGet2DynamicTableScan::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformDynamicGet2DynamicTableScan::Transform
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

	CLogicalDynamicGet *popGet = CLogicalDynamicGet::PopConvert(pexpr->Pop());
	IMemoryPool *memory_pool = pxfctxt->Pmp();

	// create/extract components for alternative
	CName *pname = GPOS_NEW(memory_pool) CName(memory_pool, popGet->Name());
	
	CTableDescriptor *ptabdesc = popGet->Ptabdesc();
	ptabdesc->AddRef();
	
	ColRefArray *pdrgpcrOutput = popGet->PdrgpcrOutput();
	GPOS_ASSERT(NULL != pdrgpcrOutput);

	pdrgpcrOutput->AddRef();
	
	ColRefArrays *pdrgpdrgpcrPart = popGet->PdrgpdrgpcrPart();
	pdrgpdrgpcrPart->AddRef();
	
	popGet->Ppartcnstr()->AddRef();
	popGet->PpartcnstrRel()->AddRef();
	
	// create alternative expression
	CExpression *pexprAlt = 
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CPhysicalDynamicTableScan
						(
						memory_pool,
						popGet->IsPartial(),
						pname, 
						ptabdesc,
						popGet->UlOpId(),
						popGet->ScanId(), 
						pdrgpcrOutput,
						pdrgpdrgpcrPart,
						popGet->UlSecondaryScanId(),
						popGet->Ppartcnstr(),
						popGet->PpartcnstrRel()
						)
			);
	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}


// EOF

