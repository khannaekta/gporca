//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//
//	@filename:
//		CStatistics.h
//
//	@doc:
//		Statistics implementation over 1D histograms
//---------------------------------------------------------------------------
#ifndef GPNAUCRATES_CStatistics_H
#define GPNAUCRATES_CStatistics_H

#include "gpos/base.h"
#include "gpos/string/CWStringDynamic.h"
#include "gpos/sync/CMutex.h"

#include "naucrates/statistics/IStatistics.h"
#include "naucrates/statistics/CStatsPredDisj.h"
#include "naucrates/statistics/CStatsPredConj.h"
#include "naucrates/statistics/CStatsPredLike.h"
#include "naucrates/statistics/CStatsPredUnsupported.h"
#include "naucrates/statistics/CUpperBoundNDVs.h"

#include "naucrates/statistics/CHistogram.h"
#include "gpos/common/CBitSet.h"

namespace gpopt
{
	class CStatisticsConfig;
	class CColumnFactory;
}  // namespace gpopt

namespace gpnaucrates
{
	using namespace gpos;
	using namespace gpdxl;
	using namespace gpmd;
	using namespace gpopt;

	// hash maps ULONG -> array of ULONGs
	typedef CHashMap<ULONG,
					 ULongPtrArray,
					 gpos::HashValue<ULONG>,
					 gpos::Equals<ULONG>,
					 CleanupDelete<ULONG>,
					 CleanupRelease<ULongPtrArray> >
		UlongUlongArrayHashMap;

	// iterator
	typedef CHashMapIter<ULONG,
						 ULongPtrArray,
						 gpos::HashValue<ULONG>,
						 gpos::Equals<ULONG>,
						 CleanupDelete<ULONG>,
						 CleanupRelease<ULongPtrArray> >
		UlongUlongArrayHashMapIter;

	//---------------------------------------------------------------------------
	//	@class:
	//		CStatistics
	//
	//	@doc:
	//		Abstract statistics API
	//---------------------------------------------------------------------------
	class CStatistics : public IStatistics
	{
	public:
		// method used to compute for columns of each source it corresponding
		// the cardinality upper bound
		enum ECardBoundingMethod
		{
			EcbmOutputCard =
				0,  // use the estimated output cardinality as the upper bound cardinality of the source
			EcbmInputSourceMaxCard,  // use the upper bound cardinality of the source in the input statistics object
			EcbmMin,  // use the minimum of the above two cardinality estimates

			EcbmSentinel
		};

		// helper method to copy stats on columns that are not excluded by bitset
		void AddNotExcludedHistograms(IMemoryPool *mp,
									  CBitSet *excluded_cols,
									  UlongHistogramHashMap *col_histogram_mapping) const;

	private:
		// private copy ctor
		CStatistics(const CStatistics &);

		// private assignment operator
		CStatistics &operator=(CStatistics &);

		// hashmap from column ids to histograms
		UlongHistogramHashMap *m_colid_histogram_mapping;

		// hashmap from column id to width
		UlongDoubleHashMap *m_colid_width_mapping;

		// number of rows
		CDouble m_rows;

		// the risk to have errors in cardinality estimation; it goes from 1 to infinity,
		// where 1 is no risk
		// when going from the leaves to the root of the plan, operators that generate joins,
		// selections and groups increment the risk
		ULONG m_stats_estimation_risk;

		// flag to indicate if input relation is empty
		BOOL m_empty;

		// statistics could be computed using predicates with external parameters (outer references),
		// this is the total number of external parameters' values
		CDouble m_num_rebinds;

		// number of predicates applied
		ULONG m_num_predicates;

		// statistics configuration
		CStatisticsConfig *m_stats_conf;

		// array of upper bound of ndv per source;
		// source can be one of the following operators: like Get, Group By, and Project
		UpperBoundNDVPtrArray *m_src_upper_bound_NDVs;

		// mutex for locking entry when accessing hashmap from source id -> upper bound of source cardinality
		CMutex m_src_upper_bound_mapping_mutex;

		// the default m_bytearray_value for operators that have no cardinality estimation risk
		static const ULONG no_card_est_risk_default_val;

		// helper method to add histograms where the column ids have been remapped
		static void AddHistogramsWithRemap(IMemoryPool *mp,
										   UlongHistogramHashMap *src_histograms,
										   UlongHistogramHashMap *dest_histograms,
										   UlongToColRefMap *colref_mapping,
										   BOOL must_exist);

		// helper method to add width information where the column ids have been remapped
		static void AddWidthInfoWithRemap(IMemoryPool *mp,
										  UlongDoubleHashMap *src_width,
										  UlongDoubleHashMap *dest_width,
										  UlongToColRefMap *colref_mapping,
										  BOOL must_exist);

	public:
		// ctor
		CStatistics(IMemoryPool *mp,
					UlongHistogramHashMap *col_histogram_mapping,
					UlongDoubleHashMap *colid_width_mapping,
					CDouble rows,
					BOOL is_empty,
					ULONG num_predicates = 0);

		// dtor
		virtual ~CStatistics();

		virtual UlongDoubleHashMap *CopyWidths(IMemoryPool *mp) const;

		virtual void CopyWidthsInto(IMemoryPool *mp,
									UlongDoubleHashMap *colid_width_mapping) const;

		virtual UlongHistogramHashMap *CopyHistograms(IMemoryPool *mp) const;

		// actual number of rows
		virtual CDouble Rows() const;

		// number of rebinds
		virtual CDouble
		NumRebinds() const
		{
			return m_num_rebinds;
		}

		// skew estimate for given column
		virtual CDouble GetSkew(ULONG colid) const;

		// what is the width in bytes of set of column id's
		virtual CDouble Width(ULongPtrArray *colids) const;

		// what is the width in bytes of set of column references
		virtual CDouble Width(IMemoryPool *mp, CColRefSet *colrefs) const;

		// what is the width in bytes
		virtual CDouble Width() const;

		// is statistics on an empty input
		virtual BOOL
		IsEmpty() const
		{
			return m_empty;
		}

		// look up the histogram of a particular column
		virtual const CHistogram *
		GetHistogram(ULONG colid) const
		{
			return m_colid_histogram_mapping->Find(&colid);
		}

		// look up the number of distinct values of a particular column
		virtual CDouble GetNDVs(const CColRef *colref);

		// look up the width of a particular column
		virtual const CDouble *GetWidth(ULONG colid) const;

		// the risk of errors in cardinality estimation
		virtual ULONG
		StatsEstimationRisk() const
		{
			return m_stats_estimation_risk;
		}

		// update the risk of errors in cardinality estimation
		virtual void
		SetStatsEstimationRisk(ULONG risk)
		{
			m_stats_estimation_risk = risk;
		}

		// inner join with another stats structure
		virtual CStatistics *CalcInnerJoinStats(IMemoryPool *mp,
												const IStatistics *other_stats,
												StatsPredJoinArray *join_preds_stats) const;

		// LOJ with another stats structure
		virtual CStatistics *CalcLOJoinStats(IMemoryPool *mp,
											 const IStatistics *other_stats,
											 StatsPredJoinArray *join_preds_stats) const;

		// left anti semi join with another stats structure
		virtual CStatistics *CalcLASJoinStats(
			IMemoryPool *mp,
			const IStatistics *other_stats,
			StatsPredJoinArray *join_preds_stats,
			BOOL
				DoIgnoreLASJHistComputation  // except for the case of LOJ cardinality estimation this flag is always
			// "true" since LASJ stats computation is very aggressive
			) const;

		// semi join stats computation
		virtual CStatistics *CalcLSJoinStats(IMemoryPool *mp,
											 const IStatistics *inner_side_stats,
											 StatsPredJoinArray *join_preds_stats) const;

		// return required props associated with stats object
		virtual CReqdPropRelational *GetReqdRelationalProps(IMemoryPool *mp) const;

		// append given stats to current object
		virtual void AppendStats(IMemoryPool *mp, IStatistics *stats);

		// set number of rebinds
		virtual void
		SetRebinds(CDouble num_rebinds)
		{
			GPOS_ASSERT(0.0 < num_rebinds);

			m_num_rebinds = num_rebinds;
		}

		// copy stats
		virtual IStatistics *CopyStats(IMemoryPool *mp) const;

		// return a copy of this stats object scaled by a given factor
		virtual IStatistics *ScaleStats(IMemoryPool *mp, CDouble factor) const;

		// copy stats with remapped column id
		virtual IStatistics *CopyStatsWithRemap(IMemoryPool *mp,
												UlongToColRefMap *colref_mapping,
												BOOL must_exist) const;

		// return the set of column references we have stats for
		virtual CColRefSet *GetColRefSet(IMemoryPool *mp) const;

		// generate the DXL representation of the statistics object
		virtual CDXLStatsDerivedRelation *GetDxlStatsDrvdRelation(IMemoryPool *mp,
																  CMDAccessor *md_accessor) const;

		// print function
		virtual IOstream &OsPrint(IOstream &os) const;

		// add upper bound of source cardinality
		virtual void AddCardUpperBound(CUpperBoundNDVs *upper_bound_NDVs);

		// return the upper bound of the number of distinct values for a given column
		virtual CDouble GetColUpperBoundNDVs(const CColRef *colref);

		// return the index of the array of upper bound ndvs to which column reference belongs
		virtual ULONG GetIndexUpperBoundNDVs(const CColRef *colref);

		// return the column identifiers of all columns statistics maintained
		virtual ULongPtrArray *GetColIdsWithStats(IMemoryPool *mp) const;

		virtual ULONG
		GetNumberOfPredicates() const
		{
			return m_num_predicates;
		}

		CStatisticsConfig *
		GetStatsConfig() const
		{
			return m_stats_conf;
		}

		UpperBoundNDVPtrArray *
		GetUpperBoundNDVs() const
		{
			return m_src_upper_bound_NDVs;
		}
		// create an empty statistics object
		static CStatistics *
		MakeEmptyStats(IMemoryPool *mp)
		{
			ULongPtrArray *colids = GPOS_NEW(mp) ULongPtrArray(mp);
			CStatistics *stats = MakeDummyStats(mp, colids, DefaultRelationRows);

			// clean up
			colids->Release();

			return stats;
		}

		// conversion function
		static CStatistics *
		CastStats(IStatistics *pstats)
		{
			GPOS_ASSERT(NULL != pstats);
			return dynamic_cast<CStatistics *>(pstats);
		}

		// create a dummy statistics object
		static CStatistics *MakeDummyStats(IMemoryPool *mp,
										   ULongPtrArray *colids,
										   CDouble rows);

		// create a dummy statistics object
		static CStatistics *MakeDummyStats(IMemoryPool *mp,
										   ULongPtrArray *col_histogram_mapping,
										   ULongPtrArray *colid_width_mapping,
										   CDouble rows);

		// default column width
		static const CDouble DefaultColumnWidth;

		// default number of rows in relation
		static const CDouble DefaultRelationRows;

		// minimum number of rows in relation
		static const CDouble MinRows;

		// epsilon
		static const CDouble Epsilon;

		// default number of distinct values
		static const CDouble DefaultDistinctValues;

		// check if the input statistics from join statistics computation empty
		static BOOL IsEmptyJoin(const CStatistics *outer_stats,
								const CStatistics *inner_side_stats,
								BOOL IsLASJ);

		// add upper bound ndvs information for a given set of columns
		static void CreateAndInsertUpperBoundNDVs(IMemoryPool *mp,
												  CStatistics *stats,
												  ULongPtrArray *colids,
												  CDouble rows);

		// cap the total number of distinct values (NDV) in buckets to the number of rows
		static void CapNDVs(CDouble rows, UlongHistogramHashMap *col_histogram_mapping);
	};  // class CStatistics

}  // namespace gpnaucrates

#endif  // !GPNAUCRATES_CStatistics_H

// EOF
