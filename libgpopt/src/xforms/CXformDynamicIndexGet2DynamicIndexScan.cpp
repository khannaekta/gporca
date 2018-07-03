//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformDynamicIndexGet2DynamicIndexScan.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformDynamicIndexGet2DynamicIndexScan.h"

#include "gpopt/operators/ops.h"
#include "gpopt/metadata/CPartConstraint.h"
#include "gpopt/metadata/CTableDescriptor.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicIndexGet2DynamicIndexScan::CXformDynamicIndexGet2DynamicIndexScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformDynamicIndexGet2DynamicIndexScan::CXformDynamicIndexGet2DynamicIndexScan
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
				GPOS_NEW(memory_pool) CLogicalDynamicIndexGet(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool))	// index lookup predicate
				)
		)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformDynamicIndexGet2DynamicIndexScan::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformDynamicIndexGet2DynamicIndexScan::Transform
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

	CLogicalDynamicIndexGet *popIndexGet = CLogicalDynamicIndexGet::PopConvert(pexpr->Pop());
	IMemoryPool *memory_pool = pxfctxt->Pmp();

	// create/extract components for alternative
	CName *pname = GPOS_NEW(memory_pool) CName(memory_pool, popIndexGet->Name());
	
	CTableDescriptor *ptabdesc = popIndexGet->Ptabdesc();
	ptabdesc->AddRef();
	
	CIndexDescriptor *pindexdesc = popIndexGet->Pindexdesc();
	pindexdesc->AddRef();
	
	ColRefArray *pdrgpcrOutput = popIndexGet->PdrgpcrOutput();
	GPOS_ASSERT(NULL != pdrgpcrOutput);
	pdrgpcrOutput->AddRef();
	
	ColRefArrays *pdrgpdrgpcrPart = popIndexGet->PdrgpdrgpcrPart();
	pdrgpdrgpcrPart->AddRef();
	
	CPartConstraint *ppartcnstr = popIndexGet->Ppartcnstr();
	ppartcnstr->AddRef();
	
	CPartConstraint *ppartcnstrRel = popIndexGet->PpartcnstrRel();
	ppartcnstrRel->AddRef();
	
	COrderSpec *pos = popIndexGet->Pos();
	pos->AddRef();
	
	// extract components
	CExpression *pexprIndexCond = (*pexpr)[0];
	pexprIndexCond->AddRef();
	
	// create alternative expression
	CExpression *pexprAlt = 
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CPhysicalDynamicIndexScan
						(
						memory_pool,
						popIndexGet->IsPartial(),
						pindexdesc,
						ptabdesc,
						pexpr->Pop()->UlOpId(),
						pname,  
						pdrgpcrOutput,
						popIndexGet->ScanId(),
						pdrgpdrgpcrPart,
						popIndexGet->UlSecondaryScanId(),
						ppartcnstr,
						ppartcnstrRel,
						pos
						),
			pexprIndexCond
			);
	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}


// EOF

