//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CDXLOperatorFactory.h
//
//	@doc:
//		Factory for creating DXL tree elements out of parsed XML attributes
//---------------------------------------------------------------------------

#ifndef GPDXL_CDXLOperatorFactory_H
#define GPDXL_CDXLOperatorFactory_H

#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/XMLStringTokenizer.hpp>

#include "gpos/base.h"
#include "naucrates/dxl/operators/dxlops.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"
#include "naucrates/dxl/xml/dxltokens.h"
#include "naucrates/md/IMDId.h"
#include "naucrates/md/CMDFunctionGPDB.h"
#include "naucrates/md/CMDRelationGPDB.h"
#include "naucrates/md/IMDIndex.h"
#include "naucrates/base/IDatum.h"
#include "gpos/common/CDouble.h"

// dynamic array of XML strings
typedef CDynamicPtrArray<XMLCh, CleanupNULL> DrgPxmlsz;

// fwd decl
namespace gpmd
{
	class CMDIdGPDB;
	class CMDIdColStats;
	class CMDIdRelStats;
	class CMDIdCast;
	class CMDIdScCmp;
}  // namespace gpmd

namespace gpdxl
{
	using namespace gpos;
	using namespace gpmd;
	using namespace gpnaucrates;

	XERCES_CPP_NAMESPACE_USE

	//fwd decl
	class CDXLMemoryManager;
	class CDXLDatum;

	// shorthand for functions for translating a DXL datum
	typedef CDXLDatum *(PfPdxldatum)(
		CDXLMemoryManager *, const Attributes &, Edxltoken, IMDId *, BOOL, BOOL);

	//---------------------------------------------------------------------------
	//	@class:
	//		CDXLOperatorFactory
	//
	//	@doc:
	//		Factory class containing static methods for creating DXL objects
	//		from parsed DXL information such as XML element's attributes
	//
	//---------------------------------------------------------------------------
	class CDXLOperatorFactory
	{
	private:
		// return the LINT m_bytearray_value of byte array
		static LINT Value(CDXLMemoryManager *memory_manager_dxl,
						  const Attributes &attrs,
						  Edxltoken target_elem,
						  BYTE *data);

		// parses a byte array representation of the datum
		static BYTE *GetByteArray(CDXLMemoryManager *memory_manager_dxl,
								  const Attributes &attrs,
								  Edxltoken target_elem,
								  ULONG *length);

	public:
		// pair of oid for datums and the factory function
		struct SDXLDatumFactoryElem
		{
			OID oid;
			PfPdxldatum *pf;
		};

		static CDXLDatum *GetDatumOid(CDXLMemoryManager *memory_manager_dxl,
									  const Attributes &attrs,
									  Edxltoken target_elem,
									  IMDId *mdid,
									  BOOL is_const_null,
									  BOOL is_const_by_val);

		static CDXLDatum *GetDatumInt2(CDXLMemoryManager *memory_manager_dxl,
									   const Attributes &attrs,
									   Edxltoken target_elem,
									   IMDId *mdid,
									   BOOL is_const_null,
									   BOOL is_const_by_val);

		static CDXLDatum *GetDatumInt4(CDXLMemoryManager *memory_manager_dxl,
									   const Attributes &attrs,
									   Edxltoken target_elem,
									   IMDId *mdid,
									   BOOL is_const_null,
									   BOOL is_const_by_val);

		static CDXLDatum *GetDatumInt8(CDXLMemoryManager *memory_manager_dxl,
									   const Attributes &attrs,
									   Edxltoken target_elem,
									   IMDId *mdid,
									   BOOL is_const_null,
									   BOOL is_const_by_val);

		static CDXLDatum *GetDatumBool(CDXLMemoryManager *memory_manager_dxl,
									   const Attributes &attrs,
									   Edxltoken target_elem,
									   IMDId *mdid,
									   BOOL is_const_null,
									   BOOL is_const_by_val);

		// parse a dxl datum of type generic
		static CDXLDatum *GetDatumGeneric(CDXLMemoryManager *memory_manager_dxl,
										  const Attributes &attrs,
										  Edxltoken target_elem,
										  IMDId *mdid,
										  BOOL is_const_null,
										  BOOL is_const_by_val);

		// parse a dxl datum of types that need double mapping
		static CDXLDatum *GetDatumStatsDoubleMappable(CDXLMemoryManager *memory_manager_dxl,
													  const Attributes &attrs,
													  Edxltoken target_elem,
													  IMDId *mdid,
													  BOOL is_const_null,
													  BOOL is_const_by_val);

		// parse a dxl datum of types that need lint mapping
		static CDXLDatum *GetDatumStatsLintMappable(CDXLMemoryManager *memory_manager_dxl,
													const Attributes &attrs,
													Edxltoken target_elem,
													IMDId *mdid,
													BOOL is_const_null,
													BOOL is_const_by_val);

		// create a table scan operator
		static CDXLPhysical *MakeDXLTblScan(CDXLMemoryManager *memory_manager_dxl,
											const Attributes &attrs);

		// create a subquery scan operator
		static CDXLPhysical *MakeDXLSubqScan(CDXLMemoryManager *memory_manager_dxl,
											 const Attributes &attrs);

		// create a result operator
		static CDXLPhysical *MakeDXLResult(CDXLMemoryManager *memory_manager_dxl);

		// create a hashjoin operator
		static CDXLPhysical *MakeDXLHashJoin(CDXLMemoryManager *memory_manager_dxl,
											 const Attributes &attrs);

		// create a nested loop join operator
		static CDXLPhysical *MakeDXLNLJoin(CDXLMemoryManager *memory_manager_dxl,
										   const Attributes &attrs);

		// create a merge join operator
		static CDXLPhysical *MakeDXLMergeJoin(CDXLMemoryManager *memory_manager_dxl,
											  const Attributes &attrs);

		// create a gather motion operator
		static CDXLPhysical *MakeDXLGatherMotion(CDXLMemoryManager *memory_manager_dxl,
												 const Attributes &attrs);

		// create a broadcast motion operator
		static CDXLPhysical *MakeDXLBroadcastMotion(CDXLMemoryManager *memory_manager_dxl,
													const Attributes &attrs);

		// create a redistribute motion operator
		static CDXLPhysical *MakeDXLRedistributeMotion(CDXLMemoryManager *memory_manager_dxl,
													   const Attributes &attrs);

		// create a routed motion operator
		static CDXLPhysical *MakeDXLRoutedMotion(CDXLMemoryManager *memory_manager_dxl,
												 const Attributes &attrs);

		// create a random motion operator
		static CDXLPhysical *MakeDXLRandomMotion(CDXLMemoryManager *memory_manager_dxl,
												 const Attributes &attrs);

		// create an append operator
		static CDXLPhysical *MakeDXLAppend(CDXLMemoryManager *memory_manager_dxl,
										   const Attributes &attrs);

		// create a limit operator
		static CDXLPhysical *MakeDXLLimit(CDXLMemoryManager *memory_manager_dxl,
										  const Attributes &attrs);

		// create an aggregation operator
		static CDXLPhysical *MakeDXLAgg(CDXLMemoryManager *memory_manager_dxl,
										const Attributes &attrs);

		// create a sort operator
		static CDXLPhysical *MakeDXLSort(CDXLMemoryManager *memory_manager_dxl,
										 const Attributes &attrs);

		// create a materialize operator
		static CDXLPhysical *MakeDXLMaterialize(CDXLMemoryManager *memory_manager_dxl,
												const Attributes &attrs);

		// create a limit count operator
		static CDXLScalar *MakeDXLLimitCount(CDXLMemoryManager *memory_manager_dxl,
											 const Attributes &attrs);

		// create a limit offset operator
		static CDXLScalar *MakeDXLLimitOffset(CDXLMemoryManager *memory_manager_dxl,
											  const Attributes &attrs);

		// create a scalar comparison operator
		static CDXLScalar *MakeDXLScalarCmp(CDXLMemoryManager *memory_manager_dxl,
											const Attributes &attrs);

		// create a distinct comparison operator
		static CDXLScalar *MakeDXLDistinctCmp(CDXLMemoryManager *memory_manager_dxl,
											  const Attributes &attrs);

		// create a scalar OpExpr
		static CDXLScalar *MakeDXLOpExpr(CDXLMemoryManager *memory_manager_dxl,
										 const Attributes &attrs);

		// create a scalar ArrayComp
		static CDXLScalar *MakeDXLArrayComp(CDXLMemoryManager *memory_manager_dxl,
											const Attributes &attrs);

		// create a BoolExpr
		static CDXLScalar *MakeDXLBoolExpr(CDXLMemoryManager *memory_manager_dxl,
										   const EdxlBoolExprType);

		// create a boolean test
		static CDXLScalar *MakeDXLBooleanTest(CDXLMemoryManager *memory_manager_dxl,
											  const EdxlBooleanTestType);

		// create a subplan operator
		static CDXLScalar *MakeDXLSubPlan(CDXLMemoryManager *memory_manager_dxl,
										  IMDId *mdid,
										  DrgPdxlcr *dxl_colref_array,
										  EdxlSubPlanType dxl_subplan_type,
										  CDXLNode *dxlnode_test_expr);

		// create a NullTest
		static CDXLScalar *MakeDXLNullTest(CDXLMemoryManager *memory_manager_dxl, const BOOL);

		// create a cast
		static CDXLScalar *MakeDXLCast(CDXLMemoryManager *memory_manager_dxl,
									   const Attributes &attrs);

		// create a coerce
		static CDXLScalar *MakeDXLCoerceToDomain(CDXLMemoryManager *memory_manager_dxl,
												 const Attributes &attrs);

		// create a CoerceViaIo
		static CDXLScalar *MakeDXLCoerceViaIO(CDXLMemoryManager *memory_manager_dxl,
											  const Attributes &attrs);

		// create a ArrayCoerceExpr
		static CDXLScalar *MakeDXLArrayCoerceExpr(CDXLMemoryManager *memory_manager_dxl,
												  const Attributes &attrs);

		// create a scalar identifier operator
		static CDXLScalar *MakeDXLScalarIdent(CDXLMemoryManager *memory_manager_dxl,
											  const Attributes &attrs);

		// create a scalar Const
		static CDXLScalar *MakeDXLConstValue(CDXLMemoryManager *memory_manager_dxl,
											 const Attributes &attrs);

		// create a CaseStmt
		static CDXLScalar *MakeDXLIfStmt(CDXLMemoryManager *memory_manager_dxl,
										 const Attributes &attrs);

		// create a FuncExpr
		static CDXLScalar *MakeDXLFuncExpr(CDXLMemoryManager *memory_manager_dxl,
										   const Attributes &attrs);

		// create a AggRef
		static CDXLScalar *MakeDXLAggFunc(CDXLMemoryManager *memory_manager_dxl,
										  const Attributes &attrs);

		// create a scalar window function (WindowRef)
		static CDXLScalar *MakeWindowRef(CDXLMemoryManager *memory_manager_dxl,
										 const Attributes &attrs);

		// create an array
		static CDXLScalar *MakeDXLArray(CDXLMemoryManager *memory_manager_dxl,
										const Attributes &attr);

		// create a proj elem
		static CDXLScalar *MakeDXLProjElem(CDXLMemoryManager *memory_manager_dxl,
										   const Attributes &attrs);

		// create a hash expr
		static CDXLScalar *MakeDXLHashExpr(CDXLMemoryManager *memory_manager_dxl,
										   const Attributes &attrs);

		// create a sort col
		static CDXLScalar *MakeDXLSortCol(CDXLMemoryManager *memory_manager_dxl,
										  const Attributes &attrs);

		// create an object representing cost estimates of a physical operator
		// from the parsed XML attributes
		static CDXLOperatorCost *MakeDXLOperatorCost(CDXLMemoryManager *memory_manager_dxl,
													 const Attributes &attrs);

		// create a table descriptor element
		static CDXLTableDescr *MakeDXLTableDescr(CDXLMemoryManager *memory_manager_dxl,
												 const Attributes &attrs);

		// create an index descriptor
		static CDXLIndexDescr *MakeDXLIndexDescr(CDXLMemoryManager *memory_manager_dxl,
												 const Attributes &attrs);

		// create a column descriptor object
		static CDXLColDescr *MakeColumnDescr(CDXLMemoryManager *memory_manager_dxl,
											 const Attributes &attrs);

		// create a column reference object
		static CDXLColRef *MakeDXLColRef(CDXLMemoryManager *memory_manager_dxl,
										 const Attributes &,
										 Edxltoken);

		// create a logical join
		static CDXLLogical *MakeLogicalJoin(CDXLMemoryManager *memory_manager_dxl,
											const Attributes &attrs);

		// parse an output segment index
		static INT ParseOutputSegId(CDXLMemoryManager *memory_manager_dxl, const Attributes &attrs);

		// parse a grouping column id
		static ULONG ParseGroupingColId(CDXLMemoryManager *memory_manager_dxl,
										const Attributes &attrs);

		// extracts the m_bytearray_value for the given attribute.
		// if there is no such attribute defined, and the given optional
		// flag is set to false then it will raise an exception
		static const XMLCh *ExtractAttrValue(const Attributes &,
											 Edxltoken target_attr,
											 Edxltoken target_elem,
											 BOOL is_optional = false);

		// extracts the boolean m_bytearray_value for the given attribute
		// will raise an exception if m_bytearray_value cannot be converted to a boolean
		static BOOL ConvertAttrValueToBool(CDXLMemoryManager *memory_manager_dxl,
										   const XMLCh *xml_val,
										   Edxltoken target_attr,
										   Edxltoken target_elem);

		// converts the XMLCh into LINT
		static LINT ConvertAttrValueToLint(CDXLMemoryManager *memory_manager_dxl,
										   const XMLCh *xml_val,
										   Edxltoken target_attr,
										   Edxltoken target_elem);

		// extracts the LINT m_bytearray_value for the given attribute
		static LINT ExtractConvertAttrValueToLint(CDXLMemoryManager *memory_manager_dxl,
												  const Attributes &attr,
												  Edxltoken target_attr,
												  Edxltoken target_elem,
												  BOOL is_optional = false,
												  LINT default_value = 0);

		// converts the XMLCh into CDouble
		static CDouble ConvertAttrValueToDouble(CDXLMemoryManager *memory_manager_dxl,
												const XMLCh *xml_val,
												Edxltoken target_attr,
												Edxltoken target_elem);

		// cxtracts the double m_bytearray_value for the given attribute
		static CDouble ExtractConvertAttrValueToDouble(CDXLMemoryManager *memory_manager_dxl,
													   const Attributes &attr,
													   Edxltoken target_attr,
													   Edxltoken target_elem);

		// converts the XMLCh into ULONG. Will raise an exception if the
		// argument cannot be converted to ULONG
		static ULONG ConvertAttrValueToUlong(CDXLMemoryManager *memory_manager_dxl,
											 const XMLCh *xml_val,
											 Edxltoken target_attr,
											 Edxltoken target_elem);

		// converts the XMLCh into ULLONG. Will raise an exception if the
		// argument cannot be converted to ULLONG
		static ULLONG ConvertAttrValueToUllong(CDXLMemoryManager *memory_manager_dxl,
											   const XMLCh *xml_val,
											   Edxltoken target_attr,
											   Edxltoken target_elem);

		// converts the XMLCh into INT. Will raise an exception if the
		// argument cannot be converted to INT
		static INT ConvertAttrValueToInt(CDXLMemoryManager *memory_manager_dxl,
										 const XMLCh *xml_val,
										 Edxltoken target_attr,
										 Edxltoken target_elem);

		// parse a INT m_bytearray_value from the m_bytearray_value for a given attribute
		// will raise an exception if the argument cannot be converted to INT
		static INT ExtractConvertAttrValueToInt(CDXLMemoryManager *memory_manager_dxl,
												const Attributes &attr,
												Edxltoken target_attr,
												Edxltoken target_elem,
												BOOL is_optional = false,
												INT default_val = 0);

		// converts the XMLCh into short int. Will raise an exception if the
		// argument cannot be converted to short int
		static SINT ConvertAttrValueToShortInt(CDXLMemoryManager *memory_manager_dxl,
											   const XMLCh *xml_val,
											   Edxltoken target_attr,
											   Edxltoken target_elem);

		// parse a short int m_bytearray_value from the m_bytearray_value for a given attribute
		// will raise an exception if the argument cannot be converted to short int
		static SINT ExtractConvertAttrValueToShortInt(CDXLMemoryManager *memory_manager_dxl,
													  const Attributes &attr,
													  Edxltoken target_attr,
													  Edxltoken target_elem,
													  BOOL is_optional = false,
													  SINT default_val = 0);

		// converts the XMLCh into char. Will raise an exception if the
		// argument cannot be converted to char
		static CHAR ConvertAttrValueToChar(CDXLMemoryManager *memory_manager_dxl,
										   const XMLCh *xml_val,
										   Edxltoken target_attr,
										   Edxltoken target_elem);

		// converts the XMLCh into oid. Will raise an exception if the
		// argument cannot be converted to OID
		static OID ConvertAttrValueToOid(CDXLMemoryManager *memory_manager_dxl,
										 const XMLCh *xml_val,
										 Edxltoken target_attr,
										 Edxltoken target_elem);

		// parse an oid m_bytearray_value from the m_bytearray_value for a given attribute
		// will raise an exception if the argument cannot be converted to OID
		static OID ExtractConvertAttrValueToOid(CDXLMemoryManager *memory_manager_dxl,
												const Attributes &attr,
												Edxltoken target_attr,
												Edxltoken target_elem,
												BOOL is_optional = false,
												OID OidDefaultValue = 0);

		// parse a bool m_bytearray_value from the m_bytearray_value for a given attribute
		static BOOL ExtractConvertAttrValueToBool(CDXLMemoryManager *memory_manager_dxl,
												  const Attributes &attr,
												  Edxltoken target_attr,
												  Edxltoken target_elem,
												  BOOL is_optional = false,
												  BOOL default_value = false);

		// parse a string m_bytearray_value from the m_bytearray_value for a given attribute
		static CHAR *ExtractConvertAttrValueToSz(CDXLMemoryManager *memory_manager_dxl,
												 const Attributes &attr,
												 Edxltoken target_attr,
												 Edxltoken target_elem,
												 BOOL is_optional = false,
												 CHAR *default_value = NULL);

		// parse a string m_bytearray_value from the m_bytearray_value for a given attribute
		static CHAR *ConvertAttrValueToSz(CDXLMemoryManager *memory_manager_dxl,
										  const XMLCh *xml_val,
										  Edxltoken target_attr,
										  Edxltoken target_elem);

		// parse a string m_bytearray_value from the m_bytearray_value for a given attribute
		static CWStringDynamic *ExtractConvertAttrValueToStr(CDXLMemoryManager *memory_manager_dxl,
															 const Attributes &attr,
															 Edxltoken target_attr,
															 Edxltoken target_elem);

		// parse a ULONG m_bytearray_value from the m_bytearray_value for a given attribute
		// will raise an exception if the argument cannot be converted to ULONG
		static ULONG ExtractConvertAttrValueToUlong(CDXLMemoryManager *memory_manager_dxl,
													const Attributes &attr,
													Edxltoken target_attr,
													Edxltoken target_elem,
													BOOL is_optional = false,
													ULONG default_value = 0);

		// parse a ULLONG m_bytearray_value from the m_bytearray_value for a given attribute
		// will raise an exception if the argument cannot be converted to ULLONG
		static ULLONG ExtractConvertAttrValueToUllong(CDXLMemoryManager *memory_manager_dxl,
													  const Attributes &attr,
													  Edxltoken target_attr,
													  Edxltoken target_elem,
													  BOOL is_optional = false,
													  ULLONG default_value = 0);

		// parse an mdid object from the given attributes
		static IMDId *ExtractConvertAttrValueToMdId(CDXLMemoryManager *memory_manager_dxl,
													const Attributes &attr,
													Edxltoken target_attr,
													Edxltoken target_elem,
													BOOL is_optional = false,
													IMDId *default_val = NULL);

		// parse an mdid object from an XMLCh
		static IMDId *MakeMdIdFromStr(CDXLMemoryManager *memory_manager_dxl,
									  const XMLCh *mdid_xml,
									  Edxltoken target_attr,
									  Edxltoken target_elem);

		// parse a GPDB mdid object from an array of its components
		static CMDIdGPDB *GetGPDBMdId(CDXLMemoryManager *memory_manager_dxl,
									  DrgPxmlsz *remaining_tokens,
									  Edxltoken target_attr,
									  Edxltoken target_elem);

		// parse a GPDB CTAS mdid object from an array of its components
		static CMDIdGPDB *GetGPDBCTASMdId(CDXLMemoryManager *memory_manager_dxl,
										  DrgPxmlsz *remaining_tokens,
										  Edxltoken target_attr,
										  Edxltoken target_elem);

		// parse a column stats mdid object from an array of its components
		static CMDIdColStats *GetColStatsMdId(CDXLMemoryManager *memory_manager_dxl,
											  DrgPxmlsz *remaining_tokens,
											  Edxltoken target_attr,
											  Edxltoken target_elem);

		// parse a relation stats mdid object from an array of its components
		static CMDIdRelStats *GetRelStatsMdId(CDXLMemoryManager *memory_manager_dxl,
											  DrgPxmlsz *remaining_tokens,
											  Edxltoken target_attr,
											  Edxltoken target_elem);

		// parse a cast func mdid from the array of its components
		static CMDIdCast *GetCastFuncMdId(CDXLMemoryManager *memory_manager_dxl,
										  DrgPxmlsz *remaining_tokens,
										  Edxltoken target_attr,
										  Edxltoken target_elem);

		// parse a comparison operator mdid from the array of its components
		static CMDIdScCmp *GetScCmpMdId(CDXLMemoryManager *memory_manager_dxl,
										DrgPxmlsz *remaining_tokens,
										Edxltoken target_attr,
										Edxltoken target_elem);

		// parse a dxl datum object
		static CDXLDatum *GetDatumVal(CDXLMemoryManager *memory_manager_dxl,
									  const Attributes &attrs,
									  Edxltoken target_elem);

		// parse a comma-separated list of MDids into a dynamic array
		// will raise an exception if list is not well-formed
		static MdidPtrArray *ExtractConvertMdIdsToArray(CDXLMemoryManager *memory_manager_dxl,
														const XMLCh *mdid_list_xml,
														Edxltoken target_attr,
														Edxltoken target_elem);

		// parse a comma-separated list of unsigned long numbers into a dynamic array
		// will raise an exception if list is not well-formed
		static ULongPtrArray *ExtractConvertValuesToArray(CDXLMemoryManager *memory_manager_dxl,
														  const Attributes &attr,
														  Edxltoken target_attr,
														  Edxltoken target_elem);

		// parse a comma-separated list of integers numbers into a dynamic array
		// will raise an exception if list is not well-formed
		template <typename T,
				  void (*CleanupFn)(T *),
				  T ValueFromXmlstr(CDXLMemoryManager *, const XMLCh *, Edxltoken, Edxltoken)>
		static CDynamicPtrArray<T, CleanupFn> *ExtractIntsToArray(
			CDXLMemoryManager *memory_manager_dxl,
			const XMLCh *xmlszUl,
			Edxltoken target_attr,
			Edxltoken target_elem);

		static ULongPtrArray *
		ExtractIntsToUlongArray(CDXLMemoryManager *memory_manager_dxl,
								const XMLCh *xmlszUl,
								Edxltoken target_attr,
								Edxltoken target_elem)
		{
			return ExtractIntsToArray<ULONG, CleanupDelete, ConvertAttrValueToUlong>(
				memory_manager_dxl, xmlszUl, target_attr, target_elem);
		}

		static IntPtrArray *
		ExtractIntsToIntArray(CDXLMemoryManager *memory_manager_dxl,
							  const XMLCh *xmlszUl,
							  Edxltoken target_attr,
							  Edxltoken target_elem)
		{
			return ExtractIntsToArray<INT, CleanupDelete, ConvertAttrValueToInt>(
				memory_manager_dxl, xmlszUl, target_attr, target_elem);
		}

		// parse a comma-separated list of CHAR partition types into a dynamic array.
		// will raise an exception if list is not well-formed
		static CharPtrArray *ExtractConvertPartitionTypeToArray(
			CDXLMemoryManager *memory_manager_dxl,
			const XMLCh *xml_val,
			Edxltoken target_attr,
			Edxltoken target_elem);

		// parse a semicolon-separated list of comma-separated unsigned
		// long numbers into a dynamc array of unsigned integer arrays
		// will raise an exception if list is not well-formed
		static ULongPtrArray2D *ExtractConvertUlongTo2DArray(CDXLMemoryManager *memory_manager_dxl,
															 const XMLCh *xml_val,
															 Edxltoken target_attr,
															 Edxltoken target_elem);

		// parse a comma-separated list of segment ids into a dynamic array
		// will raise an exception if list is not well-formed
		static IntPtrArray *ExtractConvertSegmentIdsToArray(CDXLMemoryManager *memory_manager_dxl,
															const XMLCh *seg_id_list_xml,
															Edxltoken target_attr,
															Edxltoken target_elem);

		// parse a comma-separated list of strings into a dynamic array
		// will raise an exception if list is not well-formed
		static StringPtrArray *ExtractConvertStrsToArray(CDXLMemoryManager *memory_manager_dxl,
														 const XMLCh *xml_val);

		// parses the input and output segment ids from Xerces attributes and
		// stores them in the provided DXL Motion operator
		// will raise an exception if lists are not well-formed
		static void SetSegmentInfo(CDXLMemoryManager *memory_pool,
								   CDXLPhysicalMotion *motion,
								   const Attributes &attrs,
								   Edxltoken target_elem);

		static EdxlJoinType ParseJoinType(const XMLCh *xmlszJoinType,
										  const CWStringConst *join_name);

		static EdxlIndexScanDirection ParseIndexScanDirection(
			const XMLCh *direction_xml, const CWStringConst *pstrIndexScanDirection);

		// parse system id
		static CSystemId Sysid(CDXLMemoryManager *memory_manager_dxl,
							   const Attributes &attrs,
							   Edxltoken target_attr,
							   Edxltoken target_elem);

		// parse the frame boundary
		static EdxlFrameBoundary ParseDXLFrameBoundary(const Attributes &attrs,
													   Edxltoken token_type);

		// parse the frame specification
		static EdxlFrameSpec ParseDXLFrameSpec(const Attributes &attrs);

		// parse the frame exclusion strategy
		static EdxlFrameExclusionStrategy ParseFrameExclusionStrategy(const Attributes &attrs);

		// parse comparison operator type
		static IMDType::ECmpType ParseCmpType(const XMLCh *xml_str_comp_type);

		// parse the distribution policy from the given XML string
		static IMDRelation::Ereldistrpolicy ParseRelationDistPolicy(const XMLCh *xml_val);

		// parse the storage type from the given XML string
		static IMDRelation::Erelstoragetype ParseRelationStorageType(const XMLCh *xml_val);

		// parse the OnCommit action spec for CTAS
		static CDXLCtasStorageOptions::ECtasOnCommitAction ParseOnCommitActionSpec(
			const Attributes &attr);

		// parse index type
		static IMDIndex::EmdindexType ParseIndexType(const Attributes &attrs);
	};

	// parse a comma-separated list of integers numbers into a dynamic array
	// will raise an exception if list is not well-formed
	template <typename T,
			  void (*CleanupFn)(T *),
			  T ValueFromXmlstr(CDXLMemoryManager *, const XMLCh *, Edxltoken, Edxltoken)>
	CDynamicPtrArray<T, CleanupFn> *
	CDXLOperatorFactory::ExtractIntsToArray(CDXLMemoryManager *memory_manager_dxl,
											const XMLCh *mdid_list_xml,
											Edxltoken target_attr,
											Edxltoken target_elem)
	{
		// get the memory pool from the memory manager
		IMemoryPool *memory_pool = memory_manager_dxl->Pmp();

		CDynamicPtrArray<T, CleanupFn> *pdrgpt =
			GPOS_NEW(memory_pool) CDynamicPtrArray<T, CleanupFn>(memory_pool);

		XMLStringTokenizer mdid_components(mdid_list_xml, CDXLTokens::XmlstrToken(EdxltokenComma));
		const ULONG num_tokens = mdid_components.countTokens();

		for (ULONG ul = 0; ul < num_tokens; ul++)
		{
			XMLCh *xmlszNext = mdid_components.nextToken();

			GPOS_ASSERT(NULL != xmlszNext);

			T *pt = GPOS_NEW(memory_pool)
				T(ValueFromXmlstr(memory_manager_dxl, xmlszNext, target_attr, target_elem));
			pdrgpt->Append(pt);
		}

		return pdrgpt;
	}
}  // namespace gpdxl

#endif  // !GPDXL_CDXLOperatorFactory_H

// EOF
