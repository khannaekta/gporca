//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc
//
//	@filename:
//		CDXLPhysicalValuesScan.h
//
//	@doc:
//		Class for representing DXL physical Values Scan operator.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalValuesScan_H
#define GPDXL_CDXLPhysicalValuesScan_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{

	// Class for representing DXL result operators
	class CDXLPhysicalValuesScan : public CDXLPhysical
	{
		private:

			// array of array of Const datums
			DrgPdrgPdxldatum *m_pdrgpdrgpdxldatumValuesList;

			// private copy ctor
			CDXLPhysicalValuesScan(CDXLPhysicalValuesScan&);

		public:
			// ctor
			CDXLPhysicalValuesScan(IMemoryPool *pmp, DrgPdrgPdxldatum* pdrgpdrgpdxldatum);

			// dtor
			virtual
			~CDXLPhysicalValuesScan();

			// accessors
			Edxlopid Edxlop() const;
			const CWStringConst *PstrOpName() const;

			// serialize operator in DXL format
			virtual
			void SerializeToDXL(CXMLSerializer *pxmlser, const CDXLNode *pdxln) const;

			// conversion function
			static CDXLPhysicalValuesScan *PdxlopConvert(CDXLOperator *pdxlop);

#ifdef GPOS_DEBUG
			// checks whether the operator has valid structure, i.e. number and
			// types of child nodes
			void AssertValid(const CDXLNode *, BOOL fValidateChildren) const;
#endif // GPOS_DEBUG

	};
}
#endif // !GPDXL_CDXLPhysicalValuesScan_H

// EOF

