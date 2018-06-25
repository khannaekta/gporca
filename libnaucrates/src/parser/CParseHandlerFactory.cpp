//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010-2011 EMC Corp.
//
//	@filename:
// CParseHandlerFactory.cpp
//
//	@doc:
// Implementation of the factory methods for creating parse handlers
//
//	@owner:
//
//
//	@test:
//
//---------------------------------------------------------------------------

#include "naucrates/dxl/parser/parsehandlers.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"
#include "naucrates/dxl/xml/dxltokens.h"

using namespace gpdxl;

XERCES_CPP_NAMESPACE_USE

CParseHandlerFactory::TokenParseHandlerFuncMap
	*CParseHandlerFactory::m_token_parse_handler_func_map = NULL;

// adds a new mapping of token to corresponding parse handler
void
CParseHandlerFactory::AddMapping(Edxltoken token_type,
								 ParseHandlerOpCreatorFunc *parse_handler_op_func)
{
	GPOS_ASSERT(NULL != m_token_parse_handler_func_map);
	const XMLCh *token_identifier_str = CDXLTokens::XmlstrToken(token_type);
	GPOS_ASSERT(NULL != token_identifier_str);

#ifdef GPOS_DEBUG
	BOOL success =
#endif
		m_token_parse_handler_func_map->Insert(token_identifier_str, parse_handler_op_func);

	GPOS_ASSERT(success);
}

// initialize mapping of tokens to parse handlers
void
CParseHandlerFactory::Init(IMemoryPool *memory_pool)
{
	m_token_parse_handler_func_map =
		GPOS_NEW(memory_pool) TokenParseHandlerFuncMap(memory_pool, HASH_MAP_SIZE);

	// array mapping XML Token -> Parse Handler Creator mappings to hashmap
	SParseHandlerMapping token_parse_handler_map[] = {
		{EdxltokenPlan, &CreatePlanParseHandler},
		{EdxltokenMetadata, &CreateMetadataParseHandler},
		{EdxltokenMDRequest, &CreateMDRequestParseHandler},
		{EdxltokenTraceFlags, &CreateTraceFlagsParseHandler},
		{EdxltokenOptimizerConfig, &CreateOptimizerCfgParseHandler},
		{EdxltokenRelationExternal, &CreateMDRelationExtParseHandler},
		{EdxltokenRelationCTAS, &CreateMDRelationCTASParseHandler},
		{EdxltokenEnumeratorConfig, &CreateEnumeratorCfgParseHandler},
		{EdxltokenStatisticsConfig, &CreateStatisticsCfgParseHandler},
		{EdxltokenCTEConfig, &CreateCTECfgParseHandler},
		{EdxltokenCostModelConfig, &CreateCostModelCfgParseHandler},
		{EdxltokenHint, &CreateHintParseHandler},
		{EdxltokenWindowOids, &CreateWindowOidsParseHandler},

		{EdxltokenRelation, &CreateMDRelationParseHandler},
		{EdxltokenIndex, &CreateMDIndexParseHandler},
		{EdxltokenMDType, &CreateMDTypeParseHandler},
		{EdxltokenGPDBScalarOp, &CreateMDScalarOpParseHandler},
		{EdxltokenGPDBFunc, &CreateMDFuncParseHandler},
		{EdxltokenGPDBAgg, &CreateMDAggParseHandler},
		{EdxltokenGPDBTrigger, &CreateMDTriggerParseHandler},
		{EdxltokenCheckConstraint, &CreateMDChkConstraintParseHandler},
		{EdxltokenRelationStats, &CreateRelStatsParseHandler},
		{EdxltokenColumnStats, &CreateColStatsParseHandler},
		{EdxltokenMetadataIdList, &CreateMDIdListParseHandler},
		{EdxltokenIndexInfoList, &CreateMDIndexInfoListParseHandler},
		{EdxltokenMetadataColumns, &CreateMDColsParseHandler},
		{EdxltokenMetadataColumn, &CreateMDColParseHandler},
		{EdxltokenColumnDefaultValue, &CreateColDefaultValExprParseHandler},
		{EdxltokenColumnStatsBucket, &CreateColStatsBucketParseHandler},
		{EdxltokenGPDBCast, &CreateMDCastParseHandler},
		{EdxltokenGPDBMDScCmp, &CreateMDScCmpParseHandler},
		{EdxltokenGPDBArrayCoerceCast, &CreateMDArrayCoerceCastParseHandler},

		{EdxltokenPhysical, &CreatePhysicalOpParseHandler},

		{EdxltokenPhysicalAggregate, &CreateAggParseHandler},
		{EdxltokenPhysicalTableScan, &CreateTableScanParseHandler},
		{EdxltokenPhysicalBitmapTableScan, &CreateBitmapTableScanParseHandler},
		{EdxltokenPhysicalDynamicBitmapTableScan, &CreateDynBitmapTableScanParseHandler},
		{EdxltokenPhysicalExternalScan, &CreateExternalScanParseHandler},
		{EdxltokenPhysicalHashJoin, &CreateHashJoinParseHandler},
		{EdxltokenPhysicalNLJoin, &CreateNLJoinParseHandler},
		{EdxltokenPhysicalMergeJoin, &CreateMergeJoinParseHandler},
		{EdxltokenPhysicalGatherMotion, &CreateGatherMotionParseHandler},
		{EdxltokenPhysicalBroadcastMotion, &CreateBroadcastMotionParseHandler},
		{EdxltokenPhysicalRedistributeMotion, &CreateRedistributeMotionParseHandler},
		{EdxltokenPhysicalRoutedDistributeMotion, &CreateRoutedMotionParseHandler},
		{EdxltokenPhysicalRandomMotion, &CreateRandomMotionParseHandler},
		{EdxltokenPhysicalSubqueryScan, &CreateSubqueryScanParseHandler},
		{EdxltokenPhysicalResult, &CreateResultParseHandler},
		{EdxltokenPhysicalLimit, &CreateLimitParseHandler},
		{EdxltokenPhysicalSort, &CreateSortParseHandler},
		{EdxltokenPhysicalAppend, &CreateAppendParseHandler},
		{EdxltokenPhysicalMaterialize, &CreateMaterializeParseHandler},
		{EdxltokenPhysicalDynamicTableScan, &CreateDTSParseHandler},
		{EdxltokenPhysicalDynamicIndexScan, &CreateDynamicIdxScanParseHandler},
		{EdxltokenPhysicalPartitionSelector, &CreatePartitionSelectorParseHandler},
		{EdxltokenPhysicalSequence, &CreateSequenceParseHandler},
		{EdxltokenPhysicalIndexScan, &CreateIdxScanListParseHandler},
		{EdxltokenPhysicalIndexOnlyScan, &CreateIdxOnlyScanParseHandler},
		{EdxltokenScalarBitmapIndexProbe, &CreateBitmapIdxProbeParseHandler},
		{EdxltokenIndexDescr, &CreateIdxDescrParseHandler},

		{EdxltokenPhysicalWindow, &CreateWindowParseHandler},
		{EdxltokenScalarWindowref, &CreateWindowRefParseHandler},
		{EdxltokenWindowFrame, &CreateWindowFrameParseHandler},
		{EdxltokenScalarWindowFrameLeadingEdge, &CreateFrameLeadingEdgeParseHandler},
		{EdxltokenScalarWindowFrameTrailingEdge, &CreateFrameTrailingEdgeParseHandler},
		{EdxltokenWindowKey, &CreateWindowKeyParseHandler},
		{EdxltokenWindowKeyList, &CreateWindowKeyListParseHandler},

		{EdxltokenScalarIndexCondList, &CreateIdxCondListParseHandler},

		{EdxltokenScalar, &CreateScalarOpParseHandler},

		{EdxltokenScalarFilter, &CreateFilterParseHandler},
		{EdxltokenScalarOneTimeFilter, &CreateFilterParseHandler},
		{EdxltokenScalarRecheckCondFilter, &CreateFilterParseHandler},
		{EdxltokenScalarProjList, &CreateProjListParseHandler},
		{EdxltokenScalarProjElem, &CreateProjElemParseHandler},
		{EdxltokenScalarAggref, &CreateAggRefParseHandler},
		{EdxltokenScalarSortColList, &CreateSortColListParseHandler},
		{EdxltokenScalarSortCol, &CreateSortColParseHandler},
		{EdxltokenScalarCoalesce, &CreateScCoalesceParseHandler},
		{EdxltokenScalarComp, &CreateScCmpParseHandler},
		{EdxltokenScalarDistinctComp, &CreateDistinctCmpParseHandler},
		{EdxltokenScalarIdent, &CreateScIdParseHandler},
		{EdxltokenScalarOpExpr, &CreateScOpExprParseHandler},
		{EdxltokenScalarArrayComp, &CreateScArrayCmpParseHandler},
		{EdxltokenScalarBoolOr, &CreateScBoolExprParseHandler},
		{EdxltokenScalarBoolNot, &CreateScBoolExprParseHandler},
		{EdxltokenScalarBoolAnd, &CreateScBoolExprParseHandler},
		{EdxltokenScalarMin, &CreateScMinMaxParseHandler},
		{EdxltokenScalarMax, &CreateScMinMaxParseHandler},
		{EdxltokenScalarBoolTestIsTrue, &CreateBooleanTestParseHandler},
		{EdxltokenScalarBoolTestIsNotTrue, &CreateBooleanTestParseHandler},
		{EdxltokenScalarBoolTestIsFalse, &CreateBooleanTestParseHandler},
		{EdxltokenScalarBoolTestIsNotFalse, &CreateBooleanTestParseHandler},
		{EdxltokenScalarBoolTestIsUnknown, &CreateBooleanTestParseHandler},
		{EdxltokenScalarBoolTestIsNotUnknown, &CreateBooleanTestParseHandler},
		{EdxltokenScalarSubPlan, &CreateScSubPlanParseHandler},
		{EdxltokenScalarConstValue, &CreateScConstValueParseHandler},
		{EdxltokenScalarIfStmt, &CreateIfStmtParseHandler},
		{EdxltokenScalarSwitch, &CreateScSwitchParseHandler},
		{EdxltokenScalarSwitchCase, &CreateScSwitchCaseParseHandler},
		{EdxltokenScalarCaseTest, &CreateScCaseTestParseHandler},
		{EdxltokenScalarFuncExpr, &CreateScFuncExprParseHandler},
		{EdxltokenScalarIsNull, &CreateScNullTestParseHandler},
		{EdxltokenScalarIsNotNull, &CreateScNullTestParseHandler},
		{EdxltokenScalarNullIf, &CreateScNullIfParseHandler},
		{EdxltokenScalarCast, &CreateScCastParseHandler},
		{EdxltokenScalarCoerceToDomain, CreateScCoerceToDomainParseHandler},
		{EdxltokenScalarCoerceViaIO, CreateScCoerceViaIOParseHandler},
		{EdxltokenScalarArrayCoerceExpr, CreateScArrayCoerceExprParseHandler},
		{EdxltokenScalarHashExpr, &CreateHashExprParseHandler},
		{EdxltokenScalarHashCondList, &CreateCondListParseHandler},
		{EdxltokenScalarMergeCondList, &CreateCondListParseHandler},
		{EdxltokenScalarHashExprList, &CreateHashExprListParseHandler},
		{EdxltokenScalarGroupingColList, &CreateGroupingColListParseHandler},
		{EdxltokenScalarLimitOffset, &CreateLimitOffsetParseHandler},
		{EdxltokenScalarLimitCount, &CreateLimitCountParseHandler},
		{EdxltokenScalarSubPlanTestExpr, &CreateScSubPlanTestExprParseHandler},
		{EdxltokenScalarSubPlanParamList, &CreateScSubPlanParamListParseHandler},
		{EdxltokenScalarSubPlanParam, &CreateScSubPlanParamParseHandler},
		{EdxltokenScalarOpList, &CreateScOpListParseHandler},
		{EdxltokenScalarPartOid, &CreateScPartOidParseHandler},
		{EdxltokenScalarPartDefault, &CreateScPartDefaultParseHandler},
		{EdxltokenScalarPartBound, &CreateScPartBoundParseHandler},
		{EdxltokenScalarPartBoundInclusion, &CreateScPartBoundInclParseHandler},
		{EdxltokenScalarPartBoundOpen, &CreateScPartBoundOpenParseHandler},
		{EdxltokenScalarPartListValues, &CreateScPartListValuesParseHandler},
		{EdxltokenScalarPartListNullTest, &CreateScPartListNullTestParseHandler},

		{EdxltokenScalarSubquery, &CreateScSubqueryParseHandler},
		{EdxltokenScalarBitmapAnd, &CreateScBitmapBoolOpParseHandler},
		{EdxltokenScalarBitmapOr, &CreateScBitmapBoolOpParseHandler},

		{EdxltokenScalarArray, &CreateScArrayParseHandler},
		{EdxltokenScalarArrayRef, &CreateScArrayRefParseHandler},
		{EdxltokenScalarArrayRefIndexList, &CreateScArrayRefIdxListParseHandler},

		{EdxltokenScalarAssertConstraintList, &CreateScAssertConstraintListParseHandler},

		{EdxltokenScalarDMLAction, &CreateScDMLActionParseHandler},
		{EdxltokenDirectDispatchInfo, &CreateDirectDispatchParseHandler},

		{EdxltokenQueryOutput, &CreateLogicalQueryOpParseHandler},

		{EdxltokenCost, &CreateCostParseHandler},
		{EdxltokenTableDescr, &CreateTableDescParseHandler},
		{EdxltokenColumns, &CreateColDescParseHandler},
		{EdxltokenProperties, &CreatePropertiesParseHandler},
		{EdxltokenPhysicalTVF, &CreatePhysicalTVFParseHandler},
		{EdxltokenLogicalTVF, &CreateLogicalTVFParseHandler},

		{EdxltokenQuery, &CreateQueryParseHandler},
		{EdxltokenLogicalGet, &CreateLogicalGetParseHandler},
		{EdxltokenLogicalExternalGet, &CreateLogicalExtGetParseHandler},
		{EdxltokenLogical, &CreateLogicalOpParseHandler},
		{EdxltokenLogicalProject, &CreateLogicalProjParseHandler},
		{EdxltokenLogicalSelect, &CreateLogicalSelectParseHandler},
		{EdxltokenLogicalJoin, &CreateLogicalJoinParseHandler},
		{EdxltokenLogicalGrpBy, &CreateLogicalGrpByParseHandler},
		{EdxltokenLogicalLimit, &CreateLogicalLimitParseHandler},
		{EdxltokenLogicalConstTable, &CreateLogicalConstTableParseHandler},
		{EdxltokenLogicalCTEProducer, &CreateLogicalCTEProdParseHandler},
		{EdxltokenLogicalCTEConsumer, &CreateLogicalCTEConsParseHandler},
		{EdxltokenLogicalCTEAnchor, &CreateLogicalCTEAnchorParseHandler},
		{EdxltokenCTEList, &CreateCTEListParseHandler},

		{EdxltokenLogicalWindow, &CreateLogicalWindowParseHandler},
		{EdxltokenWindowSpec, &CreateWindowSpecParseHandler},
		{EdxltokenWindowSpecList, &CreateWindowSpecListParseHandler},

		{EdxltokenLogicalInsert, &CreateLogicalInsertParseHandler},
		{EdxltokenLogicalDelete, &CreateLogicalDeleteParseHandler},
		{EdxltokenLogicalUpdate, &CreateLogicalUpdateParseHandler},
		{EdxltokenPhysicalDMLInsert, &CreatePhysicalDMLParseHandler},
		{EdxltokenPhysicalDMLDelete, &CreatePhysicalDMLParseHandler},
		{EdxltokenPhysicalDMLUpdate, &CreatePhysicalDMLParseHandler},
		{EdxltokenPhysicalSplit, &CreatePhysicalSplitParseHandler},
		{EdxltokenPhysicalRowTrigger, &CreatePhysicalRowTriggerParseHandler},
		{EdxltokenPhysicalAssert, &CreatePhysicalAssertParseHandler},
		{EdxltokenPhysicalCTEProducer, &CreatePhysicalCTEProdParseHandler},
		{EdxltokenPhysicalCTEConsumer, &CreatePhysicalCTEConsParseHandler},
		{EdxltokenLogicalCTAS, &CreateLogicalCTASParseHandler},
		{EdxltokenPhysicalCTAS, &CreatePhysicalCTASParseHandler},
		{EdxltokenCTASOptions, &CreateCTASOptionsParseHandler},

		{EdxltokenScalarSubqueryAny, &CreateScScalarSubqueryQuantifiedParseHandler},
		{EdxltokenScalarSubqueryAll, &CreateScScalarSubqueryQuantifiedParseHandler},
		{EdxltokenScalarSubqueryExists, &CreateScScalarSubqueryExistsParseHandler},
		{EdxltokenScalarSubqueryNotExists, &CreateScScalarSubqueryExistsParseHandler},

		{EdxltokenStackTrace, &CreateStackTraceParseHandler},
		{EdxltokenLogicalUnion, &CreateLogicalSetOpParseHandler},
		{EdxltokenLogicalUnionAll, &CreateLogicalSetOpParseHandler},
		{EdxltokenLogicalIntersect, &CreateLogicalSetOpParseHandler},
		{EdxltokenLogicalIntersectAll, &CreateLogicalSetOpParseHandler},
		{EdxltokenLogicalDifference, &CreateLogicalSetOpParseHandler},
		{EdxltokenLogicalDifferenceAll, &CreateLogicalSetOpParseHandler},

		{EdxltokenStatistics, &CreateStatsParseHandler},
		{EdxltokenStatsDerivedColumn, &CreateStatsDrvdColParseHandler},
		{EdxltokenStatsDerivedRelation, &CreateStatsDrvdRelParseHandler},
		{EdxltokenStatsBucketLowerBound, &CreateStatsBucketBoundParseHandler},
		{EdxltokenStatsBucketUpperBound, &CreateStatsBucketBoundParseHandler},

		{EdxltokenSearchStrategy, &CreateSearchStrategyParseHandler},
		{EdxltokenSearchStage, &CreateSearchStageParseHandler},
		{EdxltokenXform, &CreateXformParseHandler},

		{EdxltokenCostParams, &CreateCostParamsParseHandler},
		{EdxltokenCostParam, &CreateCostParamParseHandler},

		{EdxltokenScalarExpr, &CreateScExprParseHandler},
		{EdxltokenScalarValuesList, &CreateScValuesListParseHandler},
		{EdxltokenPhysicalValuesScan, &CreateValuesScanParseHandler}

	};

	const ULONG num_of_parse_handlers = GPOS_ARRAY_SIZE(token_parse_handler_map);

	for (ULONG idx = 0; idx < num_of_parse_handlers; idx++)
	{
		SParseHandlerMapping elem = token_parse_handler_map[idx];
		AddMapping(elem.token_type, elem.parse_handler_op_func);
	}
}

// creates a parse handler instance given an xml tag
CParseHandlerBase *
CParseHandlerFactory::GetParseHandler(IMemoryPool *memory_pool,
									  const XMLCh *token_identifier_str,
									  CParseHandlerManager *parse_handler_mgr,
									  CParseHandlerBase *parse_handler_root)
{
	GPOS_ASSERT(NULL != m_token_parse_handler_func_map);

	ParseHandlerOpCreatorFunc *create_parse_handler_func =
		m_token_parse_handler_func_map->Find(token_identifier_str);

	if (create_parse_handler_func != NULL)
	{
		return (*create_parse_handler_func)(memory_pool, parse_handler_mgr, parse_handler_root);
	}

	CDXLMemoryManager dxl_memory_manager(memory_pool);

	// did not find the physical operator in the table
	CWStringDynamic *str =
		CDXLUtils::CreateDynamicStringFromXMLChArray(&dxl_memory_manager, token_identifier_str);
	;

	GPOS_RAISE(gpdxl::ExmaDXL, gpdxl::ExmiDXLUnrecognizedOperator, str->GetBuffer());

	return NULL;
}

// creates a parse handler for parsing a DXL document.
CParseHandlerDXL *
CParseHandlerFactory::GetParseHandlerDXL(IMemoryPool *memory_pool,
										 CParseHandlerManager *parse_handler_mgr)
{
	return GPOS_NEW(memory_pool) CParseHandlerDXL(memory_pool, parse_handler_mgr);
}

// creates a parse handler for parsing a Plan
CParseHandlerBase *
CParseHandlerFactory::CreatePlanParseHandler(IMemoryPool *memory_pool,
											 CParseHandlerManager *parse_handler_mgr,
											 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerPlan(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMetadataParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMetadata(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a metadata request
CParseHandlerBase *
CParseHandlerFactory::CreateMDRequestParseHandler(IMemoryPool *memory_pool,
												  CParseHandlerManager *parse_handler_mgr,
												  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDRequest(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing trace flags
CParseHandlerBase *
CParseHandlerFactory::CreateTraceFlagsParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerTraceFlags(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing optimizer config
CParseHandlerBase *
CParseHandlerFactory::CreateOptimizerCfgParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerOptimizerConfig(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing enumerator config
CParseHandlerBase *
CParseHandlerFactory::CreateEnumeratorCfgParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerEnumeratorConfig(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing statistics configuration
CParseHandlerBase *
CParseHandlerFactory::CreateStatisticsCfgParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerStatisticsConfig(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing CTE configuration
CParseHandlerBase *
CParseHandlerFactory::CreateCTECfgParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerCTEConfig(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing cost model configuration
CParseHandlerBase *
CParseHandlerFactory::CreateCostModelCfgParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerCostModel(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing hint configuration
CParseHandlerBase *
CParseHandlerFactory::CreateHintParseHandler(IMemoryPool *memory_pool,
											 CParseHandlerManager *parse_handler_mgr,
											 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerHint(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing window oids configuration
CParseHandlerBase *
CParseHandlerFactory::CreateWindowOidsParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerWindowOids(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing relation metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDRelationParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDRelation(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing external relation metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDRelationExtParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDRelationExternal(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing CTAS relation metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDRelationCTASParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDRelationCtas(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a MD index
CParseHandlerBase *
CParseHandlerFactory::CreateMDIndexParseHandler(IMemoryPool *memory_pool,
												CParseHandlerManager *parse_handler_mgr,
												CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDIndex(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing relation stats
CParseHandlerBase *
CParseHandlerFactory::CreateRelStatsParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerRelStats(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing column stats
CParseHandlerBase *
CParseHandlerFactory::CreateColStatsParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerColStats(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing column stats bucket
CParseHandlerBase *
CParseHandlerFactory::CreateColStatsBucketParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerColStatsBucket(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB type metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDTypeParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDType(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific operator metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDScalarOpParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDGPDBScalarOp(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific function metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDFuncParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDGPDBFunc(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific aggregate metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDAggParseHandler(IMemoryPool *memory_pool,
											  CParseHandlerManager *parse_handler_mgr,
											  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDGPDBAgg(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific trigger metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDTriggerParseHandler(IMemoryPool *memory_pool,
												  CParseHandlerManager *parse_handler_mgr,
												  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDGPDBTrigger(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific cast metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDCastParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDCast(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific scalar comparison metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDScCmpParseHandler(IMemoryPool *memory_pool,
												CParseHandlerManager *parse_handler_mgr,
												CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDScCmp(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a list of metadata identifiers
CParseHandlerBase *
CParseHandlerFactory::CreateMDIdListParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMetadataIdList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a list of column metadata info
CParseHandlerBase *
CParseHandlerFactory::CreateMDColsParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMetadataColumns(memory_pool, parse_handler_mgr, parse_handler_root);
}

CParseHandlerBase *
CParseHandlerFactory::CreateMDIndexInfoListParseHandler(IMemoryPool *memory_pool,
														CParseHandlerManager *parse_handler_mgr,
														CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDIndexInfoList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing column info
CParseHandlerBase *
CParseHandlerFactory::CreateMDColParseHandler(IMemoryPool *memory_pool,
											  CParseHandlerManager *parse_handler_mgr,
											  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMetadataColumn(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a a default m_bytearray_value for a column
CParseHandlerBase *
CParseHandlerFactory::CreateColDefaultValExprParseHandler(IMemoryPool *memory_pool,
														  CParseHandlerManager *parse_handler_mgr,
														  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerDefaultValueExpr(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a physical operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalOpParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerPhysicalOp(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar operator
CParseHandlerBase *
CParseHandlerFactory::CreateScalarOpParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarOp(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing the properties of a physical operator
CParseHandlerBase *
CParseHandlerFactory::CreatePropertiesParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerProperties(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a filter operator
CParseHandlerBase *
CParseHandlerFactory::CreateFilterParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerFilter(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a table scan
CParseHandlerBase *
CParseHandlerFactory::CreateTableScanParseHandler(IMemoryPool *memory_pool,
												  CParseHandlerManager *parse_handler_mgr,
												  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerTableScan(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a bitmap table scan
CParseHandlerBase *
CParseHandlerFactory::CreateBitmapTableScanParseHandler(IMemoryPool *memory_pool,
														CParseHandlerManager *parse_handler_mgr,
														CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerPhysicalBitmapTableScan(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a dynamic bitmap table scan
CParseHandlerBase *
CParseHandlerFactory::CreateDynBitmapTableScanParseHandler(IMemoryPool *memory_pool,
														   CParseHandlerManager *parse_handler_mgr,
														   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool) CParseHandlerPhysicalDynamicBitmapTableScan(
		memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an external scan
CParseHandlerBase *
CParseHandlerFactory::CreateExternalScanParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerExternalScan(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a subquery scan
CParseHandlerBase *
CParseHandlerFactory::CreateSubqueryScanParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerSubqueryScan(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a result node
CParseHandlerBase *
CParseHandlerFactory::CreateResultParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerResult(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a hash join operator
CParseHandlerBase *
CParseHandlerFactory::CreateHashJoinParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerHashJoin(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a nested loop join operator
CParseHandlerBase *
CParseHandlerFactory::CreateNLJoinParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerNLJoin(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a merge join operator
CParseHandlerBase *
CParseHandlerFactory::CreateMergeJoinParseHandler(IMemoryPool *memory_pool,
												  CParseHandlerManager *parse_handler_mgr,
												  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMergeJoin(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a sort operator
CParseHandlerBase *
CParseHandlerFactory::CreateSortParseHandler(IMemoryPool *memory_pool,
											 CParseHandlerManager *parse_handler_mgr,
											 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerSort(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an append operator
CParseHandlerBase *
CParseHandlerFactory::CreateAppendParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerAppend(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a materialize operator
CParseHandlerBase *
CParseHandlerFactory::CreateMaterializeParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMaterialize(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a dynamic table scan operator
CParseHandlerBase *
CParseHandlerFactory::CreateDTSParseHandler(IMemoryPool *memory_pool,
											CParseHandlerManager *parse_handler_mgr,
											CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerDynamicTableScan(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a dynamic index scan operator
CParseHandlerBase *
CParseHandlerFactory::CreateDynamicIdxScanParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerDynamicIndexScan(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a partition selector operator
CParseHandlerBase *
CParseHandlerFactory::CreatePartitionSelectorParseHandler(IMemoryPool *memory_pool,
														  CParseHandlerManager *parse_handler_mgr,
														  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerPartitionSelector(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a sequence operator
CParseHandlerBase *
CParseHandlerFactory::CreateSequenceParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerSequence(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Limit operator
CParseHandlerBase *
CParseHandlerFactory::CreateLimitParseHandler(IMemoryPool *memory_pool,
											  CParseHandlerManager *parse_handler_mgr,
											  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLimit(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Limit Count operator
CParseHandlerBase *
CParseHandlerFactory::CreateLimitCountParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarLimitCount(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar subquery operator
CParseHandlerBase *
CParseHandlerFactory::CreateScSubqueryParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarSubquery(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar bitmap boolean operator
CParseHandlerBase *
CParseHandlerFactory::CreateScBitmapBoolOpParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarBitmapBoolOp(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar array operator.
CParseHandlerBase *
CParseHandlerFactory::CreateScArrayParseHandler(IMemoryPool *memory_pool,
												CParseHandlerManager *parse_handler_mgr,
												CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerArray(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar arrayref operator
CParseHandlerBase *
CParseHandlerFactory::CreateScArrayRefParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarArrayRef(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an arrayref index list
CParseHandlerBase *
CParseHandlerFactory::CreateScArrayRefIdxListParseHandler(IMemoryPool *memory_pool,
														  CParseHandlerManager *parse_handler_mgr,
														  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarArrayRefIndexList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar assert predicate operator.
CParseHandlerBase *
CParseHandlerFactory::CreateScAssertConstraintListParseHandler(
	IMemoryPool *memory_pool,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarAssertConstraintList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar DML action operator.
CParseHandlerBase *
CParseHandlerFactory::CreateScDMLActionParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarDMLAction(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar operator list
CParseHandlerBase *
CParseHandlerFactory::CreateScOpListParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarOpList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar part OID
CParseHandlerBase *
CParseHandlerFactory::CreateScPartOidParseHandler(IMemoryPool *memory_pool,
												  CParseHandlerManager *parse_handler_mgr,
												  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarPartOid(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar part default
CParseHandlerBase *
CParseHandlerFactory::CreateScPartDefaultParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarPartDefault(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar part boundary
CParseHandlerBase *
CParseHandlerFactory::CreateScPartBoundParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarPartBound(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar part bound inclusion
CParseHandlerBase *
CParseHandlerFactory::CreateScPartBoundInclParseHandler(IMemoryPool *memory_pool,
														CParseHandlerManager *parse_handler_mgr,
														CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarPartBoundInclusion(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar part bound openness
CParseHandlerBase *
CParseHandlerFactory::CreateScPartBoundOpenParseHandler(IMemoryPool *memory_pool,
														CParseHandlerManager *parse_handler_mgr,
														CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarPartBoundOpen(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar part list values
CParseHandlerBase *
CParseHandlerFactory::CreateScPartListValuesParseHandler(IMemoryPool *memory_pool,
														 CParseHandlerManager *parse_handler_mgr,
														 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarPartListValues(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar part list null test
CParseHandlerBase *
CParseHandlerFactory::CreateScPartListNullTestParseHandler(IMemoryPool *memory_pool,
														   CParseHandlerManager *parse_handler_mgr,
														   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarPartListNullTest(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing direct dispatch info
CParseHandlerBase *
CParseHandlerFactory::CreateDirectDispatchParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerDirectDispatchInfo(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Limit Count operator
CParseHandlerBase *
CParseHandlerFactory::CreateLimitOffsetParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarLimitOffset(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a gather motion operator
CParseHandlerBase *
CParseHandlerFactory::CreateGatherMotionParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerGatherMotion(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a broadcast motion operator
CParseHandlerBase *
CParseHandlerFactory::CreateBroadcastMotionParseHandler(IMemoryPool *memory_pool,
														CParseHandlerManager *parse_handler_mgr,
														CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerBroadcastMotion(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a redistribute motion operator
CParseHandlerBase *
CParseHandlerFactory::CreateRedistributeMotionParseHandler(IMemoryPool *memory_pool,
														   CParseHandlerManager *parse_handler_mgr,
														   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerRedistributeMotion(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a routed motion operator
CParseHandlerBase *
CParseHandlerFactory::CreateRoutedMotionParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerRoutedMotion(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a random motion operator
CParseHandlerBase *
CParseHandlerFactory::CreateRandomMotionParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerRandomMotion(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a group by operator
CParseHandlerBase *
CParseHandlerFactory::CreateAggParseHandler(IMemoryPool *memory_pool,
											CParseHandlerManager *parse_handler_mgr,
											CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerAgg(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing aggref operator
CParseHandlerBase *
CParseHandlerFactory::CreateAggRefParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarAggref(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a grouping cols list in a group by
// operator
CParseHandlerBase *
CParseHandlerFactory::CreateGroupingColListParseHandler(IMemoryPool *memory_pool,
														CParseHandlerManager *parse_handler_mgr,
														CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerGroupingColList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar comparison operator
CParseHandlerBase *
CParseHandlerFactory::CreateScCmpParseHandler(IMemoryPool *memory_pool,
											  CParseHandlerManager *parse_handler_mgr,
											  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarComp(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a distinct comparison operator
CParseHandlerBase *
CParseHandlerFactory::CreateDistinctCmpParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerDistinctComp(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar identifier operator
CParseHandlerBase *
CParseHandlerFactory::CreateScIdParseHandler(IMemoryPool *memory_pool,
											 CParseHandlerManager *parse_handler_mgr,
											 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarIdent(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar FuncExpr
CParseHandlerBase *
CParseHandlerFactory::CreateScFuncExprParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarFuncExpr(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar OpExpr
CParseHandlerBase *
CParseHandlerFactory::CreateScOpExprParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarOpExpr(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a scalar OpExpr
CParseHandlerBase *
CParseHandlerFactory::CreateScArrayCmpParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarArrayComp(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a BoolExpr
CParseHandlerBase *
CParseHandlerFactory::CreateScBoolExprParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarBoolExpr(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a MinMax
CParseHandlerBase *
CParseHandlerFactory::CreateScMinMaxParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarMinMax(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a BooleanTest
CParseHandlerBase *
CParseHandlerFactory::CreateBooleanTestParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarBooleanTest(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a NullTest
CParseHandlerBase *
CParseHandlerFactory::CreateScNullTestParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarNullTest(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a NullIf
CParseHandlerBase *
CParseHandlerFactory::CreateScNullIfParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarNullIf(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a cast
CParseHandlerBase *
CParseHandlerFactory::CreateScCastParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarCast(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a CoerceToDomain operator
CParseHandlerBase *
CParseHandlerFactory::CreateScCoerceToDomainParseHandler(IMemoryPool *memory_pool,
														 CParseHandlerManager *parse_handler_mgr,
														 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarCoerceToDomain(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a CoerceViaIO operator
CParseHandlerBase *
CParseHandlerFactory::CreateScCoerceViaIOParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarCoerceViaIO(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an array coerce expression operator
CParseHandlerBase *
CParseHandlerFactory::CreateScArrayCoerceExprParseHandler(IMemoryPool *memory_pool,
														  CParseHandlerManager *parse_handler_mgr,
														  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarArrayCoerceExpr(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a SubPlan.
CParseHandlerBase *
CParseHandlerFactory::CreateScSubPlanParseHandler(IMemoryPool *memory_pool,
												  CParseHandlerManager *parse_handler_mgr,
												  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarSubPlan(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a SubPlan test expression
CParseHandlerBase *
CParseHandlerFactory::CreateScSubPlanTestExprParseHandler(IMemoryPool *memory_pool,
														  CParseHandlerManager *parse_handler_mgr,
														  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarSubPlanTestExpr(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a SubPlan Params DXL node
CParseHandlerBase *
CParseHandlerFactory::CreateScSubPlanParamListParseHandler(IMemoryPool *memory_pool,
														   CParseHandlerManager *parse_handler_mgr,
														   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarSubPlanParamList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a single SubPlan Param
CParseHandlerBase *
CParseHandlerFactory::CreateScSubPlanParamParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarSubPlanParam(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical TVF
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalTVFParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalTVF(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a physical TVF
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalTVFParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerPhysicalTVF(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a coalesce operator
CParseHandlerBase *
CParseHandlerFactory::CreateScCoalesceParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarCoalesce(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Switch operator
CParseHandlerBase *
CParseHandlerFactory::CreateScSwitchParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarSwitch(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a SwitchCase operator
CParseHandlerBase *
CParseHandlerFactory::CreateScSwitchCaseParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarSwitchCase(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a case test
CParseHandlerBase *
CParseHandlerFactory::CreateScCaseTestParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarCaseTest(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Const
CParseHandlerBase *
CParseHandlerFactory::CreateScConstValueParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarConstValue(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an if statement
CParseHandlerBase *
CParseHandlerFactory::CreateIfStmtParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarIfStmt(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a projection list
CParseHandlerBase *
CParseHandlerFactory::CreateProjListParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerProjList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a projection element
CParseHandlerBase *
CParseHandlerFactory::CreateProjElemParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerProjElem(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a hash expr list
CParseHandlerBase *
CParseHandlerFactory::CreateHashExprListParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerHashExprList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a hash expression in a redistribute
// motion node
CParseHandlerBase *
CParseHandlerFactory::CreateHashExprParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerHashExpr(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a condition list in a hash join or
// merge join node
CParseHandlerBase *
CParseHandlerFactory::CreateCondListParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerCondList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a sorting column list in a sort node
CParseHandlerBase *
CParseHandlerFactory::CreateSortColListParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerSortColList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a sorting column in a sort node
CParseHandlerBase *
CParseHandlerFactory::CreateSortColParseHandler(IMemoryPool *memory_pool,
												CParseHandlerManager *parse_handler_mgr,
												CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerSortCol(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing the cost estimates of a physical
// operator
CParseHandlerBase *
CParseHandlerFactory::CreateCostParseHandler(IMemoryPool *memory_pool,
											 CParseHandlerManager *parse_handler_mgr,
											 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerCost(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a table descriptor
CParseHandlerBase *
CParseHandlerFactory::CreateTableDescParseHandler(IMemoryPool *memory_pool,
												  CParseHandlerManager *parse_handler_mgr,
												  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerTableDescr(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a column descriptor
CParseHandlerBase *
CParseHandlerFactory::CreateColDescParseHandler(IMemoryPool *memory_pool,
												CParseHandlerManager *parse_handler_mgr,
												CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerColDescr(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an index scan node
CParseHandlerBase *
CParseHandlerFactory::CreateIdxScanListParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerIndexScan(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an index only scan node
CParseHandlerBase *
CParseHandlerFactory::CreateIdxOnlyScanParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerIndexOnlyScan(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a bitmap index scan node
CParseHandlerBase *
CParseHandlerFactory::CreateBitmapIdxProbeParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarBitmapIndexProbe(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an index descriptor of an
// index scan node
CParseHandlerBase *
CParseHandlerFactory::CreateIdxDescrParseHandler(IMemoryPool *memory_pool,
												 CParseHandlerManager *parse_handler_mgr,
												 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerIndexDescr(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing the list of index condition in a
// index scan node
CParseHandlerBase *
CParseHandlerFactory::CreateIdxCondListParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerIndexCondList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a query
CParseHandlerBase *
CParseHandlerFactory::CreateQueryParseHandler(IMemoryPool *memory_pool,
											  CParseHandlerManager *parse_handler_mgr,
											  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerQuery(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalOpParseHandler(IMemoryPool *memory_pool,
												  CParseHandlerManager *parse_handler_mgr,
												  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalOp(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical get operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalGetParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalGet(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical external get operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalExtGetParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalExternalGet(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical project operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalProjParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalProject(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical CTE producer operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalCTEProdParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalCTEProducer(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical CTE consumer operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalCTEConsParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalCTEConsumer(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical CTE anchor operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalCTEAnchorParseHandler(IMemoryPool *memory_pool,
														 CParseHandlerManager *parse_handler_mgr,
														 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalCTEAnchor(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a CTE list
CParseHandlerBase *
CParseHandlerFactory::CreateCTEListParseHandler(IMemoryPool *memory_pool,
												CParseHandlerManager *parse_handler_mgr,
												CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerCTEList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical set operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalSetOpParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalSetOp(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical select operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalSelectParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalSelect(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical join operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalJoinParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalJoin(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing dxl representing query output
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalQueryOpParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerQueryOutput(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical group by operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalGrpByParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalGroupBy(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical limit operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalLimitParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalLimit(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical constant table operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalConstTableParseHandler(IMemoryPool *memory_pool,
														  CParseHandlerManager *parse_handler_mgr,
														  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalConstTable(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing ALL/ANY subquery operators
CParseHandlerBase *
CParseHandlerFactory::CreateScScalarSubqueryQuantifiedParseHandler(
	IMemoryPool *memory_pool,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarSubqueryQuantified(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing an EXISTS/NOT EXISTS subquery operator
CParseHandlerBase *
CParseHandlerFactory::CreateScScalarSubqueryExistsParseHandler(
	IMemoryPool *memory_pool,
	CParseHandlerManager *parse_handler_mgr,
	CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarSubqueryExists(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing relation statistics
CParseHandlerBase *
CParseHandlerFactory::CreateStatsParseHandler(IMemoryPool *memory_pool,
											  CParseHandlerManager *parse_handler_mgr,
											  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerStatistics(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a pass-through parse handler
CParseHandlerBase *
CParseHandlerFactory::CreateStackTraceParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerStacktrace(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing relation statistics
CParseHandlerBase *
CParseHandlerFactory::CreateStatsDrvdRelParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerStatsDerivedRelation(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing derived column statistics
CParseHandlerBase *
CParseHandlerFactory::CreateStatsDrvdColParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerStatsDerivedColumn(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing bucket bound in a histogram
CParseHandlerBase *
CParseHandlerFactory::CreateStatsBucketBoundParseHandler(IMemoryPool *memory_pool,
														 CParseHandlerManager *parse_handler_mgr,
														 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerStatsBound(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a window node
CParseHandlerBase *
CParseHandlerFactory::CreateWindowParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerPhysicalWindow(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing WindowRef operator
CParseHandlerBase *
CParseHandlerFactory::CreateWindowRefParseHandler(IMemoryPool *memory_pool,
												  CParseHandlerManager *parse_handler_mgr,
												  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarWindowRef(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing window frame node
CParseHandlerBase *
CParseHandlerFactory::CreateWindowFrameParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerWindowFrame(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing window key node
CParseHandlerBase *
CParseHandlerFactory::CreateWindowKeyParseHandler(IMemoryPool *memory_pool,
												  CParseHandlerManager *parse_handler_mgr,
												  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerWindowKey(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a list of window keys
CParseHandlerBase *
CParseHandlerFactory::CreateWindowKeyListParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerWindowKeyList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing window specification node
CParseHandlerBase *
CParseHandlerFactory::CreateWindowSpecParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerWindowSpec(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a list of window specifications
CParseHandlerBase *
CParseHandlerFactory::CreateWindowSpecListParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerWindowSpecList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical window operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalWindowParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalWindow(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical insert operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalInsertParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalInsert(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical delete operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalDeleteParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalDelete(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical update operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalUpdateParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalUpdate(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a logical CTAS operator
CParseHandlerBase *
CParseHandlerFactory::CreateLogicalCTASParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerLogicalCTAS(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a physical CTAS operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalCTASParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerPhysicalCTAS(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing CTAS storage options
CParseHandlerBase *
CParseHandlerFactory::CreateCTASOptionsParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerCtasStorageOptions(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a physical CTE producer operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalCTEProdParseHandler(IMemoryPool *memory_pool,
														CParseHandlerManager *parse_handler_mgr,
														CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerPhysicalCTEProducer(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a physical CTE consumer operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalCTEConsParseHandler(IMemoryPool *memory_pool,
														CParseHandlerManager *parse_handler_mgr,
														CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerPhysicalCTEConsumer(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a physical DML operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalDMLParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerPhysicalDML(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a physical split operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalSplitParseHandler(IMemoryPool *memory_pool,
													  CParseHandlerManager *parse_handler_mgr,
													  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerPhysicalSplit(memory_pool, parse_handler_mgr, parse_handler_root);
}

//	creates a parse handler for parsing a physical row trigger operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalRowTriggerParseHandler(IMemoryPool *memory_pool,
														   CParseHandlerManager *parse_handler_mgr,
														   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerPhysicalRowTrigger(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a physical assert operator
CParseHandlerBase *
CParseHandlerFactory::CreatePhysicalAssertParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerAssert(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a trailing window frame edge parser
CParseHandlerBase *
CParseHandlerFactory::CreateFrameTrailingEdgeParseHandler(IMemoryPool *memory_pool,
														  CParseHandlerManager *parse_handler_mgr,
														  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool) CParseHandlerScalarWindowFrameEdge(
		memory_pool, parse_handler_mgr, parse_handler_root, false /*fLeading*/);
}

// creates a leading window frame edge parser
CParseHandlerBase *
CParseHandlerFactory::CreateFrameLeadingEdgeParseHandler(IMemoryPool *memory_pool,
														 CParseHandlerManager *parse_handler_mgr,
														 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool) CParseHandlerScalarWindowFrameEdge(
		memory_pool, parse_handler_mgr, parse_handler_root, true /*fLeading*/);
}

// creates a parse handler for parsing search strategy
CParseHandlerBase *
CParseHandlerFactory::CreateSearchStrategyParseHandler(IMemoryPool *memory_pool,
													   CParseHandlerManager *parse_handler_mgr,
													   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerSearchStrategy(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing search stage
CParseHandlerBase *
CParseHandlerFactory::CreateSearchStageParseHandler(IMemoryPool *memory_pool,
													CParseHandlerManager *parse_handler_mgr,
													CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerSearchStage(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing xform
CParseHandlerBase *
CParseHandlerFactory::CreateXformParseHandler(IMemoryPool *memory_pool,
											  CParseHandlerManager *parse_handler_mgr,
											  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerXform(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates cost params parse handler
CParseHandlerBase *
CParseHandlerFactory::CreateCostParamsParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerCostParams(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates cost param parse handler
CParseHandlerBase *
CParseHandlerFactory::CreateCostParamParseHandler(IMemoryPool *memory_pool,
												  CParseHandlerManager *parse_handler_mgr,
												  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerCostParam(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for top level scalar expressions
CParseHandlerBase *
CParseHandlerFactory::CreateScExprParseHandler(IMemoryPool *memory_pool,
											   CParseHandlerManager *parse_handler_mgr,
											   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarExpr(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific check constraint
CParseHandlerBase *
CParseHandlerFactory::CreateMDChkConstraintParseHandler(IMemoryPool *memory_pool,
														CParseHandlerManager *parse_handler_mgr,
														CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDGPDBCheckConstraint(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Values List operator
CParseHandlerBase *
CParseHandlerFactory::CreateScValuesListParseHandler(IMemoryPool *memory_pool,
													 CParseHandlerManager *parse_handler_mgr,
													 CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerScalarValuesList(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing a Values Scan operator
CParseHandlerBase *
CParseHandlerFactory::CreateValuesScanParseHandler(IMemoryPool *memory_pool,
												   CParseHandlerManager *parse_handler_mgr,
												   CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerValuesScan(memory_pool, parse_handler_mgr, parse_handler_root);
}

// creates a parse handler for parsing GPDB-specific array coerce cast metadata
CParseHandlerBase *
CParseHandlerFactory::CreateMDArrayCoerceCastParseHandler(IMemoryPool *memory_pool,
														  CParseHandlerManager *parse_handler_mgr,
														  CParseHandlerBase *parse_handler_root)
{
	return GPOS_NEW(memory_pool)
		CParseHandlerMDArrayCoerceCast(memory_pool, parse_handler_mgr, parse_handler_root);
}

// EOF
