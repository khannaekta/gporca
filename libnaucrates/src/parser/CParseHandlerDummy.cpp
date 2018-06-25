//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerDummy.cpp
//
//	@doc:
//		Implementation of the dummy SAX parse handler class used for XSD validation.
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerDummy.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDummy::CParseHandlerDummy
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CParseHandlerDummy::CParseHandlerDummy(CDXLMemoryManager *memory_manager_dxl)
	: m_memory_manager_dxl(memory_manager_dxl)
{
}



//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerDummy::error
//
//	@doc:
//		Invoked by Xerces to process an error
//
//---------------------------------------------------------------------------
void
CParseHandlerDummy::error(const SAXParseException &sax_parse_ex)
{
	CHAR *message = XMLString::transcode(sax_parse_ex.getMessage(), m_memory_manager_dxl);
	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLValidationError, message);
}

// EOF
