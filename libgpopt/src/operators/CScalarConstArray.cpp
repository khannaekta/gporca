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

//---------------------------------------------------------------------------
//	@function:
//		CScalaCScalarConstArrayrArray::UlHash
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CScalarConstArray::UlHash() const
{
	BOOL fMultiDimensional = FMultiDimensional();
	ULONG ulHash = gpos::UlCombineHashes
					(
					UlCombineHashes(PmdidElem()->UlHash(), PmdidArray()->UlHash()),
					gpos::UlHash<BOOL>(&fMultiDimensional)
					);
	for (ULONG ul = 0; ul < UlSize(); ul++)
	{
		ulHash = gpos::UlCombineHashes(ulHash, (*m_consts)[ul]->UlHash());
	}
	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CScalarConstArray::FMatch
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarConstArray::FMatch
	(
	COperator *pop
	)
	const
{
	if (pop->Eopid() == Eopid())
	{
		CScalarConstArray *popArray = CScalarConstArray::PopConvert(pop);

		// match if components are identical
		if (popArray->FMultiDimensional() == FMultiDimensional() &&
				PmdidElem()->FEquals(popArray->PmdidElem()) &&
				PmdidArray()->FEquals(popArray->PmdidArray()) &&
				UlSize() == popArray->UlSize())
		{
			for (ULONG ul = 0; ul < UlSize(); ul++)
			{
				CScalarConst *popConst1 = (*m_consts)[ul];
				CScalarConst *popConst2 = popArray->PopConstAt(ul);
				if (!popConst1->FMatch(popConst2))
				{
					return false;
				}
			}
			return true;
		}
	}

	return false;
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
