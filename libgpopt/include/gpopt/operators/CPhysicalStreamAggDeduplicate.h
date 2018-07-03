//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CPhysicalStreamAggDeduplicate.h
//
//	@doc:
//		Sort-based stream Aggregate operator for deduplicating join outputs
//---------------------------------------------------------------------------
#ifndef GPOS_CPhysicalStreamAggDeduplicate_H
#define GPOS_CPhysicalStreamAggDeduplicate_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysicalStreamAgg.h"

namespace gpopt
{
	//---------------------------------------------------------------------------
	//	@class:
	//		CPhysicalStreamAggDeduplicate
	//
	//	@doc:
	//		Sort-based aggregate operator for deduplicating join outputs
	//
	//---------------------------------------------------------------------------
	class CPhysicalStreamAggDeduplicate : public CPhysicalStreamAgg
	{
		private:

			// array of keys from the join's child
			ColRefArray *m_pdrgpcrKeys;

			// private copy ctor
			CPhysicalStreamAggDeduplicate(const CPhysicalStreamAggDeduplicate &);

		public:

			// ctor
			CPhysicalStreamAggDeduplicate
				(
				IMemoryPool *memory_pool,
				ColRefArray *colref_array,
				ColRefArray *pdrgpcrMinimal,
				COperator::EGbAggType egbaggtype,
				ColRefArray *pdrgpcrKeys,
				BOOL fGeneratesDuplicates,
				BOOL fMultiStage
				);

			// dtor
			virtual
			~CPhysicalStreamAggDeduplicate();

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopPhysicalStreamAggDeduplicate;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CPhysicalStreamAggDeduplicate";
			}

			// array of keys from the join's child
			ColRefArray *PdrgpcrKeys() const
			{
				return m_pdrgpcrKeys;
			}

			//-------------------------------------------------------------------------------------
			// Required Plan Properties
			//-------------------------------------------------------------------------------------

			// compute required output columns of the n-th child
			virtual
			CColRefSet *PcrsRequired
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				CColRefSet *pcrsRequired,
				ULONG child_index,
				DrgPdp *, //pdrgpdpCtxt,
				ULONG //ulOptReq
				)
			{
				return PcrsRequiredAgg(memory_pool, exprhdl, pcrsRequired, child_index, m_pdrgpcrKeys);
			}

			// compute required sort columns of the n-th child
			virtual
			COrderSpec *PosRequired
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				COrderSpec *posRequired,
				ULONG child_index,
				DrgPdp *, //pdrgpdpCtxt,
				ULONG //ulOptReq
				)
				const
			{
				return PosRequiredStreamAgg(memory_pool, exprhdl, posRequired, child_index, m_pdrgpcrKeys);
			}

			// compute required distribution of the n-th child
			virtual
			CDistributionSpec *PdsRequired
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsRequired,
				ULONG child_index,
				DrgPdp *, //pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const
			{
				return PdsRequiredAgg(memory_pool, exprhdl, pdsRequired, child_index, ulOptReq, m_pdrgpcrKeys, m_pdrgpcrKeys);
			}

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// debug print
			virtual
			IOstream &OsPrint(IOstream &os) const;

			// conversion function
			static
			CPhysicalStreamAggDeduplicate *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopPhysicalStreamAggDeduplicate == pop->Eopid());

				return reinterpret_cast<CPhysicalStreamAggDeduplicate*>(pop);
			}

	}; // class CPhysicalStreamAggDeduplicate

}


#endif // !GPOS_CPhysicalStreamAggDeduplicate_H

// EOF
