//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformIndexGet2IndexScan.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformIndexGet2IndexScan.h"

#include "gpopt/operators/ops.h"
#include "gpopt/metadata/CIndexDescriptor.h"
#include "gpopt/metadata/CTableDescriptor.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformIndexGet2IndexScan::CXformIndexGet2IndexScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformIndexGet2IndexScan::CXformIndexGet2IndexScan
	(
	IMemoryPool *memory_pool
	)
	:
	// pattern
	CXformImplementation
		(
		GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalIndexGet(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool))	// index lookup predicate
				)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformIndexGet2IndexScan::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformIndexGet2IndexScan::Transform
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

	CLogicalIndexGet *pop = CLogicalIndexGet::PopConvert(pexpr->Pop());
	IMemoryPool *memory_pool = pxfctxt->Pmp();

	CIndexDescriptor *pindexdesc = pop->Pindexdesc();
	pindexdesc->AddRef();

	CTableDescriptor *ptabdesc = pop->Ptabdesc();
	ptabdesc->AddRef();

	ColRefArray *pdrgpcrOutput = pop->PdrgpcrOutput();
	GPOS_ASSERT(NULL != pdrgpcrOutput);
	pdrgpcrOutput->AddRef();

	COrderSpec *pos = pop->Pos();
	GPOS_ASSERT(NULL != pos);
	pos->AddRef();

	// extract components
	CExpression *pexprIndexCond = (*pexpr)[0];

	// addref all children
	pexprIndexCond->AddRef();

	CExpression *pexprAlt =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CPhysicalIndexScan
				(
				memory_pool,
				pindexdesc,
				ptabdesc,
				pexpr->Pop()->UlOpId(),
				GPOS_NEW(memory_pool) CName (memory_pool, pop->NameAlias()),
				pdrgpcrOutput,
				pos
				),
			pexprIndexCond
			);
	pxfres->Add(pexprAlt);
}


// EOF

