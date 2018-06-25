//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalIndexScan.cpp
//
//	@doc:
//		Implementation of DXL physical index scan operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalIndexScan.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalIndexScan::CDXLPhysicalIndexScan
//
//	@doc:
//		Construct an index scan node given its table descriptor,
//		index descriptor and filter conditions on the index
//
//---------------------------------------------------------------------------
CDXLPhysicalIndexScan::CDXLPhysicalIndexScan(IMemoryPool *memory_pool,
											 CDXLTableDescr *table_descr,
											 CDXLIndexDescr *index_descr_dxl,
											 EdxlIndexScanDirection idx_scan_direction)
	: CDXLPhysical(memory_pool),
	  m_table_descr_dxl(table_descr),
	  m_index_descr_dxl(index_descr_dxl),
	  m_index_scan_dir(idx_scan_direction)
{
	GPOS_ASSERT(NULL != m_table_descr_dxl);
	GPOS_ASSERT(NULL != m_index_descr_dxl);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalIndexScan::~CDXLPhysicalIndexScan
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysicalIndexScan::~CDXLPhysicalIndexScan()
{
	m_index_descr_dxl->Release();
	m_table_descr_dxl->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalIndexScan::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalIndexScan::GetDXLOperator() const
{
	return EdxlopPhysicalIndexScan;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalIndexScan::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalIndexScan::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalIndexScan);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalIndexScan::MakeDXLIndexDescr
//
//	@doc:
//		Index descriptor for the index scan
//
//---------------------------------------------------------------------------
const CDXLIndexDescr *
CDXLPhysicalIndexScan::GetDXLIndexDescr() const
{
	return m_index_descr_dxl;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalIndexScan::GetIndexScanDir
//
//	@doc:
//		Return the scan direction of the index
//
//---------------------------------------------------------------------------
EdxlIndexScanDirection
CDXLPhysicalIndexScan::GetIndexScanDir() const
{
	return m_index_scan_dir;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalIndexScan::MakeDXLTableDescr
//
//	@doc:
//		Return the associated table descriptor
//
//---------------------------------------------------------------------------
const CDXLTableDescr *
CDXLPhysicalIndexScan::GetDXLTableDescr() const
{
	return m_table_descr_dxl;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalIndexScan::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalIndexScan::SerializeToDXL(CXMLSerializer *xml_serializer, const CDXLNode *node) const
{
	const CWStringConst *element_name = GetOpNameStr();

	xml_serializer->OpenElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenIndexScanDirection),
								 CDXLOperator::GetIdxScanDirectionStr(m_index_scan_dir));

	// serialize properties
	node->SerializePropertiesToDXL(xml_serializer);

	// serialize children
	node->SerializeChildrenToDXL(xml_serializer);

	// serialize index descriptor
	m_index_descr_dxl->SerializeToDXL(xml_serializer);

	// serialize table descriptor
	m_table_descr_dxl->SerializeToDXL(xml_serializer);

	xml_serializer->CloseElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
								 element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalIndexScan::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalIndexScan::AssertValid(const CDXLNode *node, BOOL validate_children) const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(node, validate_children);

	// index scan has only 3 children
	GPOS_ASSERT(3 == node->Arity());

	// assert validity of the index descriptor
	GPOS_ASSERT(NULL != m_index_descr_dxl);
	GPOS_ASSERT(NULL != m_index_descr_dxl->MdName());
	GPOS_ASSERT(m_index_descr_dxl->MdName()->GetMDName()->IsValid());

	// assert validity of the table descriptor
	GPOS_ASSERT(NULL != m_table_descr_dxl);
	GPOS_ASSERT(NULL != m_table_descr_dxl->MdName());
	GPOS_ASSERT(m_table_descr_dxl->MdName()->GetMDName()->IsValid());

	CDXLNode *index_cond_dxlnode = (*node)[EdxlisIndexCondition];

	// assert children are of right type (physical/scalar)
	GPOS_ASSERT(EdxlopScalarIndexCondList == index_cond_dxlnode->GetOperator()->GetDXLOperator());

	if (validate_children)
	{
		index_cond_dxlnode->GetOperator()->AssertValid(index_cond_dxlnode, validate_children);
	}
}
#endif  // GPOS_DEBUG

// EOF
