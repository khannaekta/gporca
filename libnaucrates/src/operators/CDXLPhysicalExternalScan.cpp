//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 Pivotal, Inc.
//
//	@filename:
//		CDXLPhysicalExternalScan.cpp
//
//	@doc:
//		Implementation of DXL physical external scan operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalExternalScan.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalExternalScan::CDXLPhysicalExternalScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalExternalScan::CDXLPhysicalExternalScan
	(
	IMemoryPool *memory_pool
	)
	:
	CDXLPhysicalTableScan(memory_pool)
{}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalExternalScan::CDXLPhysicalExternalScan
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalExternalScan::CDXLPhysicalExternalScan
	(
	IMemoryPool *memory_pool,
	CDXLTableDescr *table_descr
	)
	:
	CDXLPhysicalTableScan(memory_pool, table_descr)
{}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalExternalScan::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalExternalScan::GetDXLOperator() const
{
	return EdxlopPhysicalExternalScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalExternalScan::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalExternalScan::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalExternalScan);
}

// EOF
