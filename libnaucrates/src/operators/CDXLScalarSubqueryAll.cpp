//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC, Corp.
//
//	@filename:
//		CDXLScalarSubqueryAll.cpp
//
//	@doc:
//		Implementation of subquery ALL
//---------------------------------------------------------------------------

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/operators/CDXLScalarSubqueryAll.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;
using namespace gpmd;

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubqueryAll::CDXLScalarSubqueryAll
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLScalarSubqueryAll::CDXLScalarSubqueryAll(IMemoryPool *memory_pool,
											 IMDId *scalar_op_mdid,
											 CMDName *scalar_op_mdname,
											 ULONG col_id)
	: CDXLScalarSubqueryQuantified(memory_pool, scalar_op_mdid, scalar_op_mdname, col_id)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubqueryAll::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLScalarSubqueryAll::GetDXLOperator() const
{
	return EdxlopScalarSubqueryAll;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarSubqueryAll::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarSubqueryAll::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarSubqueryAll);
}

// EOF
