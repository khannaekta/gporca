//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformUnnestTVF.cpp
//
//	@doc:
//		Implementation of TVF unnesting xform
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/common/CHashMap.h"
#include "gpopt/xforms/CXformUnnestTVF.h"
#include "gpopt/xforms/CXformUtils.h"

#include "gpopt/operators/ops.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CXformUnnestTVF::CXformUnnestTVF
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformUnnestTVF::CXformUnnestTVF
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
				GPOS_NEW(memory_pool) CLogicalTVF(memory_pool),
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CPatternMultiTree(memory_pool)) // variable number of args, each is a deep tree
				)
		)
{}


//---------------------------------------------------------------------------
//	@function:
//		CXformUnnestTVF::Exfp
//
//	@doc:
//		Compute xform promise for a given expression handle
//
//---------------------------------------------------------------------------
CXform::EXformPromise
CXformUnnestTVF::Exfp
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
			// xform is applicable if TVF argument is a subquery
			return CXform::ExfpHigh;
		}
	}

	return CXform::ExfpNone;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUnnestTVF::PdrgpcrSubqueries
//
//	@doc:
//		Return array of subquery column references in CTE consumer output
//		after mapping to consumer output
//
//---------------------------------------------------------------------------
ColRefArray *
CXformUnnestTVF::PdrgpcrSubqueries
	(
	IMemoryPool *memory_pool,
	CExpression *pexprCTEProd,
	CExpression *pexprCTECons
	)
{
	CExpression *pexprProject = (*pexprCTEProd)[0];
	GPOS_ASSERT(COperator::EopLogicalProject == pexprProject->Pop()->Eopid());

	ColRefArray *pdrgpcrProdOutput = CDrvdPropRelational::GetRelationalProperties(pexprCTEProd->PdpDerive())->PcrsOutput()->Pdrgpcr(memory_pool);
	ColRefArray *pdrgpcrConsOutput = CDrvdPropRelational::GetRelationalProperties(pexprCTECons->PdpDerive())->PcrsOutput()->Pdrgpcr(memory_pool);
	GPOS_ASSERT(pdrgpcrProdOutput->Size() == pdrgpcrConsOutput->Size());

	ColRefArray *colref_array = GPOS_NEW(memory_pool) ColRefArray(memory_pool);
	const ULONG ulPrjElems = (*pexprProject)[1]->Arity();
	for (ULONG ulOuter = 0; ulOuter < ulPrjElems; ulOuter++)
	{
		CExpression *pexprPrjElem = (*(*pexprProject)[1])[ulOuter];
		if (CDrvdPropScalar::GetDrvdScalarProps((*pexprPrjElem)[0]->PdpDerive())->FHasSubquery())
		{
			CColRef *pcrProducer = CScalarProjectElement::PopConvert(pexprPrjElem->Pop())->Pcr();
			CColRef *pcrConsumer =  CUtils::PcrMap(pcrProducer, pdrgpcrProdOutput, pdrgpcrConsOutput);
			GPOS_ASSERT(NULL != pcrConsumer);

			colref_array->Append(pcrConsumer);
		}
	}

	pdrgpcrProdOutput->Release();
	pdrgpcrConsOutput->Release();

	return colref_array;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUnnestTVF::PexprProjectSubqueries
//
//	@doc:
//		Collect subquery arguments and return a Project expression with
//		collected subqueries in project list
//
//---------------------------------------------------------------------------
CExpression *
CXformUnnestTVF::PexprProjectSubqueries
	(
	IMemoryPool *memory_pool,
	CExpression *pexprTVF
	)
{
	GPOS_ASSERT(COperator::EopLogicalTVF == pexprTVF->Pop()->Eopid());

	// collect subquery arguments
	ExpressionArray *pdrgpexprSubqueries = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	const ULONG arity = pexprTVF->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprScalarChild = (*pexprTVF)[ul];
		if (CDrvdPropScalar::GetDrvdScalarProps(pexprScalarChild->PdpDerive())->FHasSubquery())
		{
			pexprScalarChild->AddRef();
			pdrgpexprSubqueries->Append(pexprScalarChild);
		}
	}
	GPOS_ASSERT(0 < pdrgpexprSubqueries->Size());

	CExpression *pexprCTG = CUtils::PexprLogicalCTGDummy(memory_pool);
	CExpression *pexprProject = CUtils::PexprAddProjection(memory_pool, pexprCTG, pdrgpexprSubqueries);
	pdrgpexprSubqueries->Release();

	return pexprProject;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformUnnestTVF::Transform
//
//	@doc:
//		Actual transformation
//		for queries of the form 'SELECT * FROM func(1, (select 5))'
//		we generate a correlated CTE expression to execute subquery args
//		in different subplans, the resulting expression looks like this
//
//		+--CLogicalCTEAnchor (0)
//		   +--CLogicalLeftOuterCorrelatedApply
//			  |--CLogicalTVF (func) Columns: ["a" (0), "b" (1)]
//			  |  |--CScalarConst (1)     <-- constant arg
//			  |  +--CScalarIdent "ColRef_0005" (11)    <-- subquery arg replaced by column
//			  |--CLogicalCTEConsumer (0), Columns: ["ColRef_0005" (11)]
//			  +--CScalarConst (1)
//
//		where CTE(0) is a Project expression on subquery args
//
//
//---------------------------------------------------------------------------
void
CXformUnnestTVF::Transform
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

	// create a project expression on subquery arguments
	CExpression *pexprProject = PexprProjectSubqueries(memory_pool, pexpr);

	// create a CTE producer on top of the project
	CCTEInfo *pcteinfo = COptCtxt::PoctxtFromTLS()->Pcteinfo();
	const ULONG ulCTEId = pcteinfo->next_id();

	// construct CTE producer output from subquery columns
	ColRefArray *pdrgpcrOutput = GPOS_NEW(memory_pool) ColRefArray(memory_pool);
	const ULONG ulPrjElems = (*pexprProject)[1]->Arity();
	for (ULONG ulOuter = 0; ulOuter < ulPrjElems; ulOuter++)
	{
		CExpression *pexprPrjElem = (*(*pexprProject)[1])[ulOuter];
		if (CDrvdPropScalar::GetDrvdScalarProps((*pexprPrjElem)[0]->PdpDerive())->FHasSubquery())
		{
			CColRef *pcrSubq = CScalarProjectElement::PopConvert(pexprPrjElem->Pop())->Pcr();
			pdrgpcrOutput->Append(pcrSubq);
		}
	}

	CExpression *pexprCTEProd = CXformUtils::PexprAddCTEProducer(memory_pool, ulCTEId, pdrgpcrOutput, pexprProject);
	pdrgpcrOutput->Release();
	pexprProject->Release();

	// create CTE consumer
	ColRefArray *pdrgpcrProducerOutput = CDrvdPropRelational::GetRelationalProperties(pexprCTEProd->PdpDerive())->PcrsOutput()->Pdrgpcr(memory_pool);
	ColRefArray *pdrgpcrConsumerOutput = CUtils::PdrgpcrCopy(memory_pool, pdrgpcrProducerOutput);
	CLogicalCTEConsumer *popConsumer = GPOS_NEW(memory_pool) CLogicalCTEConsumer(memory_pool, ulCTEId, pdrgpcrConsumerOutput);
	CExpression *pexprCTECons = GPOS_NEW(memory_pool) CExpression(memory_pool, popConsumer);
	pcteinfo->IncrementConsumers(ulCTEId);
	pdrgpcrProducerOutput->Release();

	// find columns corresponding to subqueries in consumer's output
	ColRefArray *pdrgpcrSubqueries = PdrgpcrSubqueries(memory_pool, pexprCTEProd, pexprCTECons);

	// create new function arguments by replacing subqueries with columns in CTE consumer output
	ExpressionArray *pdrgpexprNewArgs = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	ULONG ulIndex = 0;
	const ULONG arity = pexpr->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprScalarChild = (*pexpr)[ul];
		if (CDrvdPropScalar::GetDrvdScalarProps(pexprScalarChild->PdpDerive())->FHasSubquery())
		{
			CColRef *colref =(*pdrgpcrSubqueries)[ulIndex];
			pdrgpexprNewArgs->Append(CUtils::PexprScalarIdent(memory_pool, colref));
			ulIndex++;
		}
		else
		{
			(*pexpr)[ul]->AddRef();
			pdrgpexprNewArgs->Append((*pexpr)[ul]);
		}
	}

	// finally, create correlated apply expression
	CLogicalTVF *popTVF = CLogicalTVF::PopConvert(pexpr->Pop());
	popTVF->AddRef();
	CExpression *pexprCorrApply =
		CUtils::PexprLogicalApply<CLogicalLeftOuterCorrelatedApply>
				(
				memory_pool,
				GPOS_NEW(memory_pool) CExpression(memory_pool, popTVF, pdrgpexprNewArgs),
				pexprCTECons,
				pdrgpcrSubqueries,
				COperator::EopScalarSubquery,
				CPredicateUtils::PexprConjunction(memory_pool, NULL /*pdrgpexpr*/)	// scalar expression is const True
				);

	CExpression *pexprAlt =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CLogicalCTEAnchor(memory_pool, ulCTEId),
			pexprCorrApply
			);

	// add alternative to transformation result
	pxfres->Add(pexprAlt);
}


// EOF

