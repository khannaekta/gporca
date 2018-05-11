//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CPartConstraintTest.cpp
//
//	@doc:
//      Test for CPartConstraint
//---------------------------------------------------------------------------

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#include <stdint.h>

#include "gpos/task/CAutoTraceFlag.h"

#include "naucrates/md/CMDIdGPDB.h"

#include "gpopt/base/CColumnFactory.h"
#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/metadata/CPartConstraint.h"
#include "gpopt/mdcache/CMDAccessor.h"
#include "gpopt/mdcache/CMDCache.h"

#include "unittest/base.h"
#include "unittest/gpopt/metadata/CPartConstraintTest.h"
#include "unittest/gpopt/CConstExprEvaluatorForDates.h"
#include "unittest/gpopt/CTestUtils.h"

// number of microseconds in one day
const LINT CPartConstraintTest::lMicrosecondsPerDay = 24 * 60 * 60 * INT64_C(1000000);

// date for '01-01-2012'
const LINT CPartConstraintTest::lInternalRepresentationFor2012_01_01 =
		LINT(4383) * CPartConstraintTest::lMicrosecondsPerDay;

// date for '01-21-2012'
const LINT CPartConstraintTest::lInternalRepresentationFor2012_01_21 =
		LINT(5003) * CPartConstraintTest::lMicrosecondsPerDay;

// date for '01-22-2012'
const LINT CPartConstraintTest::lInternalRepresentationFor2012_01_22 =
		LINT(5004) * CPartConstraintTest::lMicrosecondsPerDay;

// byte representation for '01-01-2012'
const WCHAR *CPartConstraintTest::wszInternalRepresentationFor2012_01_01 =
		GPOS_WSZ_LIT("HxEAAA==");

// byte representation for '01-21-2012'
const WCHAR *CPartConstraintTest::wszInternalRepresentationFor2012_01_21 =
		GPOS_WSZ_LIT("MxEAAA==");

// byte representation for '01-22-2012'
const WCHAR *CPartConstraintTest::wszInternalRepresentationFor2012_01_22 =
		GPOS_WSZ_LIT("MhEAAA==");

//---------------------------------------------------------------------------
//	@function:
//		CPartConstraintTest::EresUnittest
//
//	@doc:
//		Unittest for part constraints
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPartConstraintTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CPartConstraintTest::EresUnittest_Basic),
		GPOS_UNITTEST_FUNC(CPartConstraintTest::EresUnittest_DateIntervals),
		};
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	// setup a file-based provider
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(memory_pool, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);
	CConstExprEvaluatorForDates *pceeval = GPOS_NEW(memory_pool) CConstExprEvaluatorForDates(memory_pool);

	// install opt context in TLS
	CAutoOptCtxt aoc(memory_pool, &mda, pceeval, CTestUtils::GetCostModel(memory_pool));

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}


//---------------------------------------------------------------------------
//	@function:
//		CPartConstraintTest::EresUnittest_Basic
//
//	@doc:
//		Basic test for subsumption, equality and overlap of part constraints
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPartConstraintTest::EresUnittest_Basic()
{
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();
	
	// setup an MD accessor
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(memory_pool, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	const IMDTypeInt4 *pmdtypeint4 = mda.PtMDType<IMDTypeInt4>(CTestUtils::m_sysidDefault);
	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	CColRef *colref = col_factory->PcrCreate(pmdtypeint4, default_type_modifier);
	
	// create a constraint col \in [1,3)
	CConstraint *pcnstr13 = PcnstrInterval(memory_pool, colref, 1 /*ulLeft*/, 3 /*ulRight*/);
	
	// create a constraint col \in [1,5)
	CConstraint *pcnstr15 = PcnstrInterval(memory_pool, colref, 1 /*ulLeft*/, 5 /*ulRight*/);
		
	CPartConstraint *ppartcnstr13Default = GPOS_NEW(memory_pool) CPartConstraint(memory_pool, pcnstr13, true /*fDefaultPartition*/, false /*is_unbounded*/);
	CPartConstraint *ppartcnstr15Default = GPOS_NEW(memory_pool) CPartConstraint(memory_pool, pcnstr15, true /*fDefaultPartition*/, false /*is_unbounded*/);

	pcnstr13->AddRef();
	CPartConstraint *ppartcnstr13NoDefault = GPOS_NEW(memory_pool) CPartConstraint(memory_pool, pcnstr13, false /*fDefaultPartition*/, false /*is_unbounded*/);

	pcnstr13->AddRef();
	CPartConstraint *ppartcnstr13DefaultUnbounded = GPOS_NEW(memory_pool) CPartConstraint(memory_pool, pcnstr13, true /*fDefaultPartition*/, true /*is_unbounded*/);

	pcnstr15->AddRef();
	CPartConstraint *ppartcnstr15DefaultUnbounded = GPOS_NEW(memory_pool) CPartConstraint(memory_pool, pcnstr15, true /*fDefaultPartition*/, true /*is_unbounded*/);

	// tests
	
	// equivalence
	GPOS_ASSERT(ppartcnstr13Default->FEquivalent(ppartcnstr13Default));
	GPOS_ASSERT(ppartcnstr13DefaultUnbounded->FEquivalent(ppartcnstr13DefaultUnbounded));
	GPOS_ASSERT(ppartcnstr13DefaultUnbounded->FEquivalent(ppartcnstr15DefaultUnbounded));
	GPOS_ASSERT(!ppartcnstr13DefaultUnbounded->FEquivalent(ppartcnstr13Default));
	GPOS_ASSERT(!ppartcnstr13Default->FEquivalent(ppartcnstr13NoDefault));
	GPOS_ASSERT(!ppartcnstr13Default->FEquivalent(ppartcnstr15Default));

	// unboundedness
	GPOS_ASSERT(!ppartcnstr13Default->IsConstraintUnbounded());
	GPOS_ASSERT(ppartcnstr13DefaultUnbounded->IsConstraintUnbounded());
	
	// subsumption & overlap
	GPOS_ASSERT(ppartcnstr13DefaultUnbounded->FSubsume(ppartcnstr13Default));
	GPOS_ASSERT(ppartcnstr13DefaultUnbounded->FSubsume(ppartcnstr15Default));
	GPOS_ASSERT(ppartcnstr15Default->FSubsume(ppartcnstr13Default));
	GPOS_ASSERT(!ppartcnstr13Default->FSubsume(ppartcnstr15Default));
	GPOS_ASSERT(ppartcnstr13Default->FOverlap(memory_pool, ppartcnstr15Default));
	GPOS_ASSERT(ppartcnstr13NoDefault->FOverlap(memory_pool, ppartcnstr13Default));

	// cleanup
	ppartcnstr13Default->Release();
	ppartcnstr15Default->Release();
	ppartcnstr13DefaultUnbounded->Release();
	ppartcnstr15DefaultUnbounded->Release();
	ppartcnstr13NoDefault->Release();

	return GPOS_OK;
}

//---------------------------------------------------------------------------
//	@function:
//		CPartConstraintTest::PcnstrInterval
//
//	@doc:
//		Create an interval constraint for the given column and interval boundaries
//
//---------------------------------------------------------------------------
CConstraint *
CPartConstraintTest::PcnstrInterval
	(
	IMemoryPool *memory_pool,
	CColRef *colref,
	ULONG ulLeft,
	ULONG ulRight
	)
{
	CExpression *pexprConstLeft = CUtils::PexprScalarConstInt4(memory_pool, ulLeft);
	CExpression *pexprConstRight = CUtils::PexprScalarConstInt4(memory_pool, ulRight);

	// AND
	DrgPexpr *pdrgpexpr = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
	pdrgpexpr->Append(CUtils::PexprScalarCmp(memory_pool, colref, pexprConstLeft, IMDType::EcmptGEq));
	pdrgpexpr->Append(CUtils::PexprScalarCmp(memory_pool, colref, pexprConstRight, IMDType::EcmptL));

	CExpression *pexpr = CUtils::PexprScalarBoolOp(memory_pool, CScalarBoolOp::EboolopAnd, pdrgpexpr);
	CConstraint *pcnstr = CConstraintInterval::PciIntervalFromScalarExpr(memory_pool, pexpr, colref);
	
	pexpr->Release();
	
	return pcnstr;
}


//---------------------------------------------------------------------------
//	@function:
//		CPartConstraintTest::EresUnittest_DateIntervals
//
//	@doc:
//		Test for subsumption, equality and overlap of part constraints on date intervals
//
//---------------------------------------------------------------------------
GPOS_RESULT
CPartConstraintTest::EresUnittest_DateIntervals()
{
	CAutoTraceFlag atf(EopttraceEnableConstantExpressionEvaluation, true /*m_bytearray_value*/);

	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();

	// setup an MD accessor
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	CMDAccessor mda(memory_pool, CMDCache::Pcache(), CTestUtils::m_sysidDefault, pmdp);

	const IMDType *pmdtype = mda.Pmdtype(&CMDIdGPDB::m_mdid_date);
	CWStringConst str(GPOS_WSZ_LIT("date_col"));
	CName name(memory_pool, &str);
	CAutoP<CColRef> colref(COptCtxt::PoctxtFromTLS()->Pcf()->PcrCreate(pmdtype, default_type_modifier, name));

	// create a date interval: ['01-01-2012', '01-21-2012')
	CWStringDynamic pstrLowerDate1(memory_pool, wszInternalRepresentationFor2012_01_01);
	CWStringDynamic pstrUpperDate1(memory_pool, wszInternalRepresentationFor2012_01_21);
	CConstraintInterval *pciFirst =
			CTestUtils::PciGenericInterval
				(
				memory_pool,
				&mda,
				CMDIdGPDB::m_mdid_date,
				colref.Value(),
				&pstrLowerDate1,
				lInternalRepresentationFor2012_01_01,
				CRange::EriIncluded,
				&pstrUpperDate1,
				lInternalRepresentationFor2012_01_21,
				CRange::EriExcluded
				);

	// create a date interval: ['01-01-2012', '01-22-2012')
	CWStringDynamic pstrLowerDate2(memory_pool, wszInternalRepresentationFor2012_01_01);
	CWStringDynamic pstrUpperDate2(memory_pool, wszInternalRepresentationFor2012_01_22);
	CConstraintInterval *pciSecond =
			CTestUtils::PciGenericInterval
				(
				memory_pool,
				&mda,
				CMDIdGPDB::m_mdid_date,
				colref.Value(),
				&pstrLowerDate2,
				lInternalRepresentationFor2012_01_01,
				CRange::EriIncluded,
				&pstrUpperDate2,
				lInternalRepresentationFor2012_01_22,
				CRange::EriExcluded
				);

	CPartConstraint *ppartcnstr1Default = GPOS_NEW(memory_pool) CPartConstraint(memory_pool, pciFirst, true /*fDefaultPartition*/, false /*is_unbounded*/);
	CPartConstraint *ppartcnstr2Default = GPOS_NEW(memory_pool) CPartConstraint(memory_pool, pciSecond, true /*fDefaultPartition*/, false /*is_unbounded*/);

	pciFirst->AddRef();
	CPartConstraint *ppartcnstr1NoDefault = GPOS_NEW(memory_pool) CPartConstraint(memory_pool, pciFirst, false /*fDefaultPartition*/, false /*is_unbounded*/);

	pciFirst->AddRef();
	CPartConstraint *ppartcnstr1DefaultUnbounded = GPOS_NEW(memory_pool) CPartConstraint(memory_pool, pciFirst, true /*fDefaultPartition*/, true /*is_unbounded*/);

	pciSecond->AddRef();
	CPartConstraint *ppartcnstr2DefaultUnbounded = GPOS_NEW(memory_pool) CPartConstraint(memory_pool, pciSecond, true /*fDefaultPartition*/, true /*is_unbounded*/);

	// tests

	// equivalence
	GPOS_ASSERT(ppartcnstr1Default->FEquivalent(ppartcnstr1Default));
	GPOS_ASSERT(ppartcnstr1DefaultUnbounded->FEquivalent(ppartcnstr1DefaultUnbounded));
	GPOS_ASSERT(ppartcnstr1DefaultUnbounded->FEquivalent(ppartcnstr2DefaultUnbounded));
	GPOS_ASSERT(!ppartcnstr1DefaultUnbounded->FEquivalent(ppartcnstr1Default));
	GPOS_ASSERT(!ppartcnstr1Default->FEquivalent(ppartcnstr1NoDefault));
	GPOS_ASSERT(!ppartcnstr1Default->FEquivalent(ppartcnstr2Default));

	// unboundedness
	GPOS_ASSERT(!ppartcnstr1Default->IsConstraintUnbounded());
	GPOS_ASSERT(ppartcnstr1DefaultUnbounded->IsConstraintUnbounded());

	// subsumption & overlap
	GPOS_ASSERT(ppartcnstr1DefaultUnbounded->FSubsume(ppartcnstr1Default));
	GPOS_ASSERT(ppartcnstr1DefaultUnbounded->FSubsume(ppartcnstr2Default));
	GPOS_ASSERT(pciSecond->FContainsInterval(memory_pool, pciFirst));
	GPOS_ASSERT(ppartcnstr2Default->FSubsume(ppartcnstr1Default));
	GPOS_ASSERT(!ppartcnstr1Default->FSubsume(ppartcnstr2Default));
	GPOS_ASSERT(ppartcnstr1Default->FOverlap(memory_pool, ppartcnstr2Default));
	GPOS_ASSERT(ppartcnstr1NoDefault->FOverlap(memory_pool, ppartcnstr1Default));

	// cleanup
	ppartcnstr1Default->Release();
	ppartcnstr2Default->Release();
	ppartcnstr1DefaultUnbounded->Release();
	ppartcnstr2DefaultUnbounded->Release();
	ppartcnstr1NoDefault->Release();

	return GPOS_OK;
}

// EOF

