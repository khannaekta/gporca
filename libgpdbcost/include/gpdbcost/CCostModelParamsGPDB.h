//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CCostModelParamsGPDB.h
//
//	@doc:
//		Parameters in GPDB cost model
//---------------------------------------------------------------------------
#ifndef GPDBCOST_CCostModelParamsGPDB_H
#define GPDBCOST_CCostModelParamsGPDB_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"
#include "gpos/common/CRefCount.h"
#include "gpos/string/CWStringConst.h"

#include "gpopt/cost/ICostModelParams.h"


namespace gpopt
{
	using namespace gpos;


	//---------------------------------------------------------------------------
	//	@class:
	//		CCostModelParamsGPDB
	//
	//	@doc:
	//		Parameters in GPDB cost model
	//
	//---------------------------------------------------------------------------
	class CCostModelParamsGPDB : public ICostModelParams
	{

		public:

			// enumeration of cost model params
			enum ECostParam
			{
				EcpSeqIOBandwidth = 0,		// sequential i/o bandwidth
				EcpRandomIOBandwidth,		// random i/o bandwidth
				EcpTupProcBandwidth,		// tuple processing bandwidth
				EcpOutputBandwidth,			// output bandwidth
				EcpInitScanFactor,			// scan initialization cost factor
				EcpTableScanCostUnit,		// table scan cost per tuple
				// index scan params
				EcpInitIndexScanFactor,		// index scan initialization cost factor
				EcpIndexBlockCostUnit,		// coefficient for reading leaf index blocks
				EcpIndexFilterCostUnit,		// coefficient for index column filtering
				EcpIndexScanTupCostUnit,	// index scan cost per tuple retrieving
				EcpIndexScanTupRandomFactor,// random IO cost per tuple retrieving
				// filter operator params
				EcpFilterColCostUnit,		// coefficient for filtering tuple per column
				EcpOutputTupCostUnit,		// coefficient for output tuple per width
				// motion operator params
				EcpGatherSendCostUnit,                // sending cost per tuple in gather motion
				EcpGatherRecvCostUnit,                // receiving cost per tuple in gather motion
				EcpRedistributeSendCostUnit,          // sending cost per tuple in redistribute motion
				EcpRedistributeRecvCostUnit,          // receiving cost per tuple in redistribute motion
				EcpBroadcastSendCostUnit,             // sending cost per tuple in broadcast motion
				EcpBroadcastRecvCostUnit,             // receiving cost per tuple in broadcast motion
				EcpNoOpCostUnit,                      // cost per tuple in No-Op motion
				// general join params
				EcpJoinFeedingTupColumnCostUnit,      // feeding cost per tuple per column in join operator
				EcpJoinFeedingTupWidthCostUnit,       // feeding cost per tuple per width in join operator
				EcpJoinOutputTupCostUnit,             // output cost per tuple in join operator
				// hash join params
				EcpHJSpillingMemThreshold,            // memory threshold for hash join spilling
				EcpHJHashTableInitCostFactor,         // initial cost for building hash table for hash join
				EcpHJHashTableColumnCostUnit,         // building hash table cost for per tuple per column
				EcpHJHashTableWidthCostUnit,          // building hash table cost for per tuple with unit width
				EcpHJHashingTupWidthCostUnit,         // hashing cost per tuple with unit width in hash join
				EcpHJFeedingTupColumnSpillingCostUnit,// feeding cost per tuple per column in hash join if spilling
				EcpHJFeedingTupWidthSpillingCostUnit, // feeding cost per tuple with unit width in hash join if spilling
				EcpHJHashingTupWidthSpillingCostUnit, // hashing cost per tuple with unit width in hash join if spilling
				// hash agg params
				EcpHashAggInputTupColumnCostUnit,     // cost for building hash table for per tuple per grouping column in hash aggregate
				EcpHashAggInputTupWidthCostUnit,      // cost for building hash table for per tuple with unit width in hash aggregate
				EcpHashAggOutputTupWidthCostUnit,     // cost for outputting for per tuple with unit width in hash aggregate
				// sort params
				EcpSortTupWidthCostUnit,              // sorting cost per tuple with unit width
				// tuple processing params
				EcpTupDefaultProcCostUnit,            // cost for processing per tuple with unit width
				// materialization params
				EcpMaterializeCostUnit,               // cost for materializing per tuple with unit width

				EcpTupUpdateBandwith,		// tuple update bandwidth
				EcpNetBandwidth,			// network bandwidth
				EcpSegments,				// number of segments
				EcpNLJFactor,				// nested loop factor
				EcpHJFactor,				// hash join factor - to represent spilling cost
				EcpHashFactor,				// hash building factor
				EcpDefaultCost,				// default cost
				EcpIndexJoinAllowedRiskThreshold, // largest estimation risk for which we do not penalize index join

				EcpBitmapIOCostLargeNDV, // bitmap IO co-efficient for large NDV
				EcpBitmapIOCostSmallNDV, // bitmap IO co-efficient for smaller NDV
				EcpBitmapPageCostLargeNDV, // bitmap page cost for large NDV
				EcpBitmapPageCostSmallNDV, // bitmap page cost for smaller NDV
				EcpBitmapNDVThreshold, // bitmap NDV threshold

				EcpSentinel
			};

		private:

			// memory pool
			IMemoryPool *m_memory_pool;

			// array of parameters
			// cost param enum is used as index in this array
			SCostParam* m_rgpcp[EcpSentinel];

			// default m_bytearray_value of sequential i/o bandwidth
			static
			const CDouble DSeqIOBandwidthVal;

			// default m_bytearray_value of random i/o bandwidth
			static
			const CDouble DRandomIOBandwidthVal;

			// default m_bytearray_value of tuple processing bandwidth
			static
			const CDouble DTupProcBandwidthVal;

			// default m_bytearray_value of output bandwidth
			static
			const CDouble DOutputBandwidthVal;

			// default m_bytearray_value of scan initialization cost
			static
			const CDouble DInitScanFacorVal;

			// default m_bytearray_value of table scan cost unit
			static
			const CDouble DTableScanCostUnitVal;

			// default m_bytearray_value of index scan initialization cost
			static
			const CDouble DInitIndexScanFactorVal;

			// default m_bytearray_value of index block cost unit
			static
			const CDouble DIndexBlockCostUnitVal;

			// default m_bytearray_value of index filtering cost unit
			static
			const CDouble DIndexFilterCostUnitVal;

			// default m_bytearray_value of index scan cost unit per tuple per unit width
			static
			const CDouble DIndexScanTupCostUnitVal;

			// default m_bytearray_value of index scan random IO cost unit per tuple
			static
			const CDouble DIndexScanTupRandomFactorVal;

			// default m_bytearray_value of filter column cost unit
			static
			const CDouble DFilterColCostUnitVal;

			// default m_bytearray_value of output tuple cost unit
			static
			const CDouble DOutputTupCostUnitVal;

			// default m_bytearray_value of sending tuple cost unit for gather motion
			static
			const CDouble DGatherSendCostUnitVal;

			// default m_bytearray_value of receiving tuple cost unit for gather motion
			static
			const CDouble DGatherRecvCostUnitVal;

			// default m_bytearray_value of sending tuple cost unit for redistribute motion
			static
			const CDouble DRedistributeSendCostUnitVal;

			// default m_bytearray_value of receiving tuple cost unit for redistribute motion
			static
			const CDouble DRedistributeRecvCostUnitVal;

			// default m_bytearray_value of sending tuple cost unit for broadcast motion
			static
			const CDouble DBroadcastSendCostUnitVal;

			// default m_bytearray_value of receiving tuple cost unit for broadcast motion
			static
			const CDouble DBroadcastRecvCostUnitVal;

			// default m_bytearray_value of tuple cost unit for No-Op motion
			static
			const CDouble DNoOpCostUnitVal;

			// default m_bytearray_value of feeding cost per tuple per column in join operator
			static
			const CDouble DJoinFeedingTupColumnCostUnitVal;

			// default m_bytearray_value of feeding cost per tuple per width in join operator
			static
			const CDouble DJoinFeedingTupWidthCostUnitVal;

			// default m_bytearray_value of output cost per tuple in join operator
			static
			const CDouble DJoinOutputTupCostUnitVal;

			// default m_bytearray_value of memory threshold for hash join spilling
			static
			const CDouble DHJSpillingMemThresholdVal;

			// default m_bytearray_value of initial cost for building hash table for hash join
			static
			const CDouble DHJHashTableInitCostFactorVal;

			// default m_bytearray_value of building hash table cost for per tuple per column
			static
			const CDouble DHJHashTableColumnCostUnitVal;

			// default m_bytearray_value of building hash table cost for per tuple with unit width
			static
			const CDouble DHJHashTableWidthCostUnitVal;

			// default m_bytearray_value of hashing cost per tuple with unit width in hash join
			static
			const CDouble DHJHashingTupWidthCostUnitVal;

			// default m_bytearray_value of feeding cost per tuple per column in hash join if spilling
			static
			const CDouble DHJFeedingTupColumnSpillingCostUnitVal;

			// default m_bytearray_value of feeding cost per tuple with unit width in hash join if spilling
			static
			const CDouble DHJFeedingTupWidthSpillingCostUnitVal;

			// default m_bytearray_value of hashing cost per tuple with unit width in hash join if spilling
			static
			const CDouble DHJHashingTupWidthSpillingCostUnitVal;

			// default m_bytearray_value of cost for building hash table for per tuple per grouping column in hash aggregate
			static
			const CDouble DHashAggInputTupColumnCostUnitVal;

			// default m_bytearray_value of cost for building hash table for per tuple with unit width in hash aggregate
			static
			const CDouble DHashAggInputTupWidthCostUnitVal;

			// default m_bytearray_value of cost for outputting for per tuple with unit width in hash aggregate
			static
			const CDouble DHashAggOutputTupWidthCostUnitVal;

			// default m_bytearray_value of sorting cost per tuple with unit width
			static
			const CDouble DSortTupWidthCostUnitVal;

			// default m_bytearray_value of cost for processing per tuple with unit width
			static
			const CDouble DTupDefaultProcCostUnitVal;

			// default m_bytearray_value of cost for materializing per tuple with unit width
			static
			const CDouble DMaterializeCostUnitVal;

			// default m_bytearray_value of tuple update bandwidth
			static
			const CDouble DTupUpdateBandwidthVal;

			// default m_bytearray_value of network bandwidth
			static
			const CDouble DNetBandwidthVal;

			// default m_bytearray_value of number of segments
			static
			const CDouble DSegmentsVal;

			// default m_bytearray_value of nested loop factor
			static
			const CDouble DNLJFactorVal;

			// default m_bytearray_value of hash join factor
			static
			const CDouble DHJFactorVal;

			// default m_bytearray_value of hash building factor
			static
			const CDouble DHashFactorVal;

			// default cost m_bytearray_value when one is not computed
			static
			const CDouble DDefaultCostVal;

			// largest estimation risk for which we do not penalize index join
			static
			const CDouble DIndexJoinAllowedRiskThreshold;

			// default bitmap IO cost when NDV is smaller
			static
			const CDouble DBitmapIOCostLargeNDV;

			// default bitmap IO cost when NDV is smaller
			static
			const CDouble DBitmapIOCostSmallNDV;

			// default bitmap page cost when NDV is larger
			static
			const CDouble DBitmapPageCostLargeNDV;

			// default bitmap page cost when NDV is smaller
			static
			const CDouble DBitmapPageCostSmallNDV;

			// default threshold of NDV for bitmap costing
			static
			const CDouble DBitmapNDVThreshold;

			// private copy ctor
			CCostModelParamsGPDB(CCostModelParamsGPDB &);

		public:

			// ctor
			explicit
			CCostModelParamsGPDB(IMemoryPool *memory_pool);

			// dtor
			virtual
			~CCostModelParamsGPDB();

			// lookup param by id
			virtual
			SCostParam *PcpLookup(ULONG id) const;

			// lookup param by name
			virtual
			SCostParam *PcpLookup(const CHAR *szName) const;

			// set param by id
			virtual
			void SetParam(ULONG id, CDouble dVal, CDouble dLowerBound, CDouble dUpperBound);

			// set param by name
			virtual
			void SetParam(const CHAR *szName, CDouble dVal, CDouble dLowerBound, CDouble dUpperBound);

			// print function
			virtual
			IOstream &OsPrint(IOstream &os) const;

			virtual BOOL
			Equals(ICostModelParams *pcm) const;

			virtual const CHAR *
			SzNameLookup(ULONG id) const;

	}; // class CCostModelParamsGPDB

}

#endif // !GPDBCOST_CCostModelParamsGPDB_H

// EOF
