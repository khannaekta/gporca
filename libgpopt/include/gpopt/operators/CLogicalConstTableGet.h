//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalConstTableGet.h
//
//	@doc:
//		Constant table accessor
//---------------------------------------------------------------------------
#ifndef GPOPT_CLogicalConstTableGet_H
#define GPOPT_CLogicalConstTableGet_H

#include "gpos/base.h"
#include "gpopt/operators/CLogical.h"

namespace gpopt
{
	// dynamic array of datum arrays -- array owns elements
	typedef CDynamicPtrArray<DrgPdatum, CleanupRelease> DrgPdrgPdatum;

	//---------------------------------------------------------------------------
	//	@class:
	//		CLogicalConstTableGet
	//
	//	@doc:
	//		Constant table accessor
	//
	//---------------------------------------------------------------------------
	class CLogicalConstTableGet : public CLogical
	{

		private:
			// array of column descriptors: the schema of the const table
			DrgPcoldesc *m_pdrgpcoldesc;
		
			// array of datum arrays
			DrgPdrgPdatum *m_pdrgpdrgpdatum;
			
			// output columns
			DrgPcr *m_pdrgpcrOutput;
			
			// private copy ctor
			CLogicalConstTableGet(const CLogicalConstTableGet &);
			
			// construct column descriptors from column references
			DrgPcoldesc *PdrgpcoldescMapping(IMemoryPool *memory_pool, DrgPcr *colref_array)	const;

		public:
		
			// ctors
			explicit
			CLogicalConstTableGet(IMemoryPool *memory_pool);

			CLogicalConstTableGet
				(
				IMemoryPool *memory_pool,
				DrgPcoldesc *pdrgpcoldesc,
				DrgPdrgPdatum *pdrgpdrgpdatum
				);

			CLogicalConstTableGet
				(
				IMemoryPool *memory_pool,
				DrgPcr *pdrgpcrOutput,
				DrgPdrgPdatum *pdrgpdrgpdatum
				);

			// dtor
			virtual 
			~CLogicalConstTableGet();

			// ident accessors
			virtual 
			EOperatorId Eopid() const
			{
				return EopLogicalConstTableGet;
			}
			
			// return a string for operator name
			virtual 
			const CHAR *SzId() const
			{
				return "CLogicalConstTableGet";
			}
			
			// col descr accessor
			DrgPcoldesc *Pdrgpcoldesc() const
			{
				return m_pdrgpcoldesc;
			}
			
			// const table values accessor
			DrgPdrgPdatum *Pdrgpdrgpdatum () const
			{
				return m_pdrgpdrgpdatum;
			}
			
			// accessors
			DrgPcr *PdrgpcrOutput() const
			{
				return m_pdrgpcrOutput;
			}

			// sensitivity to order of inputs
			BOOL FInputOrderSensitive() const;

			// operator specific hash function
			virtual
			ULONG HashValue() const;

			// match function
			virtual
			BOOL Matches(COperator *pop) const;
			
			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns(IMemoryPool *memory_pool, UlongColRefHashMap *colref_mapping, BOOL must_exist);

			//-------------------------------------------------------------------------------------
			// Derived Relational Properties
			//-------------------------------------------------------------------------------------

			// derive output columns
			virtual
			CColRefSet *PcrsDeriveOutput(IMemoryPool *, CExpressionHandle &);
				
			// derive max card
			virtual
			CMaxCard Maxcard(IMemoryPool *memory_pool, CExpressionHandle &exprhdl) const;

			// derive partition consumer info
			virtual
			CPartInfo *PpartinfoDerive
				(
				IMemoryPool *memory_pool,
				CExpressionHandle & //exprhdl
				) 
				const
			{
				return GPOS_NEW(memory_pool) CPartInfo(memory_pool);
			}

			// derive constraint property
			virtual
			CPropConstraint *PpcDeriveConstraint
				(
				IMemoryPool *memory_pool,
				CExpressionHandle & // exprhdl
				)
				const
			{
				// TODO:  - Jan 11, 2013; compute constraints based on the
				// datum values in this CTG
				return GPOS_NEW(memory_pool) CPropConstraint(memory_pool, GPOS_NEW(memory_pool) DrgPcrs(memory_pool), NULL /*pcnstr*/);
			}

			//-------------------------------------------------------------------------------------
			// Required Relational Properties
			//-------------------------------------------------------------------------------------

			// compute required stat columns of the n-th child
			virtual
			CColRefSet *PcrsStat
				(
				IMemoryPool *,// memory_pool
				CExpressionHandle &,// exprhdl
				CColRefSet *,// pcrsInput
				ULONG // child_index
				)
				const
			{
				GPOS_ASSERT(!"CLogicalConstTableGet has no children");
				return NULL;
			}

			// derive statistics
			virtual
			IStatistics *PstatsDerive
						(
						IMemoryPool *memory_pool,
						CExpressionHandle &exprhdl,
						StatsArray *stats_ctxt
						)
						const;

			//-------------------------------------------------------------------------------------
			// Transformations
			//-------------------------------------------------------------------------------------

			// candidate set of xforms
			virtual
			CXformSet *PxfsCandidates(IMemoryPool *memory_pool) const;

			// stat promise
			virtual
			EStatPromise Esp(CExpressionHandle &) const
			{
				return CLogical::EspLow;
			}

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// conversion function
			static
			CLogicalConstTableGet *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopLogicalConstTableGet == pop->Eopid());
				
				return dynamic_cast<CLogicalConstTableGet*>(pop);
			}
			

			// debug print
			virtual 
			IOstream &OsPrint(IOstream &) const;

	}; // class CLogicalConstTableGet

}


#endif // !GPOPT_CLogicalConstTableGet_H

// EOF
