//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CXformImplementPartitionSelector.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementPartitionSelector.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformImplementPartitionSelector::CXformImplementPartitionSelector
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformImplementPartitionSelector::CXformImplementPartitionSelector
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
					GPOS_NEW(memory_pool) CLogicalPartitionSelector(memory_pool),
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool))	// relational child
					)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformImplementPartitionSelector::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformImplementPartitionSelector::Transform
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

	IMemoryPool *memory_pool = pxfctxt->Pmp();

	// extract components
	CLogicalPartitionSelector *popSelector = CLogicalPartitionSelector::PopConvert(pexpr->Pop());
	CExpression *pexprRelational = (*pexpr)[0];

	IMDId *mdid = popSelector->MDId();

	// addref all components
	pexprRelational->AddRef();
	mdid->AddRef();

	HMUlExpr *phmulexprFilter = GPOS_NEW(memory_pool) HMUlExpr(memory_pool);

	const ULONG ulLevels = popSelector->UlPartLevels();
	for (ULONG ul = 0; ul < ulLevels; ul++)
	{
		CExpression *pexprFilter = popSelector->PexprPartFilter(ul);
		GPOS_ASSERT(NULL != pexprFilter);
		pexprFilter->AddRef();
#ifdef GPOS_DEBUG
		BOOL fInserted =
#endif
		phmulexprFilter->Insert(GPOS_NEW(memory_pool) ULONG(ul), pexprFilter);
		GPOS_ASSERT(fInserted);
	}

	// assemble physical operator
	CPhysicalPartitionSelectorDML *popPhysicalPartitionSelector =
			GPOS_NEW(memory_pool) CPhysicalPartitionSelectorDML
						(
						memory_pool,
						mdid,
						phmulexprFilter,
						popSelector->PcrOid()
						);

	CExpression *pexprPartitionSelector =
		GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					popPhysicalPartitionSelector,
					pexprRelational
					);

	// add alternative to results
	pxfres->Add(pexprPartitionSelector);
}

// EOF

