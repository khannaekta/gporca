//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLPhysicalNLJoin.cpp
//
//	@doc:
//		Implementation of DXL physical nested loop join operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalNLJoin.h"
#include "naucrates/dxl/operators/CDXLNode.h"

#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalNLJoin::CDXLPhysicalNLJoin
//
//	@doc:
//		Constructor
//
//---------------------------------------------------------------------------
CDXLPhysicalNLJoin::CDXLPhysicalNLJoin
	(
	IMemoryPool *pmp,
	EdxlJoinType edxljt,
	BOOL fIndexNLJ,
	DrgPdxlcr *pdrgdxlcr
	)
	:
	CDXLPhysicalJoin(pmp, edxljt),
	m_fIndexNLJ(fIndexNLJ),
	m_pdrgdxlcr(pdrgdxlcr)
{
}


CDXLPhysicalNLJoin::~CDXLPhysicalNLJoin()
{
	m_pdrgdxlcr->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalNLJoin::Edxlop
//
//	@doc:
//		Operator type
//
//---------------------------------------------------------------------------
Edxlopid
CDXLPhysicalNLJoin::Edxlop() const
{
	return EdxlopPhysicalNLJoin;
}


//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalNLJoin::PstrOpName
//
//	@doc:
//		Operator name
//
//---------------------------------------------------------------------------
const CWStringConst *
CDXLPhysicalNLJoin::PstrOpName() const
{
	return CDXLTokens::PstrToken(EdxltokenPhysicalNLJoin);
}

//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalNLJoin::SerializeToDXL
//
//	@doc:
//		Serialize operator in DXL format
//
//---------------------------------------------------------------------------
void
CDXLPhysicalNLJoin::SerializeToDXL
	(
	CXMLSerializer *pxmlser,
	const CDXLNode *pdxln
	)
	const
{
	const CWStringConst *pstrElemName = PstrOpName();
	
	pxmlser->OpenElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), pstrElemName);		
	
	pxmlser->AddAttribute(CDXLTokens::PstrToken(EdxltokenJoinType), PstrJoinTypeName());
	pxmlser->AddAttribute(CDXLTokens::PstrToken(EdxltokenPhysicalNLJoinIndex), m_fIndexNLJ);

	// serialize properties
	pdxln->SerializePropertiesToDXL(pxmlser);

	// Serialize NLJ index paramlist
	pxmlser->OpenElement
	(
	 CDXLTokens::PstrToken(EdxltokenNamespacePrefix),
	 CDXLTokens::PstrToken(EdxltokenNLJIndexParamList)
	 );

	for (ULONG ul = 0; ul < m_pdrgdxlcr->UlLength(); ul++)
	{
		pxmlser->OpenElement
		(
		 CDXLTokens::PstrToken(EdxltokenNamespacePrefix),
		 CDXLTokens::PstrToken(EdxltokenNLJIndexParam)
		 );

		ULONG ulid = (*m_pdrgdxlcr)[ul]->UlID();
		pxmlser->AddAttribute(CDXLTokens::PstrToken(EdxltokenColId), ulid);

		const CMDName *pmdname = (*m_pdrgdxlcr)[ul]->Pmdname();
		const IMDId *pmdidType = (*m_pdrgdxlcr)[ul]->PmdidType();
		pxmlser->AddAttribute(CDXLTokens::PstrToken(EdxltokenColName), pmdname->Pstr());
		pmdidType->Serialize(pxmlser, CDXLTokens::PstrToken(EdxltokenTypeId));

		pxmlser->CloseElement
		(
		 CDXLTokens::PstrToken(EdxltokenNamespacePrefix),
		 CDXLTokens::PstrToken(EdxltokenNLJIndexParam)
		 );
	}

	pxmlser->CloseElement
	(
	 CDXLTokens::PstrToken(EdxltokenNamespacePrefix),
	 CDXLTokens::PstrToken(EdxltokenNLJIndexParamList)
	 );
	
	pxmlser->CloseElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), pstrElemName);

	// serialize children
	pdxln->SerializeChildrenToDXL(pxmlser);
}


#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CDXLPhysicalNLJoin::AssertValid
//
//	@doc:
//		Checks whether operator node is well-structured 
//
//---------------------------------------------------------------------------
void
CDXLPhysicalNLJoin::AssertValid
	(
	const CDXLNode *pdxln,
	BOOL fValidateChildren
	) 
	const
{
	// assert proj list and filter are valid
	CDXLPhysical::AssertValid(pdxln, fValidateChildren);
	
	GPOS_ASSERT(EdxlnljIndexSentinel == pdxln->UlArity());
	GPOS_ASSERT(EdxljtSentinel > Edxltype());
	
	CDXLNode *pdxlnJoinFilter = (*pdxln)[EdxlnljIndexJoinFilter];
	CDXLNode *pdxlnLeft = (*pdxln)[EdxlnljIndexLeftChild];
	CDXLNode *pdxlnRight = (*pdxln)[EdxlnljIndexRightChild];
	
	// assert children are of right type (physical/scalar)
	GPOS_ASSERT(EdxlopScalarJoinFilter == pdxlnJoinFilter->Pdxlop()->Edxlop());
	GPOS_ASSERT(EdxloptypePhysical == pdxlnLeft->Pdxlop()->Edxloperatortype());
	GPOS_ASSERT(EdxloptypePhysical == pdxlnRight->Pdxlop()->Edxloperatortype());
	
	if (fValidateChildren)
	{
		pdxlnJoinFilter->Pdxlop()->AssertValid(pdxlnJoinFilter, fValidateChildren);
		pdxlnLeft->Pdxlop()->AssertValid(pdxlnLeft, fValidateChildren);
		pdxlnRight->Pdxlop()->AssertValid(pdxlnRight, fValidateChildren);
	}
}
#endif // GPOS_DEBUG

// EOF
