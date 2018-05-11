//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2018 Pivotal, Inc.
//
//	@filename:
//		CFilterStatsProcessor.cpp
//
//	@doc:
//		Statistics helper routines for processing limit operations
//---------------------------------------------------------------------------

#include "gpopt/operators/ops.h"
#include "gpopt/optimizer/COptimizerConfig.h"

#include "naucrates/statistics/CStatistics.h"
#include "naucrates/statistics/CFilterStatsProcessor.h"
#include "naucrates/statistics/CJoinStatsProcessor.h"
#include "naucrates/statistics/CStatisticsUtils.h"
#include "naucrates/statistics/CScaleFactorUtils.h"

using namespace gpopt;

// derive statistics for filter operation based on given scalar expression
IStatistics *
CFilterStatsProcessor::MakeStatsFilterForScalarExpr
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	IStatistics *child_stats,
	CExpression *local_scalar_expr, // filter expression on local columns only
	CExpression *outer_refs_scalar_expr, // filter expression involving outer references
	StatsArray *all_outer_stats
	)
{
	GPOS_ASSERT(NULL != child_stats);
	GPOS_ASSERT(NULL != local_scalar_expr);
	GPOS_ASSERT(NULL != outer_refs_scalar_expr);
	GPOS_ASSERT(NULL != all_outer_stats);

	CColRefSet *outer_refs = exprhdl.GetRelationalProperties()->PcrsOuter();

	// TODO  June 13 2014, we currently only cap ndvs when we have a filter
	// immediately on top of tables
	BOOL do_cap_NDVs = (1 == exprhdl.GetRelationalProperties()->JoinDepth());

	// extract local filter
	CStatsPred *pred_stats = CStatsPredUtils::ExtractPredStats(memory_pool, local_scalar_expr, outer_refs);

	// derive stats based on local filter
	IStatistics *result_stats = CFilterStatsProcessor::MakeStatsFilter(memory_pool, dynamic_cast<CStatistics *>(child_stats), pred_stats, do_cap_NDVs);
	pred_stats->Release();

	if (exprhdl.HasOuterRefs() && 0 < all_outer_stats->Size())
	{
		// derive stats based on outer references
		IStatistics *stats = CJoinStatsProcessor::DeriveStatsWithOuterRefs
													(
													memory_pool,
													exprhdl,
													outer_refs_scalar_expr,
													result_stats,
													all_outer_stats,
													IStatistics::EsjtInnerJoin
													);
		result_stats->Release();
		result_stats = stats;
	}

	return result_stats;
}

// create new structure from a list of statistics filters
CStatistics *
CFilterStatsProcessor::MakeStatsFilter
	(
	IMemoryPool *memory_pool,
	const CStatistics *input_stats,
	CStatsPred *base_pred_stats,
	BOOL do_cap_NDVs
	)
{
	GPOS_ASSERT(NULL != base_pred_stats);

	CDouble input_rows = std::max(CStatistics::MinRows.Get(), input_stats->Rows().Get());
	CDouble scale_factor(1.0);
	ULONG num_predicates = 1;
	CDouble rows_filter = CStatistics::MinRows;
	UlongHistogramHashMap *histograms_new = NULL;

	UlongHistogramHashMap *histograms_copy = input_stats->CopyHistograms(memory_pool);

	CStatisticsConfig *stats_config = input_stats->GetStatsConfig();
	if (input_stats->IsEmpty())
	{
		histograms_new = GPOS_NEW(memory_pool) UlongHistogramHashMap(memory_pool);
		CHistogram::AddEmptyHistogram(memory_pool, histograms_new, histograms_copy);
	}
	else
	{
		if (CStatsPred::EsptDisj == base_pred_stats->GetPredStatsType())
		{
			CStatsPredDisj *pred_stats = CStatsPredDisj::ConvertPredStats(base_pred_stats);

			histograms_new  = MakeHistHashMapDisjFilter
								(
								memory_pool,
								stats_config,
								histograms_copy,
								input_rows,
								pred_stats,
								&scale_factor
								);
		}
		else
		{
			GPOS_ASSERT(CStatsPred::EsptConj == base_pred_stats->GetPredStatsType());
			CStatsPredConj *pred_stats = CStatsPredConj::ConvertPredStats(base_pred_stats);
			num_predicates = pred_stats->GetNumPreds();
			histograms_new = MakeHistHashMapConjFilter
							(
							memory_pool,
							stats_config,
							histograms_copy,
							input_rows,
							pred_stats,
							&scale_factor
							);
		}
		GPOS_ASSERT(CStatistics::MinRows.Get() <= scale_factor.Get());
		rows_filter = input_rows / scale_factor;
		rows_filter = std::max(CStatistics::MinRows.Get(), rows_filter.Get());
	}

	histograms_copy->Release();

	GPOS_ASSERT(rows_filter.Get() <= input_rows.Get());

	if (do_cap_NDVs)
	{
		CStatistics::CapNDVs(rows_filter, histograms_new);
	}

	CStatistics *filter_stats = GPOS_NEW(memory_pool) CStatistics
												(
												memory_pool,
												histograms_new,
												input_stats->CopyWidths(memory_pool),
												rows_filter,
												input_stats->IsEmpty(),
												input_stats->GetNumberOfPredicates() + num_predicates
												);

	// since the filter operation is reductive, we choose the bounding method that takes
	// the minimum of the cardinality upper bound of the source column (in the input hash map)
	// and estimated output cardinality
	CStatisticsUtils::ComputeCardUpperBounds(memory_pool, input_stats, filter_stats, rows_filter, CStatistics::EcbmMin /* card_bounding_method */);

	return filter_stats;
}

// create a new hash map of histograms after applying a conjunctive
// or a disjunctive filter
UlongHistogramHashMap *
CFilterStatsProcessor::MakeHistHashMapConjOrDisjFilter
	(
	IMemoryPool *memory_pool,
	const CStatisticsConfig *stats_config,
	UlongHistogramHashMap *input_histograms,
	CDouble input_rows,
	CStatsPred *pred_stats,
	CDouble *scale_factor
	)
{
	GPOS_ASSERT(NULL != pred_stats);
	GPOS_ASSERT(NULL != stats_config);
	GPOS_ASSERT(NULL != input_histograms);

	UlongHistogramHashMap *result_histograms = NULL;

	if (CStatsPred::EsptConj == pred_stats->GetPredStatsType())
	{
		CStatsPredConj *conjunctive_pred_stats = CStatsPredConj::ConvertPredStats(pred_stats);
		return MakeHistHashMapConjFilter
				(
				memory_pool,
				stats_config,
				input_histograms,
				input_rows,
				conjunctive_pred_stats,
				scale_factor
				);
	}

	CStatsPredDisj *disjunctive_pred_stats = CStatsPredDisj::ConvertPredStats(pred_stats);
	result_histograms  = MakeHistHashMapDisjFilter
						(
						memory_pool,
						stats_config,
						input_histograms,
						input_rows,
						disjunctive_pred_stats,
						scale_factor
						);

	GPOS_ASSERT(NULL != result_histograms);

	return result_histograms;
}

// create new hash map of histograms after applying conjunctive predicates
UlongHistogramHashMap *
CFilterStatsProcessor::MakeHistHashMapConjFilter
	(
	IMemoryPool *memory_pool,
	const CStatisticsConfig *stats_config,
	UlongHistogramHashMap *input_histograms,
	CDouble input_rows,
	CStatsPredConj *conjunctive_pred_stats,
	CDouble *scale_factor
	)
{
	GPOS_ASSERT(NULL != stats_config);
	GPOS_ASSERT(NULL != input_histograms);
	GPOS_ASSERT(NULL != conjunctive_pred_stats);

	conjunctive_pred_stats->Sort();

	CBitSet *filter_col_ids = GPOS_NEW(memory_pool) CBitSet(memory_pool);
	DrgPdouble *scale_factors = GPOS_NEW(memory_pool) DrgPdouble(memory_pool);

	// create copy of the original hash map of colid -> histogram
	UlongHistogramHashMap *result_histograms = CStatisticsUtils::CopyHistHashMap(memory_pool, input_histograms);

	// properties of last seen column
	CDouble last_scale_factor(1.0);
	ULONG last_col_id = gpos::ulong_max;

	// iterate over filters and update corresponding histograms
	const ULONG filters = conjunctive_pred_stats->GetNumPreds();
	for (ULONG ul = 0; ul < filters; ul++)
	{
		CStatsPred *child_pred_stats = conjunctive_pred_stats->GetPredStats(ul);

		GPOS_ASSERT(CStatsPred::EsptConj != child_pred_stats->GetPredStatsType());

		// get the components of the statistics filter
		ULONG col_id = child_pred_stats->GetColId();

		if (CStatsPredUtils::IsUnsupportedPredOnDefinedCol(child_pred_stats))
		{
			// for example, (expression OP const) where expression is a defined column like (a+b)
			CStatsPredUnsupported *unsupported_pred_stats = CStatsPredUnsupported::ConvertPredStats(child_pred_stats);
			scale_factors->Append(GPOS_NEW(memory_pool) CDouble(unsupported_pred_stats->ScaleFactor()));

			continue;
		}

		// the histogram to apply filter on
		CHistogram *hist_before = NULL;
		if (IsNewStatsColumn(col_id, last_col_id))
		{
			scale_factors->Append( GPOS_NEW(memory_pool) CDouble(last_scale_factor));
			last_scale_factor = CDouble(1.0);
		}

		if (CStatsPred::EsptDisj != child_pred_stats->GetPredStatsType())
		{
			GPOS_ASSERT(gpos::ulong_max != col_id);
			hist_before = result_histograms->Find(&col_id)->CopyHistogram(memory_pool);
			GPOS_ASSERT(NULL != hist_before);

			CHistogram *result_histogram = NULL;
			result_histogram = MakeHistSimpleFilter(memory_pool, child_pred_stats, filter_col_ids, hist_before, &last_scale_factor, &last_col_id);
			GPOS_DELETE(hist_before);

			GPOS_ASSERT(NULL != result_histogram);

			CHistogram *input_histogram = input_histograms->Find(&col_id);
			GPOS_ASSERT(NULL != input_histogram);
			if (input_histogram->IsEmpty())
			{
				// input histogram is empty so scaling factor does not make sense.
				// if the input itself is empty, then scaling factor is of no effect
				last_scale_factor = 1 / CHistogram::DefaultSelectivity;
			}

			CStatisticsUtils::AddHistogram(memory_pool, col_id, result_histogram, result_histograms, true /* fReplaceOld */);
			GPOS_DELETE(result_histogram);
		}
		else
		{
			CStatsPredDisj *disjunctive_pred_stats = CStatsPredDisj::ConvertPredStats(child_pred_stats);

			result_histograms->AddRef();
			UlongHistogramHashMap *disjunctive_input_histograms = result_histograms;

			CDouble disjunctive_scale_factor(1.0);
			CDouble num_disj_input_rows(CStatistics::MinRows.Get());

			if (gpos::ulong_max != col_id)
			{
				// The disjunction predicate uses a single column. The input rows to the disjunction
				// is obtained by scaling attained so far on that column
				num_disj_input_rows = std::max(CStatistics::MinRows.Get(), (input_rows / last_scale_factor).Get());
			}
			else
			{
				// the disjunction uses multiple columns therefore cannot reason about the number of input rows
				// to the disjunction
				num_disj_input_rows = input_rows.Get();
			}

			UlongHistogramHashMap *disjunctive_histograms_after = MakeHistHashMapDisjFilter
											(
											memory_pool,
											stats_config,
											result_histograms,
											num_disj_input_rows,
											disjunctive_pred_stats,
											&disjunctive_scale_factor
											);

			// replace intermediate result with the newly generated result from the disjunction
			if (gpos::ulong_max != col_id)
			{
				CHistogram *result_histogram = disjunctive_histograms_after->Find(&col_id);
				CStatisticsUtils::AddHistogram(memory_pool, col_id, result_histogram, result_histograms, true /* fReplaceOld */);
				disjunctive_histograms_after->Release();

				last_scale_factor = last_scale_factor * disjunctive_scale_factor;
			}
			else
			{
				last_scale_factor = disjunctive_scale_factor.Get();
				result_histograms->Release();
				result_histograms = disjunctive_histograms_after;
			}

			last_col_id = col_id;
			disjunctive_input_histograms->Release();
		}
	}

	// scaling factor of the last predicate
	scale_factors->Append(GPOS_NEW(memory_pool) CDouble(last_scale_factor));

	GPOS_ASSERT(NULL != scale_factors);
	CScaleFactorUtils::SortScalingFactor(scale_factors, true /* fDescending */);

	*scale_factor = CScaleFactorUtils::CalcScaleFactorCumulativeConj(stats_config, scale_factors);

	// clean up
	scale_factors->Release();
	filter_col_ids->Release();

	return result_histograms;
}

// create new hash map of histograms after applying disjunctive predicates
UlongHistogramHashMap *
CFilterStatsProcessor::MakeHistHashMapDisjFilter
	(
	IMemoryPool *memory_pool,
	const CStatisticsConfig *stats_config,
	UlongHistogramHashMap *input_histograms,
	CDouble input_rows,
	CStatsPredDisj *disjunctive_pred_stats,
	CDouble *scale_factor
	)
{
	GPOS_ASSERT(NULL != stats_config);
	GPOS_ASSERT(NULL != input_histograms);
	GPOS_ASSERT(NULL != disjunctive_pred_stats);

	CBitSet *non_updatable_cols = CStatisticsUtils::GetColsNonUpdatableHistForDisj(memory_pool, disjunctive_pred_stats);

	disjunctive_pred_stats->Sort();

	CBitSet *filter_col_ids = GPOS_NEW(memory_pool) CBitSet(memory_pool);
	DrgPdouble *scale_factors = GPOS_NEW(memory_pool) DrgPdouble(memory_pool);

	UlongHistogramHashMap *disjunctive_result_histograms = GPOS_NEW(memory_pool) UlongHistogramHashMap(memory_pool);

	CHistogram *previous_histogram = NULL;
	ULONG previous_col_id = gpos::ulong_max;
	CDouble previous_scale_factor(input_rows);

	CDouble cumulative_rows(CStatistics::MinRows.Get());

	// iterate over filters and update corresponding histograms
	const ULONG filters = disjunctive_pred_stats->GetNumPreds();
	for (ULONG ul = 0; ul < filters; ul++)
	{
		CStatsPred *child_pred_stats = disjunctive_pred_stats->GetPredStats(ul);

		// get the components of the statistics filter
		ULONG col_id = child_pred_stats->GetColId();

		if (CStatsPredUtils::IsUnsupportedPredOnDefinedCol(child_pred_stats))
		{
			CStatsPredUnsupported *unsupported_pred_stats = CStatsPredUnsupported::ConvertPredStats(child_pred_stats);
			scale_factors->Append(GPOS_NEW(memory_pool) CDouble(unsupported_pred_stats->ScaleFactor()));

			continue;
		}

		if (IsNewStatsColumn(col_id, previous_col_id))
		{
			scale_factors->Append(GPOS_NEW(memory_pool) CDouble(previous_scale_factor.Get()));
			CStatisticsUtils::UpdateDisjStatistics
								(
								memory_pool,
								non_updatable_cols,
								input_rows,
								cumulative_rows,
								previous_histogram,
								disjunctive_result_histograms,
								previous_col_id
								);
			previous_histogram = NULL;
		}

		CHistogram *histogram = input_histograms->Find(&col_id);
		CHistogram *disjunctive_child_col_histogram = NULL;

		BOOL is_pred_simple = !CStatsPredUtils::IsConjOrDisjPred(child_pred_stats);
		BOOL is_col_id_present = (gpos::ulong_max != col_id);
		UlongHistogramHashMap *child_histograms = NULL;
		CDouble child_scale_factor(1.0);

		if (is_pred_simple)
		{
			GPOS_ASSERT(NULL != histogram);
			disjunctive_child_col_histogram = MakeHistSimpleFilter(memory_pool, child_pred_stats, filter_col_ids, histogram, &child_scale_factor, &previous_col_id);

			CHistogram *input_histogram = input_histograms->Find(&col_id);
			GPOS_ASSERT(NULL != input_histogram);
			if (input_histogram->IsEmpty())
			{
				// input histogram is empty so scaling factor does not make sense.
				// if the input itself is empty, then scaling factor is of no effect
				child_scale_factor = 1 / CHistogram::DefaultSelectivity;
			}
		}
		else
		{
			child_histograms = MakeHistHashMapConjOrDisjFilter
								(
								memory_pool,
								stats_config,
								input_histograms,
								input_rows,
								child_pred_stats,
								&child_scale_factor
								);

			GPOS_ASSERT_IMP(CStatsPred::EsptDisj == child_pred_stats->GetPredStatsType(),
							gpos::ulong_max != col_id);

			if (is_col_id_present)
			{
				// conjunction or disjunction uses only a single column
				disjunctive_child_col_histogram = child_histograms->Find(&col_id)->CopyHistogram(memory_pool);
			}
		}

		CDouble num_rows_disj_child = input_rows / child_scale_factor;
		if (is_col_id_present)
		{
			// 1. a simple predicate (a == 5), (b LIKE "%%GOOD%%")
			// 2. conjunctive / disjunctive predicate where each of its component are predicates on the same column
			// e.g. (a <= 5 AND a >= 1), a in (5, 1)
			GPOS_ASSERT(NULL != disjunctive_child_col_histogram);

			if (NULL == previous_histogram)
			{
				previous_histogram = disjunctive_child_col_histogram;
				cumulative_rows = num_rows_disj_child;
			}
			else
			{
				// statistics operation already conducted on this column
				CDouble output_rows(0.0);
				CHistogram *new_histogram = previous_histogram->MakeUnionHistogramNormalize(memory_pool, cumulative_rows, disjunctive_child_col_histogram, num_rows_disj_child, &output_rows);
				cumulative_rows = output_rows;

				GPOS_DELETE(previous_histogram);
				GPOS_DELETE(disjunctive_child_col_histogram);
				previous_histogram = new_histogram;
			}

			previous_scale_factor = input_rows / std::max(CStatistics::MinRows.Get(), cumulative_rows.Get());
			previous_col_id = col_id;
		}
		else
		{
			// conjunctive predicate where each of it component are predicates on different columns
			// e.g. ((a <= 5) AND (b LIKE "%%GOOD%%"))
			GPOS_ASSERT(NULL != child_histograms);
			GPOS_ASSERT(NULL == disjunctive_child_col_histogram);

			CDouble current_rows_estimate = input_rows / CScaleFactorUtils::CalcScaleFactorCumulativeDisj(stats_config, scale_factors, input_rows);
			UlongHistogramHashMap *merged_histograms = CStatisticsUtils::CreateHistHashMapAfterMergingDisjPreds
													  	  (
													  	  memory_pool,
													  	  non_updatable_cols,
													  	  disjunctive_result_histograms,
													  	  child_histograms,
													  	  current_rows_estimate,
													  	  num_rows_disj_child
													  	  );
			disjunctive_result_histograms->Release();
			disjunctive_result_histograms = merged_histograms;

			previous_histogram = NULL;
			previous_scale_factor = child_scale_factor;
			previous_col_id = col_id;
		}

		CRefCount::SafeRelease(child_histograms);
	}

	// process the result and scaling factor of the last predicate
	CStatisticsUtils::UpdateDisjStatistics
						(
						memory_pool,
						non_updatable_cols,
						input_rows,
						cumulative_rows,
						previous_histogram,
						disjunctive_result_histograms,
						previous_col_id
						);
	previous_histogram = NULL;
	scale_factors->Append(GPOS_NEW(memory_pool) CDouble(std::max(CStatistics::MinRows.Get(), previous_scale_factor.Get())));

	*scale_factor = CScaleFactorUtils::CalcScaleFactorCumulativeDisj(stats_config, scale_factors, input_rows);

	CHistogram::AddHistograms(memory_pool, input_histograms, disjunctive_result_histograms);

	non_updatable_cols->Release();

	// clean up
	scale_factors->Release();
	filter_col_ids->Release();

	return disjunctive_result_histograms;
}

//	create a new histograms after applying the filter that is not
//	an AND/OR predicate
CHistogram *
CFilterStatsProcessor::MakeHistSimpleFilter
	(
	IMemoryPool *memory_pool,
	CStatsPred *pred_stats,
	CBitSet *filter_col_ids,
	CHistogram *hist_before,
	CDouble *last_scale_factor,
	ULONG *target_last_col_id
	)
{
	if (CStatsPred::EsptPoint == pred_stats->GetPredStatsType())
	{
		CStatsPredPoint *point_pred_stats = CStatsPredPoint::ConvertPredStats(pred_stats);
		return MakeHistPointFilter(memory_pool, point_pred_stats, filter_col_ids, hist_before, last_scale_factor, target_last_col_id);
	}

	if (CStatsPred::EsptLike == pred_stats->GetPredStatsType())
	{
		CStatsPredLike *like_pred_stats = CStatsPredLike::ConvertPredStats(pred_stats);

		return MakeHistLikeFilter(memory_pool, like_pred_stats, filter_col_ids, hist_before, last_scale_factor, target_last_col_id);
	}

	CStatsPredUnsupported *unsupported_pred_stats = CStatsPredUnsupported::ConvertPredStats(pred_stats);

	return MakeHistUnsupportedPred(memory_pool, unsupported_pred_stats, filter_col_ids, hist_before, last_scale_factor, target_last_col_id);
}

// create a new histograms after applying the point filter
CHistogram *
CFilterStatsProcessor::MakeHistPointFilter
	(
	IMemoryPool *memory_pool,
	CStatsPredPoint *pred_stats,
	CBitSet *filter_col_ids,
	CHistogram *hist_before,
	CDouble *last_scale_factor,
	ULONG *target_last_col_id
	)
{
	GPOS_ASSERT(NULL != pred_stats);
	GPOS_ASSERT(NULL != filter_col_ids);
	GPOS_ASSERT(NULL != hist_before);

	const ULONG col_id = pred_stats->GetColId();
	GPOS_ASSERT(CHistogram::SupportsFilter(pred_stats->GetCmpType()));

	CPoint *point = pred_stats->GetPredPoint();

	// note column id
	(void) filter_col_ids->ExchangeSet(col_id);

	CDouble local_scale_factor(1.0);
	CHistogram *result_histogram = hist_before->MakeHistogramFilterNormalize(memory_pool, pred_stats->GetCmpType(), point, &local_scale_factor);

	GPOS_ASSERT(DOUBLE(1.0) <= local_scale_factor.Get());

	*last_scale_factor = *last_scale_factor * local_scale_factor;
	*target_last_col_id = col_id;

	return result_histogram;
}


//	create a new histograms for an unsupported predicate
CHistogram *
CFilterStatsProcessor::MakeHistUnsupportedPred
	(
	IMemoryPool *memory_pool,
	CStatsPredUnsupported *pred_stats,
	CBitSet *filter_col_ids,
	CHistogram *hist_before,
	CDouble *last_scale_factor,
	ULONG *target_last_col_id
	)
{
	GPOS_ASSERT(NULL != pred_stats);
	GPOS_ASSERT(NULL != filter_col_ids);
	GPOS_ASSERT(NULL != hist_before);

	const ULONG col_id = pred_stats->GetColId();

	// note column id
	(void) filter_col_ids->ExchangeSet(col_id);

	// generate after histogram
	CHistogram *result_histogram = hist_before->CopyHistogram(memory_pool);
	GPOS_ASSERT(NULL != result_histogram);

	*last_scale_factor = *last_scale_factor * pred_stats->ScaleFactor();
	*target_last_col_id = col_id;

	return result_histogram;
}

//	create a new histograms after applying the LIKE filter
CHistogram *
CFilterStatsProcessor::MakeHistLikeFilter
	(
	IMemoryPool *memory_pool,
	CStatsPredLike *pred_stats,
	CBitSet *filter_col_ids,
	CHistogram *hist_before,
	CDouble *last_scale_factor,
	ULONG *target_last_col_id
	)
{
	GPOS_ASSERT(NULL != pred_stats);
	GPOS_ASSERT(NULL != filter_col_ids);
	GPOS_ASSERT(NULL != hist_before);

	const ULONG col_id = pred_stats->GetColId();

	// note column id
	(void) filter_col_ids->ExchangeSet(col_id);
	CHistogram *result_histogram = hist_before->CopyHistogram(memory_pool);

	*last_scale_factor = *last_scale_factor * pred_stats->DefaultScaleFactor();
	*target_last_col_id = col_id;

	return result_histogram;
}

// check if the column is a new column for statistic calculation
BOOL
CFilterStatsProcessor::IsNewStatsColumn
	(
	ULONG col_id,
	ULONG last_col_id
	)
{
	return (gpos::ulong_max == col_id || col_id != last_col_id);
}

// EOF
