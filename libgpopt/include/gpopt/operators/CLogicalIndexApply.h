//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal, Inc.
//
//	Base Index Apply operator for Inner and Outer Join;
//	a variant of inner/outer apply that captures the need to implement a
//	correlated-execution strategy on the physical side, where the inner
//	side is an index scan with parameters from outer side
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalIndexApply_H
#define GPOPT_CLogicalIndexApply_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalApply.h"
#include "gpopt/operators/CExpressionHandle.h"

namespace gpopt
{

	class CLogicalIndexApply : public CLogicalApply
	{
		private:

			// private copy ctor
			CLogicalIndexApply(const CLogicalIndexApply &);

		protected:

			// columns used from Apply's outer child used by index in Apply's inner child
			DrgPcr *m_pdrgpcrOuterRefs;

			// is this an outer join?
			BOOL m_fOuterJoin;

		public:

			// ctor
			CLogicalIndexApply(IMemoryPool *memory_pool,  DrgPcr *pdrgpcrOuterRefs, BOOL fOuterJoin);

			// ctor for patterns
			explicit
			CLogicalIndexApply(IMemoryPool *memory_pool);

			// dtor
			virtual
			~CLogicalIndexApply();

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalIndexApply;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CLogicalIndexApply";
			}

			// outer column references accessor
			DrgPcr *PdrgPcrOuterRefs() const
			{
				return m_pdrgpcrOuterRefs;
			}

			// outer column references accessor
			BOOL FouterJoin() const
			{
				return m_fOuterJoin;
			}

			//-------------------------------------------------------------------------------------
			// Derived Relational Properties
			//-------------------------------------------------------------------------------------

			// derive output columns
			virtual
			CColRefSet *PcrsDeriveOutput
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl
				)
			{
				GPOS_ASSERT(3 == exprhdl.Arity());

				return PcrsDeriveOutputCombineLogical(memory_pool, exprhdl);
			}

			// derive not nullable columns
			virtual
			CColRefSet *PcrsDeriveNotNull
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PcrsDeriveNotNullCombineLogical(memory_pool, exprhdl);
			}

			// derive max card
			virtual
			CMaxCard Maxcard(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			// derive constraint property
			virtual
			CPropConstraint *PpcDeriveConstraint
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PpcDeriveConstraintFromPredicates(memory_pool, exprhdl);
			}

			// applicable transformations
			virtual
			CXformSet *PxfsCandidates(IMemoryPool *memory_pool) const;

			// match function
			virtual
			BOOL Matches(COperator *pop) const;

			//-------------------------------------------------------------------------------------
			// Derived Stats
			//-------------------------------------------------------------------------------------

			// derive statistics
			virtual
			IStatistics *PstatsDerive(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, StatsArray *stats_ctxt) const;

			// stat promise
			virtual
			EStatPromise Esp
				(
				CExpressionHandle & // exprhdl
				)
				const
			{
				return CLogical::EspMedium;
			}

			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns(IMemoryPool *memory_pool, UlongColRefHashMap *colref_mapping, BOOL must_exist);

			// conversion function
			static
			CLogicalIndexApply *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalIndexApply == pop->Eopid());

				return dynamic_cast<CLogicalIndexApply*>(pop);
			}

	}; // class CLogicalIndexApply

}


#endif // !GPOPT_CLogicalIndexApply_H

// EOF
