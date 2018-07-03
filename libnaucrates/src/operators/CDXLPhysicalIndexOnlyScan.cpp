//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalIndexOnlyScan.cpp
//
//	@doc:
//		Implementation of DXL physical index only scan operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalIndexOnlyScan.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalIndexOnlyScan::CDXLPhysicalIndexOnlyScan
//
//	@doc:
//		Construct an index only scan node given its table descriptor,
//		index descriptor and filter conditions on the index
//
//---------------------------------------------------------------------------
CDXLPhysicalIndexOnlyScan::CDXLPhysicalIndexOnlyScan(IMemoryPool *mp,
													 CDXLTableDescr *table_descr,
													 CDXLIndexDescr *index_descr_dxl,
													 EdxlIndexScanDirection idx_scan_direction)
	: CDXLPhysicalIndexScan(mp, table_descr, index_descr_dxl, idx_scan_direction)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalIndexOnlyScan::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalIndexOnlyScan::GetDXLOperator() const
{
	return EdxlopPhysicalIndexOnlyScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalIndexOnlyScan::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalIndexOnlyScan::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalIndexOnlyScan);
}

// EOF
