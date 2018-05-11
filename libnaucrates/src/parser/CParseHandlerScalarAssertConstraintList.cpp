//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2015 Pivotal Inc.
//
//	@filename:
//		CParseHandlerScalarAssertConstraintList.cpp
//
//	@doc:
//		Implementation of the SAX parse handler class for parsing scalar 
//		assert operator constraint lists
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/CParseHandlerScalarAssertConstraintList.h"

#include "naucrates/dxl/operators/CDXLOperatorFactory.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerUtils.h"

using namespace gpdxl;


XERCES_CPP_NAMESPACE_USE

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarAssertConstraintList::CParseHandlerScalarAssertConstraintList
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CParseHandlerScalarAssertConstraintList::CParseHandlerScalarAssertConstraintList
	(
	IMemoryPool *memory_pool,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root
	)
	:
	CParseHandlerScalarOp(memory_pool, parse_handler_mgr, parse_handler_root),
	m_dxl_op(NULL),
	m_dxl_op_assert_constraint(NULL),
	m_dxlnode_assert_constraints_parsed_array(NULL)
{
}


//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarAssertConstraintList::StartElement
//
//	@doc:
//		Invoked by Xerces to process an opening tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarAssertConstraintList::StartElement
	(
	const XMLCh* const , // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const , // element_qname,
	const Attributes& attrs
	)
{	
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarAssertConstraintList), element_local_name))
	{
		GPOS_ASSERT(NULL == m_dxl_op);
		GPOS_ASSERT(NULL == m_dxl_op_assert_constraint);
		GPOS_ASSERT(NULL == m_dxlnode_assert_constraints_parsed_array);
		
		m_dxl_op = GPOS_NEW(m_memory_pool) CDXLScalarAssertConstraintList(m_memory_pool);
		m_dxlnode_assert_constraints_parsed_array = GPOS_NEW(m_memory_pool) DXLNodeArray(m_memory_pool);
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarAssertConstraint), element_local_name))
	{
		GPOS_ASSERT(NULL != m_dxl_op);
		GPOS_ASSERT(NULL == m_dxl_op_assert_constraint);
				
		// parse error message
		CWStringDynamic *pstrErrorMsg = CDXLOperatorFactory::ExtractConvertAttrValueToStr
										(
										m_parse_handler_mgr->GetDXLMemoryManager(), 
										attrs, 
										EdxltokenErrorMessage, 
										EdxltokenScalarAssertConstraint
										);
		m_dxl_op_assert_constraint = GPOS_NEW(m_memory_pool) CDXLScalarAssertConstraint(m_memory_pool, pstrErrorMsg);
		
		CParseHandlerBase *child_parse_handler = CParseHandlerFactory::GetParseHandler(m_memory_pool, CDXLTokens::XmlstrToken(EdxltokenScalar), m_parse_handler_mgr, this);
		m_parse_handler_mgr->ActivateParseHandler(child_parse_handler);

		this->Append(child_parse_handler);	
	}	
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerScalarAssertConstraintList::EndElement
//
//	@doc:
//		Invoked by Xerces to process a closing tag
//
//---------------------------------------------------------------------------
void
CParseHandlerScalarAssertConstraintList::EndElement
	(
	const XMLCh* const, // element_uri,
	const XMLCh* const element_local_name,
	const XMLCh* const // element_qname
	)
{
	if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarAssertConstraintList), element_local_name))
	{
		GPOS_ASSERT(NULL != m_dxl_op);
		GPOS_ASSERT(NULL != m_dxlnode_assert_constraints_parsed_array);
		
		// assemble final assert predicate node
		m_dxl_node = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, m_dxl_op, m_dxlnode_assert_constraints_parsed_array);

#ifdef GPOS_DEBUG
	m_dxl_op->AssertValid(m_dxl_node, false /* validate_children */);
#endif // GPOS_DEBUG

		// deactivate handler
		m_parse_handler_mgr->DeactivateHandler();	
	}
	else if (0 == XMLString::compareString(CDXLTokens::XmlstrToken(EdxltokenScalarAssertConstraint), element_local_name))
	{
		GPOS_ASSERT(NULL != m_dxl_op_assert_constraint);

		CParseHandlerScalarOp *child_parse_handler = dynamic_cast<CParseHandlerScalarOp*>((*this)[this->Length() - 1]);
		CDXLNode *child_dxlnode = child_parse_handler->CreateDXLNode();
		GPOS_ASSERT(NULL != child_dxlnode);
		child_dxlnode->AddRef();
		
		CDXLNode *pdxlnAssertConstraint = GPOS_NEW(m_memory_pool) CDXLNode(m_memory_pool, m_dxl_op_assert_constraint, child_dxlnode);
		m_dxlnode_assert_constraints_parsed_array->Append(pdxlnAssertConstraint);
		m_dxl_op_assert_constraint = NULL;
	}
	else
	{
		CWStringDynamic *str = CDXLUtils::CreateDynamicStringFromXMLChArray(m_parse_handler_mgr->GetDXLMemoryManager(), element_local_name);
		GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnexpectedTag, str->GetBuffer());
	}
}

// EOF
