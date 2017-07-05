//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CXformImplementValuesGet.h
//
//	@doc:
//		Implement logical values get with a physical values get
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementValuesGet_H
#define GPOPT_CXformImplementValuesGet_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementation.h"

namespace gpopt
{
	using namespace gpos;
	
	// Implement values get
	class CXformImplementValuesGet : public CXformImplementation
	{

		private:

			// private copy ctor
			CXformImplementValuesGet(const CXformImplementValuesGet &);

		public:
		
			// ctor
			explicit
			CXformImplementValuesGet(IMemoryPool *);

			// dtor
			virtual 
			~CXformImplementValuesGet() {}

			// ident accessors
			virtual
			EXformId Exfid() const
			{
				return ExfImplementValuesGet;
			}
			
			// return a string for xform name
			virtual 
			const CHAR *SzId() const
			{
				return "CXformImplementValuesGet";
			}
			
			// compute xform promise for a given expression handle
			virtual
			EXformPromise Exfp
				(
				CExpressionHandle & // exprhdl
				)
				const
			{
				return CXform::ExfpHigh;
			}

			// actual transform
			void Transform
				(
				CXformContext *pxfctxt,
				CXformResult *pxfres,
				CExpression *pexpr
				) 
				const;
		
	}; // class CXformImplementValuesGet

}


#endif // !GPOPT_CXformImplementValuesGet_H

// EOF
