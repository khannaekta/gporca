//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Software, Inc.
//
//	Left Outer Index Apply operator;
//	a variant of outer apply that captures the need to implement a
//	correlated-execution strategy on the physical side, where the inner
//	side is an index scan with parameters from outer side
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalLeftOuterIndexApply_H
#define GPOPT_CLogicalLeftOuterIndexApply_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalIndexApply.h"
#include "gpopt/operators/CExpressionHandle.h"

namespace gpopt
{

	class CLogicalLeftOuterIndexApply : public CLogicalIndexApply
	{

		private:

			// private copy ctor
			CLogicalLeftOuterIndexApply(const CLogicalLeftOuterIndexApply &);

		public:

			// ctor
			CLogicalLeftOuterIndexApply(IMemoryPool *pmp,  DrgPcr *pdrgpcrOuterRefs)
			: CLogicalIndexApply(pmp, pdrgpcrOuterRefs, true /*fOuterJoin*/)
			{}

			// ctor for patterns
			explicit
			CLogicalLeftOuterIndexApply(IMemoryPool *pmp) : CLogicalIndexApply(pmp)
			{}

			// dtor
			virtual
			~CLogicalLeftOuterIndexApply()
			{}

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalLeftOuterIndexApply;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CLogicalLeftOuterIndexApply";
			}

			virtual
			COperator *PopCopyWithRemappedColumns
				(
				IMemoryPool *pmp,
				HMUlCr *phmulcr,
				BOOL fMustExist
				)
			{
				DrgPcr *pdrgpcr = CUtils::PdrgpcrRemap(pmp, m_pdrgpcrOuterRefs, phmulcr, fMustExist);

				return GPOS_NEW(pmp) CLogicalLeftOuterIndexApply(pmp, pdrgpcr);
			}

			// conversion function
			static
			CLogicalLeftOuterIndexApply *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalLeftOuterIndexApply == pop->Eopid());

				return dynamic_cast<CLogicalLeftOuterIndexApply*>(pop);
			}

	}; // class CLogicalLeftOuterIndexApply

}


#endif // !GPOPT_CLogicalLeftOuterIndexApply_H

// EOF
