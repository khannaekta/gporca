//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformSelect2DynamicIndexGet.cpp
//
//	@doc:
//		Implementation of select over a partitioned table to a dynamic index get
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CConstraint.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/metadata/CPartConstraint.h"

#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformSelect2DynamicIndexGet.h"
#include "gpopt/xforms/CXformUtils.h"

#include "naucrates/md/CMDRelationGPDB.h"
#include "naucrates/md/CMDIndexGPDB.h"
#include "naucrates/md/IMDPartConstraint.h"

using namespace gpopt;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2DynamicIndexGet::CXformSelect2DynamicIndexGet
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformSelect2DynamicIndexGet::CXformSelect2DynamicIndexGet
	(
	IMemoryPool *memory_pool
	)
	:
	// pattern
	CXformExploration
		(
		GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalSelect(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CLogicalDynamicGet(memory_pool)), // relational child
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool))	// predicate tree
				)
		)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2DynamicIndexGet::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformSelect2DynamicIndexGet::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	if (exprhdl.GetDrvdScalarProps(1)->FHasSubquery())
	{
		return CXform::ExfpNone;
	}

	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformSelect2DynamicIndexGet::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformSelect2DynamicIndexGet::Transform
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
	CExpression *pexprRelational = (*pexpr)[0];
	CExpression *pexprScalar = (*pexpr)[1];

	// get the indexes on this relation
	CLogicalDynamicGet *popDynamicGet = CLogicalDynamicGet::PopConvert(pexprRelational->Pop());
	const ULONG ulIndices = popDynamicGet->Ptabdesc()->IndexCount();
	if (0 == ulIndices)
	{
		return;
	}

	// array of expressions in the scalar expression
	DrgPexpr *pdrgpexpr = CPredicateUtils::PdrgpexprConjuncts(memory_pool, pexprScalar);
	GPOS_ASSERT(0 < pdrgpexpr->Size());

	// derive the scalar and relational properties to build set of required columns
	CColRefSet *pcrsOutput = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput();
	CColRefSet *pcrsScalarExpr = CDrvdPropScalar::GetDrvdScalarProps(pexprScalar->PdpDerive())->PcrsUsed();

	CColRefSet *pcrsReqd = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrsReqd->Include(pcrsOutput);
	pcrsReqd->Include(pcrsScalarExpr);

	// find the indexes whose included columns meet the required columns
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	const IMDRelation *pmdrel = md_accessor->RetrieveRel(popDynamicGet->Ptabdesc()->MDId());

	for (ULONG ul = 0; ul < ulIndices; ul++)
	{
		IMDId *pmdidIndex = pmdrel->IndexMDidAt(ul);
		const IMDIndex *pmdindex = md_accessor->RetrieveIndex(pmdidIndex);
		CPartConstraint *ppartcnstrIndex = CUtils::PpartcnstrFromMDPartCnstr
								(
								memory_pool,
								COptCtxt::PoctxtFromTLS()->Pmda(),
								popDynamicGet->PdrgpdrgpcrPart(),
								pmdindex->MDPartConstraint(),
								popDynamicGet->PdrgpcrOutput()
								);
		CExpression *pexprDynamicIndexGet = CXformUtils::PexprLogicalIndexGet
							(
							memory_pool,
							md_accessor,
							pexprRelational,
							pexpr->Pop()->UlOpId(),
							pdrgpexpr,
							pcrsReqd,
							pcrsScalarExpr,
							NULL /*outer_refs*/,
							pmdindex,
							pmdrel,
							false /*fAllowPartialIndex*/,
							ppartcnstrIndex
							);
		if (NULL != pexprDynamicIndexGet)
		{
			// create a redundant SELECT on top of DynamicIndexGet to be able to use predicate in partition elimination

			CExpression *pexprRedundantSelect = CXformUtils::PexprRedundantSelectForDynamicIndex(memory_pool, pexprDynamicIndexGet);
			pexprDynamicIndexGet->Release();
			pxfres->Add(pexprRedundantSelect);
		}
	}

	pcrsReqd->Release();
	pdrgpexpr->Release();
}

// EOF

