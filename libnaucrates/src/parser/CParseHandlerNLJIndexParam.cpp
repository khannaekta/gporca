//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerNLJIndexParam.cpp
//
//	@doc:
//		
//		Implementation of the SAX parse handler class for parsing a Param
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerPhysicalOp.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

#include "naucrates/dxl/parser/CParseHandlerNLJIndexParam.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerNLJIndexParam::CParseHandlerNLJIndexParam
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerNLJIndexParam::CParseHandlerNLJIndexParam
	(
	IMemoryPool *pmp,
	CParseHandlerManager *pphm,
	CParseHandlerBase *pphRoot
	)
	:
	CParseHandlerBase(pmp, pphm, pphRoot),
	m_pdxlcr(NULL)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerNLJIndexParam::~CParseHandlerNLJIndexParam
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerNLJIndexParam::~CParseHandlerNLJIndexParam()
{
	m_pdxlcr->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerNLJIndexParam::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerNLJIndexParam::StartElement
	(
	const XMLCh* const, // xmlszUri,
	const XMLCh* const xmlszLocalname,
	const XMLCh* const, // xmlszQname,
	const Attributes &attrs
	)
{
	if (0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenNLJIndexParam), xmlszLocalname))
	{
		CWStringDynamic *pstr = CDXLUtils::PstrFromXMLCh(m_pphm->Pmm(), xmlszLocalname);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, pstr->Wsz());
	}

	m_pdxlcr = CDXLOperatorFactory::Pdxlcr(m_pphm->Pmm(), attrs, EdxltokenNLJIndexParam);
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerNLJIndexParam::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerNLJIndexParam::EndElement
	(
	const XMLCh* const, // xmlszUri,
	const XMLCh* const xmlszLocalname,
	const XMLCh* const // xmlszQname
	)
{
	if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenNLJIndexParam), xmlszLocalname))
	{
		CWStringDynamic *pstr = CDXLUtils::PstrFromXMLCh(m_pphm->Pmm(), xmlszLocalname);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag,	pstr->Wsz());
	}

	// deactivate handler
	m_pphm->DeactivateHandler();
}

// EOF
