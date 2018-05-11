//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementTVF.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementTVF.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementTVF::CXformImplementTVF
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformImplementTVF::CXformImplementTVF
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
				GPOS_NEW(memory_pool) CLogicalTVF(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternMultiLeaf(memory_pool))
				)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformImplementTVF::CXformImplementTVF
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformImplementTVF::CXformImplementTVF
	(
	CExpression *pexpr
	)
	:
	CXformImplementation(pexpr)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementTVF::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformImplementTVF::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	const ULONG arity = exprhdl.Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		if (exprhdl.GetDrvdScalarProps(ul)->FHasSubquery())
		{
			// xform is inapplicable if TVF argument is a subquery
			return CXform::ExfpNone;
		}
	}

	return CXform::ExfpHigh;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementTVF::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformImplementTVF::Transform
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

	CLogicalTVF *popTVF = CLogicalTVF::PopConvert(pexpr->Pop());
	IMemoryPool *memory_pool = pxfctxt->Pmp();

	// create/extract components for alternative
	IMDId *mdid_func = popTVF->FuncMdId();
	mdid_func->AddRef();

	IMDId *mdid_return_type = popTVF->ReturnTypeMdId();
	mdid_return_type->AddRef();

	CWStringConst *str = GPOS_NEW(memory_pool) CWStringConst(popTVF->Pstr()->GetBuffer());

	DrgPcoldesc *pdrgpcoldesc = popTVF->Pdrgpcoldesc();
	pdrgpcoldesc->AddRef();

	DrgPcr *pdrgpcrOutput = popTVF->PdrgpcrOutput();
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrs->Include(pdrgpcrOutput);

	DrgPexpr *pdrgpexpr = pexpr->PdrgPexpr();

	CPhysicalTVF *pphTVF = GPOS_NEW(memory_pool) CPhysicalTVF(memory_pool, mdid_func, mdid_return_type, str, pdrgpcoldesc, pcrs);

	CExpression *pexprAlt = NULL;
	// create alternative expression
	if(NULL == pdrgpexpr || 0 == pdrgpexpr->Size())
	{
		pexprAlt = GPOS_NEW(memory_pool) CExpression(memory_pool, pphTVF);
	}
	else
	{
		pdrgpexpr->AddRef();
		pexprAlt = GPOS_NEW(memory_pool) CExpression(memory_pool, pphTVF, pdrgpexpr);
	}

	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}


// EOF

