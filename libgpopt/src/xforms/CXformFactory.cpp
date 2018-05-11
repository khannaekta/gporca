//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2009 Greenplum, Inc.
//
//	@filename:
//		CXformFactory.cpp
//
//	@doc:
//		Management of the global xform set
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/memory/CMemoryPoolManager.h"
#include "gpopt/xforms/xforms.h"

using namespace gpopt;

// global instance of xform factory
CXformFactory* CXformFactory::m_pxff = NULL;


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::CXformFactory
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CXformFactory::CXformFactory
	(
	IMemoryPool *memory_pool
	)
	:
	m_memory_pool(memory_pool),
	m_phmszxform(NULL),
	m_pxfsExploration(NULL),
	m_pxfsImplementation(NULL)
{
	GPOS_ASSERT(NULL != memory_pool);
	
	// null out array so dtor can be called prematurely
	for (ULONG i = 0; i < CXform::ExfSentinel; i++)
	{
		m_rgpxf[i] = NULL;
	}
	m_phmszxform = GPOS_NEW(memory_pool) HMSzXform(memory_pool);
	m_pxfsExploration = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	m_pxfsImplementation = GPOS_NEW(memory_pool) CXformSet(memory_pool);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::~CXformFactory
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CXformFactory::~CXformFactory()
{
	GPOS_ASSERT(NULL == m_pxff &&
				"Xform factory has not been shut down");

	// delete all xforms in the array
	for (ULONG i = 0; i < CXform::ExfSentinel; i++)
	{
		if (NULL == m_rgpxf[i])
		{
			// dtor called after failing to populate array
			break;
		}
		
		m_rgpxf[i]->Release();
		m_rgpxf[i] = NULL;
	}

	m_phmszxform->Release();
	m_pxfsExploration->Release();
	m_pxfsImplementation->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::Add
//
//	@doc:
//		Add a given xform to the array; enforce the order in which they
//		are added for readability/debugging
//
//---------------------------------------------------------------------------
void
CXformFactory::Add
	(
	CXform *pxform
	)
{	
	GPOS_ASSERT(NULL != pxform);
	CXform::EXformId exfid = pxform->Exfid();
	
	GPOS_ASSERT_IMP(0 < exfid, m_rgpxf[exfid - 1] != NULL &&
					"Incorrect order of instantiation");
	GPOS_ASSERT(NULL == m_rgpxf[exfid]);

	m_rgpxf[exfid] = pxform;

	// create name -> xform mapping
	ULONG length = clib::Strlen(pxform->SzId());
	CHAR *szXformName = GPOS_NEW_ARRAY(m_memory_pool, CHAR, length + 1);
	clib::Strncpy(szXformName, pxform->SzId(), length + 1);

#ifdef GPOS_DEBUG
		BOOL fInserted =
#endif
		m_phmszxform->Insert(szXformName, pxform);
	GPOS_ASSERT(fInserted);

	CXformSet *xform_set = m_pxfsExploration;
	if (pxform->FImplementation())
	{
		xform_set = m_pxfsImplementation;
	}
#ifdef GPOS_DEBUG
	GPOS_ASSERT_IMP(pxform->FExploration(), xform_set == m_pxfsExploration);
	GPOS_ASSERT_IMP(pxform->FImplementation(), xform_set == m_pxfsImplementation);
	BOOL fSet =
#endif // GPOS_DEBUG
		xform_set->ExchangeSet(exfid);

	GPOS_ASSERT(!fSet);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::Instantiate
//
//	@doc:
//		Construct all xforms
//
//---------------------------------------------------------------------------
void
CXformFactory::Instantiate()
{	
	Add(GPOS_NEW(m_memory_pool) CXformProject2ComputeScalar(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformExpandNAryJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformExpandNAryJoinMinCard(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformExpandNAryJoinDP(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformGet2TableScan(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformIndexGet2IndexScan(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformDynamicGet2DynamicTableScan(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformDynamicIndexGet2DynamicIndexScan(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementSequence(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementConstTableGet(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformUnnestTVF(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementTVF(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementTVFNoArgs(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSelect2Filter(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSelect2IndexGet(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSelect2DynamicIndexGet(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSelect2PartialDynamicIndexGet(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSimplifySelectWithSubquery(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSimplifyProjectWithSubquery(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSelect2Apply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformProject2Apply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformGbAgg2Apply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSubqJoin2Apply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSubqNAryJoin2Apply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoin2IndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoin2DynamicIndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerApplyWithOuterKey2InnerJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoin2NLJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementIndexApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoin2HashJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerApply2InnerJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerApply2InnerJoinNoCorrelations(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementInnerCorrelatedApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftOuterApply2LeftOuterJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftOuterApply2LeftOuterJoinNoCorrelations(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementLeftOuterCorrelatedApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftSemiApply2LeftSemiJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftSemiApplyWithExternalCorrs2InnerJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftSemiApply2LeftSemiJoinNoCorrelations(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftAntiSemiApply2LeftAntiSemiJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftAntiSemiApply2LeftAntiSemiJoinNoCorrelations(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotIn(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftAntiSemiApplyNotIn2LeftAntiSemiJoinNotInNoCorrelations(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformPushDownLeftOuterJoin (m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSimplifyLeftOuterJoin (m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftOuterJoin2NLJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftOuterJoin2HashJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftSemiJoin2NLJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftSemiJoin2HashJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftAntiSemiJoin2CrossProduct(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftAntiSemiJoinNotIn2CrossProduct(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftAntiSemiJoin2NLJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftAntiSemiJoinNotIn2NLJoinNotIn(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftAntiSemiJoin2HashJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftAntiSemiJoinNotIn2HashJoinNotIn(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformGbAgg2HashAgg(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformGbAgg2StreamAgg(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformGbAgg2ScalarAgg(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformGbAggDedup2HashAggDedup(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformGbAggDedup2StreamAggDedup(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementLimit(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformIntersectAll2LeftSemiJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformIntersect2Join(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformDifference2LeftAntiSemiJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformDifferenceAll2LeftAntiSemiJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformUnion2UnionAll(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementUnionAll(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInsert2DML(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformDelete2DML(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformUpdate2DML(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementDML(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementRowTrigger(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementSplit(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformJoinCommutativity(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformJoinAssociativity(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSemiJoinSemiJoinSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSemiJoinAntiSemiJoinSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSemiJoinAntiSemiJoinNotInSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSemiJoinInnerJoinSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformAntiSemiJoinAntiSemiJoinSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformAntiSemiJoinAntiSemiJoinNotInSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformAntiSemiJoinSemiJoinSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformAntiSemiJoinInnerJoinSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformAntiSemiJoinNotInAntiSemiJoinSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformAntiSemiJoinNotInAntiSemiJoinNotInSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformAntiSemiJoinNotInSemiJoinSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformAntiSemiJoinNotInInnerJoinSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoinSemiJoinSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoinAntiSemiJoinSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoinAntiSemiJoinNotInSwap(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftSemiJoin2InnerJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftSemiJoin2InnerJoinUnderGb(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftSemiJoin2CrossProduct(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSplitLimit(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSimplifyGbAgg(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformCollapseGbAgg(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformPushGbBelowJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformPushGbDedupBelowJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformPushGbWithHavingBelowJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformPushGbBelowUnion(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformPushGbBelowUnionAll(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSplitGbAgg(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSplitGbAggDedup(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSplitDQA(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSequenceProject2Apply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementSequenceProject(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementAssert(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformCTEAnchor2Sequence(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformCTEAnchor2TrivialSelect(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInlineCTEConsumer(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInlineCTEConsumerUnderSelect(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementCTEProducer(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementCTEConsumer(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformExpandFullOuterJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformExternalGet2ExternalScan(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSelect2BitmapBoolOp(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformSelect2DynamicBitmapBoolOp(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementBitmapTableGet(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementDynamicBitmapTableGet(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoin2PartialDynamicIndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftOuter2InnerUnionAllLeftAntiSemiJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementLeftSemiCorrelatedApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementLeftSemiCorrelatedApplyIn(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementLeftAntiSemiCorrelatedApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementLeftAntiSemiCorrelatedApplyNotIn(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftSemiApplyIn2LeftSemiJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftSemiApplyInWithExternalCorrs2InnerJoin(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftSemiApplyIn2LeftSemiJoinNoCorrelations(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoin2BitmapIndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformImplementPartitionSelector(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformMaxOneRow2Assert(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoinWithInnerSelect2IndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoinWithInnerSelect2DynamicIndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoinWithInnerSelect2PartialDynamicIndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoin2DynamicBitmapIndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoinWithInnerSelect2BitmapIndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformInnerJoinWithInnerSelect2DynamicBitmapIndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformGbAggWithMDQA2Join(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformCollapseProject(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformRemoveSubqDistinct(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftOuterJoin2BitmapIndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftOuterJoin2IndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftOuterJoinWithInnerSelect2BitmapIndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformLeftOuterJoinWithInnerSelect2IndexGetApply(m_memory_pool));
	Add(GPOS_NEW(m_memory_pool) CXformExpandNAryJoinGreedy(m_memory_pool));

	GPOS_ASSERT(NULL != m_rgpxf[CXform::ExfSentinel - 1] &&
				"Not all xforms have been instantiated");
}

						  
//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::Pxf
//
//	@doc:
//		Accessor of xform array
//
//---------------------------------------------------------------------------
CXform*
CXformFactory::Pxf
	(
	CXform::EXformId exfid
	)
	const
{
	CXform *pxf = m_rgpxf[exfid];
	GPOS_ASSERT(pxf->Exfid() == exfid);
	
	return pxf;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::Pxf
//
//	@doc:
//		Accessor by xform name
//
//---------------------------------------------------------------------------
CXform *
CXformFactory::Pxf
	(
	const CHAR *szXformName
	)
	const
{
	return m_phmszxform->Find(szXformName);
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::Init
//
//	@doc:
//		Initializes global instance
//
//---------------------------------------------------------------------------
GPOS_RESULT
CXformFactory::Init()
{
	GPOS_ASSERT(NULL == Pxff() &&
			    "Xform factory was already initialized");

	GPOS_RESULT eres = GPOS_OK;

	// create xform factory memory pool
	IMemoryPool *memory_pool = CMemoryPoolManager::GetMemoryPoolMgr()->Create
							(
							CMemoryPoolManager::EatTracker,
							true /*fThreadSafe*/,
							gpos::ullong_max
							);
	GPOS_TRY
	{
		// create xform factory instance
		m_pxff = GPOS_NEW(memory_pool) CXformFactory(memory_pool);
	}
	GPOS_CATCH_EX(ex)
	{
		// destroy memory pool if global instance was not created
		CMemoryPoolManager::GetMemoryPoolMgr()->Destroy(memory_pool);
		m_pxff = NULL;

		if (GPOS_MATCH_EX(ex, CException::ExmaSystem, CException::ExmiOOM))
		{
			eres = GPOS_OOM;
		}
		else
		{
			eres = GPOS_FAILED;
		}

		return eres;
	}
	GPOS_CATCH_END;

	// instantiating the factory
	m_pxff->Instantiate();

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CXformFactory::Shutdown
//
//	@doc:
//		Cleans up allocated memory pool
//
//---------------------------------------------------------------------------
void
CXformFactory::Shutdown()
{
	CXformFactory *pxff = CXformFactory::Pxff();

	GPOS_ASSERT(NULL != pxff &&
			    "Xform factory has not been initialized");

	IMemoryPool *memory_pool = pxff->m_memory_pool;

	// destroy xform factory
	CXformFactory::m_pxff = NULL;
	GPOS_DELETE(pxff);

	// release allocated memory pool
	CMemoryPoolManager::GetMemoryPoolMgr()->Destroy(memory_pool);
}


// EOF

