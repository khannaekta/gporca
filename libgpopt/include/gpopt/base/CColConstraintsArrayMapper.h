//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#ifndef GPOPT_CColConstraintsArrayMapper_H
#define GPOPT_CColConstraintsArrayMapper_H

#include "gpos/memory/IMemoryPool.h"

#include "gpopt/base/CConstraint.h"
#include "gpopt/base/IColConstraintsMapper.h"

namespace gpopt
{
	class CColConstraintsArrayMapper : public IColConstraintsMapper
	{
		public:
			CColConstraintsArrayMapper
				(
				gpos::IMemoryPool *mp,
				ConstraintArray *pdrgpcnstr
				);
			virtual ConstraintArray *PdrgPcnstrLookup(CColRef *colref);

			virtual ~CColConstraintsArrayMapper();

		private:
			gpos::IMemoryPool *m_mp;
			ConstraintArray *m_pdrgpcnstr;
	};
}

#endif //GPOPT_CColConstraintsArrayMapper_H
