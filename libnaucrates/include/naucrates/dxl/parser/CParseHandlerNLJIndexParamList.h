//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CParseHandlerNLJIndexParamList.h
//
//	@doc:
//		
//		SAX parse handler class for parsing NLJ ParamList
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerNLJIndexParamList_H
#define GPDXL_CParseHandlerNLJIndexParamList_H

#include "gpos/base.h"
#include "naucrates/dxl/parser/CParseHandlerScalarOp.h"

namespace gpdxl
{
	using namespace gpos;

	XERCES_CPP_NAMESPACE_USE

	//---------------------------------------------------------------------------
	//	@class:
	//		CParseHandlerNLJIndexParamList
	//
	//	@doc:
	//		Parse handler for parsing a scalar NLJ ParamList
	//
	//---------------------------------------------------------------------------
	class CParseHandlerNLJIndexParamList : public CParseHandlerBase
	{
		private:
	
			BOOL m_fParamList;

			// array of outer column references
			DrgPdxlcr *m_pdrgdxlcr;

			// private copy ctor
			CParseHandlerNLJIndexParamList(const CParseHandlerNLJIndexParamList &);
	
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
			// ctor
			CParseHandlerNLJIndexParamList
					(
					IMemoryPool *pmp,
					CParseHandlerManager *pphm,
					CParseHandlerBase *pphRoot
					);

			// dtor
			virtual
			~CParseHandlerNLJIndexParamList();

			// return param column references
			DrgPdxlcr *Pdrgdxlcr()
			const
			{
				return m_pdrgdxlcr;
			}
	};

}
#endif // GPDXL_CParseHandlerNLJIndexParamList_H

//EOF
