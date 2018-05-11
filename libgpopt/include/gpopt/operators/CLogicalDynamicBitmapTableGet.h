//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CLogicalDynamicBitmapTableGet.h
//
//	@doc:
//		Logical operator for dynamic table access via bitmap indexes.
//
//	@owner:
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPOPT_CLogicalDynamicBitmapTableGet_H
#define GPOPT_CLogicalDynamicBitmapTableGet_H

#include "gpos/base.h"

#include "gpopt/operators/CLogicalDynamicGetBase.h"

namespace gpopt
{
	// fwd declarations
	class CColRefSet;
	class CTableDescriptor;

	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalDynamicBitmapTableGet
	//
	//	@doc:
	//		Logical operator for dynamic table access via bitmap indexes.
	//
	//---------------------------------------------------------------------------
	class CLogicalDynamicBitmapTableGet : public CLogicalDynamicGetBase
	{
		private:
			// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
			ULONG m_ulOriginOpId;

			// private copy ctor
			CLogicalDynamicBitmapTableGet(const CLogicalDynamicBitmapTableGet &);

		public:
			// ctors
			CLogicalDynamicBitmapTableGet
				(
				IMemoryPool *memory_pool,
				CTableDescriptor *ptabdesc,
				ULONG ulOriginOpId,
				const CName *pnameTableAlias,
				ULONG ulPartIndex,
				DrgPcr *pdrgpcrOutput,
				DrgDrgPcr *pdrgpdrgpcrPart,
				ULONG ulSecondaryPartIndexId,
				BOOL is_partial,
				CPartConstraint *ppartcnstr,
				CPartConstraint *ppartcnstrRel
				);

			explicit
			CLogicalDynamicBitmapTableGet(IMemoryPool *memory_pool);

			// dtor
			virtual
			~CLogicalDynamicBitmapTableGet();

			// identifier
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalDynamicBitmapTableGet;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CLogicalDynamicBitmapTableGet";
			}

			// operator specific hash function
			virtual
			ULONG HashValue() const;

			// match function
			virtual
			BOOL Matches(COperator *pop) const;

			// sensitivity to order of inputs
			virtual
			BOOL FInputOrderSensitive() const
			{
				return true;
			}

			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns(IMemoryPool *memory_pool, UlongColRefHashMap *colref_mapping, BOOL must_exist);

			// derive outer references
			virtual
			CColRefSet *PcrsDeriveOuter(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);

			// derive constraint property
			virtual
			CPropConstraint *PpcDeriveConstraint(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			// compute required stat columns of the n-th child
			virtual
			CColRefSet *PcrsStat
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &, // exprhdl
				CColRefSet *, //pcrsInput
				ULONG // child_index
				)
				const
			{
				return GPOS_NEW(memory_pool) CColRefSet(memory_pool);
			}

			// candidate set of xforms
			virtual
			CXformSet *PxfsCandidates(IMemoryPool *memory_pool) const;

			// derive statistics
			virtual
			IStatistics *PstatsDerive
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				StatsArray *stats_ctxt
				)
				const;

			// stat promise
			virtual
			EStatPromise Esp(CExpressionHandle &) const
			{
				return CLogical::EspHigh;
			}

			// origin operator id -- gpos::ulong_max if operator was not generated via a transformation
			ULONG UlOriginOpId() const
			{
				return m_ulOriginOpId;
			}

			// debug print
			virtual
			IOstream &OsPrint(IOstream &) const;

			// conversion
			static
			CLogicalDynamicBitmapTableGet *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalDynamicBitmapTableGet == pop->Eopid());

				return dynamic_cast<CLogicalDynamicBitmapTableGet *>(pop);
			}

	};  // class CLogicalDynamicBitmapTableGet
}

#endif // !GPOPT_CLogicalDynamicBitmapTableGet_H

// EOF
