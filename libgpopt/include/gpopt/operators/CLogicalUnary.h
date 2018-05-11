//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalUnary.h
//
//	@doc:
//		Base class of logical unary operators
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalUnary_H
#define GPOS_CLogicalUnary_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogical.h"

#include "naucrates/base/IDatum.h"

namespace gpopt
{
	using namespace gpnaucrates;

	// fwd declaration
	class CColRefSet;

	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalUnary
	//
	//	@doc:
	//		Base class of logical unary operators
	//
	//---------------------------------------------------------------------------
	class CLogicalUnary : public CLogical
	{
		private:

			// private copy ctor
			CLogicalUnary(const CLogicalUnary &);

		protected:

			// derive statistics for projection operators
			IStatistics *PstatsDeriveProject
							(
							IMemoryPool *memory_pool,
							CExpressionHandle &exprhdl,
							HMUlDatum *phmuldatum = NULL
							)
							const;

		public:

			// ctor
			explicit
			CLogicalUnary
				(
				IMemoryPool *memory_pool
				)
				:
				CLogical(memory_pool)
			{}

			// dtor
			virtual
			~CLogicalUnary()
			{}

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
			COperator *PopCopyWithRemappedColumns
						(
						IMemoryPool *, //memory_pool,
						UlongColRefHashMap *, //colref_mapping,
						BOOL //must_exist
						)
			{
				return PopCopyDefault();
			}

			//-------------------------------------------------------------------------------------
			// Derived Relational Properties
			//-------------------------------------------------------------------------------------

			// derive not nullable output columns
			virtual
			CColRefSet *PcrsDeriveNotNull
				(
				IMemoryPool *,// memory_pool
				CExpressionHandle &exprhdl
				)
				const
			{
				// TODO,  03/18/2012, derive nullability of columns computed by scalar child
				return PcrsDeriveNotNullPassThruOuter(exprhdl);
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
			
			// derive function properties
			virtual
			CFunctionProp *PfpDerive
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl
				)
				const
			{
				return PfpDeriveFromScalar(memory_pool, exprhdl, 1 /*ulScalarIndex*/);
			}

			//-------------------------------------------------------------------------------------
			// Derived Stats
			//-------------------------------------------------------------------------------------

			// promise level for stat derivation
			virtual
			EStatPromise Esp(CExpressionHandle &exprhdl) const;

			//-------------------------------------------------------------------------------------
			// Required Relational Properties
			//-------------------------------------------------------------------------------------

			// compute required stat columns of the n-th child
			virtual
			CColRefSet *PcrsStat
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &exprhdl,
				CColRefSet *pcrsInput,
				ULONG child_index
				)
				const
			{
				return PcrsReqdChildStats(memory_pool, exprhdl, pcrsInput, exprhdl.GetDrvdScalarProps(1)->PcrsUsed(), child_index);
			}

	}; // class CLogicalUnary

}

#endif // !GPOS_CLogicalUnary_H

// EOF
