//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CParseHandlerValuesScan.cpp
//
//	@doc:
//
//		Implementation of the SAX parse handler class for parsing m_bytearray_value scan
//		operator
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerValuesScan.h"
#include "naucrates/dxl/parser/CParseHandlerProjList.h"
#include "naucrates/dxl/parser/CParseHandlerScalarValuesList.h"
#include "naucrates/dxl/parser/CParseHandlerProperties.h"
#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE


//	ctor
CParseHandlerValuesScan::CParseHandlerValuesScan(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
	: CParseHandlerPhysicalOp(memory_pool, parse_handler_mgr, parse_handler_root)
{
}


//	processes a Xerces start element event
void
CParseHandlerValuesScan::StartElement(const XMLCh *const element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const element_qname,
									  const Attributes &attrs)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalValuesScan),
									  element_local_name))
	{
		m_dxl_op = GPOS_NEW(m_memory_pool) CDXLPhysicalValuesScan(m_memory_pool);

		// parse handler for the proj list
		CParseHandlerBase *proj_list_parse_handler =
			CParseHandlerFactory::GetParseHandler(m_memory_pool,
												  CDXLTokens::XmlstrToken(EdxltokenScalarProjList),
												  m_parse_handler_mgr,
												  this);
		m_parse_handler_mgr->ActivateParseHandler(proj_list_parse_handler);

		//parse handler for the properties of the operator
		CParseHandlerBase *prop_parse_handler = CParseHandlerFactory::GetParseHandler(
			m_memory_pool, CDXLTokens::XmlstrToken(EdxltokenProperties), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(prop_parse_handler);

		// store parse handlers
		this->Append(prop_parse_handler);
		this->Append(proj_list_parse_handler);
	}
	else
	{
		// parse scalar child
		CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(
			m_memory_pool,
			CDXLTokens::XmlstrToken(EdxltokenScalarValuesList),
			m_parse_handler_mgr,
			this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		// store parse handler
		this->Append(child_parse_handler);

		child_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
	}
}

//	processes a Xerces end element event
void
CParseHandlerValuesScan::EndElement(const XMLCh *const,  // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const  // element_qname
)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenPhysicalValuesScan),
									  element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	const ULONG arity = this->Length();
	GPOS_ASSERT(3 <= arity);

	m_dxl_node = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, m_dxl_op);

	// valuesscan has properties element as its first child
	CParseHandlerProperties *prop_parse_handler =
		dynamic_cast<CParseHandlerProperties *>((*this)[0]);

	// set statistics and physical properties
	CParseHandlerUtils::SetProperties(m_dxl_node, prop_parse_handler);

	// valuesscan has project list element as its second child
	CParseHandlerProjList *proj_list_parse_handler =
		dynamic_cast<CParseHandlerProjList *>((*this)[1]);
	AddChildFromParseHandler(proj_list_parse_handler);

	// valuesscan child m_bytearray_value list begins with third child
	for (ULONG idx = 2; idx < arity; idx++)
	{
		CParseHandlerScalarValuesList *scalar_values_list_parse_handler =
			dynamic_cast<CParseHandlerScalarValuesList *>((*this)[idx]);
		AddChildFromParseHandler(scalar_values_list_parse_handler);
	}

	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
