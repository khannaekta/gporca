//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalIntersectAll.h
//
//	@doc:
//		Logical Intersect all operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalIntersectAll_H
#define GPOPT_CLogicalIntersectAll_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalSetOp.h"

namespace gpopt
{
	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalIntersectAll
	//
	//	@doc:
	//		Intersect all operators
	//
	//---------------------------------------------------------------------------
	class CLogicalIntersectAll : public CLogicalSetOp
	{

		private:

			// private copy ctor
			CLogicalIntersectAll(const CLogicalIntersectAll &);

		public:

			// ctor
			explicit
			CLogicalIntersectAll(IMemoryPool *memory_pool);

			CLogicalIntersectAll
				(
				IMemoryPool *memory_pool,
				DrgPcr *pdrgpcrOutput,
				DrgDrgPcr *pdrgpdrgpcrInput
				);

			// dtor
			virtual
			~CLogicalIntersectAll();

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalIntersectAll;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CLogicalIntersectAll";
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
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PpcDeriveConstraintIntersectUnion(memory_pool, exprhdl, true /*fIntersect*/);
			}

			//-------------------------------------------------------------------------------------
			// Transformations
			//-------------------------------------------------------------------------------------

			// candidate set of xforms
			CXformSet *PxfsCandidates(IMemoryPool *memory_pool) const;

			//-------------------------------------------------------------------------------------
			// Derived Stats
			//-------------------------------------------------------------------------------------

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

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			
			// conversion function
			static
			CLogicalIntersectAll *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalIntersectAll == pop->Eopid());

				return reinterpret_cast<CLogicalIntersectAll*>(pop);
			}

			// derive statistics
			static
			IStatistics *PstatsDerive
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				DrgDrgPcr *pdrgpdrgpcrInput,
				DrgPcrs *output_colrefsets
				);

	}; // class CLogicalIntersectAll

}

#endif // !GPOPT_CLogicalIntersectAll_H

// EOF
