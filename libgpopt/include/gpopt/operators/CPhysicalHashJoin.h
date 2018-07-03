//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalHashJoin.h
//
//	@doc:
//		Base hash join operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalHashJoin_H
#define GPOPT_CPhysicalHashJoin_H

#include "gpos/base.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CPhysicalJoin.h"
#include "gpos/sync/CMutex.h"

namespace gpopt
{

	// fwd declarations
	class CDistributionSpecHashed;

	//---------------------------------------------------------------------------
	//	@class:
	//		CPhysicalHashJoin
	//
	//	@doc:
	//		Base hash join operator
	//
	//---------------------------------------------------------------------------
	class CPhysicalHashJoin : public CPhysicalJoin
	{

		private:

			// the array of expressions from the outer relation
			// that are extracted from the hashing condition
			ExpressionArray *m_pdrgpexprOuterKeys;

			// the array of expressions from the inner relation
			// that are extracted from the hashing condition
			ExpressionArray *m_pdrgpexprInnerKeys;

			// array redistribute request sent to the first hash join child
			DrgPds *m_pdrgpdsRedistributeRequests;

			// private copy ctor
			CPhysicalHashJoin(const CPhysicalHashJoin &);

			// create the set of redistribute requests to send to first hash join child
			void CreateHashRedistributeRequests(IMemoryPool *memory_pool);

			// compute a distribution matching the distribution delivered by given child
			CDistributionSpec *PdsMatch(IMemoryPool *memory_pool, CDistributionSpec *pds, ULONG ulSourceChildIndex) const;

			// compute required hashed distribution from the n-th child
			CDistributionSpecHashed *PdshashedRequired(IMemoryPool *memory_pool, ULONG child_index, ULONG ulReqIndex) const;

			// create (redistribute, redistribute) optimization request
			CDistributionSpec *PdsRequiredRedistribute
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsInput,
				ULONG  child_index,
				DrgPdp *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// create (non-singleton, replicate) optimization request
			CDistributionSpec *PdsRequiredReplicate
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsInput,
				ULONG  child_index,
				DrgPdp *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// create (singleton, singleton) optimization request
			CDistributionSpec *PdsRequiredSingleton
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsInput,
				ULONG  child_index,
				DrgPdp *pdrgpdpCtxt
				)
				const;

			// create a child hashed distribution request based on input hashed distribution,
			// return NULL if no such request can be created
			CDistributionSpecHashed *PdshashedPassThru
				(
				IMemoryPool *memory_pool,
				CExpressionHandle  &exprhdl,
				CDistributionSpecHashed *pdshashedInput,
				ULONG  child_index,
				DrgPdp *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// check whether a hash key is nullable
			BOOL FNullableHashKey(ULONG ulKey, CColRefSet *pcrsNotNullInner, BOOL fInner) const;

		protected:
			
			// helper for computing a hashed distribution matching the given distribution
                        CDistributionSpecHashed *PdshashedMatching(IMemoryPool *memory_pool, CDistributionSpecHashed *pdshashed, ULONG ulSourceChild) const;

			// check whether the hash keys from one child are nullable
			BOOL FNullableHashKeys(CColRefSet *pcrsNotNullInner, BOOL fInner) const;
			
		public:

			// ctor
			CPhysicalHashJoin
				(
				IMemoryPool *memory_pool,
				ExpressionArray *pdrgpexprOuterKeys,
				ExpressionArray *pdrgpexprInnerKeys
				);

			// dtor
			virtual
			~CPhysicalHashJoin();

			// inner keys
			const ExpressionArray *PdrgpexprInnerKeys() const
			{
				return m_pdrgpexprInnerKeys;
			}

			// outer keys
			const ExpressionArray *PdrgpexprOuterKeys() const
			{
				return m_pdrgpexprOuterKeys;
			}

			//-------------------------------------------------------------------------------------
			// Required Plan Properties
			//-------------------------------------------------------------------------------------

			// compute required sort order of the n-th child
			virtual
			COrderSpec *PosRequired
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				COrderSpec *posInput,
				ULONG child_index,
				DrgPdp *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// compute required rewindability of the n-th child
			virtual
			CRewindabilitySpec *PrsRequired
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				CRewindabilitySpec *prsRequired,
				ULONG child_index,
				DrgPdp *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			// compute required distribution of the n-th child
			virtual
			CDistributionSpec *PdsRequired
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				CDistributionSpec *pdsRequired,
				ULONG child_index,
				DrgPdp *pdrgpdpCtxt,
				ULONG ulOptReq
				)
				const;

			//-------------------------------------------------------------------------------------
			// Derived Plan Properties
			//-------------------------------------------------------------------------------------

			// derive sort order
			virtual
			COrderSpec *PosDerive
				(
				IMemoryPool *memory_pool,
				CExpressionHandle & // exprhdl
				)
				const
			{
				// hash join is not order-preserving
				return GPOS_NEW(memory_pool) COrderSpec(memory_pool);
			}

			//-------------------------------------------------------------------------------------
			// Enforced Properties
			//-------------------------------------------------------------------------------------

			// return order property enforcing type for this operator
			virtual
			CEnfdProp::EPropEnforcingType EpetOrder
				(
				CExpressionHandle &exprhdl,
				const CEnfdOrder *peo
				) const;

			
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// execution order of children
			virtual
			EChildExecOrder Eceo() const
			{
				// TODO - ; 01/06/2014
				// obtain this property by through MD abstraction layer, similar to scalar properties

				// hash join in GPDB executes its inner (right) child first,
				// the optimization order of hash join children follows the execution order
				return EceoRightToLeft;
			}

			// conversion function
			static
			CPhysicalHashJoin *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(CUtils::FHashJoin(pop));

				return dynamic_cast<CPhysicalHashJoin*>(pop);
			}

	}; // class CPhysicalHashJoin

}

#endif // !GPOPT_CPhysicalHashJoin_H

// EOF
