//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/minidump/CMetadataAccessorFactory.h"
#include "gpos/common/CAutoRef.h"
#include "naucrates/md/CMDProviderMemory.h"

namespace gpopt
{
	CMetadataAccessorFactory::CMetadataAccessorFactory
		(
			IMemoryPool *memory_pool,
			CDXLMinidump *pdxlmd,
			const CHAR *file_name
		)
	{

		// set up MD providers
		CAutoRef<CMDProviderMemory> apmdp(GPOS_NEW(memory_pool) CMDProviderMemory(memory_pool, file_name));
		const SysidPtrArray *pdrgpsysid = pdxlmd->GetSysidPtrArray();
		CAutoRef<MDProviderPtrArray> apdrgpmdp(GPOS_NEW(memory_pool) MDProviderPtrArray(memory_pool));

		// ensure there is at least ONE system id
		apmdp->AddRef();
		apdrgpmdp->Append(apmdp.Value());

		for (ULONG ul = 1; ul < pdrgpsysid->Size(); ul++)
		{
			apmdp->AddRef();
			apdrgpmdp->Append(apmdp.Value());
		}

		m_apmda = GPOS_NEW(memory_pool) CMDAccessor(memory_pool, CMDCache::Pcache(), pdxlmd->GetSysidPtrArray(), apdrgpmdp.Value());
	}

	CMDAccessor *CMetadataAccessorFactory::Pmda()
	{
		return m_apmda.Value();
	}
}
