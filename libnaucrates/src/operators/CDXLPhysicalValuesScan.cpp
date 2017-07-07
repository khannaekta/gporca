//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CDXLPhysicalValuesScan.cpp
//
//	@doc:
//		Implementation of DXL physical result operator
//---------------------------------------------------------------------------


#include "naucrates/dxl/operators/CDXLPhysicalValuesScan.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/xml/CXMLSerializer.h"

using namespace gpos;
using namespace gpdxl;

// Constructor
CDXLPhysicalValuesScan::CDXLPhysicalValuesScan
	(
	IMemoryPool *pmp,
	DrgPdrgPdxldatum *pdrgpdrgpdxldatum
	)
	:
	CDXLPhysical(pmp),
	m_pdrgpdrgpdxldatumValuesList(pdrgpdrgpdxldatum)
{
}


// Operator type
Edxlopid
CDXLPhysicalValuesScan::Edxlop() const
{
	return EdxlopPhysicalValuesScan;
}


// Operator name
const CWStringConst *
CDXLPhysicalValuesScan::PstrOpName() const
{
	return CDXLTokens::PstrToken(EdxltokenPhysicalValuesScan);
}


// Serialize operator in DXL format
void
CDXLPhysicalValuesScan::SerializeToDXL
	(
	CXMLSerializer *pxmlser,
	const CDXLNode *pdxln
	)
	const
{
	const CWStringConst *pstrElemName = PstrOpName();

	pxmlser->OpenElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), pstrElemName);

	// serialize properties
	pdxln->SerializePropertiesToDXL(pxmlser);

	// serialize children
	pdxln->SerializeChildrenToDXL(pxmlser);

	// serialize Values List
	pxmlser->OpenElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), CDXLTokens::PstrToken(EdxltokenValuesList));
	GPOS_ASSERT(NULL != m_pdrgpdrgpdxldatumValuesList);

	const ULONG ulTuples = m_pdrgpdrgpdxldatumValuesList->UlLength();
	for (ULONG ulTuplePos = 0; ulTuplePos < ulTuples; ulTuplePos++)
	{
		// serialize a const tuple
		pxmlser->OpenElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), CDXLTokens::PstrToken(EdxltokenConstTuple));
		DrgPdxldatum *pdrgpdxldatum = (*m_pdrgpdrgpdxldatumValuesList)[ulTuplePos];

		const ULONG ulCols = pdrgpdxldatum->UlLength();
		for (ULONG ulColPos = 0; ulColPos < ulCols; ulColPos++)
		{
			CDXLDatum *pdxldatum = (*pdrgpdxldatum)[ulColPos];
			pdxldatum->Serialize(pxmlser, CDXLTokens::PstrToken(EdxltokenDatum));
		}

		pxmlser->CloseElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), CDXLTokens::PstrToken(EdxltokenConstTuple));
	}

	pxmlser->CloseElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), CDXLTokens::PstrToken(EdxltokenValuesList));

	pxmlser->CloseElement(CDXLTokens::PstrToken(EdxltokenNamespacePrefix), pstrElemName);
}

// conversion function
CDXLPhysicalValuesScan *
CDXLPhysicalValuesScan::PdxlopConvert
	(
	CDXLOperator *pdxlop
	)
{
	GPOS_ASSERT(NULL != pdxlop);
	GPOS_ASSERT(EdxlopPhysicalValuesScan == pdxlop->Edxlop());

	return dynamic_cast<CDXLPhysicalValuesScan*>(pdxlop);
}

#ifdef GPOS_DEBUG
// Checks whether operator node is well-structured
void
CDXLPhysicalValuesScan::AssertValid
	(
	const CDXLNode *,//pdxln,
	BOOL //fValidateChildren
	) 
	const
{

//	GPOS_ASSERT(EdxlresultIndexSentinel >= pdxln->UlArity());
//
//	// check that one time filter is valid
//	CDXLNode *pdxlnOneTimeFilter = (*pdxln)[EdxlresultIndexOneTimeFilter];
//	GPOS_ASSERT(EdxlopScalarOneTimeFilter == pdxlnOneTimeFilter->Pdxlop()->Edxlop());
//
//	if (fValidateChildren)
//	{
//		pdxlnOneTimeFilter->Pdxlop()->AssertValid(pdxlnOneTimeFilter, fValidateChildren);
//	}
//
//	if (EdxlresultIndexSentinel == pdxln->UlArity())
//	{
//		CDXLNode *pdxlnChild = (*pdxln)[EdxlresultIndexChild];
//		GPOS_ASSERT(EdxloptypePhysical == pdxlnChild->Pdxlop()->Edxloperatortype());
//
//		if (fValidateChildren)
//		{
//			pdxlnChild->Pdxlop()->AssertValid(pdxlnChild, fValidateChildren);
//		}
//	}

}
#endif // GPOS_DEBUG

// EOF
