//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalUnionAll.h
//
//	@doc:
//		Logical Union all operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalUnionAll_H
#define GPOPT_CLogicalUnionAll_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalUnion.h"

namespace gpopt
{
	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalUnionAll
	//
	//	@doc:
	//		Union all operators
	//
	//---------------------------------------------------------------------------
	class CLogicalUnionAll : public CLogicalUnion
	{

		private:
			// if this union is needed for partial indexes then store the scan
			// id, otherwise this will be gpos::ulong_max
			ULONG m_ulScanIdPartialIndex;

			// private copy ctor
			CLogicalUnionAll(const CLogicalUnionAll &);

		public:

			// ctor
			explicit
			CLogicalUnionAll(IMemoryPool *memory_pool);

			CLogicalUnionAll
				(
				IMemoryPool *memory_pool,
				ColRefArray *pdrgpcrOutput,
				ColRefArrays *pdrgpdrgpcrInput,
				ULONG ulScanIdPartialIndex = gpos::ulong_max
				);

			// dtor
			virtual
			~CLogicalUnionAll();

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalUnionAll;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CLogicalUnionAll";
			}

			// if this union is needed for partial indexes then return the scan
			// id, otherwise return gpos::ulong_max
			ULONG UlScanIdPartialIndex() const
			{
				return m_ulScanIdPartialIndex;
			}

			// is this unionall needed for a partial index
			BOOL IsPartialIndex() const
			{
				return (gpos::ulong_max > m_ulScanIdPartialIndex);
			}

			// sensitivity to order of inputs
			BOOL FInputOrderSensitive() const
			{
				return true;
			}

			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns(IMemoryPool *memory_pool, UlongColRefHashMap *colref_mapping, BOOL must_exist);

			//-------------------------------------------------------------------------------------
			// Derived Relational Properties
			//-------------------------------------------------------------------------------------

			// derive max card
			virtual
			CMaxCard Maxcard(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			// derive key collections
			virtual
			CKeyCollection *PkcDeriveKeys(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			//-------------------------------------------------------------------------------------
			// Transformations
			//-------------------------------------------------------------------------------------

			// candidate set of xforms
			CXformSet *PxfsCandidates(IMemoryPool *memory_pool) const;

			// stat promise
			virtual
			EStatPromise Esp(CExpressionHandle &) const
			{
				return CLogical::EspHigh;
			}

			// derive statistics
			virtual
			IStatistics *PstatsDerive
						(
						IMemoryPool *memory_pool,
						CExpressionHandle &exprhdl,
						StatsArray *stats_ctxt
						)
						const;


			// conversion function
			static
			CLogicalUnionAll *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalUnionAll == pop->Eopid());

				return reinterpret_cast<CLogicalUnionAll*>(pop);
			}

			// derive statistics based on union all semantics
			static
			IStatistics *PstatsDeriveUnionAll(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);

	}; // class CLogicalUnionAll

}

#endif // !GPOPT_CLogicalUnionAll_H

// EOF
