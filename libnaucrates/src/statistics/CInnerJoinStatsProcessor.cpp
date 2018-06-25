//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2018 Pivotal, Inc.
//
//	@filename:
//		CInnerJoinStatsProcessor.cpp
//
//	@doc:
//		Statistics helper routines for processing Inner Joins
//---------------------------------------------------------------------------

#include "gpopt/operators/ops.h"
#include "naucrates/statistics/CInnerJoinStatsProcessor.h"

using namespace gpmd;

// return statistics object after performing inner join
CStatistics *
CInnerJoinStatsProcessor::CalcInnerJoinStatsStatic(IMemoryPool *memory_pool,
												   const IStatistics *outer_stats_input,
												   const IStatistics *inner_stats_input,
												   StatsPredJoinArray *join_preds_stats)
{
	GPOS_ASSERT(NULL != outer_stats_input);
	GPOS_ASSERT(NULL != inner_stats_input);
	GPOS_ASSERT(NULL != join_preds_stats);
	const CStatistics *outer_stats = dynamic_cast<const CStatistics *>(outer_stats_input);

	return CJoinStatsProcessor::SetResultingJoinStats(memory_pool,
													  outer_stats->GetStatsConfig(),
													  outer_stats_input,
													  inner_stats_input,
													  join_preds_stats,
													  IStatistics::EsjtInnerJoin,
													  true /* DoIgnoreLASJHistComputation */
	);
}

// EOF
