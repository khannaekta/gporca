//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CLogicalInnerJoin.h
//
//	@doc:
//		Inner join operator
//---------------------------------------------------------------------------
#ifndef GPOS_CLogicalInnerJoin_H
#define GPOS_CLogicalInnerJoin_H

#include "gpos/base.h"
#include "gpopt/operators/CLogicalJoin.h"

namespace gpopt
{
	// fwd declaration
	class CColRefSet;
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalInnerJoin
	//
	//	@doc:
	//		Inner join operator
	//
	//---------------------------------------------------------------------------
	class CLogicalInnerJoin : public CLogicalJoin
	{
		private:

			// private copy ctor
			CLogicalInnerJoin(const CLogicalInnerJoin &);

		public:

			// ctor
			explicit
			CLogicalInnerJoin(IMemoryPool *memory_pool);

			// dtor
			virtual ~CLogicalInnerJoin() 
			{}


			// ident accessors
			virtual 
			EOperatorId Eopid() const
			{
				return EopLogicalInnerJoin;
			}
			
			// return a string for operator name
			virtual 
			const CHAR *SzId() const
			{
				return "CLogicalInnerJoin";
			}

			//-------------------------------------------------------------------------------------
			// Derived Relational Properties
			//-------------------------------------------------------------------------------------

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

			//-------------------------------------------------------------------------------------
			// Transformations
			//-------------------------------------------------------------------------------------

			// candidate set of xforms
			CXformSet *PxfsCandidates(IMemoryPool *memory_pool) const;

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// conversion function
			static
			CLogicalInnerJoin *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalInnerJoin == pop->Eopid());
				
				return dynamic_cast<CLogicalInnerJoin*>(pop);
			}

			// determine if an innerJoin group expression has
			// less conjuncts than another
			static
			BOOL FFewerConj
				(
				IMemoryPool *memory_pool,
				CGroupExpression *pgexprFst,
				CGroupExpression *pgexprSnd
				);


	}; // class CLogicalInnerJoin

}


#endif // !GPOS_CLogicalInnerJoin_H

// EOF
