//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalCTEConsumer.h
//
//	@doc:
//		Logical CTE consumer operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalCTEConsumer_H
#define GPOPT_CLogicalCTEConsumer_H

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogical.h"

namespace gpopt
{
	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalCTEConsumer
	//
	//	@doc:
	//		CTE consumer operator
	//
	//---------------------------------------------------------------------------
	class CLogicalCTEConsumer : public CLogical
	{
		private:

			// cte identifier
			ULONG m_id;

			// mapped cte columns
			ColRefArray *m_pdrgpcr;

			// inlined expression
			CExpression *m_pexprInlined;

			// map of CTE producer's output column ids to consumer's output columns
			UlongToColRefMap *m_phmulcr;

			// output columns
			CColRefSet *m_pcrsOutput;

			// create the inlined version of this consumer as well as the column mapping
			void CreateInlinedExpr(IMemoryPool *mp);

			// private copy ctor
			CLogicalCTEConsumer(const CLogicalCTEConsumer &);

		public:

			// ctor
			explicit
			CLogicalCTEConsumer(IMemoryPool *mp);

			// ctor
			CLogicalCTEConsumer(IMemoryPool *mp, ULONG id, ColRefArray *colref_array);

			// dtor
			virtual
			~CLogicalCTEConsumer();

			// ident accessors
			virtual
			EOperatorId Eopid() const
			{
				return EopLogicalCTEConsumer;
			}

			virtual
			const CHAR *SzId() const
			{
				return "CLogicalCTEConsumer";
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

			// column mapping
			UlongToColRefMap *Phmulcr() const
			{
				return m_phmulcr;
			}

			CExpression *PexprInlined() const
			{
				return m_pexprInlined;
			}

			// operator specific hash function
			virtual
			ULONG HashValue() const;

			// match function
			virtual
			BOOL Matches(COperator *pop) const;

			// sensitivity to order of inputs
			virtual
			BOOL FInputOrderSensitive() const;

			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns(IMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

			//-------------------------------------------------------------------------------------
			// Derived Relational Properties
			//-------------------------------------------------------------------------------------

			// derive output columns
			virtual
			CColRefSet *PcrsDeriveOutput(IMemoryPool *mp, CExpressionHandle &exprhdl);

			// dervive keys
			virtual
			CKeyCollection *PkcDeriveKeys(IMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive max card
			virtual
			CMaxCard Maxcard(IMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive join depth
			virtual
			ULONG JoinDepth(IMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive not nullable output columns
			virtual
			CColRefSet *PcrsDeriveNotNull(IMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive constraint property
			virtual
			CPropConstraint *PpcDeriveConstraint(IMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// derive partition consumer info
			virtual
			CPartInfo *PpartinfoDerive(IMemoryPool *mp, CExpressionHandle &exprhdl) const;

			// compute required stats columns of the n-th child
			virtual
			CColRefSet *PcrsStat
				(
				IMemoryPool *,// mp
				CExpressionHandle &,// exprhdl
				CColRefSet *, //pcrsInput,
				ULONG // child_index
				)
				const
			{
				GPOS_ASSERT(!"CLogicalCTEConsumer has no children");
				return NULL;
			}

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
				IMemoryPool *mp,
				CExpressionHandle &exprhdl,
				StatsArray *stats_ctxt
				)
				const;

			//-------------------------------------------------------------------------------------
			// Transformations
			//-------------------------------------------------------------------------------------

			// candidate set of xforms
			virtual
			CXformSet *PxfsCandidates(IMemoryPool *mp) const;

			//-------------------------------------------------------------------------------------

			// conversion function
			static
			CLogicalCTEConsumer *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalCTEConsumer == pop->Eopid());

				return dynamic_cast<CLogicalCTEConsumer*>(pop);
			}

			// debug print
			virtual
			IOstream &OsPrint(IOstream &) const;

	}; // class CLogicalCTEConsumer

}

#endif // !GPOPT_CLogicalCTEConsumer_H

// EOF
