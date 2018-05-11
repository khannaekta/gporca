//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#include "gpopt/base/CColConstraintsArrayMapper.h"
#include "gpopt/base/CConstraint.h"

using namespace gpopt;

DrgPcnstr *
CColConstraintsArrayMapper::PdrgPcnstrLookup
	(
		CColRef *colref
	)
{
	const BOOL fExclusive = true;
	return CConstraint::PdrgpcnstrOnColumn(m_memory_pool, m_pdrgpcnstr, colref, fExclusive);
}

CColConstraintsArrayMapper::CColConstraintsArrayMapper
	(
		gpos::IMemoryPool *memory_pool,
		DrgPcnstr *pdrgpcnstr
	) :
	m_memory_pool(memory_pool),
	m_pdrgpcnstr(pdrgpcnstr)
{
}

CColConstraintsArrayMapper::~CColConstraintsArrayMapper()
{
	m_pdrgpcnstr->Release();
}
