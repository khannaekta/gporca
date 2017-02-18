//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarConstArray.h
//
//	@doc:
//		Class for scalar arrays of const.
//		This class is to reduce the optization time for large IN list.
//---------------------------------------------------------------------------
#ifndef GPOPT_CScalarConstArray_H
#define GPOPT_CScalarConstArray_H

#include "gpos/base.h"
#include "gpos/common/CRefCount.h"
#include "gpos/common/CDynamicPtrArray.h"
#include "naucrates/md/IMDId.h"
#include "gpopt/operators/CScalarConst.h"
#include "gpopt/operators/CScalarArray.h"

namespace gpopt
{

	using namespace gpos;
	using namespace gpmd;
	
	typedef CDynamicPtrArray<CScalarConst, CleanupRelease> DrgPconst;

	//---------------------------------------------------------------------------
	//	@class:
	//		CScalarConstArray
	//
	//	@doc:
	//		Scalar array for constants
	//
	//---------------------------------------------------------------------------
	class CScalarConstArray : public CScalarArray
	{
		private:
			DrgPconst *m_consts;

			// private copy ctor
			CScalarConstArray(const CScalarConstArray &);
		
		public:
		
			// ctor
			CScalarConstArray(IMemoryPool *pmp, IMDId *pmdidElem, IMDId *pmdidArray, BOOL fMultiDimensional, DrgPconst *pConsts);
			
			// dtor
			virtual 
			~CScalarConstArray();

			// ident accessors
			virtual 
			EOperatorId Eopid() const
			{
				return EopScalarConstArray;
			}
			
			// return a string for aggregate function
			virtual 
			const CHAR *SzId() const
			{
				return "CScalarConstArray";
			}

			DrgPconst *PConsts() const
			{
				return m_consts;
			}
			
			// sensitivity to order of inputs
			BOOL FInputOrderSensitive() const
			{
				return true;
			}
			
			// return a copy of the operator with remapped columns
			virtual
			COperator *PopCopyWithRemappedColumns
						(
						IMemoryPool *, //pmp,
						HMUlCr *, //phmulcr,
						BOOL //fMustExist
						)
			{
				return PopCopyDefault();
			}

			// conversion function
			static
			CScalarConstArray *PopConvert
				(
				COperator *pop
				)
			{
				GPOS_ASSERT(NULL != pop);
				GPOS_ASSERT(EopScalarConstArray == pop->Eopid());
				
				return reinterpret_cast<CScalarConstArray*>(pop);
			}
			
			// print
			IOstream &OsPrint(IOstream &os) const;

	}; // class CScalarConstArray

}


#endif // !GPOPT_CScalarConstArray_H

// EOF
