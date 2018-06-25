//---------------------------------------------------------------------------
//      Greenplum Database
//      Copyright (C) 2014 Pivotal Inc.
//
//      @filename:
//              CUpperBoundNDVs.cpp
//
//      @doc:
//              Implementation of upper bound on the number of distinct values for a
//              given set of columns
//---------------------------------------------------------------------------

#include "naucrates/statistics/CUpperBoundNDVs.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CColRefSetIter.h"

using namespace gpnaucrates;
using namespace gpopt;

//---------------------------------------------------------------------------
//      @function:
//              CUpperBoundNDVs::CopyUpperBoundNDVWithRemap
//
//      @doc:
//              Copy upper bound ndvs with remapped column id; function will
//              return null if there is no mapping found for any of the columns
//
//---------------------------------------------------------------------------
CUpperBoundNDVs *
CUpperBoundNDVs::CopyUpperBoundNDVWithRemap(IMemoryPool *memory_pool,
											UlongColRefHashMap *colid_to_colref_map) const
{
	BOOL mapping_not_found = false;

	CColRefSet *column_refset_copy = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	CColRefSetIter column_refset_iter(*m_column_refset);
	while (column_refset_iter.Advance() && !mapping_not_found)
	{
		ULONG col_id = column_refset_iter.Pcr()->Id();
		CColRef *column_ref = colid_to_colref_map->Find(&col_id);
		if (NULL != column_ref)
		{
			column_refset_copy->Include(column_ref);
		}
		else
		{
			mapping_not_found = true;
		}
	}

	if (0 < column_refset_copy->Size() && !mapping_not_found)
	{
		return GPOS_NEW(memory_pool) CUpperBoundNDVs(column_refset_copy, UpperBoundNDVs());
	}

	column_refset_copy->Release();

	return NULL;
}


//---------------------------------------------------------------------------
//      @function:
//              CUpperBoundNDVs::CopyUpperBoundNDVs
//
//      @doc:
//              Copy upper bound ndvs
//
//---------------------------------------------------------------------------
CUpperBoundNDVs *
CUpperBoundNDVs::CopyUpperBoundNDVs(IMemoryPool *memory_pool, CDouble upper_bound_ndv) const
{
	m_column_refset->AddRef();
	CUpperBoundNDVs *ndv_copy =
		GPOS_NEW(memory_pool) CUpperBoundNDVs(m_column_refset, upper_bound_ndv);

	return ndv_copy;
}

//---------------------------------------------------------------------------
//      @function:
//              CUpperBoundNDVs::CopyUpperBoundNDVs
//
//      @doc:
//              Copy upper bound ndvs
//
//---------------------------------------------------------------------------
CUpperBoundNDVs *
CUpperBoundNDVs::CopyUpperBoundNDVs(IMemoryPool *memory_pool) const
{
	return CopyUpperBoundNDVs(memory_pool, m_upper_bound_ndv);
}


//---------------------------------------------------------------------------
//      @function:
//              CUpperBoundNDVs::OsPrint
//
//      @doc:
//              Print function
//
//---------------------------------------------------------------------------
IOstream &
CUpperBoundNDVs::OsPrint(IOstream &os) const
{
	os << "{" << std::endl;
	m_column_refset->OsPrint(os);
	os << " Upper Bound of NDVs" << UpperBoundNDVs() << std::endl;
	os << "}" << std::endl;

	return os;
}

// EOF
