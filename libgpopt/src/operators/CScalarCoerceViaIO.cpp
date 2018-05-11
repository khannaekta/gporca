//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CScalarCoerceViaIO.cpp
//
//	@doc:
//		Implementation of scalar CoerceViaIO operators
//
//	@owner:
//
//	@test:
//
//
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/operators/CScalarCoerceViaIO.h"

using namespace gpopt;
using namespace gpmd;


//---------------------------------------------------------------------------
//	@function:
//		CScalarCoerceViaIO::CScalarCoerceViaIO
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarCoerceViaIO::CScalarCoerceViaIO
	(
	IMemoryPool *memory_pool,
	IMDId *mdid_type,
	INT type_modifier,
	ECoercionForm ecf,
	INT location
	)
	:
	CScalarCoerceBase(memory_pool, mdid_type, type_modifier, ecf, location)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CScalarCoerceViaIO::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CScalarCoerceViaIO::Matches
	(
	COperator *pop
	)
	const
{
	if (pop->Eopid() == Eopid())
	{
		CScalarCoerceViaIO *popCoerce = CScalarCoerceViaIO::PopConvert(pop);

		return popCoerce->MDIdType()->Equals(MDIdType()) &&
				popCoerce->TypeModifier() == TypeModifier() &&
				popCoerce->Ecf() == Ecf() &&
				popCoerce->Location() == Location();
	}

	return false;
}


// EOF

