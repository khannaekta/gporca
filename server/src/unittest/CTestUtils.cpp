//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CTestUtils.cpp
//
//	@doc:
//		Implementation of test utility functions
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/common/CAutoP.h"
#include "gpos/error/CMessage.h"
#include "gpos/io/COstreamString.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/string/CWStringDynamic.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "gpopt/base/CConstraintInterval.h"
#include "gpopt/base/CCTEMap.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CDefaultComparator.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/eval/CConstExprEvaluatorDefault.h"
#include "gpopt/eval/IConstExprEvaluator.h"
#include "gpopt/operators/ops.h"
#include "gpopt/operators/CScalarSubqueryAll.h"
#include "gpopt/operators/CScalarSubqueryAny.h"
#include "gpopt/xforms/CSubqueryHandler.h"

#include "naucrates/md/CMDIdGPDB.h"
#include "naucrates/md/CMDProviderMemory.h"
#include "naucrates/md/IMDTypeBool.h"
#include "naucrates/md/CMDTypeGenericGPDB.h"
#include "naucrates/md/IMDTypeInt2.h"
#include "naucrates/md/IMDTypeInt4.h"
#include "naucrates/md/IMDTypeInt8.h"
#include "naucrates/md/IMDScalarOp.h"
#include "naucrates/base/CDatumInt2GPDB.h"
#include "naucrates/base/CDatumInt4GPDB.h"
#include "naucrates/base/CDatumInt8GPDB.h"
#include "naucrates/base/CDatumBoolGPDB.h"

#include "naucrates/dxl/operators/CDXLDatumGeneric.h"
#include "naucrates/dxl/operators/CDXLDatumStatsDoubleMappable.h"
#include "naucrates/dxl/operators/CDXLDatumStatsLintMappable.h"

#include "gpopt/exception.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/CAutoOptCtxt.h"
#include "gpopt/base/CColRefSetIter.h"
#include "gpopt/base/CPrintPrefix.h"
#include "gpopt/engine/CEngine.h"
#include "gpopt/engine/CEnumeratorConfig.h"
#include "gpopt/engine/CStatisticsConfig.h"
#include "gpopt/optimizer/COptimizerConfig.h"
#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/mdcache/CMDCache.h"
#include "gpopt/minidump/CMinidumperUtils.h"
#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXformUtils.h"
#include "gpopt/translate/CTranslatorDXLToExpr.h"
#include "gpopt/translate/CTranslatorExprToDXL.h"

#include "naucrates/base/CQueryToDXLResult.h"
#include "naucrates/dxl/CDXLUtils.h"

#include "unittest/base.h"
#include "unittest/gpopt/CSubqueryTestUtils.h"
#include "unittest/gpopt/CTestUtils.h"

#define GPOPT_SEGMENT_COUNT 2 // number segments for testing

using namespace gpopt;

// static variable initialization
// default source system id
CSystemId 
CTestUtils::m_sysidDefault(IMDId::EmdidGPDB, GPOS_WSZ_STR_LENGTH("GPDB"));


// XSD path
const CHAR *
CTestUtils::m_szXSDPath = "http://greenplum.com/dxl/2010/12/ dxl.xsd";

// metadata file
const CHAR *
CTestUtils::m_szMDFileName = "../data/dxl/metadata/md.xml";

// provider file
CMDProviderMemory *
CTestUtils::m_pmdpf = NULL;

// local memory pool
IMemoryPool *
CTestUtils::m_memory_pool = NULL;

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::CTestSetup::PmdpSetupFileBasedProvider
//
//	@doc:
//		set up a file based provider
//
//---------------------------------------------------------------------------
CMDProviderMemory *
CTestUtils::CTestSetup::PmdpSetupFileBasedProvider()
{
	CMDProviderMemory *pmdp = CTestUtils::m_pmdpf;
	pmdp->AddRef();
	return pmdp;
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::CTestSetup::CTestSetup
//
//	@doc:
//		ctor
//
//---------------------------------------------------------------------------
CTestUtils::CTestSetup::CTestSetup()
	:
	m_amp(),
	m_mda(m_amp.Pmp(), CMDCache::Pcache(), CTestUtils::m_sysidDefault, PmdpSetupFileBasedProvider()),
	// install opt context in TLS
	m_aoc
		(
		m_amp.Pmp(),
		&m_mda,
		NULL,  /* pceeval */
		CTestUtils::GetCostModel(m_amp.Pmp())
		)
{
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::InitProviderFile
//
//	@doc:
//		Initialize provider file;
//		called before unittests start
//
//---------------------------------------------------------------------------
void
CTestUtils::InitProviderFile
	(
	IMemoryPool *memory_pool
	)
{
	GPOS_ASSERT(NULL == m_memory_pool);
	GPOS_ASSERT(NULL != memory_pool);

	m_memory_pool = memory_pool;
	m_pmdpf = GPOS_NEW(m_memory_pool) CMDProviderMemory(m_memory_pool, m_szMDFileName);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::DestroyMDProvider
//
//	@doc:
//		Destroy metadata file;
//		called after all unittests complete
//
//---------------------------------------------------------------------------
void
CTestUtils::DestroyMDProvider()
{
	GPOS_ASSERT(NULL != m_memory_pool);

	CRefCount::SafeRelease(m_pmdpf);

	// release local memory pool
	CMemoryPoolManager::GetMemoryPoolMgr()->Destroy(m_memory_pool);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PtabdescPlainWithColNameFormat
//
//	@doc:
//		Generate a plain table descriptor, where the column names are generated
//		using a format string containing %d
//
//---------------------------------------------------------------------------


CTableDescriptor *
CTestUtils::PtabdescPlainWithColNameFormat
	(
	IMemoryPool *memory_pool,
	ULONG num_cols,
	IMDId *mdid,
	const WCHAR *wszColNameFormat,
	const CName &nameTable,
	BOOL is_nullable // define nullable columns
	)
{
	GPOS_ASSERT(0 < num_cols);

	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	const IMDTypeInt4 *pmdtypeint4 = md_accessor->PtMDType<IMDTypeInt4>(CTestUtils::m_sysidDefault);
	CWStringDynamic *str_name = GPOS_NEW(memory_pool) CWStringDynamic(memory_pool);
	CTableDescriptor *ptabdesc = GPOS_NEW(memory_pool) CTableDescriptor
											(
											memory_pool, 
											mdid, 
											nameTable, 
											false, // convert_hash_to_random
											IMDRelation::EreldistrRandom,
											IMDRelation::ErelstorageHeap, 
											0 // ulExecuteAsUser
											);

	for (ULONG i = 0; i < num_cols; i++)
	{
		str_name->Reset();
		str_name->AppendFormat(wszColNameFormat, i);

		// create a shallow constant string to embed in a name
		CWStringConst strName(str_name->GetBuffer());
		CName nameColumnInt(&strName);

		CColumnDescriptor *pcoldescInt = GPOS_NEW(memory_pool) CColumnDescriptor(memory_pool, pmdtypeint4, default_type_modifier, nameColumnInt, i + 1, is_nullable);
		ptabdesc->AddColumn(pcoldescInt);
	}

	GPOS_DELETE(str_name);

	return ptabdesc;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PtabdescPlain
//
//	@doc:
//		Generate a plain table descriptor
//
//---------------------------------------------------------------------------
CTableDescriptor *
CTestUtils::PtabdescPlain
	(
	IMemoryPool *memory_pool,
	ULONG num_cols,
	IMDId *mdid,
	const CName &nameTable,
	BOOL is_nullable // define nullable columns
	)
{
	return PtabdescPlainWithColNameFormat(
			memory_pool, num_cols, mdid, GPOS_WSZ_LIT("column_%04d"), nameTable, is_nullable);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PtabdescCreate
//
//	@doc:
//		Generate a table descriptor
//
//---------------------------------------------------------------------------
CTableDescriptor *
CTestUtils::PtabdescCreate
	(
	IMemoryPool *memory_pool,
	ULONG num_cols,
	IMDId *mdid,
	const CName &nameTable,
	BOOL fPartitioned
	)
{
	CTableDescriptor *ptabdesc = PtabdescPlain(memory_pool, num_cols, mdid, nameTable);

	if (fPartitioned)
	{
		ptabdesc->AddPartitionColumn(0);
	}
	
	// create a keyset containing the first column
	CBitSet *pbs = GPOS_NEW(memory_pool) CBitSet(memory_pool, num_cols);
	pbs->ExchangeSet(0);
#ifdef GPOS_DEBUG
	BOOL fSuccess =
#endif // GPOS_DEBUG
		ptabdesc->FAddKeySet(pbs);
	GPOS_ASSERT(fSuccess);

	return ptabdesc;
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalGet
//
//	@doc:
//		Generate a get expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalGet
	(
	IMemoryPool *memory_pool,
	CTableDescriptor *ptabdesc,
	const CWStringConst *pstrTableAlias
	)
{
	GPOS_ASSERT(NULL != ptabdesc);

	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,                   
					GPOS_NEW(memory_pool) CLogicalGet
								(
								memory_pool,
								GPOS_NEW(memory_pool) CName(memory_pool, CName(pstrTableAlias)),
								ptabdesc
								)
					);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalGetNullable
//
//	@doc:
//		Generate a get expression over table with nullable columns
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalGetNullable
	(
	IMemoryPool *memory_pool,
	OID oidTable,
	const CWStringConst *str_table_name,
	const CWStringConst *pstrTableAlias
	)
{
	CWStringConst strName(str_table_name->GetBuffer());
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(oidTable, 1, 1);
	CTableDescriptor *ptabdesc = CTestUtils::PtabdescPlain(memory_pool, 3, mdid, CName(&strName), true /*is_nullable*/);
	CWStringConst strAlias(pstrTableAlias->GetBuffer());

	return PexprLogicalGet(memory_pool, ptabdesc, &strAlias);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalGet
//
//	@doc:
//		Generate a get expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalGet
	(
	IMemoryPool *memory_pool,
	CWStringConst *str_table_name,
	CWStringConst *pstrTableAlias,
	ULONG ulTableId
	)
{
	CTableDescriptor *ptabdesc = PtabdescCreate
									(
									memory_pool,
									GPOPT_TEST_REL_WIDTH,
									GPOS_NEW(memory_pool) CMDIdGPDB(ulTableId, 1, 1),
									CName(str_table_name)
									);

	CWStringConst strAlias(pstrTableAlias->GetBuffer());
	return PexprLogicalGet(memory_pool, ptabdesc, &strAlias);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalGet
//
//	@doc:
//		Generate a randomized get expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalGet
	(
	IMemoryPool *memory_pool
	)
{
	CWStringConst strName(GPOS_WSZ_LIT("BaseTable"));
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID, 1, 1);
	CTableDescriptor *ptabdesc = PtabdescCreate(memory_pool, 3, mdid, CName(&strName));
	CWStringConst strAlias(GPOS_WSZ_LIT("BaseTableAlias"));

	return PexprLogicalGet(memory_pool, ptabdesc, &strAlias);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalExternalGet
//
//	@doc:
//		Generate a randomized external get expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalExternalGet
	(
	IMemoryPool *memory_pool
	)
{
	CWStringConst strName(GPOS_WSZ_LIT("ExternalTable"));
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID, 1, 1);
	CTableDescriptor *ptabdesc = PtabdescCreate(memory_pool, 3, mdid, CName(&strName));
	CWStringConst strAlias(GPOS_WSZ_LIT("ExternalTableAlias"));

	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalExternalGet
								(
								memory_pool,
								GPOS_NEW(memory_pool) CName(memory_pool, &strAlias),
								ptabdesc
								)
					);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalGetPartitioned
//
//	@doc:
//		Generate a randomized get expression for a partitioned table
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalGetPartitioned
	(
	IMemoryPool *memory_pool
	)
{
	ULONG ulAttributes = 2;
	CWStringConst strName(GPOS_WSZ_LIT("PartTable"));
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID_PARTITIONED);
	CTableDescriptor *ptabdesc = PtabdescCreate(memory_pool, ulAttributes, mdid, CName(&strName), true /*fPartitioned*/);
	CWStringConst strAlias(GPOS_WSZ_LIT("PartTableAlias"));

	return PexprLogicalGet(memory_pool, ptabdesc, &strAlias);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalDynamicGetWithIndexes
//
//	@doc:
//		Generate a randomized get expression for a partitioned table
//		with indexes
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalDynamicGetWithIndexes
	(
	IMemoryPool *memory_pool
	)
{
	ULONG ulAttributes = 2;
	CWStringConst strName(GPOS_WSZ_LIT("P1"));
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID_PARTITIONED_WITH_INDEXES);
	CTableDescriptor *ptabdesc = PtabdescCreate(memory_pool, ulAttributes, mdid, CName(&strName), true /*fPartitioned*/);
	CWStringConst strAlias(GPOS_WSZ_LIT("P1Alias"));

	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalDynamicGet
								(
								memory_pool,
								GPOS_NEW(memory_pool) CName(memory_pool, CName(&strAlias)),
								ptabdesc,
								0 // ulPartIndex
								)
					);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelect
//
//	@doc:
//		Generate a Select expression with a random equality predicate
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelect
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != pexpr);

	// get any two columns
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput();
	CColRef *pcrLeft =  pcrs->PcrAny();
	CColRef *pcrRight = pcrs->PcrAny();
	CExpression *pexprPredicate = CUtils::PexprScalarEqCmp(memory_pool, pcrLeft, pcrRight);

	return CUtils::PexprLogicalSelect(memory_pool, pexpr, pexprPredicate);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelect
//
//	@doc:
//		Generate select expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelect
	(
	IMemoryPool *memory_pool,
	CWStringConst *str_table_name,
	CWStringConst *pstrTableAlias,
	ULONG ulTableId
	)
{
	CExpression *pexprGet = PexprLogicalGet(memory_pool, str_table_name, pstrTableAlias, ulTableId);
	return PexprLogicalSelect(memory_pool, pexprGet);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelect
//
//	@doc:
//		Generate randomized Select expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelect
	(
	IMemoryPool *memory_pool
	)
{
	return PexprLogicalSelect(memory_pool, PexprLogicalGet(memory_pool));
}

//---------------------------------------------------------------------------
//	@function:
// 		CTestUtils::PexprLogicalSelectWithContradiction
//
//	@doc:
//		Generate a select expression with a contradiction
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectWithContradiction
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexpr = PexprLogicalSelect(memory_pool, PexprLogicalGet(memory_pool));
	// get any column
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput();
	CColRef *colref =  pcrs->PcrAny();

	CExpression *pexprConstFirst = CUtils::PexprScalarConstInt4(memory_pool, 3 /*val*/);
	CExpression *pexprPredFirst = CUtils::PexprScalarEqCmp(memory_pool, colref, pexprConstFirst);

	CExpression *pexprConstSecond = CUtils::PexprScalarConstInt4(memory_pool, 5 /*val*/);
	CExpression *pexprPredSecond = CUtils::PexprScalarEqCmp(memory_pool, colref, pexprConstSecond);

	CExpression *pexprPredicate = CPredicateUtils::PexprConjunction(memory_pool, pexprPredFirst, pexprPredSecond);
	pexprPredFirst->Release();
	pexprPredSecond->Release();

	return CUtils::PexprLogicalSelect(memory_pool, pexpr, pexprPredicate);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectPartitioned
//
//	@doc:
//		Generate a randomized select expression over a partitioned table
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectPartitioned
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexprGet = PexprLogicalGetPartitioned(memory_pool);

	// extract first partition key
	CLogicalGet *popGet = CLogicalGet::PopConvert(pexprGet->Pop());
	const ColRefArrays *pdrgpdrgpcr = popGet->PdrgpdrgpcrPartColumns();

	GPOS_ASSERT(pdrgpdrgpcr != NULL);
	GPOS_ASSERT(0 < pdrgpdrgpcr->Size());
	ColRefArray *colref_array = (*pdrgpdrgpcr)[0];
	GPOS_ASSERT(1 == colref_array->Size());
	CColRef *pcrPartKey = (*colref_array)[0];

	// construct a comparison pk = 5
	INT val = 5;
	CExpression *pexprScalar = CUtils::PexprScalarEqCmp
										(
										memory_pool,
										CUtils::PexprScalarIdent(memory_pool, pcrPartKey),
										CUtils::PexprScalarConstInt4(memory_pool, val)
										);

	return CUtils::PexprLogicalSelect
						(
						memory_pool,
						pexprGet,
						pexprScalar
						);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalAssert
//
//	@doc:
//		Generate randomized Assert expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalAssert
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexprGet = PexprLogicalGet(memory_pool);
	
	// get any two columns
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprGet->PdpDerive())->PcrsOutput();
	CColRef *pcrLeft =  pcrs->PcrAny();
	CColRef *pcrRight = pcrs->PcrAny();
	CExpression *pexprPredicate = CUtils::PexprScalarEqCmp(memory_pool, pcrLeft, pcrRight);
	
	CWStringConst *pstrErrMsg = CXformUtils::PstrErrorMessage(memory_pool, gpos::CException::ExmaSQL, gpos::CException::ExmiSQLTest, GPOS_WSZ_LIT("Test msg"));
	CExpression *pexprAssertConstraint = GPOS_NEW(memory_pool) CExpression
											(
											memory_pool,
											GPOS_NEW(memory_pool) CScalarAssertConstraint(memory_pool, pstrErrMsg),
											pexprPredicate
											);
	CExpression *pexprAssertPredicate = GPOS_NEW(memory_pool) CExpression
												(
												memory_pool,
												GPOS_NEW(memory_pool) CScalarAssertConstraintList(memory_pool),
												pexprAssertConstraint
												);
		
	CLogicalAssert *popAssert = 
			GPOS_NEW(memory_pool) CLogicalAssert
						(
						memory_pool, 
						GPOS_NEW(memory_pool) CException(gpos::CException::ExmaSQL, gpos::CException::ExmiSQLTest)						
						);
	
	return GPOS_NEW(memory_pool) CExpression(memory_pool, popAssert, pexprGet, pexprAssertPredicate);
}

//---------------------------------------------------------------------------
//		CTestUtils::Pexpr3WayJoinPartitioned
//
//	@doc:
//		Generate a 3-way join including a partitioned table
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::Pexpr3WayJoinPartitioned
	(
	IMemoryPool *memory_pool
	)
{
	return
		PexprLogicalJoin<CLogicalInnerJoin>(memory_pool, PexprLogicalGet(memory_pool), PexprJoinPartitionedOuter<CLogicalInnerJoin>(memory_pool));

}

//---------------------------------------------------------------------------
//		CTestUtils::Pexpr4WayJoinPartitioned
//
//	@doc:
//		Generate a 4-way join including a partitioned table
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::Pexpr4WayJoinPartitioned
	(
	IMemoryPool *memory_pool
	)
{
	return
		PexprLogicalJoin<CLogicalInnerJoin>(memory_pool, PexprLogicalGet(memory_pool), Pexpr3WayJoinPartitioned(memory_pool));
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectWithNestedAnd
//
//	@doc:
//		Generate a random select expression with nested AND tree
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectWithNestedAnd
	(
	IMemoryPool *memory_pool
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CExpression *pexprGet = PexprLogicalGet(memory_pool);
	CExpression *pexprPred = PexprScalarNestedPreds(memory_pool, pexprGet, CScalarBoolOp::EboolopAnd);

	return CUtils::PexprLogicalSelect(memory_pool, pexprGet, pexprPred);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectWithNestedOr
//
//	@doc:
//		Generate a random select expression with nested OR tree
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectWithNestedOr
	(
	IMemoryPool *memory_pool
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CExpression *pexprGet = PexprLogicalGet(memory_pool);
	CExpression *pexprPred = PexprScalarNestedPreds(memory_pool, pexprGet, CScalarBoolOp::EboolopOr);

	return CUtils::PexprLogicalSelect(memory_pool, pexprGet, pexprPred);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectWithEvenNestedNot
//
//	@doc:
//		Generate a random select expression with an even number of
//		nested NOT nodes
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectWithEvenNestedNot
	(
	IMemoryPool *memory_pool
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CExpression *pexprGet = PexprLogicalGet(memory_pool);
	CExpression *pexprPred = PexprScalarNestedPreds(memory_pool, pexprGet, CScalarBoolOp::EboolopNot);

	return CUtils::PexprLogicalSelect(memory_pool, pexprGet, pexprPred);
} 


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprScIdentCmpScIdent
//
//	@doc:
//		Generate a scalar expression comparing scalar identifiers
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprScIdentCmpScIdent
	(
	IMemoryPool *memory_pool,
	CExpression *pexprLeft,
	CExpression *pexprRight,
	IMDType::ECmpType cmp_type
	)
{
	GPOS_ASSERT(NULL != pexprLeft);
	GPOS_ASSERT(NULL != pexprRight);
	GPOS_ASSERT(cmp_type <= IMDType::EcmptOther);

	CColRefSet *pcrsLeft = CDrvdPropRelational::GetRelationalProperties(pexprLeft->PdpDerive())->PcrsOutput();
	CColRef *pcrLeft =  pcrsLeft->PcrAny();

	CColRefSet *pcrsRight = CDrvdPropRelational::GetRelationalProperties(pexprRight->PdpDerive())->PcrsOutput();
	CColRef *pcrRight =  pcrsRight->PcrAny();

	CExpression *pexprPred = CUtils::PexprScalarCmp(memory_pool, pcrLeft, pcrRight, cmp_type);

	return pexprPred;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprScIdentCmpConst
//
//	@doc:
//		Generate a scalar expression comparing scalar identifier to a constant
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprScIdentCmpConst
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	IMDType::ECmpType cmp_type,
	ULONG ulVal
	)
{
	GPOS_ASSERT(NULL != pexpr);

	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput();
	CColRef *pcrLeft =  pcrs->PcrAny();
	CExpression *pexprUl = CUtils::PexprScalarConstInt4(memory_pool, ulVal);

	CExpression *pexprPred = CUtils::PexprScalarCmp(memory_pool, pcrLeft, pexprUl, cmp_type);

	return pexprPred;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectCmpToConst
//
//	@doc:
//		Generate a Select expression with an equality predicate on the first
//		column and a constant
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectCmpToConst
	(
	IMemoryPool *memory_pool
	)
{
	// generate a get expression
	CExpression *pexpr = PexprLogicalGet(memory_pool);
	CExpression *pexprPred = PexprScIdentCmpConst(memory_pool, pexpr, IMDType::EcmptEq /* cmp_type */, 10 /* ulVal */);

	return CUtils::PexprLogicalSelect(memory_pool, pexpr, pexprPred);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectArrayCmp
//
//	@doc:
//		Generate a Select expression with an array compare
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectArrayCmp
	(
	IMemoryPool *memory_pool
	)
{
	return PexprLogicalSelectArrayCmp(memory_pool, CScalarArrayCmp::EarrcmpAny, IMDType::EcmptEq);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectArrayCmp
//
//	@doc:
//		Generate a Select expression with an array compare. Takes an enum for
//		the type of array comparison (ANY or ALL) and an enum for the comparator
//		type (=, !=, <, etc).
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectArrayCmp
	(
	IMemoryPool *memory_pool,
	CScalarArrayCmp::EArrCmpType earrcmptype,
	IMDType::ECmpType ecmptype
	)
{
	const ULONG ulArraySize = 5;
	IntPtrArray *pdrgpiVals = GPOS_NEW(memory_pool) IntPtrArray(memory_pool);
	for (ULONG val = 0; val < ulArraySize; val++)
	{
		pdrgpiVals->Append(GPOS_NEW(memory_pool) INT(val));
	}
	CExpression *pexprSelect = PexprLogicalSelectArrayCmp(memory_pool, earrcmptype, ecmptype, pdrgpiVals);
	pdrgpiVals->Release();
	return pexprSelect;
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectArrayCmp
//
//	@doc:
//		Generate a Select expression with an array compare. Takes an enum for
//		the type of array comparison (ANY or ALL) and an enum for the comparator
//		type (=, !=, <, etc). The array will be populated with the given integer
//		values.
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectArrayCmp
	(
	IMemoryPool *memory_pool,
	CScalarArrayCmp::EArrCmpType earrcmptype,
	IMDType::ECmpType ecmptype,
	const IntPtrArray *pdrgpiVals
	)
{
	GPOS_ASSERT(CScalarArrayCmp::EarrcmpSentinel > earrcmptype);
	GPOS_ASSERT(IMDType::EcmptOther > ecmptype);

	// generate a get expression
	CExpression *pexprGet = PexprLogicalGet(memory_pool);

	// get the first column
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprGet->PdpDerive())->PcrsOutput();
	CColRef *colref =  pcrs->PcrAny();

	// construct an array of integers
	ExpressionArray *pdrgpexprArrayElems = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	
	const ULONG ulValsLength = pdrgpiVals->Size();
	for (ULONG ul = 0; ul < ulValsLength; ul++)
	{
		CExpression *pexprArrayElem = CUtils::PexprScalarConstInt4(memory_pool, *(*pdrgpiVals)[ul]);
		pdrgpexprArrayElems->Append(pexprArrayElem);
	}

	return CUtils::PexprLogicalSelect
				(
				memory_pool,
				pexprGet,
				CUtils::PexprScalarArrayCmp(memory_pool, earrcmptype, ecmptype, pdrgpexprArrayElems, colref)
				);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectWithOddNestedNot
//
//	@doc:
//		Generate a random select expression with an odd number of
//		nested NOT nodes
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectWithOddNestedNot
	(
	IMemoryPool *memory_pool
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CExpression *pexprGet = PexprLogicalGet(memory_pool);
	CExpression *pexprPred = PexprScalarNestedPreds(memory_pool, pexprGet, CScalarBoolOp::EboolopNot);
	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexpr->Append(pexprPred);
	CExpression *pexprNot =  CUtils::PexprScalarBoolOp(memory_pool, CScalarBoolOp::EboolopNot, pdrgpexpr);

	return CUtils::PexprLogicalSelect(memory_pool, pexprGet, pexprNot);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectWithNestedAndOrNot
//
//	@doc:
//		Generate a random select expression with nested AND-OR-NOT predicate
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectWithNestedAndOrNot
	(
	IMemoryPool *memory_pool
	)
{
	GPOS_ASSERT(NULL != memory_pool);

	CExpression *pexprGet = PexprLogicalGet(memory_pool);
	CExpression *pexprPredAnd = PexprScalarNestedPreds(memory_pool, pexprGet, CScalarBoolOp::EboolopAnd);
	CExpression *pexprPredOr = PexprScalarNestedPreds(memory_pool, pexprGet, CScalarBoolOp::EboolopOr);
	CExpression *pexprPredNot = PexprScalarNestedPreds(memory_pool, pexprGet, CScalarBoolOp::EboolopNot);

	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexpr->Append(pexprPredAnd);
	pdrgpexpr->Append(pexprPredOr);
	pdrgpexpr->Append(pexprPredNot);
	CExpression *pexprOr =  CUtils::PexprScalarBoolOp(memory_pool, CScalarBoolOp::EboolopOr, pdrgpexpr);
	CExpression *pexprPred =  CUtils::PexprNegate(memory_pool, pexprOr);

	return CUtils::PexprLogicalSelect(memory_pool, pexprGet, pexprPred);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSubqueryWithConstTableGet
//
//	@doc:
//		Generate Select expression with Any subquery predicate over a const
//		table get
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSubqueryWithConstTableGet
	(
	IMemoryPool *memory_pool,
	COperator::EOperatorId op_id
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(COperator::EopScalarSubqueryAny == op_id ||
				COperator::EopScalarSubqueryAll == op_id);

	CWStringConst strNameR(GPOS_WSZ_LIT("Rel1"));
	CMDIdGPDB *pmdidR = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_TEST_REL_OID1, 1, 1);
	CTableDescriptor *ptabdescR = PtabdescCreate(memory_pool, 3 /*num_cols*/, pmdidR, CName(&strNameR));

	CExpression *pexprOuter = PexprLogicalGet(memory_pool, ptabdescR, &strNameR);
	CExpression *pexprConstTableGet = PexprConstTableGet(memory_pool, 3 /* ulElements */);

	// get random columns from inner expression
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprConstTableGet->PdpDerive())->PcrsOutput();
	const CColRef *pcrInner = pcrs->PcrAny();

	// get random columns from outer expression
	pcrs = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();
	const CColRef *pcrOuter = pcrs->PcrAny();

	const CWStringConst *str = GPOS_NEW(memory_pool) CWStringConst(GPOS_WSZ_LIT("="));

	CExpression *pexprSubquery = NULL;
	if (COperator::EopScalarSubqueryAny == op_id)
	{
		// construct ANY subquery expression
		pexprSubquery = GPOS_NEW(memory_pool) CExpression
									(
									memory_pool,
									GPOS_NEW(memory_pool) CScalarSubqueryAny(memory_pool, GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_EQ_OP), str, pcrInner),
									pexprConstTableGet,
									CUtils::PexprScalarIdent(memory_pool, pcrOuter)
									);
	}
	else
	{
		// construct ALL subquery expression
		pexprSubquery = GPOS_NEW(memory_pool) CExpression
									(
									memory_pool,
									GPOS_NEW(memory_pool) CScalarSubqueryAll(memory_pool, GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_EQ_OP), str, pcrInner),
									pexprConstTableGet,
									CUtils::PexprScalarIdent(memory_pool, pcrOuter)
									);

	}

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuter, pexprSubquery);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectWithConstAnySubquery
//
//	@doc:
//		Generate a random select expression with constant ANY subquery
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectWithConstAnySubquery
	(
	IMemoryPool *memory_pool
	)
{
	return PexprLogicalSubqueryWithConstTableGet(memory_pool, COperator::EopScalarSubqueryAny);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectWithConstAllSubquery
//
//	@doc:
//		Generate a random select expression with constant ALL subquery
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectWithConstAllSubquery
	(
	IMemoryPool *memory_pool
	)
{
	return PexprLogicalSubqueryWithConstTableGet(memory_pool, COperator::EopScalarSubqueryAll);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectCorrelated
//
//	@doc:
//		Generate correlated select; wrapper
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectCorrelated
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexprOuter = PexprLogicalGet(memory_pool);
	CColRefSet *outer_refs = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();

	CExpression *pexpr = PexprLogicalSelectCorrelated(memory_pool, outer_refs, 8);

	pexprOuter->Release();

	return pexpr;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectOnOuterJoin
//
//	@doc:
//		Generate a select on top of outer join expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectOnOuterJoin
	(
	IMemoryPool *memory_pool
	)
{
	// generate a pair of get expressions
	CExpression *pexprOuter = NULL;
	CExpression *pexprInner = NULL;
	CSubqueryTestUtils::GenerateGetExpressions(memory_pool, &pexprOuter, &pexprInner);

	const CColRef *pcrOuter = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput()->PcrAny();
	const CColRef *pcrInner = CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->PcrsOutput()->PcrAny();
	CExpression *pexprOuterJoinPred = CUtils::PexprScalarEqCmp(memory_pool, pcrOuter, pcrInner);
	CExpression *pexprOuterJoin =
			GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalLeftOuterJoin(memory_pool),
				pexprOuter,
				pexprInner,
				pexprOuterJoinPred
				);
	CExpression *pexprPred = CUtils::PexprScalarEqCmp(memory_pool, pcrInner, CUtils::PexprScalarConstInt4(memory_pool, 5 /*val*/));

	return CUtils::PexprLogicalSelect(memory_pool, pexprOuterJoin, pexprPred);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectCorrelated
//
//	@doc:
//		Generate correlated select, different predicates depending on nesting
//		level: correlated, exclusively external, non-correlated
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectCorrelated
	(
	IMemoryPool *memory_pool,
	CColRefSet *outer_refs,
	ULONG ulLevel
	)
{
	GPOS_CHECK_STACK_SIZE;

	if (0 == ulLevel)
	{
		return PexprLogicalGet(memory_pool);
	}

	CExpression *pexpr = PexprLogicalSelectCorrelated(memory_pool, outer_refs, ulLevel - 1);

	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput();
	GPOS_ASSERT(outer_refs->IsDisjoint(pcrs));

	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);

	switch (ulLevel % 6)
	{
		case 4:
			// outer only
			EqualityPredicate(memory_pool, outer_refs, outer_refs, pdrgpexpr);

			// inner only
			EqualityPredicate(memory_pool, pcrs, pcrs, pdrgpexpr);

			// regular correlation
			EqualityPredicate(memory_pool, pcrs, outer_refs, pdrgpexpr);
			break;

		case 3:
			// outer only
			EqualityPredicate(memory_pool, outer_refs, outer_refs, pdrgpexpr);
			break;

		case 2:
			// inner only
			EqualityPredicate(memory_pool, pcrs, pcrs, pdrgpexpr);

			// regular correlation
			EqualityPredicate(memory_pool, pcrs, outer_refs, pdrgpexpr);
			break;

		case 1:
			// inner only
			EqualityPredicate(memory_pool, pcrs, pcrs, pdrgpexpr);
			break;

		case 0:
			// regular correlation
			EqualityPredicate(memory_pool, pcrs, outer_refs, pdrgpexpr);
			break;
	}

	CExpression *pexprCorrelation = CPredicateUtils::PexprConjunction(memory_pool, pdrgpexpr);

	// assemble select
	CExpression *pexprResult =
			GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CLogicalSelect(memory_pool),
					pexpr,
					pexprCorrelation);

	return pexprResult;
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalProject
//
//	@doc:
//		Generate a Project expression that re-maps a single column
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalProject
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	CColRef *colref,
	CColRef *new_colref
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(NULL != colref);
	GPOS_ASSERT(NULL != new_colref);

	return GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CLogicalProject(memory_pool),
			pexpr,
			GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CScalarProjectList(memory_pool),
				GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CScalarProjectElement(memory_pool, new_colref),
					GPOS_NEW(memory_pool) CExpression
						(
						memory_pool,
						GPOS_NEW(memory_pool) CScalarIdent(memory_pool, colref)
						)
					)
				)
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalProject
//
//	@doc:
//		Generate a randomized project expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalProject
	(
	IMemoryPool *memory_pool
	)
{
	// generate a get expression
	CExpression *pexpr = PexprLogicalGet(memory_pool);

	// get output columns
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput();

	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();
	CColRef *colref =  pcrs->PcrAny();

	// create new column to which we will an existing one remap to
	CColRef *new_colref = col_factory->PcrCreate(colref);

	return PexprLogicalProject(memory_pool, pexpr, colref, new_colref);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalProjectGbAggCorrelated
//
//	@doc:
//		Generate correlated Project over GbAgg
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalProjectGbAggCorrelated
	(
	IMemoryPool *memory_pool
	)
{
	// create a GbAgg expression
	CExpression *pexprGbAgg = PexprLogicalGbAggCorrelated(memory_pool);

	CExpression *pexprPrjList = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool));
	CExpression *pexprProject =
		GPOS_NEW(memory_pool) CExpression(memory_pool,
							 GPOS_NEW(memory_pool) CLogicalProject(memory_pool),
							 pexprGbAgg,
							 pexprPrjList);

	return pexprProject;
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalJoinCorrelated
//
//	@doc:
//		Generate correlated join
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalJoinCorrelated
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexprLeft = PexprLogicalSelectCorrelated(memory_pool);
	CExpression *pexprRight = PexprLogicalSelectCorrelated(memory_pool);

	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexpr->Append(pexprLeft);
	pdrgpexpr->Append(pexprRight);

	CColRefSet *pcrsLeft = CDrvdPropRelational::GetRelationalProperties(pexprLeft->PdpDerive())->PcrsOutput();
	CColRefSet *pcrsRight = CDrvdPropRelational::GetRelationalProperties(pexprRight->PdpDerive())->PcrsOutput();

	EqualityPredicate(memory_pool, pcrsLeft, pcrsRight, pdrgpexpr);

	return GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CLogicalInnerJoin(memory_pool), pdrgpexpr);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalJoinCorrelated
//
//	@doc:
//		Generate join with a partitioned and indexed inner child
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalJoinWithPartitionedAndIndexedInnerChild
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexprLeft = PexprLogicalGet(memory_pool);
	CExpression *pexprRight = PexprLogicalDynamicGetWithIndexes(memory_pool);

	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexpr->Append(pexprLeft);
	pdrgpexpr->Append(pexprRight);

	CColRefSet *pcrsLeft = CDrvdPropRelational::GetRelationalProperties(pexprLeft->PdpDerive())->PcrsOutput();
	CColRefSet *pcrsRight = CDrvdPropRelational::GetRelationalProperties(pexprRight->PdpDerive())->PcrsOutput();

	EqualityPredicate(memory_pool, pcrsLeft, pcrsRight, pdrgpexpr);

	return GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CLogicalInnerJoin(memory_pool), pdrgpexpr);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalNAryJoin
//
//	@doc:
//		Generate randomized n-ary join expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalNAryJoin
	(
	IMemoryPool *memory_pool,
	ExpressionArray *pdrgpexpr
	)
{
	GPOS_ASSERT(NULL != pdrgpexpr);
	GPOS_ASSERT(2 < pdrgpexpr->Size());

	return GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CLogicalNAryJoin(memory_pool),
			pdrgpexpr
			);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalNAryJoin
//
//	@doc:
//		Generate randomized n-ary join expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalNAryJoin
	(
	IMemoryPool *memory_pool
	)
{
	const ULONG  ulRels = 4;

	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	ExpressionArray *pdrgpexprConjuncts = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);

	CExpression *pexprInit = PexprLogicalGet(memory_pool);
	pdrgpexpr->Append(pexprInit);

	// build pairwise joins, extract predicate and input, then discard join
	for (ULONG ul = 0; ul < ulRels; ul++)
	{
		CExpression *pexprNext = PexprLogicalGet(memory_pool);
		pdrgpexpr->Append(pexprNext);

		pexprInit->AddRef();
		pexprNext->AddRef();

		CExpression *pexprJoin = PexprLogicalJoin<CLogicalInnerJoin>(memory_pool, pexprInit, pexprNext);

		CExpression *pexprPredicate = (*pexprJoin)[2];

		pexprPredicate->AddRef();
		pdrgpexprConjuncts->Append(pexprPredicate);

		pexprJoin->Release();
	}

	// add predicate built of conjuncts to the input array
	pdrgpexpr->Append(CUtils::PexprScalarBoolOp(memory_pool, CScalarBoolOp::EboolopAnd, pdrgpexprConjuncts));

	return PexprLogicalNAryJoin(memory_pool, pdrgpexpr);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLeftOuterJoinOnNAryJoin
//
//	@doc:
//		Generate left outer join on top of n-ary join expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLeftOuterJoinOnNAryJoin
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexprNAryJoin = PexprLogicalNAryJoin(memory_pool);
	CExpression *pexprLOJInnerChild = PexprLogicalGet(memory_pool);

	// get a random column from LOJ inner child output
	CColRef *pcrLeft1 = CDrvdPropRelational::GetRelationalProperties(pexprLOJInnerChild->PdpDerive())->PcrsOutput()->PcrAny();

	// get a random column from NAry join second child
	CColRef *pcrLeft2 = CDrvdPropRelational::GetRelationalProperties((*pexprNAryJoin)[1]->PdpDerive())->PcrsOutput()->PcrAny();

	// get a random column from NAry join output
	CColRef *pcrRight = CDrvdPropRelational::GetRelationalProperties(pexprNAryJoin->PdpDerive())->PcrsOutput()->PcrAny();

	CExpression *pexprPred1 = CUtils::PexprScalarEqCmp(memory_pool, pcrLeft1, pcrRight);
	CExpression *pexprPred2 = CUtils::PexprScalarEqCmp(memory_pool, pcrLeft2, pcrRight);
	CExpression *pexprPred = CPredicateUtils::PexprConjunction(memory_pool, pexprPred1, pexprPred2);
	pexprPred1->Release();
	pexprPred2->Release();

	return GPOS_NEW(memory_pool) CExpression (memory_pool, GPOS_NEW(memory_pool) CLogicalLeftOuterJoin(memory_pool), pexprNAryJoin, pexprLOJInnerChild, pexprPred);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprNAryJoinOnLeftOuterJoin
//
//	@doc:
//		Generate n-ary join expression on top of left outer join
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprNAryJoinOnLeftOuterJoin
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexprNAryJoin = PexprLogicalNAryJoin(memory_pool);
	CColRef *pcrNAryJoin = CDrvdPropRelational::GetRelationalProperties((*pexprNAryJoin)[0]->PdpDerive())->PcrsOutput()->PcrAny();
	CExpression *pexprScalar = (*pexprNAryJoin)[pexprNAryJoin->Arity() - 1];

	// copy NAry-Join children
	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	CUtils::AddRefAppend<CExpression>(pdrgpexpr, pexprNAryJoin->PdrgPexpr());

	// generate LOJ expression
	CExpression *pexprLOJOuterChild = PexprLogicalGet(memory_pool);
	CExpression *pexprLOJInnerChild = PexprLogicalGet(memory_pool);

	// get a random column from LOJ outer child output
	CColRef *pcrLeft = CDrvdPropRelational::GetRelationalProperties(pexprLOJOuterChild->PdpDerive())->PcrsOutput()->PcrAny();

	// get a random column from LOJ inner child output
	CColRef *pcrRight = CDrvdPropRelational::GetRelationalProperties(pexprLOJInnerChild->PdpDerive())->PcrsOutput()->PcrAny();

	CExpression *pexprPred = CUtils::PexprScalarEqCmp(memory_pool, pcrLeft, pcrRight);
	CExpression *pexprLOJ = GPOS_NEW(memory_pool) CExpression (memory_pool, GPOS_NEW(memory_pool) CLogicalLeftOuterJoin(memory_pool), pexprLOJOuterChild, pexprLOJInnerChild, pexprPred);

	// replace NAry-Join scalar predicate with LOJ expression
	pdrgpexpr->Replace(pdrgpexpr->Size() - 1, pexprLOJ);

	// create new scalar predicate for NAry join
	CExpression *pexprNewPred = CUtils::PexprScalarEqCmp(memory_pool, pcrNAryJoin, pcrRight);
	pdrgpexpr->Append(CPredicateUtils::PexprConjunction(memory_pool, pexprScalar, pexprNewPred));
	pexprNAryJoin->Release();
	pexprNewPred->Release();

	return GPOS_NEW(memory_pool) CExpression (memory_pool, GPOS_NEW(memory_pool) CLogicalNAryJoin(memory_pool), pdrgpexpr);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalLimit
//
//	@doc:
//		Generate a limit expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalLimit
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	LINT iStart,
	LINT iRows,
	BOOL fGlobal,
	BOOL fHasCount
	)
{
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(0 <= iStart);
	GPOS_ASSERT(0 <= iRows);

	COrderSpec *pos = GPOS_NEW(memory_pool) COrderSpec(memory_pool);
	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalLimit(memory_pool, pos, fGlobal, fHasCount, false /*fTopLimitUnderDML*/),
					pexpr,
					CUtils::PexprScalarConstInt8(memory_pool, iStart),
					CUtils::PexprScalarConstInt8(memory_pool, iRows)
					);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalLimit
//
//	@doc:
//		Generate a randomized limit expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalLimit
	(
	IMemoryPool *memory_pool
	)
{
	return PexprLogicalLimit
				(
				memory_pool,
				PexprLogicalGet(memory_pool),
				0, // iStart
				100, // iRows
				true, // fGlobal
				true // fHasCount
				);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalGbAggWithSum
//
//	@doc:
//		Generate a randomized aggregate expression with sum agg function
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalGbAggWithSum
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexprGb = PexprLogicalGbAgg(memory_pool);

	// get a random column from Gb child
	CColRef *colref =
		CDrvdPropRelational::GetRelationalProperties((*pexprGb)[0]->PdpDerive())->PcrsOutput()->PcrAny();

	// generate a SUM expression
	CExpression *pexprProjElem = PexprPrjElemWithSum(memory_pool, colref);
	CExpression *pexprPrjList =
		GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool), pexprProjElem);

	(*pexprGb)[0]->AddRef();
	ColRefArray *colref_array = CLogicalGbAgg::PopConvert(pexprGb->Pop())->Pdrgpcr();
	colref_array->AddRef();
	CExpression *pexprGbWithSum = CUtils::PexprLogicalGbAggGlobal(memory_pool, colref_array, (*pexprGb)[0], pexprPrjList);

	// release old Gb expression
	pexprGb->Release();

	return pexprGbWithSum;
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalGbAggWithInput
//
//	@doc:
//		Generate a randomized aggregate expression
//
//---------------------------------------------------------------------------
CExpression *CTestUtils::PexprLogicalGbAggWithInput
	(
	IMemoryPool *memory_pool,
	CExpression *pexprInput
	)
{
	// get the first few columns
	CDrvdPropRelational *pdprel = CDrvdPropRelational::GetRelationalProperties(pexprInput->PdpDerive());
	CColRefSet *pcrs = pdprel->PcrsOutput();

	ULONG num_cols = std::min(3u, pcrs->Size());

	ColRefArray *colref_array = GPOS_NEW(memory_pool) ColRefArray(memory_pool);
	CColRefSetIter crsi(*pcrs);
	for (ULONG i = 0; i < num_cols; i++)
	{
		(void) crsi.Advance();
		colref_array->Append(crsi.Pcr());
	}

	CExpression *pexprList = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool));
	return CUtils::PexprLogicalGbAggGlobal(memory_pool, colref_array, pexprInput, pexprList);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalGbAgg
//
//	@doc:
//		Generate a randomized aggregate expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalGbAgg
	(
	IMemoryPool *memory_pool
	)
{
	return PexprLogicalGbAggWithInput(memory_pool, PexprLogicalGet(memory_pool));
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprPrjElemWithSum
//
//	@doc:
//		Generate a project element with sum agg function
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprPrjElemWithSum
	(
	IMemoryPool *memory_pool,
	CColRef *colref
	)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	// generate a SUM expression
	CMDIdGPDB *pmdidSumAgg = GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_SUM_AGG);
	CWStringConst *pstrAggFunc = GPOS_NEW(memory_pool) CWStringConst(GPOS_WSZ_LIT("sum"));
	CExpression *pexprScalarAgg = CUtils::PexprAggFunc
									(
									memory_pool,
									pmdidSumAgg,
									pstrAggFunc,
									colref,
									false /*is_distinct*/,
									EaggfuncstageGlobal /*eaggfuncstage*/,
									false /*fSplit*/
									);

	// map a computed column to SUM expression
	CScalar *pop = CScalar::PopConvert(pexprScalarAgg->Pop());
	IMDId *mdid_type = pop->MDIdType();
	const IMDType *pmdtype = md_accessor->RetrieveType(mdid_type);
	CWStringConst str(GPOS_WSZ_LIT("sum_col"));
	CName name(memory_pool, &str);
	CColRef *pcrComputed = COptCtxt::PoctxtFromTLS()->Pcf()->PcrCreate(pmdtype, pop->TypeModifier(), name);

	return CUtils::PexprScalarProjectElement(memory_pool, pcrComputed, pexprScalarAgg);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalGbAggOverJoin
//
//	@doc:
//		Generate a randomized group by over join expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalGbAggOverJoin
	(
	IMemoryPool *memory_pool
	)
{
	// generate a join expression
	CExpression *pexprJoin = PexprLogicalJoin<CLogicalInnerJoin>(memory_pool);

	// include one grouping column
	ColRefArray *colref_array = GPOS_NEW(memory_pool) ColRefArray(memory_pool);
	CColRef *colref = CDrvdPropRelational::GetRelationalProperties(pexprJoin->PdpDerive())->PcrsOutput()->PcrAny();
	colref_array->Append(colref);

	return CUtils::PexprLogicalGbAggGlobal
				(
				memory_pool,
				colref_array,
				pexprJoin,
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool))
				);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalGbAggDedupOverInnerJoin
//
//	@doc:
//		Generate a dedup group by over inner join expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalGbAggDedupOverInnerJoin
	(
	IMemoryPool *memory_pool
	)
{
	// generate a join expression
	CExpression *pexprJoin = PexprLogicalJoin<CLogicalInnerJoin>(memory_pool);

	// get join's outer child
	CExpression *pexprOuter = (*pexprJoin)[0];
	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();

	// grouping columns: all columns from outer child
	ColRefArray *pdrgpcrGrp = pcrs->Pdrgpcr(memory_pool);

	// outer child keys: get a random column from its output columns
	CColRef *colref = pcrs->PcrAny();
	ColRefArray *pdrgpcrKeys = GPOS_NEW(memory_pool) ColRefArray(memory_pool);
	pdrgpcrKeys->Append(colref);

	return GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CLogicalGbAggDeduplicate(memory_pool, pdrgpcrGrp, COperator::EgbaggtypeGlobal  /*egbaggtype*/, pdrgpcrKeys),
				pexprJoin,
				GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool))
				);
	}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalGbAggCorrelated
//
//	@doc:
//		Generate correlated GbAgg
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalGbAggCorrelated
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexpr = PexprLogicalSelectCorrelated(memory_pool);

	// include one grouping column
	ColRefArray *colref_array = GPOS_NEW(memory_pool) ColRefArray(memory_pool);
	CColRef *colref = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput()->PcrAny();
	colref_array->Append(colref);

	CExpression *pexprList = GPOS_NEW(memory_pool) CExpression(memory_pool, GPOS_NEW(memory_pool) CScalarProjectList(memory_pool));
	return CUtils::PexprLogicalGbAggGlobal(memory_pool, colref_array, pexpr, pexprList);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprConstTableGet
//
//	@doc:
//		Generate logical const table get expression with one column and 
//		the given number of elements
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprConstTableGet
	(
	IMemoryPool *memory_pool, 
	ULONG ulElements
	)
{
	GPOS_ASSERT(0 < ulElements);
	
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	const IMDTypeInt4 *pmdtypeint4 = md_accessor->PtMDType<IMDTypeInt4>(CTestUtils::m_sysidDefault);

	// create an integer column descriptor
	CWStringConst strName(GPOS_WSZ_LIT("A"));
	CName name(&strName);
	CColumnDescriptor *pcoldescInt = GPOS_NEW(memory_pool) CColumnDescriptor
													(
													memory_pool,
													pmdtypeint4,
													default_type_modifier,
													name,
													1 /* attno */,
													false /*IsNullable*/
													);

	ColumnDescrArray *pdrgpcoldesc = GPOS_NEW(memory_pool) ColumnDescrArray(memory_pool);
	pdrgpcoldesc->Append(pcoldescInt);

	// generate values
	IDatumArrays *pdrgpdrgpdatum = GPOS_NEW(memory_pool) IDatumArrays(memory_pool);
	
	for (ULONG ul = 0; ul < ulElements; ul++)
	{
		IDatumInt4 *datum = pmdtypeint4->CreateInt4Datum(memory_pool, ul, false /* is_null */);
		IDatumArray *pdrgpdatum = GPOS_NEW(memory_pool) IDatumArray(memory_pool);
		pdrgpdatum->Append((IDatum *) datum);
		pdrgpdrgpdatum->Append(pdrgpdatum);
	}
	CLogicalConstTableGet *popConstTableGet = GPOS_NEW(memory_pool) CLogicalConstTableGet(memory_pool, pdrgpcoldesc, pdrgpdrgpdatum);
	
	return GPOS_NEW(memory_pool) CExpression(memory_pool, popConstTableGet);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprConstTableGet5
//
//	@doc:
//		Generate logical const table get expression with one column and 5 elements
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprConstTableGet5
	(
	IMemoryPool *memory_pool
	)
{
	return PexprConstTableGet(memory_pool, 5 /* ulElements */);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalInsert
//
//	@doc:
//		Generate logical insert expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalInsert
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexprCTG = PexprConstTableGet(memory_pool, 1 /* ulElements */);

	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprCTG->PdpDerive())->PcrsOutput();
	ColRefArray *colref_array = pcrs->Pdrgpcr(memory_pool);

	CWStringConst strName(GPOS_WSZ_LIT("BaseTable"));
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID, 1, 1);
	CTableDescriptor *ptabdesc = PtabdescCreate(memory_pool, 1, mdid, CName(&strName));

	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalInsert(memory_pool, ptabdesc, colref_array),
					pexprCTG
					);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalDelete
//
//	@doc:
//		Generate logical delete expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalDelete
	(
	IMemoryPool *memory_pool
	)
{
	CWStringConst strName(GPOS_WSZ_LIT("BaseTable"));
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID, 1, 1);
	CTableDescriptor *ptabdesc = PtabdescCreate(memory_pool, 1, mdid, CName(&strName));
	CWStringConst strAlias(GPOS_WSZ_LIT("BaseTableAlias"));

	CExpression *pexprGet = PexprLogicalGet(memory_pool, ptabdesc, &strAlias);

	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprGet->PdpDerive())->PcrsOutput();
	ColRefArray *colref_array = pcrs->Pdrgpcr(memory_pool);

	CColRef *colref = pcrs->PcrFirst();

	ptabdesc->AddRef();

	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalDelete(memory_pool, ptabdesc, colref_array, colref, colref),
					pexprGet
					);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalUpdate
//
//	@doc:
//		Generate logical update expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalUpdate
	(
	IMemoryPool *memory_pool
	)
{
	CWStringConst strName(GPOS_WSZ_LIT("BaseTable"));
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID, 1, 1);
	CTableDescriptor *ptabdesc = PtabdescCreate(memory_pool, 1, mdid, CName(&strName));
	CWStringConst strAlias(GPOS_WSZ_LIT("BaseTableAlias"));

	CExpression *pexprGet = PexprLogicalGet(memory_pool, ptabdesc, &strAlias);

	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexprGet->PdpDerive())->PcrsOutput();
	ColRefArray *pdrgpcrDelete = pcrs->Pdrgpcr(memory_pool);
	ColRefArray *pdrgpcrInsert = pcrs->Pdrgpcr(memory_pool);

	CColRef *colref = pcrs->PcrFirst();

	ptabdesc->AddRef();

	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalUpdate(memory_pool, ptabdesc, pdrgpcrDelete, pdrgpcrInsert, colref, colref, NULL /*pcrTupleOid*/),
					pexprGet
					);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalDynamicGet
//
//	@doc:
//		Generate a dynamic get expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalDynamicGet
	(
	IMemoryPool *memory_pool,
	CTableDescriptor *ptabdesc,
	const CWStringConst *pstrTableAlias,
	ULONG ulPartIndex
	)
{
	GPOS_ASSERT(NULL != ptabdesc);

	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalDynamicGet
								(
								memory_pool,
								GPOS_NEW(memory_pool) CName(memory_pool, CName(pstrTableAlias)),
								ptabdesc,
								ulPartIndex
								)
					);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalDynamicGet
//
//	@doc:
//		Generate a dynamic get expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalDynamicGet
	(
	IMemoryPool *memory_pool
	)
{
	CWStringConst strName(GPOS_WSZ_LIT("PartTable"));
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID_PARTITIONED, 1, 1);
	CTableDescriptor *ptabdesc = PtabdescCreate(memory_pool, 3, mdid, CName(&strName), true /*fPartitioned*/);
	CWStringConst strAlias(GPOS_WSZ_LIT("PartTableAlias"));

	return PexprLogicalDynamicGet(memory_pool, ptabdesc, &strAlias, GPOPT_TEST_PART_INDEX);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectWithEqPredicateOverDynamicGet
//
//	@doc:
//		Generate a select over dynamic get expression with a predicate on the
//		partition key 
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectWithEqPredicateOverDynamicGet
	(
	IMemoryPool *memory_pool
	)
{
	CWStringConst strName(GPOS_WSZ_LIT("PartTable"));
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID_PARTITIONED, 1, 1);
	CTableDescriptor *ptabdesc = PtabdescCreate(memory_pool, 3, mdid, CName(&strName), true /*fPartitioned*/);
	CWStringConst strAlias(GPOS_WSZ_LIT("PartTableAlias"));

	CExpression *pexprDynamicGet = PexprLogicalDynamicGet(memory_pool, ptabdesc, &strAlias, GPOPT_TEST_PART_INDEX);
	
	// construct scalar comparison
	CLogicalDynamicGet *popDynamicGet = CLogicalDynamicGet::PopConvert(pexprDynamicGet->Pop());
	ColRefArrays *pdrgpdrgpcr = popDynamicGet->PdrgpdrgpcrPart();
	CColRef *colref = CUtils::PcrExtractPartKey(pdrgpdrgpcr, 0 /*ulLevel*/);
	CExpression *pexprScalarIdent = CUtils::PexprScalarIdent(memory_pool, colref);
	
	CExpression *pexprConst = CUtils::PexprScalarConstInt4(memory_pool, 5 /*val*/);
	CExpression *pexprScalar = CUtils::PexprScalarEqCmp(memory_pool, pexprScalarIdent, pexprConst);
	
	return CUtils::PexprLogicalSelect(memory_pool, pexprDynamicGet, pexprScalar);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSelectWithLTPredicateOverDynamicGet
//
//	@doc:
//		Generate a select over dynamic get expression with a predicate on the
//		partition key 
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSelectWithLTPredicateOverDynamicGet
	(
	IMemoryPool *memory_pool
	)
{
	CWStringConst strName(GPOS_WSZ_LIT("PartTable"));
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(GPOPT_MDCACHE_TEST_OID_PARTITIONED, 1, 1);
	CTableDescriptor *ptabdesc = PtabdescCreate(memory_pool, 3, mdid, CName(&strName), true /*fPartitioned*/);
	CWStringConst strAlias(GPOS_WSZ_LIT("PartTableAlias"));

	CExpression *pexprDynamicGet = PexprLogicalDynamicGet(memory_pool, ptabdesc, &strAlias, GPOPT_TEST_PART_INDEX);
	
	// construct scalar comparison
	CLogicalDynamicGet *popDynamicGet = CLogicalDynamicGet::PopConvert(pexprDynamicGet->Pop());
	ColRefArrays *pdrgpdrgpcr = popDynamicGet->PdrgpdrgpcrPart();
	CColRef *colref = CUtils::PcrExtractPartKey(pdrgpdrgpcr, 0 /*ulLevel*/);
	CExpression *pexprScalarIdent = CUtils::PexprScalarIdent(memory_pool, colref);
	
	CExpression *pexprConst = CUtils::PexprScalarConstInt4(memory_pool, 5 /*val*/);
	CExpression *pexprScalar = CUtils::PexprScalarCmp(memory_pool, pexprScalarIdent, pexprConst, IMDType::EcmptL);
	
	return CUtils::PexprLogicalSelect(memory_pool, pexprDynamicGet, pexprScalar);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalTVFTwoArgs
//
//	@doc:
//		Generate a logical TVF expression with 2 constant arguments
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalTVFTwoArgs
	(
	IMemoryPool *memory_pool
	)
{
	return PexprLogicalTVF(memory_pool, 2);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalTVFThreeArgs
//
//	@doc:
//		Generate a logical TVF expression with 3 constant arguments
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalTVFThreeArgs
	(
	IMemoryPool *memory_pool
	)
{
	return PexprLogicalTVF(memory_pool, 3);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalTVFNoArgs
//
//	@doc:
//		Generate a logical TVF expression with no arguments
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalTVFNoArgs
	(
	IMemoryPool *memory_pool
	)
{
	return PexprLogicalTVF(memory_pool, 0);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalTVF
//
//	@doc:
//		Generate a TVF expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalTVF
	(
	IMemoryPool *memory_pool,
	ULONG ulArgs
	)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();

	const WCHAR *wszFuncName = GPOS_WSZ_LIT("generate_series");

	IMDId *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT8_GENERATE_SERIES);
	CWStringConst *str_func_name = GPOS_NEW(memory_pool) CWStringConst(memory_pool, wszFuncName);

	const IMDTypeInt8 *pmdtypeint8 = md_accessor->PtMDType<IMDTypeInt8>(CTestUtils::m_sysidDefault);

	// create an integer column descriptor
	CWStringConst strName(GPOS_WSZ_LIT("generate_series"));
	CName name(&strName);

	CColumnDescriptor *pcoldescInt = GPOS_NEW(memory_pool) CColumnDescriptor(memory_pool, pmdtypeint8, default_type_modifier, name, 1 /* attno */, false /*IsNullable*/);

	ColumnDescrArray *pdrgpcoldesc = GPOS_NEW(memory_pool) ColumnDescrArray(memory_pool);
	pdrgpcoldesc->Append(pcoldescInt);

	IMDId *mdid_return_type = pmdtypeint8->MDId();
	mdid_return_type->AddRef();

	CLogicalTVF *popTVF = GPOS_NEW(memory_pool) CLogicalTVF
										(
										memory_pool,
										mdid,
										mdid_return_type,
										str_func_name,
										pdrgpcoldesc
										);

	if (0 == ulArgs)
	{
		return GPOS_NEW(memory_pool) CExpression(memory_pool, popTVF);
	}

	ExpressionArray *pdrgpexprArgs = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);

	for (ULONG ul = 0; ul < ulArgs; ul++)
	{
		ULONG ulArg = 1;
		IDatum *datum = (IDatum *) pmdtypeint8->CreateInt8Datum(memory_pool, ulArg, false /* is_null */);
		CScalarConst *popConst = GPOS_NEW(memory_pool) CScalarConst(memory_pool, datum);
		pdrgpexprArgs->Append(GPOS_NEW(memory_pool) CExpression(memory_pool, popConst));
	}

	return GPOS_NEW(memory_pool) CExpression(memory_pool, popTVF, pdrgpexprArgs);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalCTEProducerOverSelect
//
//	@doc:
//		Generate a CTE producer on top of a select
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalCTEProducerOverSelect
	(
	IMemoryPool *memory_pool,
	ULONG ulCTEId
	)
{
	CExpression *pexprSelect = CTestUtils::PexprLogicalSelect(memory_pool);
	ColRefArray *colref_array = CDrvdPropRelational::GetRelationalProperties(pexprSelect->PdpDerive())->PcrsOutput()->Pdrgpcr(memory_pool);

	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalCTEProducer(memory_pool, ulCTEId, colref_array),
					pexprSelect
					);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalCTEProducerOverSelect
//
//	@doc:
//		Generate a CTE producer on top of a select
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalCTEProducerOverSelect
	(
	IMemoryPool *memory_pool
	)
{
	return PexprLogicalCTEProducerOverSelect(memory_pool, 0);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprCTETree
//
//	@doc:
//		Generate an expression with CTE producer and consumer
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprCTETree
	(
	IMemoryPool *memory_pool
	)
{
	ULONG ulCTEId = 0;
	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);

	CExpression *pexprProducer = PexprLogicalCTEProducerOverSelect(memory_pool, ulCTEId);
	COptCtxt::PoctxtFromTLS()->Pcteinfo()->AddCTEProducer(pexprProducer);

	pdrgpexpr->Append(pexprProducer);

	ColRefArray *pdrgpcrProducer = CLogicalCTEProducer::PopConvert(pexprProducer->Pop())->Pdrgpcr();
	ColRefArray *pdrgpcrConsumer = CUtils::PdrgpcrCopy(memory_pool, pdrgpcrProducer);

	CExpression *pexprConsumer =
			GPOS_NEW(memory_pool) CExpression
						(
						memory_pool,
						GPOS_NEW(memory_pool) CLogicalCTEConsumer(memory_pool, ulCTEId, pdrgpcrConsumer)
						);

	pdrgpexpr->Append(pexprConsumer);

	return PexprLogicalSequence(memory_pool, pdrgpexpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSequence
//
//	@doc:
//		Generate a dynamic get expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSequence
	(
	IMemoryPool *memory_pool,
	ExpressionArray *pdrgpexpr
	)
{
	GPOS_ASSERT(NULL != pdrgpexpr);

	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CLogicalSequence(memory_pool),
					pdrgpexpr
					);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSequence
//
//	@doc:
//		Generate a logical sequence expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSequence
	(
	IMemoryPool *memory_pool
	)
{
	const ULONG  ulRels = 2;
	const ULONG  ulCTGElements = 2;

	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);

	// build an array of get expressions
	for (ULONG ul = 0; ul < ulRels; ul++)
	{
		CExpression *pexpr = PexprLogicalGet(memory_pool);
		pdrgpexpr->Append(pexpr);
	}

	CExpression *pexprCTG = PexprConstTableGet(memory_pool, ulCTGElements);
	pdrgpexpr->Append(pexprCTG);

	CExpression *pexprDynamicGet = PexprLogicalDynamicGet(memory_pool);
	pdrgpexpr->Append(pexprDynamicGet);

	return PexprLogicalSequence(memory_pool, pdrgpexpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalUnion
//
//	@doc:
//		Generate tree of unions recursively
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalUnion
	(
	IMemoryPool *memory_pool,
	ULONG ulDepth
	)
{
	// stack check in recursion
	GPOS_CHECK_STACK_SIZE;

	CExpression *pexpr = NULL;

	if (0 == ulDepth)
	{
		// terminal case, generate table
		pexpr = PexprLogicalGet(memory_pool);
	}
	else
	{
		// recursive case, generate union w/ 3 children
		ExpressionArray *pdrgpexprInput = GPOS_NEW(memory_pool) ExpressionArray(memory_pool, 3);
		ColRefArrays *pdrgpdrgpcrInput = GPOS_NEW(memory_pool) ColRefArrays(memory_pool);

		for (ULONG i = 0; i < 3; i++)
		{
			CExpression *pexprInput = PexprLogicalUnion(memory_pool, ulDepth -1);
			COperator *pop = pexprInput->Pop();
			ColRefArray *colref_array = NULL;

			if (pop->Eopid() == COperator::EopLogicalGet)
			{
				CLogicalGet *popGet =  CLogicalGet::PopConvert(pop);
				colref_array = popGet->PdrgpcrOutput();
			}
			else
			{
				GPOS_ASSERT(COperator::EopLogicalUnion == pop->Eopid());
				CLogicalUnion *popUnion = CLogicalUnion::PopConvert(pop);
				colref_array = popUnion->PdrgpcrOutput();
			}
			pdrgpexprInput->Append(pexprInput);
			GPOS_ASSERT(NULL != colref_array);

			colref_array->AddRef();
			pdrgpdrgpcrInput->Append(colref_array);
		}

		// remap columns of first input
		ColRefArray *pdrgpcrInput = (*pdrgpdrgpcrInput)[0];
		ULONG num_cols = pdrgpcrInput->Size();
		ColRefArray *pdrgpcrOutput = GPOS_NEW(memory_pool) ColRefArray(memory_pool, num_cols);

		for (ULONG j = 0; j < num_cols; j++)
		{
			pdrgpcrOutput->Append((*pdrgpcrInput)[j]);
		}

		pexpr = GPOS_NEW(memory_pool) CExpression
							(
							memory_pool,
							GPOS_NEW(memory_pool) CLogicalUnion(memory_pool, pdrgpcrOutput, pdrgpdrgpcrInput),
							pdrgpexprInput
							);
	}

	return pexpr;
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalSequenceProject
//
//	@doc:
//		Generate a logical sequence project expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalSequenceProject
	(
	IMemoryPool *memory_pool,
	OID oidFunc,
	CExpression *pexprInput
	)
{
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	CColumnFactory *col_factory = COptCtxt::PoctxtFromTLS()->Pcf();

	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexpr->Append(CUtils::PexprScalarIdent(memory_pool, CDrvdPropRelational::GetRelationalProperties(pexprInput->PdpDerive())->PcrsOutput()->PcrAny()));
	OrderSpecArray *pdrgpos = GPOS_NEW(memory_pool) OrderSpecArray(memory_pool);
	WindowFrameArray *pdrgwf = GPOS_NEW(memory_pool) WindowFrameArray(memory_pool);

	CLogicalSequenceProject *popSeqProj =
			GPOS_NEW(memory_pool) CLogicalSequenceProject
				(
				memory_pool,
				GPOS_NEW(memory_pool) CDistributionSpecHashed(pdrgpexpr, true /*fNullsCollocated*/),
				pdrgpos,
				pdrgwf
				);

	IMDId *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(oidFunc);
	const IMDFunction *pmdfunc = md_accessor->RetrieveFunc(mdid);

	IMDId *mdid_return_type = pmdfunc->GetResultTypeMdid();
	mdid_return_type->AddRef();

	CExpression *pexprWinFunc =
			GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) CScalarWindowFunc
							(
							memory_pool,
							mdid,
							mdid_return_type,
							GPOS_NEW(memory_pool) CWStringConst(memory_pool, pmdfunc->Mdname().GetMDName()->GetBuffer()),
							CScalarWindowFunc::EwsImmediate,
							false /*is_distinct*/,
							false /*is_star_arg*/,
							false /*is_simple_agg*/
							)
				);
	// window function call is not a cast and so does not need a type modifier
	CColRef *pcrComputed = col_factory->PcrCreate(md_accessor->RetrieveType(pmdfunc->GetResultTypeMdid()), default_type_modifier);

	CExpression *pexprPrjList =
		GPOS_NEW(memory_pool) CExpression
			(
			memory_pool,
			GPOS_NEW(memory_pool) CScalarProjectList(memory_pool),
			CUtils::PexprScalarProjectElement(memory_pool, pcrComputed, pexprWinFunc)
			);

	return GPOS_NEW(memory_pool) CExpression(memory_pool, popSeqProj, pexprInput, pexprPrjList);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprOneWindowFunction
//
//	@doc:
//		Generate a random expression with one window function
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprOneWindowFunction
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexprGet = PexprLogicalGet(memory_pool);

	OID row_number_oid = COptCtxt::PoctxtFromTLS()->GetOptimizerConfig()->GetWindowOids()->OidRowNumber();

	return PexprLogicalSequenceProject(memory_pool, row_number_oid, pexprGet);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprTwoWindowFunctions
//
//	@doc:
//		Generate a random expression with two window functions
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprTwoWindowFunctions
	(
	IMemoryPool *memory_pool
	)
{
	CExpression *pexprWinFunc = PexprOneWindowFunction(memory_pool);

	OID rank_oid = COptCtxt::PoctxtFromTLS()->GetOptimizerConfig()->GetWindowOids()->OidRank();

	return PexprLogicalSequenceProject(memory_pool, rank_oid, pexprWinFunc);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PdrgpexprJoins
//
//	@doc:
//		Generate an array of join expressions for the given relation names;
//		the first generated expression is a Select of the first relation;
//		every subsequent expression is a Join between a new relation and the
//		previous join expression
//
//---------------------------------------------------------------------------
DrgPexprJoins *
CTestUtils::PdrgpexprJoins
	(
	IMemoryPool *memory_pool,
	CWStringConst *pstrRel, // array of relation names
	ULONG *pulRel, // array of relation oids
	ULONG ulRels, // number of relations
	BOOL fCrossProduct
	)
{
	DrgPexprJoins *pdrgpexpr = GPOS_NEW(memory_pool) DrgPexprJoins(memory_pool);
	for (ULONG i = 0; i < ulRels; i++)
	{
		CExpression *pexpr = NULL;
		if (fCrossProduct)
		{
			pexpr = PexprLogicalGet(memory_pool, &pstrRel[i], &pstrRel[i], pulRel[i]);
		}
		else
		{
			pexpr = PexprLogicalSelect(memory_pool, &pstrRel[i], &pstrRel[i], pulRel[i]);
		}


		if (0 == i)
		{
			// the first join is set to a Select/Get of the first relation
			pdrgpexpr->Append(pexpr);
		}
		else
		{
			CExpression *pexprJoin = NULL;
			if (fCrossProduct)
			{
				// create a cross product
				pexprJoin = CUtils::PexprLogicalJoin<CLogicalInnerJoin>
					(
					memory_pool,
					(*pdrgpexpr)[i - 1],
					pexpr,
					CPredicateUtils::PexprConjunction(memory_pool, NULL) // generate a constant True
				);
			}
			else
			{
				// otherwise, we create a new join out of the last created
				// join and the current Select expression
				pexprJoin = PexprLogicalJoin<CLogicalInnerJoin>(memory_pool, (*pdrgpexpr)[i - 1], pexpr);
			}

			pdrgpexpr->Append(pexprJoin);
		}

		GPOS_CHECK_ABORT;
	}

	return pdrgpexpr;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprLogicalNAryJoin
//
//	@doc:
//		Generate an n-ary join expression using given array of relation names
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprLogicalNAryJoin
	(
	IMemoryPool *memory_pool,
	CWStringConst *pstrRel,
	ULONG *pulRel,
	ULONG ulRels,
	BOOL fCrossProduct
	)
{
	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	for (ULONG ul = 0; ul < ulRels; ul++)
	{
		CExpression *pexpr = PexprLogicalGet(memory_pool, &pstrRel[ul], &pstrRel[ul], pulRel[ul]);
		pdrgpexpr->Append(pexpr);
	}

	ExpressionArray *pdrgpexprPred = NULL;
	if (!fCrossProduct)
	{
		pdrgpexprPred = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
		for (ULONG ul = 0; ul < ulRels - 1; ul++)
		{
			CExpression *pexprLeft = (*pdrgpexpr)[ul];
			CExpression *pexprRight = (*pdrgpexpr)[ul + 1];

			// get any two columns; one from each side
			CColRef *pcrLeft = CDrvdPropRelational::GetRelationalProperties(pexprLeft->PdpDerive())->PcrsOutput()->PcrAny();
			CColRef *pcrRight = CDrvdPropRelational::GetRelationalProperties(pexprRight->PdpDerive())->PcrsOutput()->PcrAny();
			pdrgpexprPred->Append(CUtils::PexprScalarEqCmp(memory_pool, pcrLeft, pcrRight));
		}
	}
	pdrgpexpr->Append(CPredicateUtils::PexprConjunction(memory_pool, pdrgpexprPred));

	return PexprLogicalNAryJoin(memory_pool, pdrgpexpr);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PqcGenerate
//
//	@doc:
//		Generate a dummy context from an array of column references and
//		empty sort columns for testing
//
//---------------------------------------------------------------------------
CQueryContext *
CTestUtils::PqcGenerate
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	ColRefArray *colref_array
	)
{
	// generate required columns
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrs->Include(colref_array);

	COrderSpec *pos = GPOS_NEW(memory_pool) COrderSpec(memory_pool);
	CDistributionSpec *pds =
		GPOS_NEW(memory_pool) CDistributionSpecSingleton(CDistributionSpecSingleton::EstMaster);

	CRewindabilitySpec *prs = GPOS_NEW(memory_pool) CRewindabilitySpec(CRewindabilitySpec::ErtNone /*ert*/);

	CEnfdOrder *peo = GPOS_NEW(memory_pool) CEnfdOrder(pos, CEnfdOrder::EomSatisfy);

	// we require exact matching on distribution since final query results must be sent to master
	CEnfdDistribution *ped = GPOS_NEW(memory_pool) CEnfdDistribution(pds, CEnfdDistribution::EdmExact);

	CEnfdRewindability *per = GPOS_NEW(memory_pool) CEnfdRewindability(prs, CEnfdRewindability::ErmSatisfy);
		
	CCTEReq *pcter = COptCtxt::PoctxtFromTLS()->Pcteinfo()->PcterProducers(memory_pool);

	CReqdPropPlan *prpp = GPOS_NEW(memory_pool) CReqdPropPlan(pcrs, peo, ped, per, pcter);

	colref_array->AddRef();
	MDNameArray *pdrgpmdname = GPOS_NEW(memory_pool) MDNameArray(memory_pool);
	const ULONG length = colref_array->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CColRef *colref = (*colref_array)[ul];
		CMDName *mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, colref->Name().Pstr());
		pdrgpmdname->Append(mdname);
	}

	return GPOS_NEW(memory_pool) CQueryContext(memory_pool, pexpr, prpp, colref_array, pdrgpmdname, true /*fDeriveStats*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PqcGenerate
//
//	@doc:
//		Generate a dummy context with a subset of required column, one
//		sort column and one distribution column
//
//---------------------------------------------------------------------------
CQueryContext *
CTestUtils::PqcGenerate
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr
	)
{
	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	pcrs->Include(CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput());

	// keep a subset of columns
	CColRefSet *pcrsOutput = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	CColRefSetIter crsi(*pcrs);
	while (crsi.Advance())
	{
		CColRef *colref = crsi.Pcr();
		if (1 != colref->Id() % GPOPT_TEST_REL_WIDTH)
		{
			pcrsOutput->Include(colref);
		}
	}
	pcrs->Release();

	// construct an ordered array of the output columns
	ColRefArray *colref_array = GPOS_NEW(memory_pool) ColRefArray(memory_pool);
	CColRefSetIter crsiOutput(*pcrsOutput);
	while (crsiOutput.Advance())
	{
		CColRef *colref = crsiOutput.Pcr();
		colref_array->Append(colref);
	}

	// generate a sort order
	COrderSpec *pos = GPOS_NEW(memory_pool) COrderSpec(memory_pool);
	pos->Append(GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_LT_OP), pcrsOutput->PcrAny(), COrderSpec::EntFirst);

	CDistributionSpec *pds =
		GPOS_NEW(memory_pool) CDistributionSpecSingleton(CDistributionSpecSingleton::EstMaster);

	CRewindabilitySpec *prs = GPOS_NEW(memory_pool) CRewindabilitySpec(CRewindabilitySpec::ErtNone /*ert*/);

	CEnfdOrder *peo = GPOS_NEW(memory_pool) CEnfdOrder(pos, CEnfdOrder::EomSatisfy);

	// we require exact matching on distribution since final query results must be sent to master
	CEnfdDistribution *ped = GPOS_NEW(memory_pool) CEnfdDistribution(pds, CEnfdDistribution::EdmExact);

	CEnfdRewindability *per = GPOS_NEW(memory_pool) CEnfdRewindability(prs, CEnfdRewindability::ErmSatisfy);

	CCTEReq *pcter = COptCtxt::PoctxtFromTLS()->Pcteinfo()->PcterProducers(memory_pool);

	CReqdPropPlan *prpp = GPOS_NEW(memory_pool) CReqdPropPlan(pcrsOutput, peo, ped, per, pcter);

	MDNameArray *pdrgpmdname = GPOS_NEW(memory_pool) MDNameArray(memory_pool);
	const ULONG length = colref_array->Size();
	for (ULONG ul = 0; ul < length; ul++)
	{
		CColRef *colref = (*colref_array)[ul];
		CMDName *mdname = GPOS_NEW(memory_pool) CMDName(memory_pool, colref->Name().Pstr());
		pdrgpmdname->Append(mdname);
	}

	return GPOS_NEW(memory_pool) CQueryContext(memory_pool, pexpr, prpp, colref_array, pdrgpmdname, true /*fDeriveStats*/);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprScalarNestedPreds
//
//	@doc:
//		Helper for generating a nested AND/OR/NOT tree
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprScalarNestedPreds
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr,
	CScalarBoolOp::EBoolOperator eboolop
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexpr);
	GPOS_ASSERT(CScalarBoolOp::EboolopAnd == eboolop ||
				CScalarBoolOp::EboolopOr == eboolop ||
				CScalarBoolOp::EboolopNot == eboolop);

	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput();
	CColRef *pcrLeft =  pcrs->PcrAny();
	CExpression *pexprConstActual = CUtils::PexprScalarConstInt4(memory_pool, 3 /*val*/);

	CExpression *pexprPredActual = CUtils::PexprScalarEqCmp(memory_pool, pcrLeft, pexprConstActual);
	CExpression *pexprPredExpected = NULL;

	if (CScalarBoolOp::EboolopNot != eboolop)
	{
		CExpression *pexprConstExpected = CUtils::PexprScalarConstInt4(memory_pool, 5 /*val*/);
		pexprPredExpected = CUtils::PexprScalarEqCmp(memory_pool, pcrLeft, pexprConstExpected);
	}

	ExpressionArray *pdrgpexprActual = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexprActual->Append(pexprPredActual);

	if (CScalarBoolOp::EboolopNot != eboolop)
	{
		pdrgpexprActual->Append(pexprPredExpected);
	}

	CExpression *pexprBoolOp = CUtils::PexprScalarBoolOp(memory_pool, eboolop , pdrgpexprActual);
	ExpressionArray *pdrgpexprExpected = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexprExpected->Append(pexprBoolOp);

	if (CScalarBoolOp::EboolopNot != eboolop)
	{
		pexprPredExpected->AddRef();
		pdrgpexprExpected->Append(pexprPredExpected);
	}

	return CUtils::PexprScalarBoolOp(memory_pool, eboolop , pdrgpexprExpected);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprFindFirstExpressionWithOpId
//
//	@doc:
//		DFS of expression tree to find and return a pointer to the expression
//		containing the given operator type. NULL if not found
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprFindFirstExpressionWithOpId
	(
	CExpression *pexpr,
	COperator::EOperatorId op_id
	)
{
	GPOS_ASSERT(NULL != pexpr);
	if (op_id == pexpr->Pop()->Eopid())
	{
		return pexpr;
	}

	ULONG arity = pexpr->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		CExpression *pexprFound = PexprFindFirstExpressionWithOpId((*pexpr)[ul], op_id);
		if (NULL != pexprFound)
		{
			return pexprFound;
		}
	}

	return NULL;
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::EqualityPredicate
//
//	@doc:
//		Generate equality predicate given column sets
//
//---------------------------------------------------------------------------
void
CTestUtils::EqualityPredicate
	(
	IMemoryPool *memory_pool,
	CColRefSet *pcrsLeft,
	CColRefSet *pcrsRight,
	ExpressionArray *pdrgpexpr
	)
{
	CColRef *pcrLeft = pcrsLeft->PcrAny();
	CColRef *pcrRight = pcrsRight->PcrAny();

	// generate correlated predicate
	CExpression *pexprCorrelation = CUtils::PexprScalarEqCmp(memory_pool, pcrLeft, pcrRight);

	pdrgpexpr->Append(pexprCorrelation);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PpointInt2
//
//	@doc:
//		Create an INT2 point
//
//---------------------------------------------------------------------------
CPoint *
CTestUtils::PpointInt2
	(
	IMemoryPool *memory_pool,
	INT i
	)
{
	CDatumInt2GPDB *datum = GPOS_NEW(memory_pool) CDatumInt2GPDB(m_sysidDefault, (SINT) i);
	CPoint *point = GPOS_NEW(memory_pool) CPoint(datum);
	return point;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PpointInt4
//
//	@doc:
//		Create an INT4 point
//
//---------------------------------------------------------------------------
CPoint *
CTestUtils::PpointInt4
	(
	IMemoryPool *memory_pool,
	INT i
	)
{
	CDatumInt4GPDB *datum = GPOS_NEW(memory_pool) CDatumInt4GPDB(m_sysidDefault, i);
	CPoint *point = GPOS_NEW(memory_pool) CPoint(datum);
	return point;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PpointInt4NullVal
//
//	@doc:
//		Create an INT4 point with null m_bytearray_value
//
//---------------------------------------------------------------------------
CPoint *
CTestUtils::PpointInt4NullVal
	(
	IMemoryPool *memory_pool
	)
{
	CDatumInt4GPDB *datum = GPOS_NEW(memory_pool) CDatumInt4GPDB(m_sysidDefault, 0, true /* is_null */);
	CPoint *point = GPOS_NEW(memory_pool) CPoint(datum);
	return point;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PpointInt8
//
//	@doc:
//		Create an INT8 point
//
//---------------------------------------------------------------------------
CPoint *
CTestUtils::PpointInt8
	(
	IMemoryPool *memory_pool,
	INT i
	)
{
	CDatumInt8GPDB *datum = GPOS_NEW(memory_pool) CDatumInt8GPDB(m_sysidDefault, (LINT) i);
	CPoint *point = GPOS_NEW(memory_pool) CPoint(datum);
	return point;
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PpointBool
//
//	@doc:
//		Create a point
//
//---------------------------------------------------------------------------
CPoint *
CTestUtils::PpointBool
	(
	IMemoryPool *memory_pool,
	BOOL value
	)
{
	CDatumBoolGPDB *datum = GPOS_NEW(memory_pool) CDatumBoolGPDB(m_sysidDefault, value);
	CPoint *point = GPOS_NEW(memory_pool) CPoint(datum);
	return point;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprReadQuery
//
//	@doc:
//		Return query logical expression from minidump
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprReadQuery
	(
	IMemoryPool *memory_pool,
	const CHAR *szQueryFileName
	)
{
	CHAR *szQueryDXL = CDXLUtils::Read(memory_pool, szQueryFileName);

	// parse the DXL query tree from the given DXL document
	CQueryToDXLResult *ptroutput = CDXLUtils::ParseQueryToQueryDXLTree(memory_pool, szQueryDXL, NULL);

	// get md accessor
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	GPOS_ASSERT(NULL != md_accessor);

	// translate DXL tree into CExpression
	CTranslatorDXLToExpr trdxl2expr(memory_pool, md_accessor);
	CExpression *pexprQuery =
			trdxl2expr.PexprTranslateQuery
						(
						ptroutput->CreateDXLNode(),
						ptroutput->GetOutputColumnsDXLArray(),
						ptroutput->GetCTEProducerDXLArray()
						);

	GPOS_DELETE(ptroutput);
	GPOS_DELETE_ARRAY(szQueryDXL);

	return pexprQuery;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::EresTranslate
//
//	@doc:
//		Rehydrate a query from the given file, optimize it, translate back to
//		DXL and compare the resulting plan to the given DXL document
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTestUtils::EresTranslate
	(
	IMemoryPool *memory_pool,
	const CHAR *szQueryFileName,
	const CHAR *szPlanFileName,
	BOOL fIgnoreMismatch
	)
{
	// debug print
	CWStringDynamic str(memory_pool);
	COstreamString oss(&str);

	GPOS_TRACE(str.GetBuffer());

	// read the dxl document
	CHAR *szQueryDXL = CDXLUtils::Read(memory_pool, szQueryFileName);

	// parse the DXL query tree from the given DXL document
	CQueryToDXLResult *ptroutput = CDXLUtils::ParseQueryToQueryDXLTree(memory_pool, szQueryDXL, NULL);

	// get md accessor
	CMDAccessor *md_accessor = COptCtxt::PoctxtFromTLS()->Pmda();
	GPOS_ASSERT(NULL != md_accessor);

	// translate DXL tree into CExpression
	CTranslatorDXLToExpr ptrdxl2expr(memory_pool, md_accessor);
	CExpression *pexprQuery = ptrdxl2expr.PexprTranslateQuery
											(
											ptroutput->CreateDXLNode(),
											ptroutput->GetOutputColumnsDXLArray(),
											ptroutput->GetCTEProducerDXLArray()
											);

	CQueryContext *pqc = CQueryContext::PqcGenerate
												(
												memory_pool,
												pexprQuery,
												ptrdxl2expr.PdrgpulOutputColRefs(),
												ptrdxl2expr.Pdrgpmdname(),
												true /*fDeriveStats*/
												);

#ifdef GPOS_DEBUG
	pqc->OsPrint(oss);
#endif //GPOS_DEBUG

	gpopt::CEngine eng(memory_pool);
	eng.Init(pqc, NULL /*search_stage_array*/);

#ifdef GPOS_DEBUG
	eng.RecursiveOptimize();
#else
	eng.Optimize();
#endif //GPOS_DEBUG

	gpopt::CExpression *pexprPlan = eng.PexprExtractPlan();
	GPOS_ASSERT(NULL != pexprPlan);

	(void) pexprPlan->PrppCompute(memory_pool, pqc->Prpp());
	pexprPlan->OsPrint(oss);

	// translate plan back to DXL
	CTranslatorExprToDXL ptrexpr2dxl(memory_pool, md_accessor, PdrgpiSegments(memory_pool));
	CDXLNode *pdxlnPlan = ptrexpr2dxl.PdxlnTranslate(pexprPlan, pqc->PdrgPcr(), pqc->Pdrgpmdname());
	GPOS_ASSERT(NULL != pdxlnPlan);

	COptimizerConfig *optimizer_config = COptCtxt::PoctxtFromTLS()->GetOptimizerConfig();

	CWStringDynamic strTranslatedPlan(memory_pool);
	COstreamString osTranslatedPlan(&strTranslatedPlan);

	CDXLUtils::SerializePlan(memory_pool, osTranslatedPlan, pdxlnPlan, optimizer_config->GetEnumeratorCfg()->GetPlanId(), optimizer_config->GetEnumeratorCfg()->GetPlanSpaceSize(), true /*serialize_header_footer*/, true /*indentation*/);

	GPOS_TRACE(str.GetBuffer());
	GPOS_RESULT eres = GPOS_OK;
	if (NULL != szPlanFileName)
	{
		// parse the DXL plan tree from the given DXL file
		CHAR *szExpectedPlan = CDXLUtils::Read(memory_pool, szPlanFileName);

		CWStringDynamic strExpectedPlan(memory_pool);
		strExpectedPlan.AppendFormat(GPOS_WSZ_LIT("%s"), szExpectedPlan);

		eres = EresCompare(oss, &strTranslatedPlan, &strExpectedPlan, fIgnoreMismatch);
		GPOS_DELETE_ARRAY(szExpectedPlan);
	}

	// cleanup
	pexprQuery->Release();
	pexprPlan->Release();
	pdxlnPlan->Release();
	GPOS_DELETE_ARRAY(szQueryDXL);
	GPOS_DELETE(ptroutput);
	GPOS_DELETE(pqc);
	return eres;
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::EresCompare
//
//	@doc:
//		Compare expected and actual output
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTestUtils::EresCompare
	(
	IOstream &os,
	CWStringDynamic *pstrActual,
	CWStringDynamic *pstrExpected,
	BOOL fIgnoreMismatch
	)
{
	if (!pstrExpected->Equals(pstrActual))
	{
		os << "Output does not match expected DXL document" << std::endl;
		os << "Actual: " << std::endl;
		os << pstrActual->GetBuffer() << std::endl;

		os << "Expected: " << std::endl;
		os << pstrExpected->GetBuffer() << std::endl;

		if (fIgnoreMismatch)
		{
			return GPOS_OK;
		}
		return GPOS_FAILED;
	}
	else
	{
		os << "Output matches expected DXL document" << std::endl;
		return GPOS_OK;
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::FPlanMatch
//
//	@doc:
//		Match given two plans using string comparison
//
//---------------------------------------------------------------------------
BOOL
CTestUtils::FPlanMatch
	(
	IMemoryPool *memory_pool,
	IOstream &os,
	const CDXLNode *pdxlnActual,
	ULLONG ullPlanIdActual,
	ULLONG ullPlanSpaceSizeActual,
	const CDXLNode *pdxlnExpected,
	ULLONG ullPlanIdExpected,
	ULLONG ullPlanSpaceSizeExpected
	)
{
	if (NULL == pdxlnActual && NULL == pdxlnExpected)
	{
		CAutoTrace at(memory_pool);
		at.Os() << "Both plans are NULL." << std::endl;

		return true;
	}

	if (NULL != pdxlnActual && NULL == pdxlnExpected)
	{
		CAutoTrace at(memory_pool);
		at.Os() << "Plan comparison *** FAILED ***" << std::endl;
		at.Os() << "Expected plan is NULL. Actual: " << std::endl;
		CDXLUtils::SerializePlan(memory_pool, at.Os(), pdxlnActual, ullPlanIdActual, ullPlanSpaceSizeActual, false /*serialize_document_header_footer*/, true /*indentation*/);
		at.Os() << std::endl;

		return false;
	}

	if (NULL == pdxlnActual && NULL != pdxlnExpected)
	{
		CAutoTrace at(memory_pool);
		at.Os() << "Plan comparison *** FAILED ***" << std::endl;
		at.Os()  << "Actual plan is NULL. Expected: " << std::endl;
		CDXLUtils::SerializePlan(memory_pool, at.Os(), pdxlnExpected, ullPlanIdExpected, ullPlanSpaceSizeExpected, false /*serialize_document_header_footer*/, true /*indentation*/);
		at.Os()  << std::endl;

		return false;
	}		
		
	GPOS_ASSERT(NULL != pdxlnActual);
	GPOS_ASSERT(NULL != pdxlnExpected);
	
	// plan id's and space sizes are already compared before this point,
	// overwrite PlanId's and space sizes with zeros to pass string comparison on plan body
	CWStringDynamic strActual(memory_pool);
	COstreamString osActual(&strActual);
	CDXLUtils::SerializePlan(memory_pool, osActual, pdxlnActual, 0 /*ullPlanIdActual*/, 0 /*ullPlanSpaceSizeActual*/, false /*serialize_document_header_footer*/, true /*indentation*/);
	GPOS_CHECK_ABORT;

	CWStringDynamic strExpected(memory_pool);
	COstreamString osExpected(&strExpected);
	CDXLUtils::SerializePlan(memory_pool, osExpected, pdxlnExpected, 0 /*ullPlanIdExpected*/, 0 /*ullPlanSpaceSizeExpected*/, false /*serialize_document_header_footer*/, true /*indentation*/);
	GPOS_CHECK_ABORT;

	BOOL result = strActual.Equals(&strExpected);

	if (!result)
	{
		// serialize plans again to restore id's and space size before printing error message
		CWStringDynamic strActual(memory_pool);
		COstreamString osActual(&strActual);
		CDXLUtils::SerializePlan(memory_pool, osActual, pdxlnActual, ullPlanIdActual, ullPlanSpaceSizeActual, false /*serialize_document_header_footer*/, true /*indentation*/);
		GPOS_CHECK_ABORT;

		CWStringDynamic strExpected(memory_pool);
		COstreamString osExpected(&strExpected);
		CDXLUtils::SerializePlan(memory_pool, osExpected, pdxlnExpected, ullPlanIdExpected, ullPlanSpaceSizeExpected, false /*serialize_document_header_footer*/, true /*indentation*/);
		GPOS_CHECK_ABORT;

		{
			CAutoTrace at(memory_pool);

			at.Os() << "Plan comparison *** FAILED ***" << std::endl;
			at.Os() << "Actual: " << std::endl;
			at.Os()  << strActual.GetBuffer() << std::endl;
		}

		os << "Expected: " << std::endl;
		os << strExpected.GetBuffer() << std::endl;
	}
	
	
	GPOS_CHECK_ABORT;
	return result;
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::FPlanCompare
//
//	@doc:
//		Compare two plans based on their string representation
//
//---------------------------------------------------------------------------
BOOL
CTestUtils::FPlanCompare
	(
	IMemoryPool *memory_pool,
	IOstream &os,
	const CDXLNode *pdxlnActual,
	ULLONG ullPlanIdActual,
	ULLONG ullPlanSpaceSizeActual,
	const CDXLNode *pdxlnExpected,
	ULLONG ullPlanIdExpected,
	ULLONG ullPlanSpaceSizeExpected,
	BOOL fMatchPlans,
	INT iCmpSpaceSize
	)
{
	BOOL fPlanSpaceUnchanged = true;

	if (!fMatchPlans)
	{
		return true;
	}

	CAutoTrace at(memory_pool);

	if (ullPlanIdActual != ullPlanIdExpected)
	{
		at.Os()
		<< "Plan Id mismatch." << std::endl
		<< "\tActual Id: " << ullPlanIdActual << std::endl
		<< "\tExpected Id: " << ullPlanIdExpected << std::endl;

		return false;
	}

	// check plan space size required comparison
	if (
		(0 == iCmpSpaceSize && ullPlanSpaceSizeActual != ullPlanSpaceSizeExpected) ||	// required comparison is equality
		(-1 == iCmpSpaceSize && ullPlanSpaceSizeActual > ullPlanSpaceSizeExpected) ||	// required comparison is (Actual <= Expected)
		(1 == iCmpSpaceSize && ullPlanSpaceSizeActual < ullPlanSpaceSizeExpected)  // required comparison is (Actual >= Expected)
		)
	{
		at.Os()
				<< "Plan space size comparison *** FAILED ***" << std::endl
				<< "Required comparison: " << iCmpSpaceSize << std::endl
				<< "\tActual size: " << ullPlanSpaceSizeActual << std::endl
				<< "\tExpected size: " << ullPlanSpaceSizeExpected << std::endl;

		fPlanSpaceUnchanged = false;
	}

	// perform deep matching on plan bodies
	return FPlanMatch(memory_pool, os, pdxlnActual, ullPlanIdActual, ullPlanSpaceSizeActual, pdxlnExpected, ullPlanIdExpected, ullPlanSpaceSizeExpected) && fPlanSpaceUnchanged;
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PdrgpiSegments
//
//	@doc:
//		Helper function for creating an array of segment ids for the target system
//
//---------------------------------------------------------------------------
IntPtrArray *
CTestUtils::PdrgpiSegments
	(
	IMemoryPool *memory_pool
	)
{
	IntPtrArray *pdrgpiSegments = GPOS_NEW(memory_pool) IntPtrArray(memory_pool);
	const ULONG ulSegments = GPOPT_SEGMENT_COUNT;
	GPOS_ASSERT(0 < ulSegments);

	for (ULONG ul = 0; ul < ulSegments; ul++)
	{
		pdrgpiSegments->Append(GPOS_NEW(memory_pool) INT(ul));
	}
	return pdrgpiSegments;
}

#ifdef GPOS_DEBUG
//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::FFaultSimulation
//
//	@doc:
//		Check if we are in fault simulation mode
//
//---------------------------------------------------------------------------
BOOL
CTestUtils::FFaultSimulation()
{
	return GPOS_FTRACE(EtraceSimulateAbort) 
			|| GPOS_FTRACE(EtraceSimulateIOError) 
			|| GPOS_FTRACE(EtraceSimulateOOM) 
			|| IWorker::m_enforce_time_slices;
}
#endif // GPOS_DEBUG


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::SzMinidumpFileName
//
//	@doc:
//		Generate minidump file name from passed file name
//
//---------------------------------------------------------------------------
CHAR *
CTestUtils::SzMinidumpFileName
	(
	IMemoryPool *memory_pool,
	const CHAR *file_name
	)
{
	GPOS_ASSERT(NULL != file_name);

	if (!GPOS_FTRACE(EopttraceEnableSpacePruning))
	{
		return const_cast<CHAR *>(file_name);
	}

	CWStringDynamic *pstrMinidumpFileName = GPOS_NEW(memory_pool) CWStringDynamic(memory_pool);
	COstreamString oss(pstrMinidumpFileName);
	oss << file_name << "-space-pruned";

	// convert wide char to regular char
	const WCHAR *wsz = pstrMinidumpFileName->GetBuffer();
	const ULONG ulInputLength = GPOS_WSZ_LENGTH(wsz);
	const ULONG ulWCHARSize = GPOS_SIZEOF(WCHAR);
	const ULONG ulMaxLength = (ulInputLength + 1) * ulWCHARSize;
	CHAR *sz = GPOS_NEW_ARRAY(memory_pool, CHAR, ulMaxLength);
	gpos::clib::Wcstombs(sz, const_cast<WCHAR *>(wsz), ulMaxLength);
	sz[ulMaxLength - 1] = '\0';

	GPOS_DELETE(pstrMinidumpFileName);

	return sz;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::EresRunMinidump
//
//	@doc:
//		Run one minidump-based test using passed MD Accessor
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTestUtils::EresRunMinidump
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	const CHAR *file_name,
	ULONG *pulTestCounter,
	ULONG ulSessionId,
	ULONG ulCmdId,
	BOOL fMatchPlans,
	INT iCmpSpaceSize,
	IConstExprEvaluator *pceeval
	)
{
	GPOS_ASSERT(NULL != md_accessor);

	GPOS_RESULT eres = GPOS_OK;

	{
		CAutoTrace at(memory_pool);
		at.Os() << "executing " << file_name;
	}

	// load dump file
	CDXLMinidump *pdxlmd = CMinidumperUtils::PdxlmdLoad(memory_pool, file_name);
	GPOS_CHECK_ABORT;

	COptimizerConfig *optimizer_config = pdxlmd->GetOptimizerConfig();

	if (NULL == optimizer_config)
	{
		optimizer_config = COptimizerConfig::PoconfDefault(memory_pool);
	}
	else
	{
		optimizer_config->AddRef();
	}

	ULONG ulSegments = UlSegments(optimizer_config);

	// allow sampler to throw invalid plan exception
	optimizer_config->GetEnumeratorCfg()->SetSampleValidPlans(false /*fSampleValidPlans*/);

	CDXLNode *pdxlnPlan = NULL;

	CHAR *szMinidumpFileName = SzMinidumpFileName(memory_pool, file_name);

	pdxlnPlan = CMinidumperUtils::PdxlnExecuteMinidump
					(
					memory_pool,
					md_accessor,
					pdxlmd,
					szMinidumpFileName,
					ulSegments,
					ulSessionId,
					ulCmdId,
					optimizer_config,
					pceeval
					);

	if (szMinidumpFileName != file_name)
	{
		// a new name was generated
		GPOS_DELETE_ARRAY(szMinidumpFileName);
	}

	GPOS_CHECK_ABORT;


	{
		CAutoTrace at(memory_pool);
		if (!CTestUtils::FPlanCompare
			(
			memory_pool,
			at.Os(),
			pdxlnPlan,
			optimizer_config->GetEnumeratorCfg()->GetPlanId(),
			optimizer_config->GetEnumeratorCfg()->GetPlanSpaceSize(),
			pdxlmd->PdxlnPlan(),
			pdxlmd->GetPlanId(),
			pdxlmd->GetPlanSpaceSize(),
			fMatchPlans,
			iCmpSpaceSize
			)
		)
		{
			eres = GPOS_FAILED;
		}
    }

	GPOS_CHECK_ABORT;

	{
		CAutoTrace at(memory_pool);
		at.Os() << std::endl;
	}

	// cleanup
	GPOS_DELETE(pdxlmd);
	pdxlnPlan->Release();
	optimizer_config->Release();

	(*pulTestCounter)++;
	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::EresRunMinidumps
//
//	@doc:
//		Run minidump-based tests
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTestUtils::EresRunMinidumps
	(
	IMemoryPool *, // pmpInput,
	const CHAR *rgszFileNames[],
	ULONG ulTests,
	ULONG *pulTestCounter,
	ULONG ulSessionId,
	ULONG ulCmdId,
	BOOL fMatchPlans,
	BOOL fTestSpacePruning,
	const CHAR *, // szMDFilePath,
	IConstExprEvaluator *pceeval
	)
{
	GPOS_RESULT eres = GPOS_OK;
	BOOL fSuccess = true;

	for (ULONG ul = *pulTestCounter; ul < ulTests; ul++)
	{
		// each test uses a new memory pool to keep total memory consumption low
		CAutoMemoryPool amp;
		IMemoryPool *memory_pool = amp.Pmp();

		if (fMatchPlans)
		{
			{
				CAutoTrace at(memory_pool);
				at.Os() << "IsRunning test with EXHAUSTIVE SEARCH:";
			}

			eres = EresRunMinidumpsUsingOneMDFile
				(
				memory_pool,
				rgszFileNames[ul],
				&rgszFileNames[ul],
				pulTestCounter,
				ulSessionId,
				ulCmdId,
				fMatchPlans,
				0, // iCmpSpaceSize
				pceeval
				);

			if (GPOS_FAILED == eres)
			{
				fSuccess = false;
			}
		}

		if (GPOS_OK == eres && fTestSpacePruning)
		{

			{
				CAutoTrace at(memory_pool);
				at.Os() << "IsRunning test with BRANCH-AND-BOUND SEARCH:";
			}

			// enable space pruning
			CAutoTraceFlag atf(EopttraceEnableSpacePruning, true /*m_bytearray_value*/);

			eres = EresRunMinidumpsUsingOneMDFile
				(
				memory_pool,
				rgszFileNames[ul],
				&rgszFileNames[ul],
				pulTestCounter,
				ulSessionId,
				ulCmdId,
				fMatchPlans,
				-1, // iCmpSpaceSize
				pceeval
				);

			if (GPOS_FAILED == eres)
			{
				fSuccess = false;
			}
		}
	}

	*pulTestCounter = 0;

	// return GPOS_OK if all the minidump tests passed.
	// the minidump test runner, EresRunMinidump(), only returns
	// GPOS_FAILED in case of a failure, hence remaining error codes need
	// not be handled here
	return fSuccess ? GPOS_OK : GPOS_FAILED;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::EresRunMinidumpsUsingOneMDFile
//
//	@doc:
//		Run all minidumps based on one metadata file
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTestUtils::EresRunMinidumpsUsingOneMDFile
	(
	IMemoryPool *memory_pool,
	const CHAR *szMDFilePath,
	const CHAR *rgszFileNames[],
	ULONG *pulTestCounter,
	ULONG ulSessionId,
	ULONG ulCmdId,
	BOOL fMatchPlans,
	INT iCmpSpaceSize,
	IConstExprEvaluator *pceeval
	)
{
	GPOS_ASSERT(NULL != rgszFileNames);
	GPOS_ASSERT(NULL != szMDFilePath);

	// reset metadata cache
	CMDCache::Reset();

	// load metadata file
	CDXLMinidump *pdxlmd = CMinidumperUtils::PdxlmdLoad(memory_pool, szMDFilePath);
	GPOS_CHECK_ABORT;

	// set up MD providers
	CMDProviderMemory *pmdp = GPOS_NEW(memory_pool) CMDProviderMemory(memory_pool, szMDFilePath);
	GPOS_CHECK_ABORT;

	const SysidPtrArray *pdrgpsysid = pdxlmd->GetSysidPtrArray();
	MDProviderPtrArray *pdrgpmdp = GPOS_NEW(memory_pool) MDProviderPtrArray(memory_pool);
	pdrgpmdp->Append(pmdp);

	for (ULONG ul = 1; ul < pdrgpsysid->Size(); ul++)
	{
		pmdp->AddRef();
		pdrgpmdp->Append(pmdp);
	}

	GPOS_RESULT eres = GPOS_OK;

	{	// scope for MD accessor
		CMDAccessor mda(memory_pool, CMDCache::Pcache(), pdrgpsysid, pdrgpmdp);

		eres = EresRunMinidump(memory_pool, &mda, rgszFileNames[0], pulTestCounter, ulSessionId, ulCmdId, fMatchPlans, iCmpSpaceSize, pceeval);

		*pulTestCounter = 0;
	}

	// cleanup
	pdrgpmdp->Release();
	GPOS_DELETE(pdxlmd);
	GPOS_CHECK_ABORT;

	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::EresSamplePlans
//
//	@doc:
//		Test plan sampling
//		to extract attribute 'X' m_bytearray_value from xml file:
//		xpath distr.xml //dxl:Value/@X | grep 'X=' | sed 's/\"//g' | sed 's/X=//g' | tr ' ' '\n'
//
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTestUtils::EresSamplePlans
	(
	const CHAR *rgszFileNames[],
	ULONG ulTests,
	ULONG *pulTestCounter,
	ULONG ulSessionId,
	ULONG ulCmdId
	)
{
	GPOS_RESULT eres = GPOS_OK;

	for (ULONG ul = *pulTestCounter; ul < ulTests; ul++)
	{
		// each test uses a new memory pool to keep total memory consumption low
		CAutoMemoryPool amp;
		IMemoryPool *memory_pool = amp.Pmp();

		// reset metadata cache
		CMDCache::Reset();

		CAutoTraceFlag atf1(EopttraceEnumeratePlans, true);
		CAutoTraceFlag atf2(EopttraceSamplePlans, true);

		// load dump file
		CDXLMinidump *pdxlmd = CMinidumperUtils::PdxlmdLoad(memory_pool, rgszFileNames[ul]);
		GPOS_CHECK_ABORT;

		// set up MD providers
		CMDProviderMemory *pmdp = GPOS_NEW(memory_pool) CMDProviderMemory(memory_pool, rgszFileNames[ul]);
		GPOS_CHECK_ABORT;

		const SysidPtrArray *pdrgpsysid = pdxlmd->GetSysidPtrArray();
		MDProviderPtrArray *pdrgpmdp = GPOS_NEW(memory_pool) MDProviderPtrArray(memory_pool);
		pdrgpmdp->Append(pmdp);

		for (ULONG ulSys = 1; ulSys < pdrgpsysid->Size(); ulSys++)
		{
			pmdp->AddRef();
			pdrgpmdp->Append(pmdp);
		}

		COptimizerConfig *optimizer_config = pdxlmd->GetOptimizerConfig();
			
		if (NULL == optimizer_config)
		{
			optimizer_config = GPOS_NEW(memory_pool) COptimizerConfig
								(
								GPOS_NEW(memory_pool) CEnumeratorConfig(memory_pool, 0 /*plan_id*/, 1000 /*ullSamples*/),
								CStatisticsConfig::PstatsconfDefault(memory_pool),
								CCTEConfig::PcteconfDefault(memory_pool),
								ICostModel::PcmDefault(memory_pool),
								CHint::PhintDefault(memory_pool),
								CWindowOids::GetWindowOids(memory_pool)
								);
		}
		else
		{
			optimizer_config->AddRef();
		}

		ULONG ulSegments = UlSegments(optimizer_config);

		// allow sampler to throw invalid plan exception
		optimizer_config->GetEnumeratorCfg()->SetSampleValidPlans(false /*fSampleValidPlans*/);

		{
			// scope for MD accessor
			CMDAccessor mda(memory_pool, CMDCache::Pcache(), pdrgpsysid, pdrgpmdp);

			CDXLNode *pdxlnPlan = CMinidumperUtils::PdxlnExecuteMinidump
							(
							memory_pool,
							&mda,
							pdxlmd,
							rgszFileNames[ul],
							ulSegments,
							ulSessionId,
							ulCmdId,
							optimizer_config,
							NULL // pceeval
							);

			GPOS_CHECK_ABORT;

			{
				CAutoTrace at(memory_pool);

				at.Os() << "Generated " <<  optimizer_config->GetEnumeratorCfg()->UlCreatedSamples() <<" samples ... " << std::endl;

				// print ids of sampled plans
				CWStringDynamic *str = CDXLUtils::SerializeSamplePlans(memory_pool, optimizer_config->GetEnumeratorCfg(), true /*fIdent*/);
				at.Os() << str->GetBuffer();
				GPOS_DELETE(str);

				// print fitted cost distribution
				at.Os() << "Cost Distribution: " << std::endl;
				const ULONG size = optimizer_config->GetEnumeratorCfg()->UlCostDistrSize();
				for (ULONG ul = 0; ul < size; ul++)
				{
					at.Os() << optimizer_config->GetEnumeratorCfg()->DCostDistrX(ul) << "\t" << optimizer_config->GetEnumeratorCfg()->DCostDistrY(ul) << std::endl;
				}

				// print serialized cost distribution
				str = CDXLUtils::SerializeCostDistr(memory_pool, optimizer_config->GetEnumeratorCfg(), true /*fIdent*/);

				at.Os() << str->GetBuffer();
				GPOS_DELETE(str);

			}

			// cleanup
			GPOS_DELETE(pdxlmd);
			pdxlnPlan->Release();

		} // end of MDAccessor scope


		// cleanup
		optimizer_config->Release();
		pdrgpmdp->Release();


		(*pulTestCounter)++;
	}

	*pulTestCounter = 0;
	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::EresCheckPlans
//
//	@doc:
//		Check all enumerated plans using given PlanChecker function
//
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTestUtils::EresCheckPlans
	(
	const CHAR *rgszFileNames[],
	ULONG ulTests,
	ULONG *pulTestCounter,
	ULONG ulSessionId,
	ULONG ulCmdId,
	FnPlanChecker *pfpc
	)
{
	GPOS_ASSERT(NULL != pfpc);

	GPOS_RESULT eres = GPOS_OK;

	for (ULONG ul = *pulTestCounter; ul < ulTests; ul++)
	{
		// each test uses a new memory pool to keep total memory consumption low
		CAutoMemoryPool amp;
		IMemoryPool *memory_pool = amp.Pmp();

		// reset metadata cache
		CMDCache::Reset();

		CAutoTraceFlag atf1(EopttraceEnumeratePlans, true);

		// load dump file
		CDXLMinidump *pdxlmd = CMinidumperUtils::PdxlmdLoad(memory_pool, rgszFileNames[ul]);
		GPOS_CHECK_ABORT;

		// set up MD providers
		CMDProviderMemory *pmdp = GPOS_NEW(memory_pool) CMDProviderMemory(memory_pool, rgszFileNames[ul]);
		GPOS_CHECK_ABORT;

		const SysidPtrArray *pdrgpsysid = pdxlmd->GetSysidPtrArray();
		MDProviderPtrArray *pdrgpmdp = GPOS_NEW(memory_pool) MDProviderPtrArray(memory_pool);
		pdrgpmdp->Append(pmdp);

		for (ULONG ulSys = 1; ulSys < pdrgpsysid->Size(); ulSys++)
		{
			pmdp->AddRef();
			pdrgpmdp->Append(pmdp);
		}

		COptimizerConfig *optimizer_config = pdxlmd->GetOptimizerConfig();

		if (NULL == optimizer_config)
		{
			optimizer_config = GPOS_NEW(memory_pool) COptimizerConfig
								(
								GPOS_NEW(memory_pool) CEnumeratorConfig(memory_pool, 0 /*plan_id*/, 1000 /*ullSamples*/),
								CStatisticsConfig::PstatsconfDefault(memory_pool),
								CCTEConfig::PcteconfDefault(memory_pool),
								ICostModel::PcmDefault(memory_pool),
								CHint::PhintDefault(memory_pool),
								CWindowOids::GetWindowOids(memory_pool)
								);
		}
		else
		{
			optimizer_config->AddRef();
		}

		ULONG ulSegments = UlSegments(optimizer_config);

		// set plan checker
		optimizer_config->GetEnumeratorCfg()->SetPlanChecker(pfpc);

		// allow sampler to throw invalid plan exception
		optimizer_config->GetEnumeratorCfg()->SetSampleValidPlans(false /*fSampleValidPlans*/);

		{
			// scope for MD accessor
			CMDAccessor mda(memory_pool, CMDCache::Pcache(), pdrgpsysid, pdrgpmdp);

			CDXLNode *pdxlnPlan = CMinidumperUtils::PdxlnExecuteMinidump
							(
							memory_pool,
							&mda,
							pdxlmd,
							rgszFileNames[ul],
							ulSegments,
							ulSessionId,
							ulCmdId,
							optimizer_config,
							NULL // pceeval
							);

			GPOS_CHECK_ABORT;

			// cleanup
			GPOS_DELETE(pdxlmd);
			pdxlnPlan->Release();

		} // end of MDAcessor scope

		optimizer_config->Release();
		pdrgpmdp->Release();

		(*pulTestCounter)++;
	}

	*pulTestCounter = 0;
	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::UlSegments
//
//	@doc:
//		Return the number of segments, default return GPOPT_TEST_SEGMENTS
//
//---------------------------------------------------------------------------
ULONG
CTestUtils::UlSegments
	(
	COptimizerConfig *optimizer_config
	)
{
	GPOS_ASSERT(NULL != optimizer_config);
	ULONG ulSegments = GPOPT_TEST_SEGMENTS;
	if (NULL != optimizer_config->GetCostModel())
	{
		ULONG ulSegs = optimizer_config->GetCostModel()->UlHosts();
		if (ulSegments < ulSegs)
		{
			ulSegments = ulSegs;
		}
	}

	return ulSegments;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::EresCheckOptimizedPlan
//
//	@doc:
//		Check the optimized plan using given DXLPlanChecker function. Does
//		not take ownership of the given pdrgpcp. The cost model configured
//		in the minidumps must be the calibrated one.
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTestUtils::EresCheckOptimizedPlan
	(
	const CHAR *rgszFileNames[],
	ULONG ulTests,
	ULONG *pulTestCounter,
	ULONG ulSessionId,
	ULONG ulCmdId,
	FnDXLPlanChecker *pfdpc,
	CostModelParamsArray *pdrgpcp
	)
{
	GPOS_ASSERT(NULL != pfdpc);

	GPOS_RESULT eres = GPOS_OK;

	for (ULONG ul = *pulTestCounter; ul < ulTests; ul++)
	{
		// each test uses a new memory pool to keep total memory consumption low
		CAutoMemoryPool amp;
		IMemoryPool *memory_pool = amp.Pmp();

		// reset metadata cache
		CMDCache::Reset();

		CAutoTraceFlag atf1(EopttraceEnableSpacePruning, true /*m_bytearray_value*/);

		// load dump file
		CDXLMinidump *pdxlmd = CMinidumperUtils::PdxlmdLoad(memory_pool, rgszFileNames[ul]);
		GPOS_CHECK_ABORT;

		// set up MD providers
		CMDProviderMemory *pmdp = GPOS_NEW(memory_pool) CMDProviderMemory(memory_pool, rgszFileNames[ul]);
		GPOS_CHECK_ABORT;

		const SysidPtrArray *pdrgpsysid = pdxlmd->GetSysidPtrArray();
		MDProviderPtrArray *pdrgpmdp = GPOS_NEW(memory_pool) MDProviderPtrArray(memory_pool);
		pdrgpmdp->Append(pmdp);

		for (ULONG ulSys = 1; ulSys < pdrgpsysid->Size(); ulSys++)
		{
			pmdp->AddRef();
			pdrgpmdp->Append(pmdp);
		}

		COptimizerConfig *optimizer_config = pdxlmd->GetOptimizerConfig();
		GPOS_ASSERT(NULL != optimizer_config);

		if (NULL != pdrgpcp)
		{
			optimizer_config->GetCostModel()->SetParams(pdrgpcp);
		}
		optimizer_config->AddRef();
		ULONG ulSegments = UlSegments(optimizer_config);

		// allow sampler to throw invalid plan exception
		optimizer_config->GetEnumeratorCfg()->SetSampleValidPlans(false /*fSampleValidPlans*/);

		{
			// scope for MD accessor
			CMDAccessor mda(memory_pool, CMDCache::Pcache(), pdrgpsysid, pdrgpmdp);

			CDXLNode *pdxlnPlan = CMinidumperUtils::PdxlnExecuteMinidump
							(
							memory_pool,
							&mda,
							pdxlmd,
							rgszFileNames[ul],
							ulSegments,
							ulSessionId,
							ulCmdId,
							optimizer_config,
							NULL // pceeval
							);
			if (!pfdpc(pdxlnPlan))
			{
				eres = GPOS_FAILED;
				{
					CAutoTrace at(memory_pool);
					at.Os() << "Failed check for minidump " << rgszFileNames[ul] << std::endl;
					CDXLUtils::SerializePlan
						(
						memory_pool,
						at.Os(),
						pdxlnPlan,
						0, // plan_id
						0, // plan_space_size
						true, // serialize_header_footer
						true // indentation
						);
					at.Os() << std::endl;
				}
			}

			GPOS_CHECK_ABORT;

			// cleanup
			GPOS_DELETE(pdxlmd);
			pdxlnPlan->Release();

		} // end of MDAcessor scope

		optimizer_config->Release();
		pdrgpmdp->Release();

		(*pulTestCounter)++;
	}

	*pulTestCounter = 0;
	return eres;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::CreateGenericDatum
//
//	@doc:
//		Create a datum with a given type, encoded m_bytearray_value and int m_bytearray_value.
//
//---------------------------------------------------------------------------
IDatum *
CTestUtils::CreateGenericDatum
	(
	IMemoryPool *memory_pool,
	CMDAccessor *md_accessor,
	IMDId *mdid_type,
	CWStringDynamic *pstrEncodedValue,
	LINT value
	)
{
	GPOS_ASSERT(NULL != md_accessor);

	GPOS_ASSERT(!mdid_type->Equals(&CMDIdGPDB::m_mdid_numeric));
	const IMDType *pmdtype = md_accessor->RetrieveType(mdid_type);
	ULONG ulbaSize = 0;
	BYTE *data = CDXLUtils::DecodeByteArrayFromString(memory_pool, pstrEncodedValue, &ulbaSize);

	CDXLDatumGeneric *datum_dxl = NULL;
	if (CMDTypeGenericGPDB::IsTimeRelatedType(mdid_type))
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumStatsDoubleMappable(memory_pool, mdid_type, default_type_modifier, pmdtype->IsPassedByValue() /*is_const_by_val*/, false /*is_const_null*/, data, ulbaSize, CDouble(value));
	}
	else if (mdid_type->Equals(&CMDIdGPDB::m_mdid_bpchar))
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumStatsLintMappable(memory_pool, mdid_type, default_type_modifier, pmdtype->IsPassedByValue() /*is_const_by_val*/, false /*is_const_null*/, data, ulbaSize, value);
	}
	else
	{
		datum_dxl = GPOS_NEW(memory_pool) CDXLDatumGeneric(memory_pool, mdid_type, default_type_modifier, pmdtype->IsPassedByValue() /*is_const_by_val*/, false /*is_const_null*/, data, ulbaSize);
	}

	IDatum *datum = pmdtype->GetDatumForDXLDatum(memory_pool, datum_dxl);
	datum_dxl->Release();

	return datum;
}

//---------------------------------------------------------------------------
//      @function:
//              CConstraintTest::PciGenericInterval
//
//      @doc:
//              Create an interval for generic data types.
//              Does not take ownership of any argument.
//              Caller takes ownership of returned pointer.
//
//---------------------------------------------------------------------------
CConstraintInterval *
CTestUtils::PciGenericInterval
        (
        IMemoryPool *memory_pool,
        CMDAccessor *md_accessor,
        const CMDIdGPDB &mdidType,
        CColRef *colref,
        CWStringDynamic *pstrLower,
        LINT lLower,
        CRange::ERangeInclusion eriLeft,
        CWStringDynamic *pstrUpper,
        LINT lUpper,
        CRange::ERangeInclusion eriRight
        )
{
	GPOS_ASSERT(NULL != md_accessor);

	IDatum *pdatumLower =
			CTestUtils::CreateGenericDatum(memory_pool, md_accessor, GPOS_NEW(memory_pool) CMDIdGPDB(mdidType), pstrLower, lLower);
	IDatum *pdatumUpper =
			CTestUtils::CreateGenericDatum(memory_pool, md_accessor, GPOS_NEW(memory_pool) CMDIdGPDB(mdidType), pstrUpper, lUpper);

	RangeArray *pdrgprng = GPOS_NEW(memory_pool) RangeArray(memory_pool);
	CMDIdGPDB *mdid = GPOS_NEW(memory_pool) CMDIdGPDB(CMDIdGPDB::m_mdid_date);
	CRange *prange = GPOS_NEW(memory_pool) CRange
				(
				mdid,
				COptCtxt::PoctxtFromTLS()->Pcomp(),
				pdatumLower,
				eriLeft,
				pdatumUpper,
				eriRight
				);
	pdrgprng->Append(prange);

	return GPOS_NEW(memory_pool) CConstraintInterval(memory_pool, colref, pdrgprng, false /*is_null*/);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprScalarCmpIdentToConstant
//
//	@doc:
//		Helper for generating a scalar compare identifier to a constant
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprScalarCmpIdentToConstant
	(
	IMemoryPool *memory_pool,
	CExpression *pexpr
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexpr);

	CColRefSet *pcrs = CDrvdPropRelational::GetRelationalProperties(pexpr->PdpDerive())->PcrsOutput();
	CColRef *pcrAny =  pcrs->PcrAny();
	CExpression *pexprConst =  CUtils::PexprScalarConstInt4(memory_pool, 10 /* val */);

	return CUtils::PexprScalarEqCmp(memory_pool, pcrAny, pexprConst);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprExistsSubquery
//
//	@doc:
//		Helper for generating an exists subquery
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprExistsSubquery
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexprOuter);

	CExpression *pexprInner = CTestUtils::PexprLogicalGet(memory_pool);

	return CSubqueryTestUtils::PexprSubqueryExistential
			(
			memory_pool,
			COperator::EopScalarSubqueryExists,
			pexprOuter,
			pexprInner,
			false /* fCorrelated */
			);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexpSubqueryAll
//
//	@doc:
//		Helper for generating an ALL subquery
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexpSubqueryAll
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexprOuter);

	CColRefSet *outer_refs = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();
	const CColRef *pcrOuter = outer_refs->PcrAny();


	CExpression *pexprInner = CTestUtils::PexprLogicalGet(memory_pool);
	CColRefSet *pcrsInner = CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->PcrsOutput();
	const CColRef *pcrInner = pcrsInner->PcrAny();

	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CScalarSubqueryAll
								(
								memory_pool,
								GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_EQ_OP),
								GPOS_NEW(memory_pool) CWStringConst(GPOS_WSZ_LIT("=")),
								pcrInner
								),
					pexprInner,
					CUtils::PexprScalarIdent(memory_pool, pcrOuter)
					);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexpSubqueryAny
//
//	@doc:
//		Helper for generating an ANY subquery
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexpSubqueryAny
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexprOuter);

	CColRefSet *outer_refs = CDrvdPropRelational::GetRelationalProperties(pexprOuter->PdpDerive())->PcrsOutput();
	const CColRef *pcrOuter = outer_refs->PcrAny();


	CExpression *pexprInner = CTestUtils::PexprLogicalGet(memory_pool);
	CColRefSet *pcrsInner = CDrvdPropRelational::GetRelationalProperties(pexprInner->PdpDerive())->PcrsOutput();
	const CColRef *pcrInner = pcrsInner->PcrAny();

	return GPOS_NEW(memory_pool) CExpression
					(
					memory_pool,
					GPOS_NEW(memory_pool) CScalarSubqueryAny
								(
								memory_pool,
								GPOS_NEW(memory_pool) CMDIdGPDB(GPDB_INT4_EQ_OP),
								GPOS_NEW(memory_pool) CWStringConst(GPOS_WSZ_LIT("=")),
								pcrInner
								),
								pexprInner,
					CUtils::PexprScalarIdent(memory_pool, pcrOuter)
					);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprNotExistsSubquery
//
//	@doc:
//		Helper for generating a not exists subquery
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprNotExistsSubquery
	(
	IMemoryPool *memory_pool,
	CExpression *pexprOuter
	)
{
	GPOS_ASSERT(NULL != memory_pool);
	GPOS_ASSERT(NULL != pexprOuter);

	CExpression *pexprInner = CTestUtils::PexprLogicalGet(memory_pool);

	return CSubqueryTestUtils::PexprSubqueryExistential
			(
			memory_pool,
			COperator::EopScalarSubqueryNotExists,
			pexprOuter,
			pexprInner,
			false /* fCorrelated */
			);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::FHasOp
//
//	@doc:
//		 Recursively traverses the subtree rooted at the given expression, and
//  	 return the first subexpression it encounters that has the given id
//
//---------------------------------------------------------------------------
const CExpression *
CTestUtils::PexprFirst
	(
	const CExpression *pexpr,
	const COperator::EOperatorId op_id
	)
{
	GPOS_CHECK_STACK_SIZE;
	GPOS_ASSERT(NULL != pexpr);

	if (pexpr->Pop()->Eopid() == op_id)
	{
		return pexpr;
	}

	// recursively check children
	const ULONG arity = pexpr->Arity();
	for (ULONG ul = 0; ul < arity; ul++)
	{
		const CExpression *pexprFirst = PexprFirst((*pexpr)[ul], op_id);
		if (NULL != pexprFirst)
		{
			return pexprFirst;
		}

	}

	return NULL;
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprAnd
//
//	@doc:
//		Generate a scalar AND expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprAnd
	(
	IMemoryPool *memory_pool,
	CExpression *pexprActual,
	CExpression *pexprExpected
	)
{
	GPOS_ASSERT(NULL != pexprActual);
	GPOS_ASSERT(NULL != pexprExpected);

	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexpr->Append(pexprActual);
	pdrgpexpr->Append(pexprExpected);

	return CUtils::PexprScalarBoolOp(memory_pool, CScalarBoolOp::EboolopAnd, pdrgpexpr);
}


//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::PexprOr
//
//	@doc:
//		Generate a scalar OR expression
//
//---------------------------------------------------------------------------
CExpression *
CTestUtils::PexprOr
	(
	IMemoryPool *memory_pool,
	CExpression *pexprActual,
	CExpression *pexprExpected
	)
{
	GPOS_ASSERT(NULL != pexprActual);
	GPOS_ASSERT(NULL != pexprExpected);

	ExpressionArray *pdrgpexpr = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
	pdrgpexpr->Append(pexprActual);
	pdrgpexpr->Append(pexprExpected);

	return CUtils::PexprScalarBoolOp(memory_pool, CScalarBoolOp::EboolopOr, pdrgpexpr);
}

//---------------------------------------------------------------------------
//	@function:
//		CTestUtils::EresUnittest_RunTests
//
//	@doc:
//		Run  Minidump-based tests in the given array of files
//
//---------------------------------------------------------------------------
GPOS_RESULT
CTestUtils::EresUnittest_RunTests
	(
	const CHAR **rgszFileNames,
	ULONG *pulTestCounter,
	ULONG ulTests
	)
{
	BOOL fMatchPlans = false;
	BOOL fTestSpacePruning = false;
#if defined(GPOS_Darwin) || defined(GPOS_Linux)
	// restrict plan matching to OsX and Linux to avoid arithmetic operations differences
	// across systems
	fMatchPlans = true;
	fTestSpacePruning = true;
#endif // GPOS_Darwin || GPOS_Linux
	// enable (Redistribute, Broadcast) hash join plans
	CAutoTraceFlag atf1(EopttraceEnableRedistributeBroadcastHashJoin, true /*m_bytearray_value*/);

	// enable plan enumeration only if we match plans
	CAutoTraceFlag atf2(EopttraceEnumeratePlans, fMatchPlans);

	// enable stats derivation for DPE
	CAutoTraceFlag atf3(EopttraceDeriveStatsForDPE, true /*m_bytearray_value*/);

	// prefer MDQA
	CAutoTraceFlag atf5(EopttraceForceExpandedMDQAs, true);

	GPOS_RESULT eres = EresUnittest_RunTestsWithoutAdditionalTraceFlags
						(
						rgszFileNames,
						pulTestCounter,
						ulTests,
						fMatchPlans,
						fTestSpacePruning
						);
	return eres;
}


GPOS_RESULT
CTestUtils::EresUnittest_RunTestsWithoutAdditionalTraceFlags
	(
	const CHAR **rgszFileNames,
	ULONG *pulTestCounter,
	ULONG ulTests,
	BOOL fMatchPlans,
	BOOL fTestSpacePruning
	)
{
	CAutoMemoryPool amp;
	IMemoryPool *memory_pool = amp.Pmp();


	GPOS_RESULT eres =
			CTestUtils::EresRunMinidumps
						(
						memory_pool,
						rgszFileNames,
						ulTests,
						pulTestCounter,
						1, // ulSessionId
						1,  // ulCmdId
						fMatchPlans,
						fTestSpacePruning
						);

	return eres;
}


// Create Equivalence Class based on the breakpoints
ColRefSetArray *
CTestUtils::createEquivalenceClasses(IMemoryPool *memory_pool, CColRefSet *pcrs, INT setBoundary[]) {
	INT i = 0;
	ULONG bpIndex = 0;

	ColRefSetArray *pdrgcrs = GPOS_NEW(memory_pool) ColRefSetArray(memory_pool);

	CColRefSetIter crsi(*pcrs);
	CColRefSet *pcrsLoop = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	while (crsi.Advance())
	{
		if (i == setBoundary[bpIndex]) {
			pdrgcrs->Append(pcrsLoop);
			CColRefSet *pcrsLoop1 = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
			pcrsLoop = pcrsLoop1;
			bpIndex++;
		}

		CColRef *colref = crsi.Pcr();
		pcrsLoop->Include(colref);
		i++;
	}
	pdrgcrs->Append(pcrsLoop);
	return pdrgcrs;
}
// EOF
