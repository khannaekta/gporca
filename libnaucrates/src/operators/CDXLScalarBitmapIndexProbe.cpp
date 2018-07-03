//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CDXLScalarBitmapIndexProbe.cpp
//
//	@doc:
//		Class for representing DXL bitmap index probe operators
//---------------------------------------------------------------------------

#include "naucrates/dxl/operators/CDXLIndexDescr.h"
#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/operators/CDXLScalarBitmapIndexProbe.h"
#include "naucrates/dxl/operators/CDXLTableDescr.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"
#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpdxl;


//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapIndexProbe::CDXLScalarBitmapIndexProbe
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLScalarBitmapIndexProbe::CDXLScalarBitmapIndexProbe(IMemoryPool *mp,
													   CDXLIndexDescr *index_descr_dxl)
	: CDXLScalar(mp), m_index_descr_dxl(index_descr_dxl)
{
	GPOS_ASSERT(NULL != m_index_descr_dxl);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapIndexProbe::~CDXLScalarBitmapIndexProbe
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CDXLScalarBitmapIndexProbe::~CDXLScalarBitmapIndexProbe()
{
	m_index_descr_dxl->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapIndexProbe::GetOpNameStr
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLScalarBitmapIndexProbe::GetOpNameStr() const
{
	return CDXLTokens::GetDXLTokenStr(EdxltokenScalarBitmapIndexProbe);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapIndexProbe::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLScalarBitmapIndexProbe::SerializeToDXL(CXMLSerializer *xml_serializer,
										   const CDXLNode *dxlnode) const
{
	const CWStringConst *element_name = GetOpNameStr();
	xml_serializer->OpenElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix), element_name);

	// serialize children
	dxlnode->SerializeChildrenToDXL(xml_serializer);

	// serialize index descriptor
	m_index_descr_dxl->SerializeToDXL(xml_serializer);

	xml_serializer->CloseElement(CDXLTokens::GetDXLTokenStr(EdxltokenNamespacePrefix),
								 element_name);
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLScalarBitmapIndexProbe::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured
//
//---------------------------------------------------------------------------
void
CDXLScalarBitmapIndexProbe::AssertValid(const CDXLNode *dxlnode, BOOL validate_children) const
{
	// bitmap index probe has 1 child: the index condition list
	GPOS_ASSERT(1 == dxlnode->Arity());

	if (validate_children)
	{
		CDXLNode *pdxlnIndexCondList = (*dxlnode)[0];
		GPOS_ASSERT(EdxlopScalarIndexCondList ==
					pdxlnIndexCondList->GetOperator()->GetDXLOperator());
		pdxlnIndexCondList->GetOperator()->AssertValid(pdxlnIndexCondList, validate_children);
	}

	// assert validity of index descriptor
	GPOS_ASSERT(NULL != m_index_descr_dxl->MdName());
	GPOS_ASSERT(m_index_descr_dxl->MdName()->GetMDName()->IsValid());
}
#endif  // GPOS_DEBUG

// EOF
