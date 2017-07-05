//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementValuesGet.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementValuesGet.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;


// Ctor
CXformImplementValuesGet::CXformImplementValuesGet
	(
	IMemoryPool *pmp
	)
	:
	CXformImplementation
		(
		 // pattern
		GPOS_NEW(pmp) CExpression
				(
				pmp,
				GPOS_NEW(pmp) CLogicalValuesGet(pmp)
				)
		)
{}


// Actual transformation
void
CXformImplementValuesGet::Transform
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

	CLogicalValuesGet *popValuesGet = CLogicalValuesGet::PopConvert(pexpr->Pop());
	IMemoryPool *pmp = pxfctxt->Pmp();

	// create/extract components for alternative
	DrgPcoldesc *pdrgpcoldesc = popValuesGet->Pdrgpcoldesc();
	pdrgpcoldesc->AddRef();
	
	DrgPdrgPdatum *pdrgpdrgpdatum = popValuesGet->Pdrgpdrgpdatum();
	pdrgpdrgpdatum->AddRef();
	
	DrgPcr *pdrgpcrOutput = popValuesGet->PdrgpcrOutput();
	pdrgpcrOutput->AddRef();
		
	// create alternative expression
	CExpression *pexprAlt = 
		GPOS_NEW(pmp) CExpression
			(
			pmp,
			GPOS_NEW(pmp) CPhysicalValuesGet(pmp, pdrgpcoldesc, pdrgpdrgpdatum, pdrgpcrOutput)
			);
	
	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}


// EOF

