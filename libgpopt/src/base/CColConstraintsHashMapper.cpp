//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#include "gpos/common/CAutoRef.h"
#include "gpopt/base/CColConstraintsHashMapper.h"

using namespace gpopt;

ConstraintArray *
CColConstraintsHashMapper::PdrgPcnstrLookup
	(
		CColRef *colref
	)
{
	ConstraintArray *pdrgpcnstrCol = m_phmColConstr->Find(colref);
	pdrgpcnstrCol->AddRef();
	return pdrgpcnstrCol;
}

// mapping between columns and single column constraints in array of constraints
static
ColRefToConstraintArrayMap *
PhmcolconstrSingleColConstr
	(
		IMemoryPool *memory_pool,
		ConstraintArray *drgPcnstr
	)
{
	CAutoRef<ConstraintArray> arpdrgpcnstr(drgPcnstr);
	ColRefToConstraintArrayMap *phmcolconstr = GPOS_NEW(memory_pool) ColRefToConstraintArrayMap(memory_pool);

	const ULONG length = arpdrgpcnstr->Size();

	for (ULONG ul = 0; ul < length; ul++)
	{
		CConstraint *pcnstrChild = (*arpdrgpcnstr)[ul];
		CColRefSet *pcrs = pcnstrChild->PcrsUsed();

		if (1 == pcrs->Size())
		{
			CColRef *colref = pcrs->PcrFirst();
			ConstraintArray *pcnstrMapped = phmcolconstr->Find(colref);
			if (NULL == pcnstrMapped)
			{
				pcnstrMapped = GPOS_NEW(memory_pool) ConstraintArray(memory_pool);
				phmcolconstr->Insert(colref, pcnstrMapped);
			}
			pcnstrChild->AddRef();
			pcnstrMapped->Append(pcnstrChild);
		}
	}

	return phmcolconstr;
}

CColConstraintsHashMapper::CColConstraintsHashMapper
	(
		IMemoryPool *memory_pool,
		ConstraintArray *pdrgpcnstr
	) :
	m_phmColConstr(PhmcolconstrSingleColConstr(memory_pool, pdrgpcnstr))
{
}

CColConstraintsHashMapper::~CColConstraintsHashMapper()
{
	m_phmColConstr->Release();
}
