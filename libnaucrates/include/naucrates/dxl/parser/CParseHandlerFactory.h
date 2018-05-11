//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerFactory.h
//
//	@doc:
//		Factory methods for creating SAX parse handlers
//---------------------------------------------------------------------------

#ifndef GPDXL_CParseHandlerFactory_H
#define GPDXL_CParseHandlerFactory_H

#include "gpos/base.h"
#include "gpos/common/CHashMap.h"

#include "naucrates/exception.h"
#include "naucrates/dxl/operators/CDXLPhysical.h"
#include "naucrates/dxl/parser/CParseHandlerBase.h"
#include "naucrates/dxl/xml/dxltokens.h"

namespace gpdxl
{
	using namespace gpos;
	
	XERCES_CPP_NAMESPACE_USE

	// shorthand for functions creating operator parse handlers 
	typedef CParseHandlerBase* (ParseHandlerOpCreatorFunc) (IMemoryPool *memory_pool, CParseHandlerManager *, CParseHandlerBase *);
	
	// fwd decl
	class CDXLTokens;
	
	const ULONG HASH_MAP_SIZE = 128;
	
	// function for hashing xerces strings
	inline 
	ULONG GetHashXMLStr(const XMLCh *xml_str)
	{
		return (ULONG) XMLString::hash(xml_str, HASH_MAP_SIZE);
	}
	
	// function for equality on xerces strings
	inline 
	BOOL IsXMLStrEqual(const XMLCh *xml_str1, const XMLCh *xml_str2)
	{
		return (0 == XMLString::compareString(xml_str1, xml_str2));
	}

	
	//---------------------------------------------------------------------------
	//	@class:
	//		CParseHandlerFactory
	//
	//	@doc:
	//		Factory class for creating DXL SAX parse handlers
	//
	//---------------------------------------------------------------------------
	class CParseHandlerFactory
	{
		
		typedef CHashMap<const XMLCh, ParseHandlerOpCreatorFunc, GetHashXMLStr, IsXMLStrEqual,
			CleanupNULL, CleanupNULL > TokenParseHandlerFuncMap;

		// pair of DXL token type and the corresponding parse handler
		struct SParseHandlerMapping
		{
			// type
			Edxltoken token_type;

			// translator function pointer
			ParseHandlerOpCreatorFunc *parse_handler_op_func;
		};
		
		private:
			// mappings DXL token -> ParseHandler creator
			static 
			TokenParseHandlerFuncMap *m_token_parse_handler_func_map;

			static 
			void AddMapping(Edxltoken token_type, ParseHandlerOpCreatorFunc *parse_handler_op_func);
						
			// construct a physical op parse handlers
			static
			CParseHandlerBase *CreatePhysicalOpParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a GPDB plan parse handler
			static
			CParseHandlerBase *CreatePlanParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a metadata parse handler
			static
			CParseHandlerBase *CreateMetadataParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a metadata request parse handler
			static
			CParseHandlerBase *CreateMDRequestParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *pph
				);
			
			// construct a parse handler for the optimizer configuration
			static 
			CParseHandlerBase *CreateOptimizerCfgParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a parse handler for the enumerator configuration
			static
			CParseHandlerBase *CreateEnumeratorCfgParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a parse handler for the statistics configuration
			static
			CParseHandlerBase *CreateStatisticsCfgParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a parse handler for the CTE configuration
			static
			CParseHandlerBase *CreateCTECfgParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a parse handler for the cost model configuration
			static
			CParseHandlerBase *CreateCostModelCfgParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct hint parse handler
			static
			CParseHandlerBase *CreateHintParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct window oids parse handler
			static
			CParseHandlerBase *CreateWindowOidsParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a trace flag parse handler
			static 
			CParseHandlerBase *CreateTraceFlagsParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a MD relation parse handler
			static 
			CParseHandlerBase *CreateMDRelationParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a MD external relation parse handler
			static
			CParseHandlerBase *CreateMDRelationExtParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a MD CTAS relation parse handler
			static
			CParseHandlerBase *CreateMDRelationCTASParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an MD index parse handler
			static 
			CParseHandlerBase *CreateMDIndexParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a relation stats parse handler
			static 
			CParseHandlerBase *CreateRelStatsParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a column stats parse handler
			static 
			CParseHandlerBase *CreateColStatsParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a column stats bucket parse handler
			static 
			CParseHandlerBase *CreateColStatsBucketParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an MD type parse handler
			static
			CParseHandlerBase *CreateMDTypeParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct an MD scalarop parse handler
			static
			CParseHandlerBase *CreateMDScalarOpParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct an MD function parse handler
			static
			CParseHandlerBase *CreateMDFuncParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct an MD aggregate operation parse handler
			static
			CParseHandlerBase *CreateMDAggParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct an MD trigger parse handler
			static
			CParseHandlerBase *CreateMDTriggerParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct an MD cast parse handler
			static
			CParseHandlerBase *CreateMDCastParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct an MD scalar comparison parse handler
			static
			CParseHandlerBase *CreateMDScCmpParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an MD check constraint parse handler
			static
			CParseHandlerBase *CreateMDChkConstraintParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a parse handler for a list of MD ids
			static
			CParseHandlerBase *CreateMDIdListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a metadata columns parse handler
			static
			CParseHandlerBase *CreateMDColsParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			static
			CParseHandlerBase * CreateMDIndexInfoListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a column MD parse handler
			static
			CParseHandlerBase *CreateMDColParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a column default m_bytearray_value expression parse handler
			static
			CParseHandlerBase *CreateColDefaultValExprParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a scalar operator parse handler
			static
			CParseHandlerBase *CreateScalarOpParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a properties parse handler
			static
			CParseHandlerBase *CreatePropertiesParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a filter operator parse handler
			static
			CParseHandlerBase *CreateFilterParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a table scan parse handler
			static
			CParseHandlerBase *CreateTableScanParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a bitmap table scan parse handler
			static
			CParseHandlerBase *CreateBitmapTableScanParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a dynamic bitmap table scan parse handler
			static
			CParseHandlerBase *CreateDynBitmapTableScanParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an external scan parse handler
			static
			CParseHandlerBase *CreateExternalScanParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a subquery scan parse handler
			static
			CParseHandlerBase *CreateSubqueryScanParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a result node parse handler
			static
			CParseHandlerBase *CreateResultParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a HJ parse handler
			static
			CParseHandlerBase *CreateHashJoinParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a NLJ parse handler
			static
			CParseHandlerBase *CreateNLJoinParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a merge join parse handler
			static
			CParseHandlerBase *CreateMergeJoinParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a sort parse handler
			static
			CParseHandlerBase *CreateSortParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct an append parse handler
			static
			CParseHandlerBase *CreateAppendParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a materialize parse handler
			static
			CParseHandlerBase *CreateMaterializeParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a dynamic table scan parse handler
			static
			CParseHandlerBase *CreateDTSParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a dynamic index scan parse handler
			static
			CParseHandlerBase *CreateDynamicIdxScanParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a partition selector parse handler
			static
			CParseHandlerBase *CreatePartitionSelectorParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a sequence parse handler
			static
			CParseHandlerBase *CreateSequenceParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a limit (physical) parse handler
			static
			CParseHandlerBase *CreateLimitParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a limit count parse handler
			static
			CParseHandlerBase *CreateLimitCountParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a limit offset parse handler
			static
			CParseHandlerBase *CreateLimitOffsetParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a subquery parse handler
			static
			CParseHandlerBase *CreateScSubqueryParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a subquery parse handler
			static
			CParseHandlerBase *CreateScBitmapBoolOpParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct an array parse handler
			static
			CParseHandlerBase *CreateScArrayParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an arrayref parse handler
			static
			CParseHandlerBase *CreateScArrayRefParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an arrayref index list parse handler
			static
			CParseHandlerBase *CreateScArrayRefIdxListParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an assert predicate parse handler
			static
			CParseHandlerBase *CreateScAssertConstraintListParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);


			// construct a DML action parse handler
			static
			CParseHandlerBase *CreateScDMLActionParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a scalar operator list
			static
			CParseHandlerBase *CreateScOpListParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a scalar part oid
			static
			CParseHandlerBase *CreateScPartOidParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a scalar part default
			static
			CParseHandlerBase *CreateScPartDefaultParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a scalar part bound
			static
			CParseHandlerBase *CreateScPartBoundParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a scalar part bound inclusion
			static
			CParseHandlerBase *CreateScPartBoundInclParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a scalar part bound openness
			static
			CParseHandlerBase *CreateScPartBoundOpenParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a scalar part list values
			static
			CParseHandlerBase *CreateScPartListValuesParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a scalar part list null test
			static
			CParseHandlerBase *CreateScPartListNullTestParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a direct dispatch info parse handler
			static
			CParseHandlerBase *CreateDirectDispatchParseHandler
				(
				IMemoryPool* memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a gather motion parse handler
			static
			CParseHandlerBase *CreateGatherMotionParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a broadcast motion parse handler
			static
			CParseHandlerBase *CreateBroadcastMotionParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a redistribute motion parse handler
			static
			CParseHandlerBase *CreateRedistributeMotionParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a routed motion parse handler
			static
			CParseHandlerBase *CreateRoutedMotionParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a random motion parse handler
			static
			CParseHandlerBase *CreateRandomMotionParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a physical aggregate parse handler
			static
			CParseHandlerBase *CreateAggParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an aggregate function parse handler
			static
			CParseHandlerBase *CreateAggRefParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a parse handler for a physical window node
			static
			CParseHandlerBase *CreateWindowParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an window function parse handler
			static
			CParseHandlerBase *CreateWindowRefParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an window frame parse handler
			static
			CParseHandlerBase *CreateWindowFrameParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an window key parse handler
			static
			CParseHandlerBase *CreateWindowKeyParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a parse handler to parse the list of window keys
			static
			CParseHandlerBase *CreateWindowKeyListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an window specification parse handler
			static
			CParseHandlerBase *CreateWindowSpecParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a parse handler to parse the list of window specifications
			static
			CParseHandlerBase *CreateWindowSpecListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a grouping column list parse handler
			static
			CParseHandlerBase *CreateGroupingColListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a comparison operator parse handler
			static
			CParseHandlerBase *CreateScCmpParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a distinct compare parse handler
			static
			CParseHandlerBase *CreateDistinctCmpParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a scalar identifier parse handler
			static
			CParseHandlerBase *CreateScIdParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a scalar operator parse handler
			static
			CParseHandlerBase *CreateScOpExprParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an array compare parse handler
			static
			CParseHandlerBase *CreateScArrayCmpParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a boolean expression parse handler
			static
			CParseHandlerBase *CreateScBoolExprParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a min/max parse handler
			static
			CParseHandlerBase *CreateScMinMaxParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a boolean test parse handler
			static
			CParseHandlerBase *CreateBooleanTestParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a null test parse handler
			static
			CParseHandlerBase *CreateScNullTestParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a nullif parse handler
			static
			CParseHandlerBase *CreateScNullIfParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a cast parse handler
			static
			CParseHandlerBase *CreateScCastParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a coerce parse handler
			static
			CParseHandlerBase *CreateScCoerceToDomainParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a coerceviaio parse handler
			static
			CParseHandlerBase *CreateScCoerceViaIOParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a ArrayCoerceExpr parse handler
			static
			CParseHandlerBase *CreateScArrayCoerceExprParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a sub plan parse handler
			static
			CParseHandlerBase *CreateScSubPlanParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// create a parse handler for parsing a SubPlan test expression
			static
			CParseHandlerBase *CreateScSubPlanTestExprParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a sub plan params parse handler
			static
			CParseHandlerBase *CreateScSubPlanParamListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a sub plan param parse handler
			static
			CParseHandlerBase *CreateScSubPlanParamParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a logical TVF parse handler
			static
			CParseHandlerBase *CreateLogicalTVFParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a physical TVF parse handler
			static
			CParseHandlerBase *CreatePhysicalTVFParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a coalesce parse handler
			static
			CParseHandlerBase *CreateScCoalesceParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a switch parse handler
			static
			CParseHandlerBase *CreateScSwitchParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a switch case parse handler
			static
			CParseHandlerBase *CreateScSwitchCaseParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a case test parse handler
			static
			CParseHandlerBase *CreateScCaseTestParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a constant parse handler
			static
			CParseHandlerBase *CreateScConstValueParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an if statement parse handler
			static
			CParseHandlerBase *CreateIfStmtParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a function parse handler
			static
			CParseHandlerBase *CreateScFuncExprParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a project list parse handler
			static
			CParseHandlerBase *CreateProjListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a project element parse handler
			static
			CParseHandlerBase *CreateProjElemParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a hash expression list parse handler
			static
			CParseHandlerBase *CreateHashExprListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);		
			
			// construct a hash expression parse handler
			static
			CParseHandlerBase *CreateHashExprParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a condition list parse handler
			static
			CParseHandlerBase *CreateCondListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a sort column list parse handler
			static
			CParseHandlerBase *CreateSortColListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a sort column parse handler
			static
			CParseHandlerBase *CreateSortColParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a cost parse handler
			static
			CParseHandlerBase *CreateCostParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a table descriptor parse handler
			static
			CParseHandlerBase *CreateTableDescParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a column descriptor parse handler
			static
			CParseHandlerBase *CreateColDescParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct an index scan list parse handler
			static
			CParseHandlerBase *CreateIdxScanListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an index only scan parse handler
			static
			CParseHandlerBase *CreateIdxOnlyScanParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a bitmap index scan list parse handler
			static
			CParseHandlerBase *CreateBitmapIdxProbeParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an index descriptor list parse handler
			static
			CParseHandlerBase *CreateIdxDescrParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct an index condition list parse handler
			static
			CParseHandlerBase *CreateIdxCondListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);


			// construct a query parse handler
			static
			CParseHandlerBase *CreateQueryParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a logical get parse handler
			static
			CParseHandlerBase *CreateLogicalGetParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a logical external get parse handler
			static
			CParseHandlerBase *CreateLogicalExtGetParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a logical operator parse handler
			static
			CParseHandlerBase *CreateLogicalOpParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a logical project parse handler
			static
			CParseHandlerBase *CreateLogicalProjParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a logical CTE producer parse handler
			static
			CParseHandlerBase *CreateLogicalCTEProdParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a logical CTE consumer parse handler
			static
			CParseHandlerBase *CreateLogicalCTEConsParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a logical CTE anchor parse handler
			static
			CParseHandlerBase *CreateLogicalCTEAnchorParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a CTE list
			static
			CParseHandlerBase *CreateCTEListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a logical window parse handler
			static
			CParseHandlerBase *CreateLogicalWindowParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a logical insert parse handler
			static
			CParseHandlerBase *CreateLogicalInsertParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a logical delete parse handler
			static
			CParseHandlerBase *CreateLogicalDeleteParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a logical update parse handler
			static
			CParseHandlerBase *CreateLogicalUpdateParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a logical CTAS parse handler
			static
			CParseHandlerBase *CreateLogicalCTASParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a physical CTAS parse handler
			static
			CParseHandlerBase *CreatePhysicalCTASParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a parse handler for parsing CTAS storage options
			static
			CParseHandlerBase *CreateCTASOptionsParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a physical CTE producer parse handler
			static
			CParseHandlerBase *CreatePhysicalCTEProdParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a physical CTE consumer parse handler
			static
			CParseHandlerBase *CreatePhysicalCTEConsParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a physical DML parse handler
			static
			CParseHandlerBase *CreatePhysicalDMLParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a physical split parse handler
			static
			CParseHandlerBase *CreatePhysicalSplitParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a physical row trigger parse handler
			static
			CParseHandlerBase *CreatePhysicalRowTriggerParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a physical assert parse handler
			static
			CParseHandlerBase *CreatePhysicalAssertParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a logical set operator parse handler
			static
			CParseHandlerBase *CreateLogicalSetOpParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a logical select parse handler
			static
			CParseHandlerBase *CreateLogicalSelectParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a logical join parse handler
			static
			CParseHandlerBase *CreateLogicalJoinParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a logical query output parse handler
			static
			CParseHandlerBase *CreateLogicalQueryOpParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a logical groupby parse handler
			static
			CParseHandlerBase *CreateLogicalGrpByParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);	

			// construct a logical limit parse handler
			static
			CParseHandlerBase *CreateLogicalLimitParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a logical const table parse handler
			static
			CParseHandlerBase *CreateLogicalConstTableParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a quantified subquery parse handler
			static
			CParseHandlerBase *CreateScScalarSubqueryQuantifiedParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
	
			// construct a subquery parse handler
			static
			CParseHandlerBase *CreateScScalarSubqueryExistsParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a pass-through parse handler for stack traces
			static
			CParseHandlerBase *CreateStackTraceParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a statistics parse handler
			static
			CParseHandlerBase *CreateStatsParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a derived column parse handler
			static
			CParseHandlerBase *CreateStatsDrvdColParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a derived relation stats parse handler
			static
			CParseHandlerBase *CreateStatsDrvdRelParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a bucket bound parse handler
			static
			CParseHandlerBase *CreateStatsBucketBoundParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);
			
			// construct a trailing window frame edge parser
			static
			CParseHandlerBase *CreateFrameTrailingEdgeParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a leading window frame edge parser
			static
			CParseHandlerBase *CreateFrameLeadingEdgeParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct search strategy parse handler
			static
			CParseHandlerBase *CreateSearchStrategyParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct search stage parse handler
			static
			CParseHandlerBase *CreateSearchStageParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct xform parse handler
			static
			CParseHandlerBase *CreateXformParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct cost params parse handler
			static
			CParseHandlerBase *CreateCostParamsParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct cost param parse handler
			static
			CParseHandlerBase *CreateCostParamParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a scalar expression parse handler
			static
			CParseHandlerBase *CreateScExprParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a scalar values list parse handler
			static
			CParseHandlerBase *CreateScValuesListParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a values scan parse handler
			static
			CParseHandlerBase *CreateValuesScanParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// construct a md array coerce cast parse handler
			static
			CParseHandlerBase *CreateMDArrayCoerceCastParseHandler
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

		public:
			
			// initialize mappings of tokens to parse handlers
			static 
			void Init(IMemoryPool *memory_pool);
			
			// return the parse handler creator for operator with the given name
			static 
			CParseHandlerBase *GetParseHandler
				(
				IMemoryPool *memory_pool,
				const XMLCh *xml_str,
				CParseHandlerManager *parse_handler_mgr,
				CParseHandlerBase *parse_handler_root
				);

			// factory methods for creating parse handlers
			static 
			CParseHandlerDXL *GetParseHandlerDXL
				(
				IMemoryPool *memory_pool,
				CParseHandlerManager*
				);
	};
}

#endif // !GPDXL_CParseHandlerFactory_H

// EOF
