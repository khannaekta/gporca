//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalIntersect.cpp
//
//	@doc:
//		Implementation of Intersect operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/operators/CLogicalIntersect.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalIntersectAll.h"
#include "gpopt/operators/CLogicalGbAgg.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalIntersect::CLogicalIntersect
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalIntersect::CLogicalIntersect
	(
	IMemoryPool *memory_pool
	)
	:
	CLogicalSetOp(memory_pool)
{
	m_fPattern = true;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalIntersect::CLogicalIntersect
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalIntersect::CLogicalIntersect
	(
	IMemoryPool *memory_pool,
	ColRefArray *pdrgpcrOutput,
	ColRefArrays *pdrgpdrgpcrInput
	)
	:
	CLogicalSetOp(memory_pool, pdrgpcrOutput, pdrgpdrgpcrInput)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalIntersect::~CLogicalIntersect
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalIntersect::~CLogicalIntersect()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalIntersect::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalIntersect::Maxcard
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	// contradictions produce no rows
	if (CDrvdPropRelational::GetRelationalProperties(exprhdl.Pdp())->Ppc()->FContradiction())
	{
		return CMaxCard(0 /*ull*/);
	}

	CMaxCard maxcardL = exprhdl.GetRelationalProperties(0)->Maxcard();
	CMaxCard maxcardR = exprhdl.GetRelationalProperties(1)->Maxcard();

	if (maxcardL <= maxcardR)
	{
		return maxcardL;
	}

	return maxcardR;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalIntersect::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalIntersect::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	ColRefArray *pdrgpcrOutput = CUtils::PdrgpcrRemap(memory_pool, m_pdrgpcrOutput, colref_mapping, must_exist);
	ColRefArrays *pdrgpdrgpcrInput = CUtils::PdrgpdrgpcrRemap(memory_pool, m_pdrgpdrgpcrInput, colref_mapping, must_exist);

	return GPOS_NEW(memory_pool) CLogicalIntersect(memory_pool, pdrgpcrOutput, pdrgpdrgpcrInput);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalIntersect::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalIntersect::PxfsCandidates
	(
	IMemoryPool *memory_pool
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfIntersect2Join);
	return xform_set;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalIntersect::PstatsDerive
//
//	@doc:
//		Derive statistics
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalIntersect::PstatsDerive
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	StatsArray * // not used
	)
	const
{
	GPOS_ASSERT(Esp(exprhdl) > EspNone);

	// intersect is transformed into a group by over an intersect all
	// we follow the same route to compute statistics

	ColRefSetArray *output_colrefsets = GPOS_NEW(memory_pool) ColRefSetArray(memory_pool);
	const ULONG size = m_pdrgpdrgpcrInput->Size();
	for (ULONG ul = 0; ul < size; ul++)
	{
		CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool, (*m_pdrgpdrgpcrInput)[ul]);
		output_colrefsets->Append(pcrs);
	}

	IStatistics *pstatsIntersectAll =
			CLogicalIntersectAll::PstatsDerive(memory_pool, exprhdl, m_pdrgpdrgpcrInput, output_colrefsets);

	// computed columns
	ULongPtrArray *pdrgpulComputedCols = GPOS_NEW(memory_pool) ULongPtrArray(memory_pool);

	IStatistics *stats = CLogicalGbAgg::PstatsDerive
											(
											memory_pool,
											pstatsIntersectAll,
											(*m_pdrgpdrgpcrInput)[0], // we group by the columns of the first child
											pdrgpulComputedCols, // no computed columns for set ops
											NULL // no keys, use all grouping cols
											);
	// clean up
	pdrgpulComputedCols->Release();
	pstatsIntersectAll->Release();
	output_colrefsets->Release();

	return stats;
}

// EOF
