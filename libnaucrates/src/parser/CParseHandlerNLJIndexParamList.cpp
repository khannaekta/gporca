//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerNLJIndexParamList.cpp
//
//	@doc:
//		
//		Implementation of the SAX parse handler class for parsing the ParamList
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/CDXLUtils.h"
#include "naucrates/dxl/operators/CDXLOperatorFactory.h"

#include "naucrates/dxl/parser/CParseHandlerNLJIndexParamList.h"
#include "naucrates/dxl/parser/CParseHandlerNLJIndexParam.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerNLJIndexParamList::CParseHandlerNLJIndexParamList
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerNLJIndexParamList::CParseHandlerNLJIndexParamList
	(
	IMemoryPool *pmp,
	CParseHandlerManager *pphm,
	CParseHandlerBase *pphRoot
	)
	:
	CParseHandlerBase(pmp, pphm, pphRoot)
{
	m_pdrgdxlcr = GPOS_NEW(pmp) DrgPdxlcr(pmp);
	m_fParamList = false;
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerNLJIndexParamList::~CParseHandlerNLJIndexParamList
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CParseHandlerNLJIndexParamList::~CParseHandlerNLJIndexParamList()
{
	m_pdrgdxlcr->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerNLJIndexParamList::StartElement
//
//	@doc:
//		Processes a Xerces start element event
//
//---------------------------------------------------------------------------
void
CParseHandlerNLJIndexParamList::StartElement
	(
	const XMLCh* const xmlszUri,
	const XMLCh* const xmlszLocalname,
	const XMLCh* const xmlszQname,
	const Attributes &attrs
	)
{
	if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenNLJIndexParamList), xmlszLocalname))
	{
		// we can't have seen a paramlist already
		GPOS_ASSERT(!m_fParamList);
		// start the paramlist
		m_fParamList = true;
	}
	else if(0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenNLJIndexParam), xmlszLocalname))
	{
		// we must have seen a paramlist already
		GPOS_ASSERT(m_fParamList);

		// start new param
		CParseHandlerBase *pphParam = CParseHandlerFactory::Pph(m_pmp, CDXLTokens::XmlstrToken(EdxltokenNLJIndexParam), m_pphm, this);
		m_pphm->ActivateParseHandler(pphParam);

		// store parse handler
		this->Append(pphParam);

		pphParam->startElement(xmlszUri, xmlszLocalname, xmlszQname, attrs);
	}
	else
	{
		CWStringDynamic *pstr = CDXLUtils::PstrFromXMLCh(m_pphm->Pmm(), xmlszLocalname);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, pstr->Wsz());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerNLJIndexParamList::EndElement
//
//	@doc:
//		Processes a Xerces end element event
//
//---------------------------------------------------------------------------
void
CParseHandlerNLJIndexParamList::EndElement
	(
	const XMLCh* const, // xmlszUri,
	const XMLCh* const xmlszLocalname,
	const XMLCh* const // xmlszQname
	)
{
	if(0 != XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenNLJIndexParamList), xmlszLocalname))
	{
		CWStringDynamic *pstr = CDXLUtils::PstrFromXMLCh(m_pphm->Pmm(), xmlszLocalname);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, pstr->Wsz());
	}

	const ULONG ulSize = this->UlLength();
	// add constructed children from child parse handlers
	for (ULONG ul = 0; ul < ulSize; ul++)
	{
		CParseHandlerNLJIndexParam *pphParam = dynamic_cast<CParseHandlerNLJIndexParam *>((*this)[ul]);

		CDXLColRef *pdxlcr = pphParam->Pdxlcr();
		pdxlcr->AddRef();
		m_pdrgdxlcr->Append(pdxlcr);
	}

	// deactivate handler
	m_pphm->DeactivateHandler();
}

// EOF
