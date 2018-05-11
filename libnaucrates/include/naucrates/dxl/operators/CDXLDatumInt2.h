//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal Inc.
//
//	@filename:
//		CDXLDatumInt2.h
//
//	@doc:
//		Class for representing DXL short integer datum
//
//	@owner: 
//		
//
//	@test:
//
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLDatumInt2_H
#define GPDXL_CDXLDatumInt2_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLDatum.h"

namespace gpdxl
{
	using namespace gpos;

	// fwd decl
	class CXMLSerializer;

	//---------------------------------------------------------------------------
	//	@class:
	//		CDXLDatumInt2
	//
	//	@doc:
	//		Class for representing DXL short integer datums
	//
	//---------------------------------------------------------------------------
	class CDXLDatumInt2 : public CDXLDatum
	{
		private:
			// int2 m_bytearray_value
			SINT m_val;

			// private copy ctor
			CDXLDatumInt2(const CDXLDatumInt2 &);

		public:
			// ctor
			CDXLDatumInt2
				(
				IMemoryPool *memory_pool,
				IMDId *mdid_type,
				BOOL is_null,
				SINT val
				);

			// dtor
			virtual
			~CDXLDatumInt2(){};

			// accessor of int m_bytearray_value
			SINT Value() const;

			// serialize the datum as the given element
			virtual
			void Serialize(CXMLSerializer *xml_serializer);

			// datum type
			virtual
			EdxldatumType GetDatumType() const
			{
				return CDXLDatum::EdxldatumInt2;
			}

			// is type passed by m_bytearray_value
			virtual
			BOOL IsPassedByValue() const
			{
				return true;
			}

			// conversion function
			static
			CDXLDatumInt2 *Cast
				(
				CDXLDatum *datum_dxl
				)
			{
				GPOS_ASSERT(NULL != datum_dxl);
				GPOS_ASSERT(CDXLDatum::EdxldatumInt2 == datum_dxl->GetDatumType());

				return dynamic_cast<CDXLDatumInt2*>(datum_dxl);
			}
	};
}

#endif // !GPDXL_CDXLDatumInt2_H

// EOF

