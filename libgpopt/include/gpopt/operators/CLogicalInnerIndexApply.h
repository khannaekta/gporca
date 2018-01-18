//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Software, Inc.
//
//	Inner Index Apply operator;
//	a variant of inner apply that captures the need to implement a
//	correlated-execution strategy on the physical side, where the inner
//	side is an index scan with parameters from outer side
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalInnerIndexApply_H
#define GPOPT_CLogicalInnerIndexApply_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalIndexApply.h"
#include "gpopt/operators/CExpressionHandle.h"

namespace gpopt
{

	class CLogicalInnerIndexApply : public CLogicalIndexApply
	{

		private:

			// private copy ctor
			CLogicalInnerIndexApply(const CLogicalInnerIndexApply &);

		public:

			// ctor
			CLogicalInnerIndexApply(IMemoryPool *pmp,  DrgPcr *pdrgpcrOuterRefs)
			: CLogicalIndexApply(pmp, pdrgpcrOuterRefs, false /*fOuterJoin*/)
			{}

			// ctor for patterns
			explicit
			CLogicalInnerIndexApply(IMemoryPool *pmp) : CLogicalIndexApply(pmp)
			{}

			// dtor
			virtual
			~CLogicalInnerIndexApply()
			{}

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalInnerIndexApply;
			}

			// return a string for operator name
			virtual
			const CHAR *SzId() const
			{
				return "CLogicalInnerIndexApply";
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

				return GPOS_NEW(pmp) CLogicalInnerIndexApply(pmp, pdrgpcr);
			}

			// conversion function
			static
			CLogicalInnerIndexApply *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalInnerIndexApply == pop->Eopid());

				return dynamic_cast<CLogicalInnerIndexApply*>(pop);
			}

	}; // class CLogicalInnerIndexApply

}


#endif // !GPOPT_CLogicalInnerIndexApply_H

// EOF
