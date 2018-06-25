//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalTableScan.cpp
//
//	@doc:
//		Implementation of DXL physical table scan operators
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalTableScan.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTableScan::CDXLPhysicalTableScan
//
//	@doc:
//		Construct a table scan node with uninitialized table descriptor
//
//---------------------------------------------------------------------------
CDXLPhysicalTableScan::CDXLPhysicalTableScan(IMemoryPool *memory_pool)
	: CDXLPhysical(memory_pool), m_table_descr_dxl(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTableScan::CDXLPhysicalTableScan
//
//	@doc:
//		Construct a table scan node given its table descriptor
//
//---------------------------------------------------------------------------
CDXLPhysicalTableScan::CDXLPhysicalTableScan(IMemoryPool *memory_pool, CDXLTableDescr *table_descr)
	: CDXLPhysical(memory_pool), m_table_descr_dxl(table_descr)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTableScan::~CDXLPhysicalTableScan
//
//	@doc:
//		Destructor
//
//---------------------------------------------------------------------------
CDXLPhysicalTableScan::~CDXLPhysicalTableScan()
{
	CRefCount::SafeRelease(m_table_descr_dxl);
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTableScan::SetTableDescriptor
//
//	@doc:
//		Set table descriptor
//
//---------------------------------------------------------------------------
void
CDXLPhysicalTableScan::SetTableDescriptor(CDXLTableDescr *table_descr)
{
	// allow setting table descriptor only once
	GPOS_ASSERT(NULL == m_table_descr_dxl);

	m_table_descr_dxl = table_descr;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTableScan::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalTableScan::GetDXLOperator() const
{
	return EdxlopPhysicalTableScan;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTableScan::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalTableScan::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalTableScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTableScan::MakeDXLTableDescr
//
//	@doc:
//		Table descriptor for the table scan
//
//---------------------------------------------------------------------------
const CDXLTableDescr *
CDXLPhysicalTableScan::GetDXLTableDescr()
{
	return m_table_descr_dxl;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTableScan::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalTableScan::SerializeToDXL(CXMLSerializer *xml_serializer, const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serialize properties
	dxlnode->SerializePropertiesToDXL(xml_serializer);

	// serialize children
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	// serialize table descriptor
	m_table_descr_dxl->SerializeToDXL(xml_serializer);

	xml_serializer->CloseElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
								 element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalTableScan::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalTableScan::AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(dxlnode, validate_children);

	// table scan has only 2 children
	GPOS_ASSERT(2 == dxlnode->Arity());

	// assert validity of table descriptor
	GPOS_ASSERT(NULL != m_table_descr_dxl);
	GPOS_ASSERT(NULL != m_table_descr_dxl->MdName());
	GPOS_ASSERT(m_table_descr_dxl->MdName()->GetMDName()->IsValid());
}
#endif  // GPOS_DEBUG

// EOF
