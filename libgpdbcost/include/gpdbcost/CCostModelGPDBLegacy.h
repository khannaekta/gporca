//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CCostModelGPDBLegacy.h
//
//	@doc:
//		GPDB's legacy (uncalibrated) cost model
//---------------------------------------------------------------------------
#ifndef GPDBCOST_CCostModelGPDBLegacy_H
#define GPDBCOST_CCostModelGPDBLegacy_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"

#include "gpopt/cost/CCost.h"
#include "gpopt/cost/ICostModel.h"
#include "gpopt/cost/ICostModelParams.h"

#include "gpdbcost/CCostModelParamsGPDBLegacy.h"


namespace gpdbcost
{
	using namespace gpos;
	using namespace gpopt;
	using namespace gpmd;


	//---------------------------------------------------------------------------
	//	@class:
	//		CCostModelGPDBLegacy
	//
	//	@doc:
	//		GPDB's legacy cost model
	//
	//---------------------------------------------------------------------------
	class CCostModelGPDBLegacy : public ICostModel
	{

		private:

			// definition of operator processor
			typedef CCost(FnCost)(IMemoryPool *, CExpressionHandle &, const CCostModelGPDBLegacy *, const SCostingInfo *);

			//---------------------------------------------------------------------------
			//	@struct:
			//		SCostMapping
			//
			//	@doc:
			//		Mapping of operator to a cost function
			//
			//---------------------------------------------------------------------------
			struct SCostMapping
			{
				// physical operator id
				COperator::EOperatorId m_eopid;

				// pointer to cost function
				FnCost *m_pfnc;

			}; // struct SCostMapping

			// memory pool
			IMemoryPool *m_memory_pool;

			// number of segments
			ULONG m_num_of_segments;

			// cost model parameters
			CCostModelParamsGPDBLegacy *m_cost_model_params;

			// array of mappings
			static
			const SCostMapping m_rgcm[];

			// return cost of processing the given number of rows
			static
			CCost CostTupleProcessing(DOUBLE rows, DOUBLE width, ICostModelParams *pcp);

			// helper function to return cost of a plan rooted by unary operator
			static
			CCost CostUnary(DOUBLE rows, DOUBLE width, DOUBLE num_rebinds, DOUBLE *pdcostChildren, ICostModelParams *pcp);

			// cost of spooling
			static
			CCost CostSpooling(DOUBLE rows, DOUBLE width, DOUBLE num_rebinds, DOUBLE *pdcostChildren, ICostModelParams *pcp);

			// cost of redistribute motion operators
			static
			CCost CostRedistribute(DOUBLE rows, DOUBLE width, ICostModelParams *pcp);

			// add up array of costs
			static
			CCost CostSum(DOUBLE *pdCost, ULONG size);

			// check if given operator is unary
			static
			BOOL FUnary(COperator::EOperatorId op_id);

			// cost of scan
			static
			CCost CostScan(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of index scan
			static
			CCost CostIndexScan(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of bitmap table scan
			static
			CCost CostBitmapTableScan(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of sequence project
			static
			CCost CostSequenceProject(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of CTE producer
			static
			CCost CostCTEProducer(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of CTE consumer
			static
			CCost CostCTEConsumer(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of const table get
			static
			CCost CostConstTableGet(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of DML
			static
			CCost CostDML(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of hash agg
			static
			CCost CostHashAgg(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of scalar agg
			static
			CCost CostScalarAgg(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of stream agg
			static
			CCost CostStreamAgg(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of sequence
			static
			CCost CostSequence(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of sort
			static
			CCost CostSort(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of TVF
			static
			CCost CostTVF(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of UnionAll
			static
			CCost CostUnionAll(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of hash join
			static
			CCost CostHashJoin(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of nljoin
			static
			CCost CostNLJoin(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of inner or outer index-nljoin
			static
			CCost CostIndexNLJoin(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

			// cost of motion
			static
			CCost CostMotion(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, const CCostModelGPDBLegacy *pcmgpdb, const SCostingInfo *pci);

		public:

			// ctor
			CCostModelGPDBLegacy(IMemoryPool *memory_pool, ULONG ulSegments, CostModelParamsArray *pdrgpcp = NULL);

			// dtor
			virtual
			~CCostModelGPDBLegacy();

			// number of segments
			ULONG UlHosts() const
			{
				return m_num_of_segments;
			}

			// return number of rows per host
			virtual
			CDouble DRowsPerHost(CDouble dRowsTotal) const;

			// return cost model parameters
			virtual
			ICostModelParams *GetCostModelParams() const
			{
				return m_cost_model_params;
			}
			
			// main driver for cost computation
			virtual
			CCost Cost(CExpressionHandle &exprhdl, const SCostingInfo *pci) const;
			
			// cost model type
			virtual
			ECostModelType Ecmt() const
			{
				return ICostModel::EcmtGPDBLegacy;
			}

	}; // class CCostModelGPDBLegacy

}

#endif // !GPDBCOST_CCostModelGPDBLegacy_H

// EOF
