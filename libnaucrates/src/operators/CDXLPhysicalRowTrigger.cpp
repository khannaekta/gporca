//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLPhysicalRowTrigger.cpp
//
//	@doc:
//		Implementation of DXL physical row trigger operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLPhysicalRowTrigger.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/CDXLUtils.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRowTrigger::CDXLPhysicalRowTrigger
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLPhysicalRowTrigger::CDXLPhysicalRowTrigger(IMemoryPool *memory_pool,
											   IMDId *rel_mdid,
											   INT type,
											   ULongPtrArray *col_ids_old,
											   ULongPtrArray *col_ids_new)
	: CDXLPhysical(memory_pool),
	  m_rel_mdid(rel_mdid),
	  m_type(type),
	  m_col_ids_old(col_ids_old),
	  m_col_ids_new(col_ids_new)
{
	GPOS_ASSERT(rel_mdid->IsValid());
	GPOS_ASSERT(0 != type);
	GPOS_ASSERT(NULL != col_ids_new || NULL != col_ids_old);
	GPOS_ASSERT_IMP(NULL != col_ids_new && NULL != col_ids_old,
					col_ids_new->Size() == col_ids_old->Size());
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRowTrigger::~CDXLPhysicalRowTrigger
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLPhysicalRowTrigger::~CDXLPhysicalRowTrigger()
{
	m_rel_mdid->Release();
	CRefCount::SafeRelease(m_col_ids_old);
	CRefCount::SafeRelease(m_col_ids_new);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRowTrigger::GetDXLOperator
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalRowTrigger::GetDXLOperator() const
{
	return EdxlopPhysicalRowTrigger;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRowTrigger::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalRowTrigger::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenPhysicalRowTrigger);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRowTrigger::SerializeToDXL
//
//	@doc:
//		Serialize function descriptor in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalRowTrigger::SerializeToDXL(CXMLSerializer *xml_serializer,
									   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);
	m_rel_mdid->Serialize(xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenRelationMdid));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenMDType), m_type);

	if (NULL != m_col_ids_old)
	{
		CWStringDynamic *pstrColsOld = CDXLUtils::Serialize(m_memory_pool, m_col_ids_old);
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenOldCols), pstrColsOld);
		GPOS_DELETE(pstrColsOld);
	}

	if (NULL != m_col_ids_new)
	{
		CWStringDynamic *pstrColsNew = CDXLUtils::Serialize(m_memory_pool, m_col_ids_new);
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenNewCols), pstrColsNew);
		GPOS_DELETE(pstrColsNew);
	}

	dxlnode->SerializePropertiesToDXL(xml_serializer);

	// serialize project list
	(*dxlnode)[0]->SerializeToDXL(xml_serializer);

	// serialize physical child
	(*dxlnode)[1]->SerializeToDXL(xml_serializer);

	xml_serializer->CloseElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
								 element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalRowTrigger::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLPhysicalRowTrigger::AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const
{
	GPOS_ASSERT(2 == dxlnode->Arity());
	CDXLNode *child_dxlnode = (*dxlnode)[1];
	GPOS_ASSERT(EdxloptypePhysical == child_dxlnode->GetOperator()->GetDXLOperatorType());

	if (validate_children)
	{
		child_dxlnode->GetOperator()->AssertValid(child_dxlnode, validate_children);
	}
}

#endif  // GPOS_DEBUG


// EOF
