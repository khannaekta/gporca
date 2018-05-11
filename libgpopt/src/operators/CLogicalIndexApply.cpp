//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//
//	Implementation of inner / left outer index apply operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/operators/CLogicalIndexApply.h"
#include "naucrates/statistics/CJoinStatsProcessor.h"

using namespace gpopt;

CLogicalIndexApply::CLogicalIndexApply
	(
	IMemoryPool *memory_pool
	)
	:
	CLogicalApply(memory_pool),
	m_pdrgpcrOuterRefs(NULL),
	m_fOuterJoin(false)
{
	m_fPattern = true;
}

CLogicalIndexApply::CLogicalIndexApply
	(
	IMemoryPool *memory_pool,
	DrgPcr *pdrgpcrOuterRefs,
	BOOL fOuterJoin
	)
	:
	CLogicalApply(memory_pool),
	m_pdrgpcrOuterRefs(pdrgpcrOuterRefs),
	m_fOuterJoin(fOuterJoin)
{
	GPOS_ASSERT(NULL != pdrgpcrOuterRefs);
}


CLogicalIndexApply::~CLogicalIndexApply()
{
	CRefCount::SafeRelease(m_pdrgpcrOuterRefs);
}


CMaxCard
CLogicalIndexApply::Maxcard
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	return CLogical::Maxcard(exprhdl, 2 /*ulScalarIndex*/, MaxcardDef(exprhdl));
}


CXformSet *
CLogicalIndexApply::PxfsCandidates
	(
	IMemoryPool *memory_pool
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfImplementIndexApply);
	return xform_set;
}

BOOL
CLogicalIndexApply::Matches
	(
	COperator *pop
	)
	const
{
	GPOS_ASSERT(NULL != pop);

	if (pop->Eopid() == Eopid())
	{
		return m_pdrgpcrOuterRefs->Equals(CLogicalIndexApply::PopConvert(pop)->PdrgPcrOuterRefs());
	}

	return false;
}


IStatistics *
CLogicalIndexApply::PstatsDerive
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	StatsArray* // stats_ctxt
	)
	const
{
	GPOS_ASSERT(EspNone < Esp(exprhdl));

	IStatistics *outer_stats = exprhdl.Pstats(0);
	IStatistics *inner_side_stats = exprhdl.Pstats(1);
	CExpression *pexprScalar = exprhdl.PexprScalarChild(2 /*child_index*/);

	// join stats of the children
	StatsArray *statistics_array = GPOS_NEW(memory_pool) StatsArray(memory_pool);
	outer_stats->AddRef();
	statistics_array->Append(outer_stats);
	inner_side_stats->AddRef();
	statistics_array->Append(inner_side_stats);
	IStatistics::EStatsJoinType join_type = IStatistics::EsjtInnerJoin;
	// we use Inner Join semantics here except in the case of Left Outer Join
	if (m_fOuterJoin)
	{
		join_type = IStatistics::EsjtLeftOuterJoin;
	}
	IStatistics *stats = CJoinStatsProcessor::CalcAllJoinStats(memory_pool, statistics_array, pexprScalar, join_type);
	statistics_array->Release();

	return stats;
}

// return a copy of the operator with remapped columns
COperator *
CLogicalIndexApply::PopCopyWithRemappedColumns
(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	DrgPcr *colref_array = CUtils::PdrgpcrRemap(memory_pool, m_pdrgpcrOuterRefs, colref_mapping, must_exist);

	return GPOS_NEW(memory_pool) CLogicalIndexApply(memory_pool, colref_array, m_fOuterJoin);
}

// EOF

