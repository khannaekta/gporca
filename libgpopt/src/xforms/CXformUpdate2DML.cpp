//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformUpdate2DML.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformUpdate2DML.h"
#include "gpopt/xforms/CXformUtils.h"

#include "gpopt/operators/ops.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/optimizer/COptimizerConfig.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformUpdate2DML::CXformUpdate2DML
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformUpdate2DML::CXformUpdate2DML
	(
	IMemoryPool *memory_pool
	)
	:
	CXformExploration
		(
		 // pattern
		GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalUpdate(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternLeaf(memory_pool))
				)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformUpdate2DML::Exfp
//
//	@doc:
//		Compute promise of xform
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformUpdate2DML::Exfp
	(
	CExpressionHandle & // exprhdl
	)
	const
{
	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformUpdate2DML::Transform
//
//	@doc:
//		Actual transformation
//
//---------------------------------------------------------------------------
void
CXformUpdate2DML::Transform
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

	CLogicalUpdate *popUpdate = CLogicalUpdate::PopConvert(pexpr->Pop());
	IMemoryPool *memory_pool = pxfctxt->Pmp();

	// extract components for alternative

	CTableDescriptor *ptabdesc = popUpdate->Ptabdesc();
	DrgPcr *pdrgpcrDelete = popUpdate->PdrgpcrDelete();
	DrgPcr *pdrgpcrInsert = popUpdate->PdrgpcrInsert();
	CColRef *pcrCtid = popUpdate->PcrCtid();
	CColRef *pcrSegmentId = popUpdate->PcrSegmentId();
	CColRef *pcrTupleOid = popUpdate->PcrTupleOid();
	
	// child of update operator
	CExpression *pexprChild = (*pexpr)[0];
	pexprChild->AddRef();
	
	IMDId *rel_mdid = ptabdesc->MDId();
	if (CXformUtils::FTriggersExist(CLogicalDML::EdmlUpdate, ptabdesc, true /*fBefore*/))
	{
		rel_mdid->AddRef();
		pdrgpcrDelete->AddRef();
		pdrgpcrInsert->AddRef();
		pexprChild = CXformUtils::PexprRowTrigger
							(
							memory_pool,
							pexprChild,
							CLogicalDML::EdmlUpdate,
							rel_mdid,
							true /*fBefore*/,
							pdrgpcrDelete,
							pdrgpcrInsert
							);
	}

	// generate the action column and split operator
	COptCtxt *poctxt = COptCtxt::PoctxtFromTLS();
	CMDAccessor *md_accessor = poctxt->Pmda();
	CColumnFactory *col_factory = poctxt->Pcf();

	pdrgpcrDelete->AddRef();
	pdrgpcrInsert->AddRef();

	const IMDType *pmdtype = md_accessor->PtMDType<IMDTypeInt4>();
	CColRef *pcrAction = col_factory->PcrCreate(pmdtype, default_type_modifier);
	
	CExpression *pexprProjElem = GPOS_NEW(memory_pool) CExpression
											(
											memory_pool,
											GPOS_NEW(memory_pool) CScalarProjectElement(memory_pool, pcrAction),
											GPOS_NEW(memory_pool) CExpression
														(
														memory_pool,
														GPOS_NEW(memory_pool) CScalarDMLAction(memory_pool)
														)
											);
	
	CExpression *pexprProjList = GPOS_NEW(memory_pool) CExpression
											(
											memory_pool,
											GPOS_NEW(memory_pool) CScalarProjectList(memory_pool),
											pexprProjElem
											);
	CExpression *pexprSplit =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CLogicalSplit(memory_pool,	pdrgpcrDelete, pdrgpcrInsert, pcrCtid, pcrSegmentId, pcrAction, pcrTupleOid),
			pexprChild,
			pexprProjList
			);

	// add assert checking that no NULL values are inserted for nullable columns or no check constraints are violated
	COptimizerConfig *optimizer_config = COptCtxt::PoctxtFromTLS()->GetOptimizerConfig();
	CExpression *pexprAssertConstraints;
	if (optimizer_config->GetHint()->FEnforceConstraintsOnDML())
	{
		pexprAssertConstraints = CXformUtils::PexprAssertConstraints
			(
			memory_pool,
			pexprSplit,
			ptabdesc,
			pdrgpcrInsert
			);
	}
	else
	{
		pexprAssertConstraints = pexprSplit;
	}

	// generate oid column and project operator
	CExpression *pexprProject = NULL;
	CColRef *pcrTableOid = NULL;
	if (ptabdesc->IsPartitioned())
	{
		// generate a partition selector
		pexprProject = CXformUtils::PexprLogicalPartitionSelector(memory_pool, ptabdesc, pdrgpcrInsert, pexprAssertConstraints);
		pcrTableOid = CLogicalPartitionSelector::PopConvert(pexprProject->Pop())->PcrOid();
	}
	else
	{
		// generate a project operator
		IMDId *pmdidTable = ptabdesc->MDId();

		OID oidTable = CMDIdGPDB::CastMdid(pmdidTable)->OidObjectId();
		CExpression *pexprOid = CUtils::PexprScalarConstOid(memory_pool, oidTable);

		pexprProject = CUtils::PexprAddProjection(memory_pool, pexprAssertConstraints, pexprOid);

		CExpression *pexprPrL = (*pexprProject)[1];
		pcrTableOid = CUtils::PcrFromProjElem((*pexprPrL)[0]);
	}
	
	GPOS_ASSERT(NULL != pcrTableOid);

	const ULONG num_cols = pdrgpcrInsert->Size();

	CBitSet *pbsModified = GPOS_NEW(memory_pool) CBitSet(memory_pool, ptabdesc->ColumnCount());
	for (ULONG ul = 0; ul < num_cols; ul++)
	{
		CColRef *pcrInsert = (*pdrgpcrInsert)[ul];
		CColRef *pcrDelete = (*pdrgpcrDelete)[ul];
		if (pcrInsert != pcrDelete)
		{
			// delete columns refer to the original tuple's descriptor, if it's different 
			// from the corresponding insert column, then we're modifying the column
			// at that position
			pbsModified->ExchangeSet(ul);
		}
	}
	// create logical DML
	ptabdesc->AddRef();
	pdrgpcrInsert->AddRef();
	CExpression *pexprDML =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CLogicalDML(memory_pool, CLogicalDML::EdmlUpdate, ptabdesc, pdrgpcrInsert, pbsModified, pcrAction, pcrTableOid, pcrCtid, pcrSegmentId, pcrTupleOid),
			pexprProject
			);
	
	// TODO:  - Oct 30, 2012; detect and handle AFTER triggers on update
	
	pxfres->Add(pexprDML);

}

// EOF
