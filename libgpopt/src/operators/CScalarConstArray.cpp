//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.


#include "gpopt/operators/CScalarConstArray.h"
#include "gpopt/operators/CExpression.h"

using namespace gpopt;
using namespace gpmd;

CScalarConstArray::CScalarConstArray
		(
				IMemoryPool *pmp,
				CScalarArray *array,
				DrgPexpr *pConstExpressions
		)
		:
		CScalarArray(pmp, array->PmdidElem(), array->PmdidArray(), array->FMultiDimensional()),
		m_constExpressions(pConstExpressions)
{
	GPOS_ASSERT(pConstExpressions);
}

CScalarConstArray::~CScalarConstArray()
{
	m_constExpressions->Release();
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
	os << "}";
	return os;
}
