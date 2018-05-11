//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 Greenplum, Inc.
//
//	@filename:
//		CMDProviderTest.cpp
//
//	@doc:
//		Tests the file-based metadata provider.
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/io/ioutils.h"
#include "gpos/io/COstreamString.h"
#include "gpos/task/CAutoTaskProxy.h"
#include "gpos/test/CUnittest.h"

#include "unittest/gpopt/mdcache/CMDProviderTest.h"
#include "unittest/gpopt/CTestUtils.h"

#include "naucrates/md/CMDProviderMemory.h"
#include "naucrates/md/CMDIdRelStats.h"
#include "naucrates/md/CMDIdColStats.h"

#include "naucrates/exception.h"
#include "naucrates/dxl/CDXLUtils.h"

#include "gpopt/mdcache/CAutoMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"

using namespace gpos;
using namespace gpdxl;
using namespace gpopt;

const CHAR *CMDProviderTest::file_name = "../data/dxl/metadata/md.xml";

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderTest::EresUnittest
//
//	@doc:
//		
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDProviderTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CMDProviderTest::EresUnittest_Basic),
		GPOS_UNITTEST_FUNC(CMDProviderTest::EresUnittest_Stats),
		GPOS_UNITTEST_FUNC_THROW
			(
			CMDProviderTest::EresUnittest_Negative,
			gpdxl::ExmaMD,
			gpdxl::ExmiMDCacheEntryNotFound
			),
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderTest::EresUnittest_Basic
//
//	@doc:
//		Test fetching existing metadata objects from a file-based provider
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDProviderTest::EresUnittest_Basic()
{
	// create memory pool
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();
	
	// test lookup with a file-based provider
	CMDProviderMemory *pmdpFile = GPOS_NEW(memory_pool) CMDProviderMemory(memory_pool, file_name);
	pmdpFile->AddRef();
	
	TestMDLookup(memory_pool, pmdpFile);
	
	pmdpFile->Release();
	
	// test lookup with a memory-based provider
	CHAR *dxl_string = CDXLUtils::Read(memory_pool, file_name);

	IMDCachePtrArray *mdcache_obj_array = CDXLUtils::ParseDXLToIMDObjectArray(memory_pool, dxl_string, NULL /*xsd_file_path*/);
	
	CMDProviderMemory *pmdpMemory = GPOS_NEW(memory_pool) CMDProviderMemory(memory_pool, mdcache_obj_array);
	pmdpMemory->AddRef();
	TestMDLookup(memory_pool, pmdpMemory);

	GPOS_DELETE_ARRAY(dxl_string);
	mdcache_obj_array->Release();
	pmdpMemory->Release();
	
	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderTest::TestMDLookup
//
//	@doc:
//		Test looking up objects using given MD provider
//
//---------------------------------------------------------------------------
void
CMDProviderTest::TestMDLookup
	(
	IMemoryPool *memory_pool,
	IMDProvider *pmdp
	)
{
	CAutoMDAccessor amda(memory_pool, pmdp, CTestUtils::m_sysidDefault, CMDCache::Pcache());

	// lookup different objects
	CMDIdGPDB *pmdid1 = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID, 1 /* major version */, 1 /* minor version */);
	CMDIdGPDB *pmdid2 = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID, 12 /* version */, 1 /* minor version */);

	CWStringBase *pstrMDObject1 = pmdp->GetMDObjDXLStr(memory_pool, amda.Pmda(), pmdid1);
	CWStringBase *pstrMDObject2 = pmdp->GetMDObjDXLStr(memory_pool, amda.Pmda(), pmdid2);

	GPOS_ASSERT(NULL != pstrMDObject1 && NULL != pstrMDObject2);

	IMDCacheObject *pimdobj1 = CDXLUtils::ParseDXLToIMDIdCacheObj(memory_pool, pstrMDObject1, NULL);

	IMDCacheObject *pimdobj2 = CDXLUtils::ParseDXLToIMDIdCacheObj(memory_pool, pstrMDObject2, NULL);

	GPOS_ASSERT(NULL != pimdobj1 && pmdid1->Equals(pimdobj1->MDId()));
	GPOS_ASSERT(NULL != pimdobj2 && pmdid2->Equals(pimdobj2->MDId()));

	// cleanup
	pmdid1->Release();
	pmdid2->Release();
	GPOS_DELETE(pstrMDObject1);
	GPOS_DELETE(pstrMDObject2);
	pimdobj1->Release();
	pimdobj2->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderTest::EresUnittest_Stats
//
//	@doc:
//		Test fetching existing stats objects from a file-based provider
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDProviderTest::EresUnittest_Stats()
{
	// create memory pool
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();
	
	CMDProviderMemory *pmdpFile = GPOS_NEW(memory_pool) CMDProviderMemory(memory_pool, file_name);

	{
		pmdpFile->AddRef();
		CAutoMDAccessor amda(memory_pool, pmdpFile, CTestUtils::m_sysidDefault, CMDCache::Pcache());

		// lookup different objects
		CMDIdRelStats *rel_stats_mdid = GPOS_NEW(memory_pool) CMDIdRelStats(GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID, 1, 1));

		CWStringBase *pstrRelStats = pmdpFile->GetMDObjDXLStr(memory_pool, amda.Pmda(), rel_stats_mdid);
		GPOS_ASSERT(NULL != pstrRelStats);
		IMDCacheObject *pmdobjRelStats = CDXLUtils::ParseDXLToIMDIdCacheObj(memory_pool, pstrRelStats, NULL);
		GPOS_ASSERT(NULL != pmdobjRelStats);

		CMDIdColStats *mdid_col_stats =
				GPOS_NEW(memory_pool) CMDIdColStats(GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID, 1, 1), 1 /* attno */);

		CWStringBase *pstrColStats = pmdpFile->GetMDObjDXLStr(memory_pool, amda.Pmda(), mdid_col_stats);
		GPOS_ASSERT(NULL != pstrColStats);
		IMDCacheObject *pmdobjColStats = CDXLUtils::ParseDXLToIMDIdCacheObj(memory_pool, pstrColStats, NULL);
		GPOS_ASSERT(NULL != pmdobjColStats);

		// cleanup
		rel_stats_mdid->Release();
		mdid_col_stats->Release();
		GPOS_DELETE(pstrRelStats);
		GPOS_DELETE(pstrColStats);
		pmdobjRelStats->Release();
		pmdobjColStats->Release();
	}

	pmdpFile->Release();
	
	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CMDProviderTest::EresUnittest_Negative
//
//	@doc:
//		Test fetching non-exiting metadata objects from a file-based provider
//
//---------------------------------------------------------------------------
GPOS_RESULT
CMDProviderTest::EresUnittest_Negative()
{
	CAutoMemoryPool amp(CAutoMemoryPool::ElcNone);
	IMemoryPool *memory_pool = amp.Pmp();
	
	CMDProviderMemory *pmdpFile = GPOS_NEW(memory_pool) CMDProviderMemory(memory_pool, file_name);
	pmdpFile->AddRef();
	
	// we need to use an auto pointer for the cache here to ensure
	// deleting memory of cached objects when we throw
	CAutoP<CMDAccessor::MDCache> apcache;
	apcache = CCacheFactory::CreateCache<gpopt::IMDCacheObject*, gpopt::CMDKey*>
				(
				true, // fUnique
				0 /* unlimited cache quota */,
				CMDKey::UlHashMDKey,
				CMDKey::FEqualMDKey
				);

	CMDAccessor::MDCache *pcache = apcache.Value();

	{
		CAutoMDAccessor amda(memory_pool, pmdpFile, CTestUtils::m_sysidDefault, pcache);

		// lookup a non-existing objects
		CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID, 15 /* major version */, 1 /* minor version */);

		// call should result in an exception
		(void) pmdpFile->GetMDObjDXLStr(memory_pool, amda.Pmda(), mdid);
	}
	
	return GPOS_FAILED;
}

// EOF

