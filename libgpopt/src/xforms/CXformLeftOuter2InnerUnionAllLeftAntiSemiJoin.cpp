//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin.cpp
//
//	@doc:
//		Transform
//      LOJ
//        |--Small
//        +--Big
//
// 		to
//
//      UnionAll
//      |---CTEConsumer(A)
//      +---Project_{append nulls)
//          +---LASJ_(key(Small))
//                   |---CTEConsumer(B)
//                   +---Gb(keys(Small))
//                        +---CTEConsumer(A)
//
//		where B is the CTE that produces Small
//		and A is the CTE that produces InnerJoin(Big, CTEConsumer(B)).
//
//	@owner:
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpopt/xforms/CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin.h"

#include "gpos/memory/CAutoMemoryPool.h"

#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformUtils.h"

#include "naucrates/traceflags/traceflags.h"

using namespace gpopt;

// if ratio of the cardinalities outer/inner is below this m_bytearray_value, we apply the xform
const DOUBLE
CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::m_dOuterInnerRatioThreshold = 0.001;

//---------------------------------------------------------------------------
//	@function:
//		CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin
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
				GPOS_NEW(memory_pool) CLogicalLeftOuterJoin(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool)), // left child
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool)), // right child
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool))  // predicate
				)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle.
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::Exfp
	(
	CExpressionHandle &exprhdl
	)
	const
{
	CColRefSet *pcrsInner = exprhdl.GetRelationalProperties(1 /*child_index*/)->PcrsOutput();
	CExpression *pexprScalar = exprhdl.PexprScalarChild(2 /*child_index*/);
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	if (!CPredicateUtils::FSimpleEqualityUsingCols(memory_pool, pexprScalar, pcrsInner))
	{
		return ExfpNone;
	}

	if (GPOS_FTRACE(gpos::EopttraceApplyLeftOuter2InnerUnionAllLeftAntiSemiJoinDisregardingStats)
			|| NULL == exprhdl.Pgexpr())
	{
		return CXform::ExfpHigh;
	}

	// check if stats are derivable on child groups
	const ULONG arity = exprhdl.Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CGroup *pgroupChild = (*exprhdl.Pgexpr())[ul];
		if (!pgroupChild->FScalar() && !pgroupChild->FStatsDerivable(memory_pool))
		{
			// stats must be derivable on every child
			return CXform::ExfpNone;
		}
	}

	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::FCheckStats
//
//	@doc:
//		Check the stats ratio to decide whether to apply the xform or not.
//
//---------------------------------------------------------------------------
BOOL
CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::FApplyXformUsingStatsInfo
	(
	const IStatistics *outer_stats,
	const IStatistics *inner_side_stats
	)
	const
{
	if (GPOS_FTRACE(gpos::EopttraceApplyLeftOuter2InnerUnionAllLeftAntiSemiJoinDisregardingStats))
	{
		return true;
	}

	if (NULL == outer_stats || NULL == inner_side_stats)
	{
		return false;
	}

	DOUBLE num_rows_outer = outer_stats->Rows().Get();
	DOUBLE dRowsInner = inner_side_stats->Rows().Get();
	GPOS_ASSERT(0 < dRowsInner);

	return num_rows_outer / dRowsInner <= m_dOuterInnerRatioThreshold;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::Transform
//
//	@doc:
//		Apply the transformation, e.g.
//  Input:
//  +--CLogicalLeftOuterJoin
//     |--CLogicalGet "items", Columns: ["i_item_sk" (95)]
//     |--CLogicalGet "store_sales", Columns: ["ss_item_sk" (124)]
//     +--CScalarCmp (=)
//        |--CScalarIdent "i_item_sk"
//        +--CScalarIdent "ss_item_sk"
//  Output:
//  Alternatives:
//  0:
//  +--CLogicalCTEAnchor (2)
//     +--CLogicalCTEAnchor (3)
//        +--CLogicalUnionAll ["i_item_sk" (95), "ss_item_sk" (124)]
//           |--CLogicalCTEConsumer (3), Columns: ["i_item_sk" (95), "ss_item_sk" (124)]
//           +--CLogicalProject
//              |--CLogicalLeftAntiSemiJoin
//              |  |--CLogicalCTEConsumer (2), Columns: ["i_item_sk" (342)]
//              |  |--CLogicalGbAgg( GetGlobalMemoryPool ) Grp Cols: ["i_item_sk" (343)][Global], Minimal Grp Cols: [], Generates Duplicates :[ 0 ]
//              |  |  |--CLogicalCTEConsumer (3), Columns: ["i_item_sk" (343), "ss_item_sk" (344)]
//              |  |  +--CScalarProjectList
//              |  +--CScalarBoolOp (EboolopNot)
//              |     +--CScalarIsDistinctFrom (=)
//              |        |--CScalarIdent "i_item_sk" (342)
//              |        +--CScalarIdent "i_item_sk" (343)
//              +--CScalarProjectList
//                 +--CScalarProjectElement "ss_item_sk" (466)
//                    +--CScalarConst (null)
//
//  +--CLogicalCTEProducer (2), Columns: ["i_item_sk" (190)]
//     +--CLogicalGet "items", Columns: ["i_item_sk" (190)]
//
//  +--CLogicalCTEProducer (3), Columns: ["i_item_sk" (247), "ss_item_sk" (248)]
//      +--CLogicalInnerJoin
//         |--CLogicalCTEConsumer (0), Columns: ["ss_item_sk" (248)]
//         |--CLogicalCTEConsumer (2), Columns: ["i_item_sk" (247)]
//         +--CScalarCmp (=)
//            |--CScalarIdent "i_item_sk" (247)
//            +--CScalarIdent "ss_item_sk" (248)
//
//---------------------------------------------------------------------------
void
CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::Transform
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
	CExpression *pexprOuter = (*pexpr)[0];
	CExpression *pexprInner = (*pexpr)[1];
	CExpression *pexprScalar = (*pexpr)[2];

	if (!FValidInnerExpr(pexprInner))
	{
		return;
	}

	if (!FApplyXformUsingStatsInfo(pexprOuter->Pstats(), pexprInner->Pstats()))
	{
		return;
	}

	const ULONG ulCTEOuterId = COptCtxt::PoctxtFromTLS()->Pcteinfo()->next_id();
	CColRefSet *outer_refs = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();
	ColRefArray *pdrgpcrOuter = outer_refs->Pdrgpcr(memory_pool);
	(void) CXformUtils::PexprAddCTEProducer(memory_pool, ulCTEOuterId, pdrgpcrOuter, pexprOuter);

	// invert the order of the branches of the original join, so that the small one becomes
	// inner
	pexprInner->AddRef();
	pexprScalar->AddRef();
	CExpression *pexprInnerJoin = GPOS_NEW(memory_pool) CExpression
									(
									memory_pool,
									GPOS_NEW(memory_pool) CLogicalInnerJoin(memory_pool),
									pexprInner,
									CXformUtils::PexprCTEConsumer(memory_pool, ulCTEOuterId, pdrgpcrOuter),
									pexprScalar
									);

	CColRefSet *pcrsJoinOutput = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput();
	ColRefArray *pdrgpcrJoinOutput = pcrsJoinOutput->Pdrgpcr(memory_pool);
	const ULONG ulCTEJoinId = COptCtxt::PoctxtFromTLS()->Pcteinfo()->next_id();
	(void) CXformUtils::PexprAddCTEProducer(memory_pool, ulCTEJoinId, pdrgpcrJoinOutput, pexprInnerJoin);

	CColRefSet *pcrsScalar = CDrvdPropScalar::GetDrvdScalarProps(pexprScalar->PdpDerive())->PcrsUsed();
	CColRefSet *pcrsInner = CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->PcrsOutput();

	ColRefArray *pdrgpcrProjectOutput = NULL;
	CExpression *pexprProjectAppendNulls = PexprProjectOverLeftAntiSemiJoin
											(
											memory_pool,
											pdrgpcrOuter,
											pcrsScalar,
											pcrsInner,
											pdrgpcrJoinOutput,
											ulCTEJoinId,
											ulCTEOuterId,
											&pdrgpcrProjectOutput
											);
	GPOS_ASSERT(NULL != pdrgpcrProjectOutput);

	ColRefArrays *pdrgpdrgpcrUnionInput = GPOS_NEW(memory_pool) ColRefArrays(memory_pool);
	pdrgpcrJoinOutput->AddRef();
	pdrgpdrgpcrUnionInput->Append(pdrgpcrJoinOutput);
	pdrgpdrgpcrUnionInput->Append(pdrgpcrProjectOutput);
	pdrgpcrJoinOutput->AddRef();

	CExpression *pexprUnionAll =
			GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalUnionAll(memory_pool, pdrgpcrJoinOutput, pdrgpdrgpcrUnionInput),
					CXformUtils::PexprCTEConsumer(memory_pool, ulCTEJoinId, pdrgpcrJoinOutput),
					pexprProjectAppendNulls
					);
	CExpression *pexprJoinAnchor = GPOS_NEW(memory_pool) CExpression
									(
									memory_pool,
									GPOS_NEW(memory_pool) CLogicalCTEAnchor(memory_pool, ulCTEJoinId),
									pexprUnionAll
									);
	CExpression *pexprOuterAnchor = GPOS_NEW(memory_pool) CExpression
									(
									memory_pool,
									GPOS_NEW(memory_pool) CLogicalCTEAnchor(memory_pool, ulCTEOuterId),
									pexprJoinAnchor
									);
	pexprInnerJoin->Release();

	pxfres->Add(pexprOuterAnchor);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::FValidInnerExpr
//
//	@doc:
//		Check if the inner expression is of a type which should be considered
//		by this xform.
//
//---------------------------------------------------------------------------
BOOL
CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::FValidInnerExpr
	(
	CExpression *pexprInner
	)
{
	GPOS_ASSERT(NULL != pexprInner);

	// set of inner operator ids that should not be considered because they usually
	// generate a relatively small number of tuples
	COperator::EOperatorId rgeopids[] =
	{
		COperator::EopLogicalConstTableGet,
		COperator::EopLogicalGbAgg,
		COperator::EopLogicalLimit,
	};

	const COperator::EOperatorId op_id = pexprInner->Pop()->Eopid();
	for (ULONG ul = 0; ul < GPOS_ARRAY_SIZE(rgeopids); ++ul)
	{
		if (rgeopids[ul] == op_id)
		{
			return false;
		}
	}

	return true;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::PexprLeftAntiSemiJoinWithInnerGroupBy
//
//	@doc:
//		Construct a left anti semi join with the CTE consumer (ulCTEJoinId) as outer
//		and a group by as inner.
//
//---------------------------------------------------------------------------
CExpression *
CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::PexprLeftAntiSemiJoinWithInnerGroupBy
	(
	IMemoryPool *memory_pool,
	ColRefArray *pdrgpcrOuter,
	ColRefArray *pdrgpcrOuterCopy,
	CColRefSet *pcrsScalar,
	CColRefSet *pcrsInner,
	ColRefArray *pdrgpcrJoinOutput,
	ULONG ulCTEJoinId,
	ULONG ulCTEOuterId
	)
{
	// compute the original outer keys and their correspondent keys on the two branches
	// of the LASJ
	CColRefSet *pcrsOuterKeys = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrsOuterKeys->Include(pcrsScalar);
	pcrsOuterKeys->Difference(pcrsInner);
	ColRefArray *pdrgpcrOuterKeys = pcrsOuterKeys->Pdrgpcr(memory_pool);

	ColRefArray *pdrgpcrConsumer2Output = CUtils::PdrgpcrCopy(memory_pool, pdrgpcrJoinOutput);
	ULongPtrArray *pdrgpulIndexesOfOuterInGby = pdrgpcrJoinOutput->IndexesOfSubsequence(pdrgpcrOuterKeys);

	GPOS_ASSERT(NULL != pdrgpulIndexesOfOuterInGby);
	ColRefArray *pdrgpcrGbyKeys =
			CXformUtils::PdrgpcrReorderedSubsequence(memory_pool, pdrgpcrConsumer2Output, pdrgpulIndexesOfOuterInGby);

	CExpression *pexprGby =
			GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalGbAgg(memory_pool, pdrgpcrGbyKeys, COperator::EgbaggtypeGlobal),
				CXformUtils::PexprCTEConsumer(memory_pool, ulCTEJoinId, pdrgpcrConsumer2Output),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool))
				);

	ULongPtrArray *pdrgpulIndexesOfOuterKeys = pdrgpcrOuter->IndexesOfSubsequence(pdrgpcrOuterKeys);
	GPOS_ASSERT(NULL != pdrgpulIndexesOfOuterKeys);
	ColRefArray *pdrgpcrKeysInOuterCopy =
			CXformUtils::PdrgpcrReorderedSubsequence(memory_pool, pdrgpcrOuterCopy, pdrgpulIndexesOfOuterKeys);

	ColRefArrays *pdrgpdrgpcrLASJInput = GPOS_NEW(memory_pool) ColRefArrays(memory_pool);
	pdrgpdrgpcrLASJInput->Append(pdrgpcrKeysInOuterCopy);
	pdrgpcrGbyKeys->AddRef();
	pdrgpdrgpcrLASJInput->Append(pdrgpcrGbyKeys);

	pcrsOuterKeys->Release();
	pdrgpcrOuterKeys->Release();

	CExpression *pexprLeftAntiSemi =
			GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalLeftAntiSemiJoin(memory_pool),
					CXformUtils::PexprCTEConsumer(memory_pool, ulCTEOuterId, pdrgpcrOuterCopy),
					pexprGby,
					CUtils::PexprConjINDFCond(memory_pool, pdrgpdrgpcrLASJInput)
					);

	pdrgpdrgpcrLASJInput->Release();
	pdrgpulIndexesOfOuterInGby->Release();
	pdrgpulIndexesOfOuterKeys->Release();

	return pexprLeftAntiSemi;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::PexprProjectOverLeftAntiSemiJoin
//
//	@doc:
//		Return a project over a left anti semi join that appends nulls for all
//		columns in the original inner child.
//
//---------------------------------------------------------------------------
CExpression *
CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::PexprProjectOverLeftAntiSemiJoin
	(
	IMemoryPool *memory_pool,
	ColRefArray *pdrgpcrOuter,
	CColRefSet *pcrsScalar,
	CColRefSet *pcrsInner,
	ColRefArray *pdrgpcrJoinOutput,
	ULONG ulCTEJoinId,
	ULONG ulCTEOuterId,
	ColRefArray **ppdrgpcrProjectOutput
	)
{
	GPOS_ASSERT(NULL != pdrgpcrOuter);
	GPOS_ASSERT(NULL != pcrsScalar);
	GPOS_ASSERT(NULL != pcrsInner);
	GPOS_ASSERT(NULL != pdrgpcrJoinOutput);

	// make a copy of outer for the second CTE consumer (outer of LASJ)
	ColRefArray *pdrgpcrOuterCopy = CUtils::PdrgpcrCopy(memory_pool, pdrgpcrOuter);

	CExpression *pexprLeftAntiSemi = PexprLeftAntiSemiJoinWithInnerGroupBy
									(
									memory_pool,
									pdrgpcrOuter,
									pdrgpcrOuterCopy,
									pcrsScalar,
									pcrsInner,
									pdrgpcrJoinOutput,
									ulCTEJoinId,
									ulCTEOuterId
									);

	ULongPtrArray *pdrgpulIndexesOfOuter = pdrgpcrJoinOutput->IndexesOfSubsequence(pdrgpcrOuter);
	GPOS_ASSERT(NULL != pdrgpulIndexesOfOuter);

	UlongColRefHashMap *colref_mapping = GPOS_NEW(memory_pool) UlongColRefHashMap(memory_pool);
	const ULONG ulOuterCopyLength = pdrgpcrOuterCopy->Size();

	for (ULONG ul = 0; ul < ulOuterCopyLength; ++ul)
	{
		ULONG ulOrigIndex = *(*pdrgpulIndexesOfOuter)[ul];
		CColRef *pcrOriginal = (*pdrgpcrJoinOutput)[ulOrigIndex];
#ifdef GPOS_DEBUG
		BOOL fInserted =
#endif
		colref_mapping->Insert(GPOS_NEW(memory_pool) ULONG(pcrOriginal->Id()), (*pdrgpcrOuterCopy)[ul]);
		GPOS_ASSERT(fInserted);
	}

	ColRefArray *pdrgpcrInner = pcrsInner->Pdrgpcr(memory_pool);
	CExpression *pexprProject =
			CUtils::PexprLogicalProjectNulls(memory_pool, pdrgpcrInner, pexprLeftAntiSemi, colref_mapping);

	// compute the output array in the order needed by the union-all above the projection
	*ppdrgpcrProjectOutput =
			CUtils::PdrgpcrRemap(memory_pool, pdrgpcrJoinOutput, colref_mapping, true /*must_exist*/);

	pdrgpcrInner->Release();
	colref_mapping->Release();
	pdrgpulIndexesOfOuter->Release();

	return pexprProject;
}

BOOL
CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin::IsApplyOnce()
{
	return true;
}
// EOF
