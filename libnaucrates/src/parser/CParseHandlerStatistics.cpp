//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerStatistics.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing a DXL document
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerStatistics.h"

#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/parser/CParseHandlerStatsDerivedRelation.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatistics::CParseHandlerStatistics
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerStatistics::CParseHandlerStatistics(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
	: CParseHandlerBase(memory_pool, parse_handler_mgr, parse_handler_root),
	  m_stats_derived_rel_dxl_array(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatistics::~CParseHandlerStatistics
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerStatistics::~CParseHandlerStatistics()
{
	CRefCount::SafeRelease(m_stats_derived_rel_dxl_array);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatistics::GetParseHandlerType
//
//	@doc:
//		Return the type of the parse handler. Currently we overload this method to
//		return a specific type for the plann, query and metadata parse handlers.
//
//---------------------------------------------------------------------------
EDxlParseHandlerType
CParseHandlerStatistics::GetParseHandlerType() const
{
	return EdxlphStatistics;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatistics::GetStatsDerivedRelDXLArray
//
//	@doc:
//		Returns the list of statistics objects constructed by the parser
//
//---------------------------------------------------------------------------
DXLStatsDerivedRelArray *
CParseHandlerStatistics::GetStatsDerivedRelDXLArray() const
{
	return m_stats_derived_rel_dxl_array;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatistics::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerStatistics::StartElement(const XMLCh *const element_uri,
									  const XMLCh *const element_local_name,
									  const XMLCh *const element_qname,
									  const Attributes &attrs)
{
	if (0 ==
		XMLString::compareString(element_local_name, CDXLTokens::XmlstrToken(EdxltokenStatistics)))
	{
		// start of the statistics section in the DXL document
		GPOS_ASSERT(NULL == m_stats_derived_rel_dxl_array);

		m_stats_derived_rel_dxl_array =
			GPOS_NEW(m_memory_pool) DXLStatsDerivedRelArray(m_memory_pool);
	}
	else
	{
		// currently we only have derived relation statistics objects
		GPOS_ASSERT(NULL != m_stats_derived_rel_dxl_array);

		// install a parse handler for the given element
		CParseHandlerBase *parse_handler_base = CParseHandlerFactory::GetParseHandler(
			m_memory_pool, element_local_name, m_parse_handler_mgr, this);

		m_parse_handler_mgr->ActivateParseHandler(parse_handler_base);

		// store parse handler
		this->Append(parse_handler_base);

		parse_handler_base->startElement(element_uri, element_local_name, element_qname, attrs);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerStatistics::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerStatistics::EndElement(const XMLCh *const,  // element_uri,
									const XMLCh *const element_local_name,
									const XMLCh *const  // element_qname
)
{
	if (0 !=
		XMLString::compareString(element_local_name, CDXLTokens::XmlstrToken(EdxltokenStatistics)))
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(
			m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}

	GPOS_ASSERT(NULL != m_stats_derived_rel_dxl_array);

	const ULONG num_of_stats = this->Length();
	for (ULONG idx = 0; idx < num_of_stats; idx++)
	{
		CParseHandlerStatsDerivedRelation *stats_derived_rel_parse_handler =
			dynamic_cast<CParseHandlerStatsDerivedRelation *>((*this)[idx]);

		CDXLStatsDerivedRelation *dxl_stats_derived_relation =
			stats_derived_rel_parse_handler->GetDxlStatsDrvdRelation();
		dxl_stats_derived_relation->AddRef();
		m_stats_derived_rel_dxl_array->Append(dxl_stats_derived_relation);
	}

	m_parse_handler_mgr->DeactivateHandler();
}


// EOF
