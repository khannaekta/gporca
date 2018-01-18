//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Software, Inc.
//
//	Implementing Inner Index Apply
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformImplementInnerIndexApply_H
#define GPOPT_CXformImplementInnerIndexApply_H

#include "gpos/base.h"
#include "gpopt/xforms/CXformImplementIndexApply.h"

namespace gpopt
{
	using namespace gpos;

	class CXformImplementInnerIndexApply : public CXformImplementIndexApply
		<CLogicalInnerIndexApply, CPhysicalInnerIndexNLJoin>
	{

		private:

			// private copy ctor
			CXformImplementInnerIndexApply(const CXformImplementInnerIndexApply &);

		public:

			// ctor
			explicit
			CXformImplementInnerIndexApply(IMemoryPool *pmp)
			: CXformImplementIndexApply<CLogicalInnerIndexApply, CPhysicalInnerIndexNLJoin>(pmp)
			{}

			// dtor
			virtual
			~CXformImplementInnerIndexApply()
			{}

			// ident accessors
			virtual
			EXformId Exfid() const
			{
				return ExfImplementInnerIndexApply;
			}

			virtual
			const CHAR *SzId() const
			{
				return "CXformImplementInnerIndexApply";
			}

	}; // class CXformImplementInnerIndexApply

}

#endif // !GPOPT_CXformImplementInnerIndexApply_H

// EOF
