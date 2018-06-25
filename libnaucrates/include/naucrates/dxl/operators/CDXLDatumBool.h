//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLDatumBool.h
//
//	@doc:
//		Class for representing DXL boolean datum
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLDatumBool_H
#define GPDXL_CDXLDatumBool_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLDatum.h"

namespace gpdxl
{
	using namespace gpos;

	// fwd decl
	class CXMLSerializer;

	//---------------------------------------------------------------------------
	//	@class:
	//		CDXLDatumBool
	//
	//	@doc:
	//		Class for representing DXL boolean datums
	//
	//---------------------------------------------------------------------------
	class CDXLDatumBool : public CDXLDatum
	{
	private:
		// boolean m_bytearray_value
		BOOL m_value;

		// private copy ctor
		CDXLDatumBool(const CDXLDatumBool &);

	public:
		// ctor
		CDXLDatumBool(IMemoryPool *memory_pool, IMDId *mdid_type, BOOL is_null, BOOL value);

		// dtor
		virtual ~CDXLDatumBool()
		{
		}

		// serialize the datum as the given element
		virtual void Serialize(CXMLSerializer *xml_serializer);

		// is type passed by m_bytearray_value
		virtual BOOL
		IsPassedByValue() const
		{
			return true;
		}

		// datum type
		virtual EdxldatumType
		GetDatumType() const
		{
			return CDXLDatum::EdxldatumBool;
		}

		// accessor of boolean m_bytearray_value
		BOOL
		GetValue() const
		{
			return m_value;
		}

		// conversion function
		static CDXLDatumBool *
		Cast(CDXLDatum *datum_dxl)
		{
			GPOS_ASSERT(NULL != datum_dxl);
			GPOS_ASSERT(CDXLDatum::CDXLDatum::EdxldatumBool == datum_dxl->GetDatumType());

			return dynamic_cast<CDXLDatumBool *>(datum_dxl);
		}
	};
}  // namespace gpdxl

#endif  // !GPDXL_CDXLDatumBool_H

// EOF
