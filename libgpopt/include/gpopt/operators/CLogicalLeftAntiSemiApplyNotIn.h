//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2013 EMC Corp.
//
//	@filename:
//		CLogicalLeftAntiSemiApplyNotIn.h
//
//	@doc:
//		Logical Left Anti Semi Apply operator used in NOT IN/ALL subqueries
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftAntiSemiApplyNotIn_H
#define GPOPT_CLogicalLeftAntiSemiApplyNotIn_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftAntiSemiApply.h"
#include "gpopt/operators/CExpressionHandle.h"

namespace gpopt
{


	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalLeftAntiSemiApplyNotIn
	//
	//	@doc:
	//		Logical Apply operator used in NOT IN/ALL subqueries
	//
	//---------------------------------------------------------------------------
	class CLogicalLeftAntiSemiApplyNotIn : public CLogicalLeftAntiSemiApply
	{

		private:

			// private copy ctor
			CLogicalLeftAntiSemiApplyNotIn(const CLogicalLeftAntiSemiApplyNotIn &);

		public:

			// ctor
			explicit
			CLogicalLeftAntiSemiApplyNotIn
				(
				IMemoryPool *memory_pool
				)
				:
				CLogicalLeftAntiSemiApply(memory_pool)
			{}

			// ctor
			CLogicalLeftAntiSemiApplyNotIn
				(
				IMemoryPool *memory_pool,
				DrgPcr *pdrgpcrInner,
				EOperatorId eopidOriginSubq
				)
				:
				CLogicalLeftAntiSemiApply(memory_pool, pdrgpcrInner, eopidOriginSubq)
			{}

			// dtor
			virtual
			~CLogicalLeftAntiSemiApplyNotIn()
			{}

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalLeftAntiSemiApplyNotIn;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CLogicalLeftAntiSemiApplyNotIn";
			}

			//-------------------------------------------------------------------------------------
			// Transformations
			//-------------------------------------------------------------------------------------

			// candidate set of xforms
			virtual
			CXformSet *PxfsCandidates(IMemoryPool *memory_pool) const;

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns(IMemoryPool *memory_pool, UlongColRefHashMap *colref_mapping, BOOL must_exist);

			// conversion function
			static
			CLogicalLeftAntiSemiApplyNotIn *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalLeftAntiSemiApplyNotIn == pop->Eopid());

				return dynamic_cast<CLogicalLeftAntiSemiApplyNotIn*>(pop);
			}

	}; // class CLogicalLeftAntiSemiApplyNotIn

}


#endif // !GPOPT_CLogicalLeftAntiSemiApplyNotIn_H

// EOF
