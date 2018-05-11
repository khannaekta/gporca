//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CPhysicalTableScan.h
//
//	@doc:
//		Table scan operator
//---------------------------------------------------------------------------
#ifndef GPOPT_CPhysicalTableScan_H
#define GPOPT_CPhysicalTableScan_H

#include "gpos/base.h"
#include "gpopt/operators/CPhysicalScan.h"

namespace gpopt
{
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CPhysicalTableScan
	//
	//	@doc:
	//		Table scan operator
	//
	//---------------------------------------------------------------------------
	class CPhysicalTableScan : public CPhysicalScan
	{

		private:

			// private copy ctor
			CPhysicalTableScan(const CPhysicalTableScan&);

		public:
		
			// ctors
			explicit
			CPhysicalTableScan(IMemoryPool *memory_pool);
			CPhysicalTableScan(IMemoryPool *, const CName *, CTableDescriptor *, DrgPcr *);

			// ident accessors
			virtual 
			EOperatorId Eopid() const
			{
				return EopPhysicalTableScan;
			}
			
			// return a string for operator name
			virtual 
			const CHAR *SzId() const
			{
				return "CPhysicalTableScan";
			}
			
			// operator specific hash function
			virtual ULONG HashValue() const;
			
			// match function
			BOOL Matches(COperator *) const;
		
			// derive partition index map
			virtual
			CPartIndexMap *PpimDerive
				(
				IMemoryPool *memory_pool,
				CExpressionHandle &, // exprhdl
				CDrvdPropCtxt * //pdpctxt
				)
				const
			{
				return GPOS_NEW(memory_pool) CPartIndexMap(memory_pool);
			}

			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------
			//-------------------------------------------------------------------------------------

			// debug print
			virtual 
			IOstream &OsPrint(IOstream &) const;


			// conversion function
			static
			CPhysicalTableScan *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopPhysicalTableScan == pop->Eopid() ||
							EopPhysicalExternalScan == pop->Eopid());

				return reinterpret_cast<CPhysicalTableScan*>(pop);
			}

			// statistics derivation during costing
			virtual
			IStatistics *PstatsDerive
				(
				IMemoryPool *, // memory_pool
				CExpressionHandle &, // exprhdl
				CReqdPropPlan *, // prpplan
				StatsArray * //stats_ctxt
				)
				const
			{
				GPOS_ASSERT(!"stats derivation during costing for table scan is invalid");

				return NULL;
			}


	}; // class CPhysicalTableScan

}

#endif // !GPOPT_CPhysicalTableScan_H

// EOF
