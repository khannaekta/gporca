//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#include "gpopt/operators/CScalarConstArray.h"
#include "gpopt/operators/CExpression.h"

using namespace gpopt;
using namespace gpmd;

CScalarConstArray::CScalarConstArray
	(
	IMemoryPool *pmp,
	IMDId *pmdidElem,
	IMDId *pmdidArray,
	BOOL fMultiDimensional,
	DrgPconst *pConsts
	)
	:
	CScalarArray(pmp, pmdidElem, pmdidArray, fMultiDimensional),
	m_consts(pConsts)
{
	GPOS_ASSERT(pConsts);
}

CScalarConstArray::~CScalarConstArray()
{
	m_consts->Release();
}

// return the number of elements in the const array
ULONG
CScalarConstArray::UlSize() const
{
	if (m_consts)
	{
		return m_consts->UlLength();
	}

	return 0;
}

// return the const element at the given position in the const array
CScalarConst *
CScalarConstArray::PopConstAt(ULONG ul)
{
	GPOS_ASSERT(ul < UlSize());
	return (*m_consts)[ul];
}

IOstream &
CScalarConstArray::OsPrint(IOstream &os) const
{
	os << "CScalarConstArray: {eleMDId: ";
	PmdidElem()->OsPrint(os);
	os << ", arrayMDId: ";
	PmdidArray()->OsPrint(os);
	if (FMultiDimensional())
	{
		os << ", multidimensional";
	}
	os << ", elements: [";
	ULONG ulSize = UlSize();
	for (ULONG ul = 0; ul < ulSize; ul++)
	{
		const CScalarConst *popConst = (*m_consts)[ul];
		popConst->Pdatum()->OsPrint(os);
		if (ul < ulSize - 1)
		{
			os << ", ";
		}
	}
	os << "]}";
	return os;
}
