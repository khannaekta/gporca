//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLDatumInt8.cpp
//
//	@doc:
//		Implementation of DXL datum of type long int
//		
//	@owner: 
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#include "gpos/string/CWStringDynamic.h"

#include "naucrates/dxl/operators/CDXLDatumInt8.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatumInt8::CDXLDatumInt8
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CDXLDatumInt8::CDXLDatumInt8
	(
	IMemoryPool *memory_pool,
	IMDId *mdid_type,
	BOOL is_null,
	LINT val
	)
	:
	CDXLDatum(memory_pool, mdid_type, default_type_modifier, is_null, 8 /*length*/),
	m_val(val)
{
	if (is_null)
	{
		m_val = 0;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatumInt8::Value
//
//	@doc:
//		Return the long int m_bytearray_value
//
//---------------------------------------------------------------------------
LINT
CDXLDatumInt8::Value() const
{
	return m_val;
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLDatumInt8::Serialize
//
//	@doc:
//		Serialize datum in DXL format
//
//---------------------------------------------------------------------------
void
CDXLDatumInt8::Serialize
	(
	CXMLSerializer *xml_serializer
	)
{
	m_mdid_type->Serialize(xml_serializer, CDXLTokens::GetDXLTokenStr(EdxltokenTypeId));
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenIsNull), m_is_null);
	xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenIsByValue), IsPassedByValue());
	
	if (!m_is_null)
	{
		xml_serializer->AddAttribute(CDXLTokens::GetDXLTokenStr(EdxltokenValue), m_val);
	}
}

// EOF
