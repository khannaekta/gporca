//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLDatumOid.h
//
//	@doc:
//		Class for representing DXL oid datum
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLDatumOid_H
#define GPDXL_CDXLDatumOid_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLDatum.h"

namespace gpdxl
{
	using namespace gpos;

	// fwd decl
	class CXMLSerializer;

	//---------------------------------------------------------------------------
	//	@class:
	//		CDXLDatumOid
	//
	//	@doc:
	//		Class for representing DXL oid datums
	//
	//---------------------------------------------------------------------------
	class CDXLDatumOid : public CDXLDatum
	{
	private:
		// oid m_bytearray_value
		OID m_oid_val;

		// private copy ctor
		CDXLDatumOid(const CDXLDatumOid &);

	public:
		// ctor
		CDXLDatumOid(IMemoryPool *mp, IMDId *mdid_type, BOOL is_null, OID oid_val);

		// dtor
		virtual ~CDXLDatumOid(){};

		// accessor of oid m_bytearray_value
		OID OidValue() const;

		// serialize the datum as the given element
		virtual void Serialize(CXMLSerializer *xml_serializer);

		// datum type
		virtual EdxldatumType
		GetDatumType() const
		{
			return CDXLDatum::EdxldatumOid;
		}

		// is type passed by m_bytearray_value
		virtual BOOL
		IsPassedByValue() const
		{
			return true;
		}

		// conversion function
		static CDXLDatumOid *
		Cast(CDXLDatum *datum_dxl)
		{
			GPOS_ASSERT(NULL != datum_dxl);
			GPOS_ASSERT(CDXLDatum::EdxldatumOid == datum_dxl->GetDatumType());

			return dynamic_cast<CDXLDatumOid *>(datum_dxl);
		}
	};
}  // namespace gpdxl

#endif  // !GPDXL_CDXLDatumOid_H

// EOF
