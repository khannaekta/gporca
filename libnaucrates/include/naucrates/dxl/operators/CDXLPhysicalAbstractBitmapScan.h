//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2014 Pivotal, Inc.
//
//	@filename:
//		CDXLPhysicalAbstractBitmapScan.h
//
//	@doc:
//		Parent class for representing DXL bitmap table scan operators,
//		both not partitioned and dynamic.
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLPhysicalAbstractBitmapScan_H
#define GPDXL_CDXLPhysicalAbstractBitmapScan_H

#include "gpos/base.h"

#include "naucrates/dxl/operators/CDXLPhysical.h"

namespace gpdxl
{
	using namespace gpos;

	enum Edxlbs
	{
		EdxlbsIndexProjList = 0,
		EdxlbsIndexFilter,
		EdxlbsIndexRecheckCond,
		EdxlbsIndexBitmap,
		EdxlbsSentinel
	};

	// fwd declarations
	class CDXLTableDescr;
	class CXMLSerializer;

	//---------------------------------------------------------------------------
	//	@class:
	//		CDXLPhysicalAbstractBitmapScan
	//
	//	@doc:
	//		Parent class for representing DXL bitmap table scan operators, both not
	//		partitioned and dynamic.
	//
	//---------------------------------------------------------------------------
	class CDXLPhysicalAbstractBitmapScan : public CDXLPhysical
	{
		private:
			// private copy ctor
			CDXLPhysicalAbstractBitmapScan(const CDXLPhysicalAbstractBitmapScan &);

		protected:
			// table descriptor for the scanned table
			CDXLTableDescr *m_table_descr_dxl;

		public:
			// ctor
			CDXLPhysicalAbstractBitmapScan
				(
				IMemoryPool *memory_pool,
				CDXLTableDescr *table_descr
				)
				:
				CDXLPhysical(memory_pool),
				m_table_descr_dxl(table_descr)
			{
				GPOS_ASSERT(NULL != table_descr);
			}

			// dtor
			virtual
			~CDXLPhysicalAbstractBitmapScan();

			// table descriptor
			const CDXLTableDescr *GetDXLTableDescr()
			{
				return m_table_descr_dxl;
			}

#ifdef GPOS_DEBUG
			// checks whether the operator has valid structure, i.e. number and
			// types of child nodes
			virtual
			void AssertValid(const CDXLNode *node, BOOL validate_children) const;
#endif // GPOS_DEBUG
	};  // class CDXLPhysicalAbstractBitmapScan
}

#endif  // !GPDXL_CDXLPhysicalAbstractBitmapScan_H

// EOF
