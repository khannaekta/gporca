//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerNLJIndexParam.h
//
//	@doc:
//		
//		SAX parse handler class for parsing a single Param of NLJ
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerNLJIndexParam_H
#define GPDXL_CParseHandlerNLJIndexParam_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
	using namespace gpos;

	XERCES_CPP_NAMESPACE_USE

	//---------------------------------------------------------------------------
	//	@class:
	//		CParseHandlerNLJIndexParam
	//
	//	@doc:
	//		Parse handler for parsing a Param of NLJ
	//
	//---------------------------------------------------------------------------
	class CParseHandlerNLJIndexParam : public CParseHandlerBase
	{
		private:
	
			// column reference
			CDXLColRef *m_pdxlcr;

			// private copy ctor
			CParseHandlerNLJIndexParam(const CParseHandlerNLJIndexParam &);
	
			// process the start of an element
			void StartElement
					(
					const XMLCh* const xmlszUri, 		// URI of element's namespace
					const XMLCh* const xmlszLocalname,	// local part of element's name
					const XMLCh* const xmlszQname,		// element's qname
					const Attributes& attr				// element's attributes
					);
	
			// process the end of an element
			void EndElement
					(
					const XMLCh* const xmlszUri, 		// URI of element's namespace
					const XMLCh* const xmlszLocalname,	// local part of element's name
					const XMLCh* const xmlszQname		// element's qname
					);
	
		public:
			// ctor/dtor
			CParseHandlerNLJIndexParam
					(
					IMemoryPool *pmp,
					CParseHandlerManager *pphm,
					CParseHandlerBase *pphRoot
					);

			virtual
			~CParseHandlerNLJIndexParam();

			// return column reference
			CDXLColRef *Pdxlcr(void)
			const
			{
				return m_pdxlcr;
			}

			// return param type
			IMDId *Pmdid(void)
			const
			{
				return m_pdxlcr->PmdidType();
			}
	};

}
#endif // GPDXL_CParseHandlerNLJIndexParam_H

//EOF
