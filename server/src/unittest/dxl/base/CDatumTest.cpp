//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CDatumTest.cpp
//
//	@doc:
//		Tests for datum classes
//---------------------------------------------------------------------------
#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"

#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/base/CQueryContext.h"

#include "unittest/base.h"
#include "unittest/dxl/base/CDatumTest.h"
#include "unittest/gpopt/translate/CTranslatorExprToDXLTest.h"
#include "unittest/gpopt/CTestUtils.h"

#include "naucrates/base/CDatumGenericGPDB.h"
#include "naucrates/base/CDatumOidGPDB.h"
#include "naucrates/base/CDatumInt2GPDB.h"
#include "naucrates/base/CDatumInt4GPDB.h"
#include "naucrates/base/CDatumInt8GPDB.h"
#include "naucrates/base/CDatumBoolGPDB.h"

#include "naucrates/md/IMDType.h"
#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/CMDTypeGenericGPDB.h"

#include "naucrates/dxl/gpdb_types.h"

//---------------------------------------------------------------------------
//	@function:
//		CDatumTest::EresUnittest
//
//	@doc:
//		Unittest for datum classes
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDatumTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CDatumTest::EresUnittest_Basics)
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumTest::EresUnittest_Basics
//
//	@doc:
//		Basic datum tests; verify correct creation
//
//---------------------------------------------------------------------------
GPOS_RESULT
CDatumTest::EresUnittest_Basics()
{
	// create memory pool
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(memory_pool, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc
					(
					memory_pool,
					&mda,
					NULL, /* pceeval */
					CTestUtils::GetCostModel(memory_pool)
					);

	typedef IDatum *(*Pfpdatum)(IMemoryPool*, BOOL);

	Pfpdatum rgpf[] =
		{
		CreateInt2Datum,
		CreateInt4Datum,
		CreateInt8Datum,
		CreateBoolDatum,
		CreateOidDatum,
		CreateGenericDatum,
		};
	
	BOOL rgf[] = {true, false};
	
	const ULONG ulFuncs = GPOS_ARRAY_SIZE(rgpf);
	const ULONG ulOptions = GPOS_ARRAY_SIZE(rgf);
	
	for (ULONG ul1 = 0; ul1 < ulFuncs; ul1++)
	{
		for (ULONG ul2 = 0; ul2 < ulOptions; ul2++)
		{
			CAutoTrace at(memory_pool);
			IOstream &os(at.Os());
			
			// generate datum
			BOOL is_null = rgf[ul2];
			IDatum *datum = rgpf[ul1](memory_pool, is_null);
			IDatum *pdatumCopy = datum->MakeCopy(memory_pool);
			
			GPOS_ASSERT(datum->Matches(pdatumCopy));
			
			const CWStringConst *pstrDatum = datum->GetStrRepr(memory_pool);
			
	#ifdef GPOS_DEBUG
			os << std::endl;
			(void) datum->OsPrint(os);
			os << std::endl << pstrDatum->GetBuffer() << std::endl;
	#endif // GPOS_DEBUG
	
			os << "Datum type: " << datum->GetDatumType() << std::endl;
	
			if (datum->StatsMappable())
			{
				IDatumStatisticsMappable *mappable_datum = (IDatumStatisticsMappable *) datum;
				
				if (mappable_datum->IsDatumMappableToLINT())
				{
					os << "LINT stats m_bytearray_value: " << mappable_datum->GetLINTMapping() << std::endl;
				}
	
				if (mappable_datum->IsDatumMappableToDouble())
				{
					os << "Double stats m_bytearray_value: " << mappable_datum->GetDoubleMapping() << std::endl;
				}
			}
			
			// cleanup
			datum->Release();
			pdatumCopy->Release();
			GPOS_DELETE(pstrDatum);
		}
	}

	return GPOS_OK;
}


//---------------------------------------------------------------------------
//	@function:
//		CDatumTest::CreateOidDatum
//
//	@doc:
//		Create an oid datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumTest::CreateOidDatum
	(
	IMemoryPool *memory_pool,
	BOOL is_null
	)
{
	return GPOS_NEW(memory_pool) CDatumOidGPDB(CTestUtils::m_sysidDefault, 1 /*val*/, is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumTest::CreateInt2Datum
//
//	@doc:
//		Create an int2 datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumTest::CreateInt2Datum
	(
	IMemoryPool *memory_pool,
	BOOL is_null
	)
{
	return GPOS_NEW(memory_pool) CDatumInt2GPDB(CTestUtils::m_sysidDefault, 1 /*val*/, is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumTest::CreateInt4Datum
//
//	@doc:
//		Create an int4 datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumTest::CreateInt4Datum
	(
	IMemoryPool *memory_pool,
	BOOL is_null
	)
{
	return GPOS_NEW(memory_pool) CDatumInt4GPDB(CTestUtils::m_sysidDefault, 1 /*val*/, is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumTest::CreateInt8Datum
//
//	@doc:
//		Create an int8 datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumTest::CreateInt8Datum
	(
	IMemoryPool *memory_pool,
	BOOL is_null
	)
{
	return GPOS_NEW(memory_pool) CDatumInt8GPDB(CTestUtils::m_sysidDefault, 1 /*val*/, is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumTest::CreateBoolDatum
//
//	@doc:
//		Create a bool datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumTest::CreateBoolDatum
	(
	IMemoryPool *memory_pool,
	BOOL is_null
	)
{
	return GPOS_NEW(memory_pool) CDatumBoolGPDB(CTestUtils::m_sysidDefault, false /*m_bytearray_value*/, is_null);
}

//---------------------------------------------------------------------------
//	@function:
//		CDatumTest::CreateGenericDatum
//
//	@doc:
//		Create a generic datum
//
//---------------------------------------------------------------------------
IDatum *
CDatumTest::CreateGenericDatum
	(
	IMemoryPool *memory_pool,
	BOOL is_null
	)
{
	CMDIdGPDB *pmdidChar = GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_CHAR);

	const CHAR *val = "test";
	return GPOS_NEW(memory_pool) CDatumGenericGPDB
							(
							memory_pool,
							pmdidChar,
							default_type_modifier,
							val,
							5 /*length*/,
							is_null,
							0 /*m_bytearray_value*/,
							0/*m_bytearray_value*/
							);
}


// EOF
