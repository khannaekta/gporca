//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLDatumGeneric.h
//
//	@doc:
//		Class for representing DXL datum of type generic
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLDatumGeneric_H
#define GPDXL_CDXLDatumGeneric_H

#include "gpos/base.h"
#include "gpos/common/CDouble.h"
#include "naucrates/dxl/operators/CDXLDatum.h"

namespace gpdxl
{
	using namespace gpos;

	// fwd decl
	class CXMLSerializer;

	//---------------------------------------------------------------------------
	//	@class:
	//		CDXLDatumGeneric
	//
	//	@doc:
	//		Class for representing DXL datum of type generic
	//
	//---------------------------------------------------------------------------
	class CDXLDatumGeneric : public CDXLDatum
	{
	private:
		// private copy ctor
		CDXLDatumGeneric(const CDXLDatumGeneric &);

	protected:
		// is datum passed by m_bytearray_value or by reference
		BOOL m_is_passed_by_value;

		// datum byte array
		BYTE *m_byte_array;

	public:
		// ctor
		CDXLDatumGeneric(IMemoryPool *memory_pool,
						 IMDId *mdid_type,
						 INT type_modifier,
						 BOOL is_passed_by_value,
						 BOOL is_null,
						 BYTE *data,
						 ULONG length);

		// dtor
		virtual ~CDXLDatumGeneric();

		// byte array
		const BYTE *GetByteArray() const;

		// serialize the datum as the given element
		virtual void Serialize(CXMLSerializer *xml_serializer);

		// is type passed by m_bytearray_value
		virtual BOOL
		IsPassedByValue() const
		{
			return m_is_passed_by_value;
		}

		// datum type
		virtual EdxldatumType
		GetDatumType() const
		{
			return CDXLDatum::EdxldatumGeneric;
		}

		// conversion function
		static CDXLDatumGeneric *
		Cast(CDXLDatum *datum_dxl)
		{
			GPOS_ASSERT(NULL != datum_dxl);
			GPOS_ASSERT(CDXLDatum::EdxldatumGeneric == datum_dxl->GetDatumType() ||
						CDXLDatum::EdxldatumStatsDoubleMappable == datum_dxl->GetDatumType() ||
						CDXLDatum::EdxldatumStatsLintMappable == datum_dxl->GetDatumType());

			return dynamic_cast<CDXLDatumGeneric *>(datum_dxl);
		}

		// statistics related APIs

		// can datum be mapped to LINT
		virtual BOOL
		IsDatumMappableToLINT() const
		{
			return false;
		}

		// return the lint mapping needed for statistics computation
		virtual LINT
		GetLINTMapping() const
		{
			GPOS_ASSERT(IsDatumMappableToLINT());

			return 0;
		}

		// can datum be mapped to a double
		virtual BOOL
		IsDatumMappableToDouble() const
		{
			return false;
		}

		// return the double mapping needed for statistics computation
		virtual CDouble
		GetDoubleMapping() const
		{
			GPOS_ASSERT(IsDatumMappableToDouble());
			return 0;
		}
	};
}  // namespace gpdxl

#endif  // !GPDXL_CDXLDatumGeneric_H

// EOF
