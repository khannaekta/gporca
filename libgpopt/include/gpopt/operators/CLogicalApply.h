//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2011 EMC Corp.
//
//	@filename:
//		CLogicalApply.h
//
//	@doc:
//		Logical Apply operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalApply_H
#define GPOPT_CLogicalApply_H

#include "gpos/base.h"
#include "gpopt/operators/CLogical.h"

namespace gpopt
{


	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalApply
	//
	//	@doc:
	//		Logical Apply operator; parent of different Apply operators used
	//		in subquery transformations
	//
	//---------------------------------------------------------------------------
	class CLogicalApply : public CLogical
	{

		private:

			// private copy ctor
			CLogicalApply(const CLogicalApply &);

		protected:

			// columns used from Apply's inner child
			ColRefArray *m_pdrgpcrInner;

			// origin subquery id
			EOperatorId m_eopidOriginSubq;

			// ctor
			explicit
			CLogicalApply(IMemoryPool *memory_pool);

			// ctor
			CLogicalApply(IMemoryPool *memory_pool, ColRefArray *pdrgpcrInner, EOperatorId eopidOriginSubq);

			// dtor
			virtual
			~CLogicalApply();

		public:

			// match function
			virtual
			BOOL Matches(COperator *pop) const;

			// sensitivity to order of inputs
			virtual
			BOOL FInputOrderSensitive() const
			{
				return true;
			}

			// inner column references accessor
			ColRefArray *PdrgPcrInner() const
			{
				return m_pdrgpcrInner;
			}

			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns
						(
						IMemoryPool *, //memory_pool,
						UlongColRefHashMap *, //colref_mapping,
						BOOL //must_exist
						)
			{
				return PopCopyDefault();
			}

			// derive partition consumer info
			virtual
			CPartInfo *PpartinfoDerive
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl
				) 
				const
			{
				return PpartinfoDeriveCombine(memory_pool, exprhdl);
			}

			// derive keys
			CKeyCollection *PkcDeriveKeys
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PkcCombineKeys(memory_pool, exprhdl);
			}

			// derive function properties
			virtual
			CFunctionProp *PfpDerive
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PfpDeriveFromScalar(memory_pool, exprhdl, 2 /*ulScalarIndex*/);
			}

			//-------------------------------------------------------------------------------------
			// Derived Stats
			//-------------------------------------------------------------------------------------

			// derive statistics
			virtual
			IStatistics *PstatsDerive
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				StatsArray *// stats_ctxt
				)
				const
			{
				// we should use stats from the corresponding Join tree if decorrelation succeeds
				return PstatsDeriveDummy(memory_pool, exprhdl, CStatistics::DefaultRelationRows);
			}

			// promise level for stat derivation
			virtual
			EStatPromise Esp
				(
				CExpressionHandle & // exprhdl
				)
				const
			{
				// whenever we can decorrelate an Apply tree, we should use the corresponding Join tree
				return EspLow;
			}

			//-------------------------------------------------------------------------------------
			// Required Relational Properties
			//-------------------------------------------------------------------------------------

			// compute required stat columns of the n-th child
			virtual
			CColRefSet *PcrsStat(IMemoryPool *memory_pool, CExpressionHandle &exprhdl, CColRefSet *pcrsInput, ULONG child_index) const;

			// return true if operator is a correlated apply
			virtual
			BOOL FCorrelated() const
			{
				return false;
			}

			// return true if operator is a left semi apply
			virtual
			BOOL FLeftSemiApply() const
			{
				return false;
			}

			// return true if operator is a left anti semi apply
			virtual
			BOOL FLeftAntiSemiApply() const
			{
				return false;
			}

			// return true if operator can select a subset of input tuples based on some predicate
			virtual
			BOOL FSelectionOp() const
			{
				return true;
			}

			// origin subquery id
			EOperatorId EopidOriginSubq() const
			{
				return m_eopidOriginSubq;
			}

			// print function
			virtual
			IOstream &OsPrint(IOstream &os) const;

			// conversion function
			static
			CLogicalApply *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(CUtils::FApply(pop));

				return dynamic_cast<CLogicalApply*>(pop);
			}

	}; // class CLogicalApply

}


#endif // !GPOPT_CLogicalApply_H

// EOF
