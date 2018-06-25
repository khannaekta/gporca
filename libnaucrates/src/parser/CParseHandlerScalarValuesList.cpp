//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CParseHandlerScalarValuesList.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing m_bytearray_value list.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarValuesList.h"

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/operators/CDXLScalarValuesList.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

// ctor
CParseHandlerScalarValuesList::CParseHandlerScalarValuesList(
	IMemoryPool *memory_pool,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
	: CParseHandlerOp(memory_pool, parse_handler_mgr, parse_handler_root)
{
}

// invoked by Xerces to process an opening tag
void
CParseHandlerScalarValuesList::StartElement(const XMLCh *const element_uri,
											const XMLCh *const element_local_name,
											const XMLCh *const element_qname,
											const Attributes &attrs)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarValuesList),
									  element_local_name))
	{
		CDXLScalarValuesList *dxl_op = GPOS_NEW(m_memory_pool) CDXLScalarValuesList(m_memory_pool);
		m_dxl_node = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, dxl_op);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarConstValue),
										   element_local_name))
	{
		CParseHandlerBase *parse_handler_const_value = CParseHandlerFactory::GetParseHandler(
			m_memory_pool, element_local_name, m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(parse_handler_const_value);

		this->Append(parse_handler_const_value);

		parse_handler_const_value->startElement(
			element_uri, element_local_name, element_qname, attrs);
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
}

// invoked by Xerces to process a closing tag
void
CParseHandlerScalarValuesList::EndElement(const XMLCh *const,  // element_uri,
										  const XMLCh *const,  //element_local_name,
										  const XMLCh *const   // element_qname
)
{
	const ULONG arity = this->Length();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CParseHandlerScalarOp *child_parse_handler =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[ul]);
		AddChildFromParseHandler(child_parse_handler);
	}
	m_parse_handler_mgr->DeactivateHandler();
}
// EOF
