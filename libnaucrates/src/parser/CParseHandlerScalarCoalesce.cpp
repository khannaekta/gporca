//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerScalarCoalesce.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for a coalesce operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerScalarCoalesce.h"


using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarCoalesce::CParseHandlerScalarCoalesce
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarCoalesce::CParseHandlerScalarCoalesce(IMemoryPool *memory_pool,
														 CParseHandlerManager *parse_handler_mgr,
														 CParseHandlerBase *parse_handler_root)
	: CParseHandlerScalarOp(memory_pool, parse_handler_mgr, parse_handler_root), m_mdid_type(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarCoalesce::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarCoalesce::StartElement(const XMLCh *const element_uri,
										  const XMLCh *const element_local_name,
										  const XMLCh *const element_qname,
										  const Attributes &attrs)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarCoalesce),
									  element_local_name) &&
		NULL == m_mdid_type)
	{
		// parse type id
		m_mdid_type = CDXLOperatorFactory::ExtractConvertAttrValueToMdId(
			m_parse_handler_mgr->GetDXLMemoryManager(),
			attrs,
			EdxltokenTypeId,
			EdxltokenScalarCoalesce);
	}
	else
	{
		// parse scalar child
		CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(
			m_memory_pool, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handlers
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarCoalesce::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarCoalesce::EndElement(const XMLCh *const,  // element_uri
										const XMLCh *const element_local_name,
										const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarCoalesce),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	// construct node
	m_dxl_node = GPOS_NEW(m_memory_pool) CDXLNode(
		m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarCoalesce(m_memory_pool, m_mdid_type));

	// loop over children and add them to this parsehandler
	const ULONG size = this->Length();
	for (ULONG idx = 0; idx < size; idx++)
	{
		CParseHandlerScalarOp *child_parse_handler =
			dynamic_cast<CParseHandlerScalarOp *>((*this)[idx]);
		AddChildFromParseHandler(child_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

//EOF
