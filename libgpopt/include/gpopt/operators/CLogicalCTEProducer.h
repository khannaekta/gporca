//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalCTEProducer.h
//
//	@doc:
//		Logical CTE producer operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalCTEProducer_H
#define GPOPT_CLogicalCTEProducer_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogical.h"

namespace gpopt
{
	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalCTEProducer
	//
	//	@doc:
	//		CTE producer operator
	//
	//---------------------------------------------------------------------------
	class CLogicalCTEProducer : public CLogical
	{
		private:

			// cte identifier
			ULONG m_id;

			// cte columns
			ColRefArray *m_pdrgpcr;

			// output columns, same as cte columns but in CColRefSet
			CColRefSet *m_pcrsOutput;

			// private copy ctor
			CLogicalCTEProducer(const CLogicalCTEProducer &);

		public:

			// ctor
			explicit
			CLogicalCTEProducer(IMemoryPool *memory_pool);

			// ctor
			CLogicalCTEProducer(IMemoryPool *memory_pool, ULONG id, ColRefArray *colref_array);

			// dtor
			virtual
			~CLogicalCTEProducer();

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalCTEProducer;
			}

			virtual
			const CHAR *SzId() const
			{
				return "CLogicalCTEProducer";
			}

			// cte identifier
			ULONG UlCTEId() const
			{
				return m_id;
			}

			// cte columns
			ColRefArray *Pdrgpcr() const
			{
				return m_pdrgpcr;
			}

			// cte columns in CColRefSet
			CColRefSet *pcrsOutput() const
			{
				return m_pcrsOutput;
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
				return false;
			}

			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns(IMemoryPool *memory_pool, UlongColRefHashMap *colref_mapping, BOOL must_exist);

			//-------------------------------------------------------------------------------------
			// Derived Relational Properties
			//-------------------------------------------------------------------------------------

			// derive output columns
			virtual
			CColRefSet *PcrsDeriveOutput(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);

			// dervive keys
			virtual
			CKeyCollection *PkcDeriveKeys(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			// derive max card
			virtual
			CMaxCard Maxcard(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			// derive not nullable output columns
			virtual
			CColRefSet *PcrsDeriveNotNull(IMemoryPool *memory_pool,	CExpressionHandle &exprhdl)	const;

			// derive constraint property
			virtual
			CPropConstraint *PpcDeriveConstraint
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PpcDeriveConstraintRestrict(memory_pool, exprhdl, m_pcrsOutput);
			}

			// derive partition consumer info
			virtual
			CPartInfo *PpartinfoDerive
				(
				IMemoryPool *, // memory_pool,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PpartinfoPassThruOuter(exprhdl);
			}

			// compute required stats columns of the n-th child
			virtual
			CColRefSet *PcrsStat
				(
				IMemoryPool *,// memory_pool
				CExpressionHandle &,// exprhdl
				CColRefSet *pcrsInput,
				ULONG // child_index
				)
				const
			{
				return PcrsStatsPassThru(pcrsInput);
			}

			// derive statistics
			virtual
			IStatistics *PstatsDerive
						(
						IMemoryPool *, //memory_pool,
						CExpressionHandle &exprhdl,
						StatsArray * //stats_ctxt
						)
						const
			{
				return PstatsPassThruOuter(exprhdl);
			}

			// stat promise
			virtual
			EStatPromise Esp(CExpressionHandle &) const
			{
				return CLogical::EspHigh;
			}

			//-------------------------------------------------------------------------------------
			// Transformations
			//-------------------------------------------------------------------------------------

			// candidate set of xforms
			virtual
			CXformSet *PxfsCandidates(IMemoryPool *memory_pool) const;

			//-------------------------------------------------------------------------------------

			// conversion function
			static
			CLogicalCTEProducer *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalCTEProducer == pop->Eopid());

				return dynamic_cast<CLogicalCTEProducer*>(pop);
			}

			// debug print
			virtual
			IOstream &OsPrint(IOstream &) const;

	}; // class CLogicalCTEProducer

}

#endif // !GPOPT_CLogicalCTEProducer_H

// EOF
