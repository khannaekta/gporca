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

IOstream &
CScalarConstArray::OsPrint(IOstream &os) const
{
	// TODO: output constant values in array
	os << "CScalarConstArray: {eleMDId: ";
	PmdidElem()->OsPrint(os);
	os << ", arrayMDId: ";
	PmdidArray()->OsPrint(os);
	if (FMultiDimensional())
	{
		os << ", multidimensional";
	}
	os << "}";
	return os;
}
