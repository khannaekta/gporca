//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CScalarSubqueryAny.cpp
//
//	@doc:
//		Implementation of scalar subquery ANY operator
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpopt/operators/CScalarSubqueryAny.h"
#include "gpopt/base/CUtils.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryAny::CScalarSubqueryAny
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CScalarSubqueryAny::CScalarSubqueryAny
	(
	IMemoryPool *memory_pool,
	IMDId *scalar_op_mdid,
	const CWStringConst *pstrScalarOp,
	const CColRef *colref
	)
	:
	CScalarSubqueryQuantified(memory_pool, scalar_op_mdid, pstrScalarOp, colref)
{}

//---------------------------------------------------------------------------
//	@function:
//		CScalarSubqueryAny::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CScalarSubqueryAny::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	CColRef *colref = CUtils::PcrRemap(Pcr(), colref_mapping, must_exist);

	IMDId *scalar_op_mdid = MdIdOp();
	scalar_op_mdid->AddRef();

	CWStringConst *pstrScalarOp = GPOS_NEW(memory_pool) CWStringConst(memory_pool, PstrOp()->GetBuffer());

	return GPOS_NEW(memory_pool) CScalarSubqueryAny(memory_pool, scalar_op_mdid, pstrScalarOp, colref);
}

// EOF
