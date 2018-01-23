//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Software, Inc.
//
//	Implementing Left Outer Index Apply
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementLeftOuterIndexApply_H
#define GPOPT_CXformImplementLeftOuterIndexApply_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementIndexApply.h"

namespace gpopt
{
	using namespace gpos;

	class CXformImplementLeftOuterIndexApply : public CXformImplementIndexApply
		<CLogicalIndexApply, CPhysicalLeftOuterIndexNLJoin>
	{

		private:

			// private copy ctor
			CXformImplementLeftOuterIndexApply(const CXformImplementLeftOuterIndexApply &);

		public:

			// ctor
			explicit
			CXformImplementLeftOuterIndexApply(IMemoryPool *pmp)
			: CXformImplementIndexApply<CLogicalIndexApply, CPhysicalLeftOuterIndexNLJoin>(pmp)
			{}

			// dtor
			virtual
			~CXformImplementLeftOuterIndexApply()
			{}

			// ident accessors
			virtual
			EXformId Exfid() const
			{
				return ExfImplementLeftOuterIndexApply;
			}

			virtual
			const CHAR *SzId() const
			{
				return "CXformImplementLeftOuterIndexApply";
			}

	}; // class CXformImplementLeftOuterIndexApply

}

#endif // !GPOPT_CXformImplementLeftOuterIndexApply_H

// EOF
