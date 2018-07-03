//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalDifferenceAll.h
//
//	@doc:
//		Logical Difference all operator (Difference all does not remove
//		duplicates from the left child)
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalDifferenceAll_H
#define GPOPT_CLogicalDifferenceAll_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalSetOp.h"

namespace gpopt
{
	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalDifferenceAll
	//
	//	@doc:
	//		Difference all operators
	//
	//---------------------------------------------------------------------------
	class CLogicalDifferenceAll : public CLogicalSetOp
	{

		private:

			// private copy ctor
			CLogicalDifferenceAll(const CLogicalDifferenceAll &);

		public:

			// ctor
			explicit
			CLogicalDifferenceAll(IMemoryPool *memory_pool);

			CLogicalDifferenceAll
				(
				IMemoryPool *memory_pool,
				ColRefArray *pdrgpcrOutput,
				ColRefArrays *pdrgpdrgpcrInput
				);

			// dtor
			virtual
			~CLogicalDifferenceAll();

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalDifferenceAll;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CLogicalDifferenceAll";
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

			// derive constraint property
			virtual
			CPropConstraint *PpcDeriveConstraint
				(
				IMemoryPool *, //memory_pool,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PpcDeriveConstraintPassThru(exprhdl, 0 /*ulChild*/);
			}

			//-------------------------------------------------------------------------------------
			// Transformations
			//-------------------------------------------------------------------------------------

			// candidate set of xforms
			CXformSet *PxfsCandidates(IMemoryPool *memory_pool) const;

			// stat promise
			virtual
			EStatPromise Esp
				(
				CExpressionHandle &  // exprhdl
				)
				const
			{
				return CLogical::EspLow;
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
			CLogicalDifferenceAll *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalDifferenceAll == pop->Eopid());

				return reinterpret_cast<CLogicalDifferenceAll*>(pop);
			}

	}; // class CLogicalDifferenceAll

}

#endif // !GPOPT_CLogicalDifferenceAll_H

// EOF
