//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2012 EMC Corp.
//
//	@filename:
//		CLogicalLeftOuterCorrelatedApply.h
//
//	@doc:
//		Logical Left Outer Correlated Apply operator;
//		a variant of left outer apply that captures the need to implement a
//		correlated-execution strategy on the physical side
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftOuterCorrelatedApply_H
#define GPOPT_CLogicalLeftOuterCorrelatedApply_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalLeftOuterApply.h"

namespace gpopt
{


	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalLeftOuterCorrelatedApply
	//
	//	@doc:
	//		Logical Apply operator used in scalar subquery transformations
	//
	//---------------------------------------------------------------------------
	class CLogicalLeftOuterCorrelatedApply : public CLogicalLeftOuterApply
	{

		private:

			// private copy ctor
			CLogicalLeftOuterCorrelatedApply(const CLogicalLeftOuterCorrelatedApply &);

		public:

			// ctor
			CLogicalLeftOuterCorrelatedApply(IMemoryPool *memory_pool, DrgPcr *pdrgpcrInner, EOperatorId eopidOriginSubq);

			// ctor for patterns
			explicit
			CLogicalLeftOuterCorrelatedApply(IMemoryPool *memory_pool);

			// dtor
			virtual
			~CLogicalLeftOuterCorrelatedApply()
			{}

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalLeftOuterCorrelatedApply;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CLogicalLeftOuterCorrelatedApply";
			}

			// match function
			virtual
			BOOL Matches(COperator *pop) const;

			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns(IMemoryPool *memory_pool, UlongColRefHashMap *colref_mapping, BOOL must_exist);

			// applicable transformations
			virtual
			CXformSet *PxfsCandidates(IMemoryPool *memory_pool) const;

			// return true if operator is a correlated apply
			virtual
			BOOL FCorrelated() const
			{
				return true;
			}

			// conversion function
			static
			CLogicalLeftOuterCorrelatedApply *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalLeftOuterCorrelatedApply == pop->Eopid());

				return dynamic_cast<CLogicalLeftOuterCorrelatedApply*>(pop);
			}

	}; // class CLogicalLeftOuterCorrelatedApply

}


#endif // !GPOPT_CLogicalLeftOuterCorrelatedApply_H

// EOF
