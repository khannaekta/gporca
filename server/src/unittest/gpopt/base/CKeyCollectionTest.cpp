//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 - 2012 EMC Corp.
//
//	@filename:
//		CKeyCollectionTest.cpp
//
//	@doc:
//		Tests for CKeyCollectionTest
//---------------------------------------------------------------------------
#include "gpopt/base/CKeyCollection.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/metadata/CName.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"

#include "unittest/base.h"
#include "unittest/gpopt/CTestUtils.h"
#include "unittest/gpopt/base/CKeyCollectionTest.h"

#include "naucrates/md/IMDTypeInt4.h"
#include "naucrates/md/CMDProviderMemory.h"


using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CKeyCollectionTest::EresUnittest
//
//	@doc:
//		Unittest for key collections
//
//---------------------------------------------------------------------------
GPOS_RESULT
CKeyCollectionTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CKeyCollectionTest::EresUnittest_Basics),
		GPOS_UNITTEST_FUNC(CKeyCollectionTest::EresUnittest_Subsumes)
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CKeyCollectionTest::EresUnittest_Basics
//
//	@doc:
//		Basic test for key collections
//
//---------------------------------------------------------------------------
GPOS_RESULT
CKeyCollectionTest::EresUnittest_Basics()
{
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();

	CMDAccessor mda(memory_pool, CMDCache::Pcache());
	mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc
				(
				memory_pool,
				&mda,
				NULL, /* pceeval */
				CTestUtils::GetCostModel(memory_pool)
				);

	// get column factory from optimizer context object
	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	
	// create test set
	CWStringConst strName(GPOS_WSZ_LIT("Test Column"));
	CName name(&strName);
	const IMDTypeInt4 *pmdtypeint4 = mda.PtMDType<IMDTypeInt4>();

	CKeyCollection *pkc = GPOS_NEW(memory_pool) CKeyCollection(memory_pool);

	const ULONG num_cols = 10;
	for(ULONG i = 0; i < num_cols; i++)
	{
		CColRef *colref = col_factory->PcrCreate(pmdtypeint4, default_type_modifier, name);
		pcrs->Include(colref);
	}

	pkc->Add(pcrs);	
	GPOS_ASSERT(pkc->FKey(pcrs));
	
	DrgPcr *colref_array = pkc->PdrgpcrKey(memory_pool);
	GPOS_ASSERT(pkc->FKey(memory_pool, colref_array));
	
	pcrs->Include(colref_array);
	GPOS_ASSERT(pkc->FKey(pcrs));

	colref_array->Release();
	
	pkc->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CKeyCollectionTest::EresUnittest_Subsumes
//
//	@doc:
//		Basic test for triming key collections
//
//---------------------------------------------------------------------------
GPOS_RESULT
CKeyCollectionTest::EresUnittest_Subsumes()
{
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	// Setup an MD cache with a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();

	CMDAccessor mda(memory_pool, CMDCache::Pcache());
	mda.RegisterProvider(CTestUtils::m_sysidDefault, pmdp);

	// install opt context in TLS
	CAutoOptCtxt aoc
					(
					memory_pool,
					&mda,
					NULL, /* pceeval */
					CTestUtils::GetCostModel(memory_pool)
					);

	// get column factory from optimizer context object
	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

	// create test set
	CWStringConst strName(GPOS_WSZ_LIT("Test Column"));
	CName name(&strName);
	const IMDTypeInt4 *pmdtypeint4 = mda.PtMDType<IMDTypeInt4>();

	CKeyCollection *pkc = GPOS_NEW(memory_pool) CKeyCollection(memory_pool);

	CColRefSet *pcrs0 = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	CColRefSet *pcrs1 = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	CColRefSet *pcrs2 = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	const ULONG num_cols = 10;
	const ULONG ulLen1 = 3;
	for(ULONG ul = 0; ul < num_cols; ul++)
	{
		CColRef *colref = col_factory->PcrCreate(pmdtypeint4, default_type_modifier, name);
		pcrs0->Include(colref);

		if (ul < ulLen1)
		{
			pcrs1->Include(colref);
		}

		if (ul == 0)
		{
			pcrs2->Include(colref);
		}
	}

	pkc->Add(pcrs0);
	pkc->Add(pcrs1);
	pkc->Add(pcrs2);

	GPOS_ASSERT(pkc->FKey(pcrs2));

	// get the second key
	DrgPcr *colref_array = pkc->PdrgpcrKey(memory_pool, 1);
	GPOS_ASSERT(ulLen1 == colref_array->Size());
	GPOS_ASSERT(pkc->FKey(memory_pool, colref_array));

	// get the subsumed key
	DrgPcr *pdrgpcrSubsumed = pkc->PdrgpcrTrim(memory_pool, colref_array);
	GPOS_ASSERT(colref_array->Size() >= pdrgpcrSubsumed->Size());

	CColRefSet *pcrsSubsumed = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrsSubsumed->Include(colref_array);

#ifdef GPOS_DEBUG
	const ULONG ulLenSubsumed = pdrgpcrSubsumed->Size();
	for (ULONG ul = 0; ul < ulLenSubsumed; ul++)
	{
		CColRef *colref = (*pdrgpcrSubsumed)[ul];
		GPOS_ASSERT(pcrsSubsumed->FMember(colref));
	}
#endif // GPOS_DEBUG

	pcrsSubsumed->Release();
	colref_array->Release();
	pdrgpcrSubsumed->Release();
	pkc->Release();

	return GPOS_OK;
}
// EOF
