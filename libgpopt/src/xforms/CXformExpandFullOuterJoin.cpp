//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformExpandFullOuterJoin.cpp
//
//	@doc:
//		Implementation of transform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/xforms/CXformExpandFullOuterJoin.h"
#include "gpopt/xforms/CXformUtils.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformExpandFullOuterJoin::CXformExpandFullOuterJoin
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformExpandFullOuterJoin::CXformExpandFullOuterJoin
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
				GPOS_NEW(memory_pool) CLogicalFullOuterJoin(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool)), // outer child
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool)), // inner child
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternTree(memory_pool)) // scalar child
				)
		)
{}

//---------------------------------------------------------------------------
//	@function:
//		CXformExpandFullOuterJoin::Exfp
//
//	@doc:
//		Compute promise of xform
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformExpandFullOuterJoin::Exfp
	(
	CExpressionHandle & //exprhdl
	)
	const
{
	return CXform::ExfpHigh;
}

//---------------------------------------------------------------------------
//	@function:
//		CXformExpandFullOuterJoin::Transform
//
//	@doc:
//		Actual transformation
// 		The expression A FOJ B is translated to:
//
//		CTEAnchor(cteA)
//		+-- CTEAnchor(cteB)
//			+--UnionAll
//				|--	LOJ
//				|	|--	CTEConsumer(cteA)
//				|	+--	CTEConsumer(cteB)
//				+--	Project
//					+--	LASJ
//					|	|--	CTEConsumer(cteB)
//					|	+--	CTEConsumer(cteA)
//					+-- (NULLS - same schema of A)
//
//		Also, two CTE producers for cteA and cteB are added to CTE info
//
//---------------------------------------------------------------------------
void
CXformExpandFullOuterJoin::Transform
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

	CExpression *pexprA = (*pexpr)[0];
	CExpression *pexprB = (*pexpr)[1];
	CExpression *pexprScalar = (*pexpr)[2];

	// 1. create the CTE producers
	const ULONG ulCTEIdA = COptCtxt::PoctxtFromTLS()->Pcteinfo()->next_id();
	ColRefArray *pdrgpcrOutA = CDrvdPropRelational::GetRelationalProperties(pexprA->PdpDerive())->PcrsOutput()->Pdrgpcr(memory_pool);
	(void) CXformUtils::PexprAddCTEProducer(memory_pool, ulCTEIdA, pdrgpcrOutA, pexprA);

	const ULONG ulCTEIdB = COptCtxt::PoctxtFromTLS()->Pcteinfo()->next_id();
	ColRefArray *pdrgpcrOutB = CDrvdPropRelational::GetRelationalProperties(pexprB->PdpDerive())->PcrsOutput()->Pdrgpcr(memory_pool);
	(void) CXformUtils::PexprAddCTEProducer(memory_pool, ulCTEIdB, pdrgpcrOutB, pexprB);

	// 2. create the right child (PROJECT over LASJ)
	ColRefArray *pdrgpcrRightA = CUtils::PdrgpcrCopy(memory_pool, pdrgpcrOutA);
	ColRefArray *pdrgpcrRightB = CUtils::PdrgpcrCopy(memory_pool, pdrgpcrOutB);
	CExpression *pexprScalarRight = CXformUtils::PexprRemapColumns
									(
									memory_pool,
									pexprScalar,
									pdrgpcrOutA,
									pdrgpcrRightA,
									pdrgpcrOutB,
									pdrgpcrRightB
									);
	CExpression *pexprLASJ = PexprLogicalJoinOverCTEs(memory_pool, EdxljtLeftAntiSemijoin, ulCTEIdB, pdrgpcrRightB, ulCTEIdA, pdrgpcrRightA, pexprScalarRight);
	CExpression *pexprProject = CUtils::PexprLogicalProjectNulls(memory_pool, pdrgpcrRightA, pexprLASJ);

	// 3. create the left child (LOJ) - this has to use the original output
	//    columns and the original scalar expression
	pexprScalar->AddRef();
	CExpression *pexprLOJ = PexprLogicalJoinOverCTEs(memory_pool, EdxljtLeft, ulCTEIdA, pdrgpcrOutA, ulCTEIdB, pdrgpcrOutB, pexprScalar);

	// 4. create the UNION ALL expression

	// output columns of the union are the same as the outputs of the first child (LOJ)
	ColRefArray *pdrgpcrOutput = GPOS_NEW(memory_pool) ColRefArray(memory_pool);
	pdrgpcrOutput->AppendArray(pdrgpcrOutA);
	pdrgpcrOutput->AppendArray(pdrgpcrOutB);

	// input columns of the union
	ColRefArrays *pdrgdrgpcrInput = GPOS_NEW(memory_pool) ColRefArrays(memory_pool);

	// inputs from the first child (LOJ)
	pdrgpcrOutput->AddRef();
	pdrgdrgpcrInput->Append(pdrgpcrOutput);

	// inputs from the second child have to be in the correct order
	// a. add new computed columns from the project only
	CDrvdPropRelational *pdprelProject = CDrvdPropRelational::GetRelationalProperties(pexprProject->PdpDerive());
	CColRefSet *pcrsProjOnly = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrsProjOnly->Include(pdprelProject->PcrsOutput());
	pcrsProjOnly->Exclude(pdrgpcrRightB);
	ColRefArray *pdrgpcrProj = pcrsProjOnly->Pdrgpcr(memory_pool);
	pcrsProjOnly->Release();
	// b. add columns from the LASJ expression
	pdrgpcrProj->AppendArray(pdrgpcrRightB);

	pdrgdrgpcrInput->Append(pdrgpcrProj);

	CExpression *pexprUnionAll = GPOS_NEW(memory_pool) CExpression
											(
											memory_pool,
											GPOS_NEW(memory_pool) CLogicalUnionAll(memory_pool, pdrgpcrOutput, pdrgdrgpcrInput),
											pexprLOJ,
											pexprProject
											);

	// 5. Add CTE anchor for the B subtree
	CExpression *pexprAnchorB = GPOS_NEW(memory_pool) CExpression
											(
											memory_pool,
											GPOS_NEW(memory_pool) CLogicalCTEAnchor(memory_pool, ulCTEIdB),
											pexprUnionAll
											);

	// 6. Add CTE anchor for the A subtree
	CExpression *pexprAnchorA = GPOS_NEW(memory_pool) CExpression
											(
											memory_pool,
											GPOS_NEW(memory_pool) CLogicalCTEAnchor(memory_pool, ulCTEIdA),
											pexprAnchorB
											);

	// add alternative to xform result
	pxfres->Add(pexprAnchorA);
}

//---------------------------------------------------------------------------
//	@function:
//		CXformExpandFullOuterJoin::PexprLogicalJoinOverCTEs
//
//	@doc:
//		Construct a join expression of two CTEs using the given CTE ids
// 		and output columns
//
//---------------------------------------------------------------------------
CExpression *
CXformExpandFullOuterJoin::PexprLogicalJoinOverCTEs
	(
	IMemoryPool *memory_pool,
	EdxlJoinType edxljointype,
	ULONG ulLeftCTEId,
	ColRefArray *pdrgpcrLeft,
	ULONG ulRightCTEId,
	ColRefArray *pdrgpcrRight,
	CExpression *pexprScalar
	)
	const
{
	GPOS_ASSERT(NULL != pexprScalar);

	ExpressionArray *pdrgpexprChildren = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	CCTEInfo *pcteinfo = COptCtxt::PoctxtFromTLS()->Pcteinfo();

	CLogicalCTEConsumer *popConsumerLeft = GPOS_NEW(memory_pool) CLogicalCTEConsumer(memory_pool, ulLeftCTEId, pdrgpcrLeft);
	CExpression *pexprLeft = GPOS_NEW(memory_pool) CExpression(memory_pool, popConsumerLeft);
	pcteinfo->IncrementConsumers(ulLeftCTEId);

	CLogicalCTEConsumer *popConsumerRight = GPOS_NEW(memory_pool) CLogicalCTEConsumer(memory_pool, ulRightCTEId, pdrgpcrRight);
	CExpression *pexprRight = GPOS_NEW(memory_pool) CExpression(memory_pool, popConsumerRight);
	pcteinfo->IncrementConsumers(ulRightCTEId);

	pdrgpexprChildren->Append(pexprLeft);
	pdrgpexprChildren->Append(pexprRight);
	pdrgpexprChildren->Append(pexprScalar);

	return CUtils::PexprLogicalJoin(memory_pool, edxljointype, pdrgpexprChildren);
}

// EOF
