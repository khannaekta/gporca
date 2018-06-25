//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CDXLWindowKey.h
//
//	@doc:
//		Class for representing DXL window key
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLWindowKey_H
#define GPDXL_CDXLWindowKey_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLWindowFrame.h"

namespace gpdxl
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CDXLWindowKey
	//
	//	@doc:
	//		Class for representing DXL window key
	//
	//---------------------------------------------------------------------------
	class CDXLWindowKey : public CRefCount
	{
	private:
		// memory pool;
		IMemoryPool *m_memory_pool;

		// window frame associated with the window key
		CDXLWindowFrame *m_window_frame_dxl;

		// private copy ctor
		CDXLWindowKey(const CDXLWindowKey &);

		// sorting columns
		CDXLNode *m_sort_col_list_dxl;

	public:
		// ctor
		explicit CDXLWindowKey(IMemoryPool *memory_pool);

		// dtor
		virtual ~CDXLWindowKey();

		// serialize operator in DXL format
		virtual void SerializeToDXL(CXMLSerializer *) const;

		// set window frame definition
		void SetWindowFrame(CDXLWindowFrame *window_frame);

		// return window frame
		CDXLWindowFrame *
		GetWindowFrame() const
		{
			return m_window_frame_dxl;
		}

		// set the list of sort columns
		void SetSortColList(CDXLNode *sort_col_list_dxl);

		// sort columns
		CDXLNode *
		GetSortColListDXL() const
		{
			return m_sort_col_list_dxl;
		}
	};

	typedef CDynamicPtrArray<CDXLWindowKey, CleanupRelease> CDXLWindowKeyArray;
}  // namespace gpdxl
#endif  // !GPDXL_CDXLWindowKey_H

// EOF
