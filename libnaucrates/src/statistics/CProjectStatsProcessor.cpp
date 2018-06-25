//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2018 Pivotal, Inc.
//
//	@filename:
//		CProjectStatsProcessor.cpp
//
//	@doc:
//		Statistics helper routines for processing project operations
//---------------------------------------------------------------------------

#include "naucrates/statistics/CProjectStatsProcessor.h"
#include "naucrates/statistics/CStatisticsUtils.h"

using namespace gpopt;


//  return a statistics object for a project operation
CStatistics *
CProjectStatsProcessor::CalcProjStats(IMemoryPool *memory_pool,
									  const CStatistics *input_stats,
									  ULongPtrArray *projection_colids,
									  HMUlDatum *datum_map)
{
	GPOS_ASSERT(NULL != projection_colids);

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

	// create hash map from colid -> histogram for resultant structure
	UlongHistogramHashMap *histograms_new =
		GPOS_NEW(memory_pool) UlongHistogramHashMap(memory_pool);

	// column ids on which widths are to be computed
	UlongDoubleHashMap *col_id_width_mapping =
		GPOS_NEW(memory_pool) UlongDoubleHashMap(memory_pool);

	const ULONG length = projection_colids->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		ULONG col_id = *(*projection_colids)[ul];
		const CHistogram *histogram = input_stats->GetHistogram(col_id);

		if (NULL == histogram)
		{
			// create histogram for the new project column
			BucketArray *proj_col_bucket = GPOS_NEW(memory_pool) BucketArray(memory_pool);
			CDouble null_freq = 0.0;

			BOOL is_well_defined = false;
			if (NULL != datum_map)
			{
				IDatum *datum = datum_map->Find(&col_id);
				if (NULL != datum)
				{
					is_well_defined = true;
					if (!datum->IsNull())
					{
						proj_col_bucket->Append(CBucket::MakeBucketSingleton(memory_pool, datum));
					}
					else
					{
						null_freq = 1.0;
					}
				}
			}

			CHistogram *proj_col_histogram = NULL;
			CColRef *colref = col_factory->LookupColRef(col_id);
			GPOS_ASSERT(NULL != colref);

			if (0 == proj_col_bucket->Size() &&
				IMDType::EtiBool == colref->Pmdtype()->GetDatumType())
			{
				proj_col_bucket->Release();
				proj_col_histogram = CHistogram::MakeDefaultBoolHistogram(memory_pool);
			}
			else
			{
				proj_col_histogram =
					GPOS_NEW(memory_pool) CHistogram(proj_col_bucket,
													 is_well_defined,
													 null_freq,
													 CHistogram::DefaultNDVRemain,
													 CHistogram::DefaultNDVFreqRemain);
			}

			histograms_new->Insert(GPOS_NEW(memory_pool) ULONG(col_id), proj_col_histogram);
		}
		else
		{
			histograms_new->Insert(GPOS_NEW(memory_pool) ULONG(col_id),
								   histogram->CopyHistogram(memory_pool));
		}

		// look up width
		const CDouble *width = input_stats->GetWidth(col_id);
		if (NULL == width)
		{
			CColRef *colref = col_factory->LookupColRef(col_id);
			GPOS_ASSERT(NULL != colref);

			CDouble width = CStatisticsUtils::DefaultColumnWidth(colref->Pmdtype());
			col_id_width_mapping->Insert(GPOS_NEW(memory_pool) ULONG(col_id),
										 GPOS_NEW(memory_pool) CDouble(width));
		}
		else
		{
			col_id_width_mapping->Insert(GPOS_NEW(memory_pool) ULONG(col_id),
										 GPOS_NEW(memory_pool) CDouble(*width));
		}
	}

	CDouble input_rows = input_stats->Rows();
	// create an output stats object
	CStatistics *projection_stats =
		GPOS_NEW(memory_pool) CStatistics(memory_pool,
										  histograms_new,
										  col_id_width_mapping,
										  input_rows,
										  input_stats->IsEmpty(),
										  input_stats->GetNumberOfPredicates());

	// In the output statistics object, the upper bound source cardinality of the project column
	// is equivalent the estimate project cardinality.
	CStatisticsUtils::ComputeCardUpperBounds(
		memory_pool,
		input_stats,
		projection_stats,
		input_rows,
		CStatistics::EcbmInputSourceMaxCard /* card_bounding_method */);

	// add upper bound card information for the project columns
	CStatistics::CreateAndInsertUpperBoundNDVs(
		memory_pool, projection_stats, projection_colids, input_rows);

	return projection_stats;
}

// EOF
