//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CPhysicalDynamicBitmapTableScan.cpp
//
//	@doc:
//		Dynamic bitmap table scan physical operator
//
//	@owner:
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpopt/operators/CPhysicalDynamicBitmapTableScan.h"

#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/metadata/CPartConstraint.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPredicateUtils.h"

#include "naucrates/statistics/CStatisticsUtils.h"
#include "naucrates/statistics/CFilterStatsProcessor.h"
using namespace gpopt;
using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicBitmapTableScan::CPhysicalDynamicBitmapTableScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalDynamicBitmapTableScan::CPhysicalDynamicBitmapTableScan
	(
		IMemoryPool *memory_pool,
		BOOL is_partial,
		CTableDescriptor *ptabdesc,
		ULONG ulOriginOpId,
		const CName *pnameAlias,
		ULONG scan_id,
		DrgPcr *pdrgpcrOutput,
		DrgDrgPcr *pdrgpdrgpcrParts,
		ULONG ulSecondaryScanId,
		CPartConstraint *ppartcnstr,
		CPartConstraint *ppartcnstrRel
	)
	:
	CPhysicalDynamicScan(memory_pool, is_partial, ptabdesc, ulOriginOpId, pnameAlias, scan_id, pdrgpcrOutput, pdrgpdrgpcrParts, ulSecondaryScanId, ppartcnstr, ppartcnstrRel)
{}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicBitmapTableScan::Matches
//
//	@doc:
//		match operator
//
//---------------------------------------------------------------------------
BOOL
CPhysicalDynamicBitmapTableScan::Matches
	(
	COperator *pop
	)
	const
{
	return CUtils::FMatchDynamicBitmapScan(this, pop);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalDynamicBitmapTableScan::PstatsDerive
//
//	@doc:
//		Statistics derivation during costing
//
//---------------------------------------------------------------------------
IStatistics *
CPhysicalDynamicBitmapTableScan::PstatsDerive
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	CReqdPropPlan *prpplan,
	StatsArray *stats_ctxt
	)
	const
{
	GPOS_ASSERT(NULL != prpplan);

	IStatistics *pstatsBaseTable = CStatisticsUtils::DeriveStatsForDynamicScan
									(
									memory_pool,
									exprhdl,
									ScanId(),
									prpplan->Pepp()->PpfmDerived()
									);

	CExpression *pexprCondChild = exprhdl.PexprScalarChild(0 /*ulChidIndex*/);
	CExpression *local_expr = NULL;
	CExpression *expr_with_outer_refs = NULL;

	// get outer references from expression handle
	CColRefSet *outer_refs = exprhdl.GetRelationalProperties()->PcrsOuter();

	CPredicateUtils::SeparateOuterRefs(memory_pool, pexprCondChild, outer_refs, &local_expr, &expr_with_outer_refs);

	IStatistics *stats = CFilterStatsProcessor::MakeStatsFilterForScalarExpr
							(
							memory_pool,
							exprhdl,
							pstatsBaseTable,
							local_expr,
							expr_with_outer_refs,
							stats_ctxt
							);

	pstatsBaseTable->Release();
	local_expr->Release();
	expr_with_outer_refs->Release();

	return stats;
}

// EOF
