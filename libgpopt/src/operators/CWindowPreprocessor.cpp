//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2015 Pivotal Inc.
//
//	@filename:
//		CWindowPreprocessor.cpp
//
//	@doc:
//		Preprocessing routines of window functions
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CPredicateUtils.h"
#include "gpopt/operators/ops.h"
#include "gpopt/operators/CWindowPreprocessor.h"
#include "gpopt/xforms/CXformUtils.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CWindowPreprocessor::SplitPrjList
//
//	@doc:
//		Iterate over project elements and split them elements between
//		Distinct Aggs list, and Others list
//
//---------------------------------------------------------------------------
void
CWindowPreprocessor::SplitPrjList
	(
	IMemoryPool *memory_pool,
	CExpression *pexprSeqPrj,
	ExpressionArray **ppdrgpexprDistinctAggsPrEl, // output: list of project elements with Distinct Aggs
	ExpressionArray **ppdrgpexprOtherPrEl, // output: list of project elements with Other window functions
	OrderSpecArray **ppdrgposOther, // output: array of order specs of window functions used in Others list
	WindowFrameArray **ppdrgpwfOther // output: array of frame specs of window functions used in Others list
	)
{
	GPOS_ASSERT(NULL != pexprSeqPrj);
	GPOS_ASSERT(NULL != ppdrgpexprDistinctAggsPrEl);
	GPOS_ASSERT(NULL != ppdrgpexprOtherPrEl);
	GPOS_ASSERT(NULL != ppdrgposOther);
	GPOS_ASSERT(NULL != ppdrgpwfOther);

	CLogicalSequenceProject *popSeqPrj = CLogicalSequenceProject::PopConvert(pexprSeqPrj->Pop());
	CExpression *pexprPrjList = (*pexprSeqPrj)[1];

	OrderSpecArray *pdrgpos = popSeqPrj->Pdrgpos();
	BOOL fHasOrderSpecs = popSeqPrj->FHasOrderSpecs();

	WindowFrameArray *pdrgpwf = popSeqPrj->Pdrgpwf();
	BOOL fHasFrameSpecs = popSeqPrj->FHasFrameSpecs();

	ExpressionArray *pdrgpexprDistinctAggsPrEl = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);

	ExpressionArray *pdrgpexprOtherPrEl = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	OrderSpecArray *pdrgposOther = GPOS_NEW(memory_pool) OrderSpecArray(memory_pool);
	WindowFrameArray *pdrgpwfOther = GPOS_NEW(memory_pool) WindowFrameArray(memory_pool);

	// iterate over project list and split project elements between
	// Distinct Aggs list, and Others list
	const ULONG arity = pexprPrjList->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprPrjEl = (*pexprPrjList)[ul];
		CExpression *pexprWinFunc = (*pexprPrjEl)[0];
		CScalarWindowFunc *popScWinFunc = CScalarWindowFunc::PopConvert(pexprWinFunc->Pop());
		CScalarProjectElement *popScPrjElem = CScalarProjectElement::PopConvert(pexprPrjEl->Pop());
		CColRef *pcrPrjElem = popScPrjElem->Pcr();

		if (popScWinFunc->IsDistinct() && popScWinFunc->FAgg())
		{
			CExpression *pexprAgg = CXformUtils::PexprWinFuncAgg2ScalarAgg(memory_pool, pexprWinFunc);
			CExpression *pexprNewPrjElem = CUtils::PexprScalarProjectElement(memory_pool, pcrPrjElem, pexprAgg);
			pdrgpexprDistinctAggsPrEl->Append(pexprNewPrjElem);
		}
		else
		{
			if (fHasOrderSpecs)
			{
				(*pdrgpos)[ul]->AddRef();
				pdrgposOther->Append((*pdrgpos)[ul]);
			}

			if (fHasFrameSpecs)
			{
				(*pdrgpwf)[ul]->AddRef();
				pdrgpwfOther->Append((*pdrgpwf)[ul]);
			}

			pexprWinFunc->AddRef();
			CExpression *pexprNewPrjElem = CUtils::PexprScalarProjectElement(memory_pool, pcrPrjElem, pexprWinFunc);
			pdrgpexprOtherPrEl->Append(pexprNewPrjElem);
		}
	}

	*ppdrgpexprDistinctAggsPrEl = pdrgpexprDistinctAggsPrEl;
	*ppdrgpexprOtherPrEl = pdrgpexprOtherPrEl;
	*ppdrgposOther = pdrgposOther;
	*ppdrgpwfOther = pdrgpwfOther;
}


//---------------------------------------------------------------------------
//	@function:
//		CWindowPreprocessor::SplitSeqPrj
//
//	@doc:
//		Split SeqPrj expression into:
//		- A GbAgg expression containing distinct Aggs, and
//		- A SeqPrj expression containing all remaining window functions
//
//---------------------------------------------------------------------------
void
CWindowPreprocessor::SplitSeqPrj
	(
	IMemoryPool *memory_pool,
	CExpression *pexprSeqPrj,
	CExpression **ppexprGbAgg,	// output: GbAgg expression containing distinct Aggs
	CExpression **ppexprOutputSeqPrj // output: SeqPrj expression containing all remaining window functions
	)
{
	GPOS_ASSERT(NULL != pexprSeqPrj);
	GPOS_ASSERT(NULL != ppexprGbAgg);
	GPOS_ASSERT(NULL != ppexprOutputSeqPrj);

	// split project elements between Distinct Aggs list, and Others list
	ExpressionArray *pdrgpexprDistinctAggsPrEl = NULL;
	ExpressionArray *pdrgpexprOtherPrEl = NULL;
	OrderSpecArray *pdrgposOther = NULL;
	WindowFrameArray *pdrgpwfOther = NULL;
	SplitPrjList(memory_pool, pexprSeqPrj, &pdrgpexprDistinctAggsPrEl, &pdrgpexprOtherPrEl, &pdrgposOther, &pdrgpwfOther);

	// check distribution spec of original SeqPrj and extract grouping columns
	// from window (PARTITION BY) clause
	CLogicalSequenceProject *popSeqPrj = CLogicalSequenceProject::PopConvert(pexprSeqPrj->Pop());
	CDistributionSpec *pds = popSeqPrj->Pds();
	ColRefArray *pdrgpcrGrpCols = NULL;
	if (CDistributionSpec::EdtHashed == pds->Edt())
	{
		CColRefSet *pcrs = CUtils::PcrsExtractColumns(memory_pool, CDistributionSpecHashed::PdsConvert(pds)->Pdrgpexpr());
		pdrgpcrGrpCols = pcrs->Pdrgpcr(memory_pool);
		pcrs->Release();
	}
	else
	{
		// no (PARTITION BY) clause
		pdrgpcrGrpCols = GPOS_NEW(memory_pool) ColRefArray(memory_pool);
	}

	CExpression *pexprSeqPrjChild = (*pexprSeqPrj)[0];
	pexprSeqPrjChild->AddRef();
	*ppexprGbAgg =
				GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalGbAgg(memory_pool, pdrgpcrGrpCols, COperator::EgbaggtypeGlobal),
					pexprSeqPrjChild,
					GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool), pdrgpexprDistinctAggsPrEl)
					);

	pexprSeqPrjChild->AddRef();
	if (0 == pdrgpexprOtherPrEl->Size())
	{
		// no remaining window functions after excluding distinct aggs,
		// reuse the original SeqPrj child in this case
		pdrgpexprOtherPrEl->Release();
		pdrgposOther->Release();
		pdrgpwfOther->Release();
		*ppexprOutputSeqPrj = pexprSeqPrjChild;

		return;
	}

	// create a new SeqPrj expression for remaining window functions
	pds->AddRef();
	*ppexprOutputSeqPrj =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CLogicalSequenceProject(memory_pool, pds, pdrgposOther, pdrgpwfOther),
			pexprSeqPrjChild,
			GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool), pdrgpexprOtherPrEl)
			);
}


//---------------------------------------------------------------------------
//	@function:
//		CWindowPreprocessor::CreateCTE
//
//	@doc:
//		Create a CTE with two consumers using the child expression of
//		Sequence Project
//
//---------------------------------------------------------------------------
void
CWindowPreprocessor::CreateCTE
	(
	IMemoryPool *memory_pool,
	CExpression *pexprSeqPrj,
	CExpression **ppexprFirstConsumer,
	CExpression **ppexprSecondConsumer
	)
{
	GPOS_ASSERT(NULL != pexprSeqPrj);
	GPOS_ASSERT(COperator::EopLogicalSequenceProject == pexprSeqPrj->Pop()->Eopid());
	GPOS_ASSERT(NULL != ppexprFirstConsumer);
	GPOS_ASSERT(NULL != ppexprSecondConsumer);

	CExpression *pexprChild = (*pexprSeqPrj)[0];
	CColRefSet *pcrsChildOutput = CDrvdPropRelational::GetRelationalProperties(pexprChild->PdpDerive())->PcrsOutput();
	ColRefArray *pdrgpcrChildOutput = pcrsChildOutput->Pdrgpcr(memory_pool);

	// create a CTE producer based on SeqPrj child expression
	CCTEInfo *pcteinfo = COptCtxt::PoctxtFromTLS()->Pcteinfo();
	const ULONG ulCTEId = pcteinfo->next_id();
	CExpression *pexprCTEProd = CXformUtils::PexprAddCTEProducer(memory_pool, ulCTEId, pdrgpcrChildOutput, pexprChild);
	ColRefArray *pdrgpcrProducerOutput = CDrvdPropRelational::GetRelationalProperties(pexprCTEProd->PdpDerive())->PcrsOutput()->Pdrgpcr(memory_pool);

	// first consumer creates new output columns to be used later as input to GbAgg expression
	*ppexprFirstConsumer =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CLogicalCTEConsumer(memory_pool, ulCTEId, CUtils::PdrgpcrCopy(memory_pool, pdrgpcrProducerOutput))
			);
	pcteinfo->IncrementConsumers(ulCTEId);
	pdrgpcrProducerOutput->Release();

	// second consumer reuses the same output columns of SeqPrj child to be able to provide any requested columns upstream
	*ppexprSecondConsumer =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CLogicalCTEConsumer(memory_pool, ulCTEId, pdrgpcrChildOutput)
			);
	pcteinfo->IncrementConsumers(ulCTEId);
}


//---------------------------------------------------------------------------
//	@function:
//		CWindowPreprocessor::PdrgpcrGrpCols
//
//	@doc:
//		Extract grouping columns from given expression,
//		we expect expression to be either a GbAgg expression or a join
//		whose inner child is a GbAgg expression
//
//---------------------------------------------------------------------------
ColRefArray *
CWindowPreprocessor::PdrgpcrGrpCols
	(
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);

	COperator *pop = pexpr->Pop();

	if (COperator::EopLogicalGbAgg == pop->Eopid())
	{
		// passed expression is a Group By, return grouping columns
		return CLogicalGbAgg::PopConvert(pop)->Pdrgpcr();
	}

	if (CUtils::FLogicalJoin(pop))
	{
		// pass expression is a join, we expect a Group By on the inner side
		COperator *popInner = (*pexpr)[1]->Pop();
		if (COperator::EopLogicalGbAgg == popInner->Eopid())
		{
			return CLogicalGbAgg::PopConvert(popInner)->Pdrgpcr();
		}
	}

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		CWindowPreprocessor::PexprSeqPrj2Join
//
//	@doc:
//		Transform sequence project expression with distinct aggregates
//		into an inner join expression,
//			- the outer child of the join is a GbAgg expression that computes
//			distinct aggs,
//			- the inner child of the join is a SeqPrj expression that computes
//			remaining window functions
//
//		we use a CTE to compute the input to both join children, while maintaining
//		all column references upstream by reusing the same computed columns in the
//		original SeqPrj expression
//
//---------------------------------------------------------------------------
CExpression *
CWindowPreprocessor::PexprSeqPrj2Join
	(
	IMemoryPool *memory_pool,
	CExpression *pexprSeqPrj
	)
{
	GPOS_ASSERT(NULL != pexprSeqPrj);
	GPOS_ASSERT(COperator::EopLogicalSequenceProject == pexprSeqPrj->Pop()->Eopid());
	GPOS_ASSERT(0 < CDrvdPropScalar::GetDrvdScalarProps((*pexprSeqPrj)[1]->PdpDerive())->UlDistinctAggs());

	// split SeqPrj expression into a GbAgg expression (for distinct Aggs), and
	// another SeqPrj expression (for remaining window functions)
	CExpression *pexprGbAgg = NULL;
	CExpression *pexprWindow = NULL;
	SplitSeqPrj(memory_pool, pexprSeqPrj, &pexprGbAgg, &pexprWindow);

	// create CTE using SeqPrj child expression
	CExpression *pexprGbAggConsumer = NULL;
	CExpression *pexprWindowConsumer = NULL;
	CreateCTE(memory_pool, pexprSeqPrj, &pexprGbAggConsumer, &pexprWindowConsumer);

	// extract output columns of SeqPrj child expression
	CExpression *pexprChild = (*pexprSeqPrj)[0];
	ColRefArray *pdrgpcrChildOutput = CDrvdPropRelational::GetRelationalProperties(pexprChild->PdpDerive())->PcrsOutput()->Pdrgpcr(memory_pool);

	// to match requested columns upstream, we have to re-use the same computed
	// columns that define the aggregates, we avoid recreating new columns during
	// expression copy by passing must_exist as false
	ColRefArray *pdrgpcrConsumerOutput = CLogicalCTEConsumer::PopConvert(pexprGbAggConsumer->Pop())->Pdrgpcr();
	UlongColRefHashMap *colref_mapping = CUtils::PhmulcrMapping(memory_pool, pdrgpcrChildOutput, pdrgpcrConsumerOutput);
	CExpression *pexprGbAggRemapped = pexprGbAgg->PexprCopyWithRemappedColumns(memory_pool, colref_mapping, false /*must_exist*/);
	colref_mapping->Release();
	pdrgpcrChildOutput->Release();
	pexprGbAgg->Release();

	// finalize GbAgg expression by replacing its child with CTE consumer
	pexprGbAggRemapped->Pop()->AddRef();
	(*pexprGbAggRemapped)[1]->AddRef();
	CExpression *pexprGbAggWithConsumer = GPOS_NEW(memory_pool) CExpression(memory_pool, pexprGbAggRemapped->Pop(), pexprGbAggConsumer, (*pexprGbAggRemapped)[1]);
	pexprGbAggRemapped->Release();

	// in case of multiple Distinct Aggs, we need to expand the GbAgg expression
	// into a join expression where leaves carry single Distinct Aggs
	CExpression *pexprJoinDQAs = CXformUtils::PexprGbAggOnCTEConsumer2Join(memory_pool, pexprGbAggWithConsumer);
	pexprGbAggWithConsumer->Release();

	CExpression *pexprWindowFinal = NULL;
	if (COperator::EopLogicalSequenceProject == pexprWindow->Pop()->Eopid())
	{
		// create a new SeqPrj expression for remaining window functions,
		// and replace expression child withCTE consumer
		pexprWindow->Pop()->AddRef();
		(*pexprWindow)[1]->AddRef();
		pexprWindowFinal = GPOS_NEW(memory_pool) CExpression(memory_pool, pexprWindow->Pop(), pexprWindowConsumer, (*pexprWindow)[1]);
	}
	else
	{
		// no remaining window functions, simply reuse created CTE consumer
		pexprWindowFinal = pexprWindowConsumer;
	}
	pexprWindow->Release();

	// extract grouping columns from created join expression
	ColRefArray *pdrgpcrGrpCols = PdrgpcrGrpCols(pexprJoinDQAs);

	// create final join condition
	CExpression *pexprJoinCondition = NULL;

	if (NULL != pdrgpcrGrpCols && 0 < pdrgpcrGrpCols->Size())
	{
		// extract PARTITION BY columns from original SeqPrj expression
		CLogicalSequenceProject *popSeqPrj = CLogicalSequenceProject::PopConvert(pexprSeqPrj->Pop());
		CDistributionSpec *pds = popSeqPrj->Pds();
		CColRefSet *pcrs = CUtils::PcrsExtractColumns(memory_pool, CDistributionSpecHashed::PdsConvert(pds)->Pdrgpexpr());
		ColRefArray *pdrgpcrPartitionBy = pcrs->Pdrgpcr(memory_pool);
		pcrs->Release();
		GPOS_ASSERT(pdrgpcrGrpCols->Size() == pdrgpcrPartitionBy->Size() &&
				"Partition By columns in window function are not the same as grouping columns in created Aggs");

		// create a conjunction of INDF expressions comparing a GROUP BY column to a PARTITION BY column
		pexprJoinCondition = CPredicateUtils::PexprINDFConjunction(memory_pool, pdrgpcrGrpCols, pdrgpcrPartitionBy);
		pdrgpcrPartitionBy->Release();
	}
	else
	{
		// no PARTITION BY, join condition is const True
		pexprJoinCondition = CUtils::PexprScalarConstBool(memory_pool, true /*m_bytearray_value*/);
	}

	// create a join between expanded DQAs and Window expressions
	CExpression *pexprJoin =
		CUtils::PexprLogicalJoin<CLogicalInnerJoin>(memory_pool, pexprJoinDQAs, pexprWindowFinal, pexprJoinCondition);

	ULONG ulCTEId = CLogicalCTEConsumer::PopConvert(pexprGbAggConsumer->Pop())->UlCTEId();
	return GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalCTEAnchor(memory_pool, ulCTEId),
				pexprJoin
				);
}


//---------------------------------------------------------------------------
//	@function:
//		CWindowPreprocessor::PexprPreprocess
//
//	@doc:
//		Main driver
//
//---------------------------------------------------------------------------
CExpression *
CWindowPreprocessor::PexprPreprocess
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr
	)
{
	// protect against stack overflow during recursion
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexpr);

	COperator *pop = pexpr->Pop();
	if (COperator::EopLogicalSequenceProject == pop->Eopid() &&
		0 < CDrvdPropScalar::GetDrvdScalarProps((*pexpr)[1]->PdpDerive())->UlDistinctAggs())
	{
		CExpression *pexprJoin = PexprSeqPrj2Join(memory_pool, pexpr);

		// recursively process the resulting expression
		CExpression *pexprResult = PexprPreprocess(memory_pool, pexprJoin);
		pexprJoin->Release();

		return pexprResult;
	}

	// recursively process child expressions
	const ULONG arity = pexpr->Arity();
	ExpressionArray *pdrgpexprChildren = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprChild = PexprPreprocess(memory_pool, (*pexpr)[ul]);
		pdrgpexprChildren->Append(pexprChild);
	}

	pop->AddRef();
	return GPOS_NEW(memory_pool) CExpression(memory_pool, pop, pdrgpexprChildren);
}

// EOF
