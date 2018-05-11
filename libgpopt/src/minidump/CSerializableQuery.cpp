//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CSerializableQuery.cpp
//
//	@doc:
//		Serializable query object
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/error/CErrorContext.h"
#include "gpos/task/CTask.h"

#include "naucrates/dxl/CDXLUtils.h"

#include "gpopt/minidump/CSerializableQuery.h"

using namespace gpos;
using namespace gpopt;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CSerializableQuery::CSerializableQuery
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CSerializableQuery::CSerializableQuery
	(
	IMemoryPool *memory_pool,
	const CDXLNode *query,
	const DXLNodeArray *query_output_dxlnode_array,
	const DXLNodeArray *cte_producers
	)
	:
	CSerializable(),
	m_memory_pool(memory_pool),
	m_query_dxl_root(query),
	m_query_output(query_output_dxlnode_array),
	m_cte_producers(cte_producers)
{
	GPOS_ASSERT(NULL != query);
	GPOS_ASSERT(NULL != query_output_dxlnode_array);
}


//---------------------------------------------------------------------------
//	@function:
//		CSerializableQuery::~CSerializableQuery
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CSerializableQuery::~CSerializableQuery()
{
}

//---------------------------------------------------------------------------
//	@function:
//		CSerializableQuery::Serialize
//
//	@doc:
//		Serialize contents into provided stream
//
//---------------------------------------------------------------------------
void
CSerializableQuery::Serialize
	(
	COstream &oos
	)
{
	CDXLUtils::SerializeQuery
			(
			m_memory_pool,
			oos,
			m_query_dxl_root,
			m_query_output,
			m_cte_producers,
			false /*fSerializeHeaders*/,
			false /*indentation*/
			);
}

// EOF

