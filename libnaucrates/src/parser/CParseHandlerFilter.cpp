//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerFilter.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing filter operators.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerFilter.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

#include "naucrates/dxl/operators/CDXLScalarFilter.h"
#include "naucrates/dxl/operators/CDXLScalarOneTimeFilter.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerFilter::CParseHandlerFilter
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerFilter::CParseHandlerFilter
	(
	IMemoryPool *memory_pool,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerScalarOp(memory_pool, parse_handler_mgr, parse_handler_root)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerFilter::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerFilter::StartElement
	(
	const XMLCh* const element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const element_qname,
	const Attributes& attrs
	)
{	
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarFilter), element_local_name))
	{
		// start the filter
		m_dxl_node = GPOS_NEW(m_memory_pool) CDXLNode (m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarFilter(m_memory_pool));
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarJoinFilter), element_local_name))
	{
		// start the filter
		m_dxl_node = GPOS_NEW(m_memory_pool) CDXLNode (m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarJoinFilter(m_memory_pool));
	} 
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarOneTimeFilter), element_local_name))
	{
		// start the filter
		m_dxl_node = GPOS_NEW(m_memory_pool) CDXLNode (m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarOneTimeFilter(m_memory_pool));
	}
	else if (0 == XMLString::compareString(
			CDXLTokens::XmlstrToken(EdxltokenScalarRecheckCondFilter), element_local_name))
	{
		// start the filter
		m_dxl_node = GPOS_NEW(m_memory_pool) CDXLNode (m_memory_pool, GPOS_NEW(m_memory_pool) CDXLScalarRecheckCondFilter(m_memory_pool));
	}
	else
	{
		GPOS_ASSERT(NULL != m_dxl_node);
		
		// install a scalar element parser for parsing the condition element
		CParseHandlerBase *op_parse_handler = CParseHandlerFactory::GetParseHandler(m_memory_pool, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);

		m_parse_handler_mgr->ActivateParseHandler(op_parse_handler);
		
		// store parse handler
		this->Append(op_parse_handler);
		
		op_parse_handler->startElement(element_uri, element_local_name, element_qname, attrs);
		
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerFilter::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerFilter::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarFilter), element_local_name) &&
		0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarJoinFilter), element_local_name) &&
		0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarOneTimeFilter), element_local_name) &&
		0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarRecheckCondFilter), element_local_name))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
	
	if (0 < this->Length())
	{
		// filter node was not empty 
		CParseHandlerScalarOp *op_parse_handler = dynamic_cast<CParseHandlerScalarOp *>((*this)[0]);
		
		AddChildFromParseHandler(op_parse_handler);	
	}
	
	// deactivate handler
	m_parse_handler_mgr->DeactivateHandler();
}

// EOF
