//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright 2012 EMC Corp.
//
//	@filename:
//		CXformUtils.h
//
//	@doc:
//		Utility functions for xforms
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformUtils_H
#define GPOPT_CXformUtils_H

#include "gpos/base.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CColRef.h"

#include "gpopt/operators/ops.h"
#include "gpopt/xforms/CXform.h"

namespace gpopt
{

	using namespace gpos;

	// forward declarations
	class CGroupExpression;
	class CColRefSet;
	class CExpression;
	class CLogical;
	class CLogicalDynamicGet;
	class CPartConstraint;
	class CTableDescriptor;
	
	// structure describing a candidate for a partial dynamic index scan
	struct SPartDynamicIndexGetInfo
	{
		// md index
		const IMDIndex *m_pmdindex;

		// part constraint
		CPartConstraint *m_part_constraint;
		
		// index predicate expressions
		ExpressionArray *m_pdrgpexprIndex;

		// residual expressions
		ExpressionArray *m_pdrgpexprResidual;

		// ctor
		SPartDynamicIndexGetInfo
			(
			const IMDIndex *pmdindex,
			CPartConstraint *ppartcnstr,
			ExpressionArray *pdrgpexprIndex,
			ExpressionArray *pdrgpexprResidual
			)
			:
			m_pmdindex(pmdindex),
			m_part_constraint(ppartcnstr),
			m_pdrgpexprIndex(pdrgpexprIndex),
			m_pdrgpexprResidual(pdrgpexprResidual)
		{
			GPOS_ASSERT(NULL != ppartcnstr);

		}

		// dtor
		~SPartDynamicIndexGetInfo()
		{
			m_part_constraint->Release();
			CRefCount::SafeRelease(m_pdrgpexprIndex);
			CRefCount::SafeRelease(m_pdrgpexprResidual);
		}
	};

	// arrays over partial dynamic index get candidates
	typedef CDynamicPtrArray<SPartDynamicIndexGetInfo, CleanupDelete> PartDynamicIndexGetInfoArray;
	typedef CDynamicPtrArray<PartDynamicIndexGetInfoArray, CleanupRelease> PartDynamicIndexGetInfoArrays;

	// map of expression to array of expressions
	typedef CHashMap<CExpression, ExpressionArray, CExpression::HashValue, CUtils::Equals,
		CleanupRelease<CExpression>, CleanupRelease<ExpressionArray> > HMExprDrgPexpr;

	// iterator of map of expression to array of expressions
	typedef CHashMapIter<CExpression, ExpressionArray, CExpression::HashValue, CUtils::Equals,
		CleanupRelease<CExpression>, CleanupRelease<ExpressionArray> > HMExprDrgPexprIter;

	// array of array of expressions
	typedef CDynamicPtrArray<ExpressionArray, CleanupRelease> ExpressionArrays;

	//---------------------------------------------------------------------------
	//	@class:
	//		CXformUtils
	//
	//	@doc:
	//		Utility functions for xforms
	//
	//---------------------------------------------------------------------------
	class CXformUtils
	{
		private:
			
			// enum marking the index column types
			enum
			EIndexCols
			{
				EicKey,
				EicIncluded
			};

			typedef CLogical *(*PDynamicIndexOpConstructor)
						(
						IMemoryPool *memory_pool,
						const IMDIndex *pmdindex,
						CTableDescriptor *ptabdesc,
						ULONG ulOriginOpId,
						CName *pname,
						ULONG ulPartIndex,
						ColRefArray *pdrgpcrOutput,
						ColRefArrays *pdrgpdrgpcrPart,
						ULONG ulSecondaryPartIndexId,
						CPartConstraint *ppartcnstr,
						CPartConstraint *ppartcnstrRel
						);

			typedef CLogical *(*PStaticIndexOpConstructor)
						(
						IMemoryPool *memory_pool,
						const IMDIndex *pmdindex,
						CTableDescriptor *ptabdesc,
						ULONG ulOriginOpId,
						CName *pname,
						ColRefArray *pdrgpcrOutput
						);

			typedef CExpression *(PRewrittenIndexPath)
						(
						IMemoryPool *memory_pool,
						CExpression *pexprIndexCond,
						CExpression *pexprResidualCond,
						const IMDIndex *pmdindex,
						CTableDescriptor *ptabdesc,
						COperator *popLogical
						);

			// private copy ctor
			CXformUtils(const CXformUtils &);

			// create a logical assert for the not nullable columns of the given table
			// on top of the given child expression
			static
			CExpression *PexprAssertNotNull
				(
				IMemoryPool *memory_pool,
				CExpression *pexprChild,
				CTableDescriptor *ptabdesc,
				ColRefArray *colref_array
				);

			// create a logical assert for the check constraints on the given table
			static
			CExpression *PexprAssertCheckConstraints
				(
				IMemoryPool *memory_pool,
				CExpression *pexprChild,
				CTableDescriptor *ptabdesc,
				ColRefArray *colref_array
				);

			// add a min(col) project element to the given expression list for
			// each column in the given column array
			static
			void AddMinAggs
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				CColumnFactory *col_factory,
				ColRefArray *colref_array,
				HMCrCr *phmcrcr,
				ExpressionArray *pdrgpexpr,
				ColRefArray **ppdrgpcrNew
				);

			// check if all columns support MIN aggregate
			static
			BOOL FSupportsMinAgg(ColRefArray *colref_array);

			// helper for extracting foreign key
			static
			CColRefSet *PcrsFKey
				(
				IMemoryPool *memory_pool,
				ExpressionArray *pdrgpexpr,
				CColRefSet *prcsOutput,
				CColRefSet *pcrsKey
				);

			// return the set of columns from the given array of columns which appear
			// in the index included / key columns
			static
			CColRefSet *PcrsIndexColumns
				(
				IMemoryPool *memory_pool,
				ColRefArray *colref_array,
				const IMDIndex *pmdindex,
				const IMDRelation *pmdrel,
				EIndexCols eic
				);
			
			// return the ordered array of columns from the given array of columns which appear
			// in the index included / key columns
			static
			ColRefArray *PdrgpcrIndexColumns
				(
				IMemoryPool *memory_pool,
				ColRefArray *colref_array,
				const IMDIndex *pmdindex,
				const IMDRelation *pmdrel,
				EIndexCols eic
				);

			// lookup hash join keys in scalar child group
			static
			void LookupHashJoinKeys(IMemoryPool *memory_pool, CExpression *pexpr, ExpressionArray **ppdrgpexprOuter, ExpressionArray **ppdrgpexprInner);

			// cache hash join keys on scalar child group
			static
			void CacheHashJoinKeys(CExpression *pexpr, ExpressionArray *pdrgpexprOuter, ExpressionArray *pdrgpexprInner);

			// helper to extract equality from a given expression
			static
			BOOL FExtractEquality
				(
				CExpression *pexpr,
				CExpression **ppexprEquality, // output: extracted equality expression, set to NULL if extraction failed
				CExpression **ppexprOther // output: sibling of equality expression, set to NULL if extraction failed
				);

			// check if given xform id is in the given array of xforms
			static
			BOOL FXformInArray(CXform::EXformId exfid, CXform::EXformId rgXforms[], ULONG ulXforms);

#ifdef GPOS_DEBUG
			// check whether the given join type is swapable
			static
			BOOL FSwapableJoinType(COperator::EOperatorId op_id);
#endif // GPOS_DEBUG

			// helper function for adding hash join alternative
			template <class T>
			static
			void AddHashJoinAlternative
				(
				IMemoryPool *memory_pool,
				CExpression *pexprJoin,
				ExpressionArray *pdrgpexprOuter,
				ExpressionArray *pdrgpexprInner,
				CXformResult *pxfres
				);

			// helper for transforming SubqueryAll into aggregate subquery
			static
			void SubqueryAllToAgg
				(
				IMemoryPool *memory_pool,
				CExpression *pexprSubquery,
				CExpression **ppexprNewSubquery, // output argument for new scalar subquery
				CExpression **ppexprNewScalar   // output argument for new scalar expression
				);

			// helper for transforming SubqueryAny into aggregate subquery
			static
			void SubqueryAnyToAgg
				(
				IMemoryPool *memory_pool,
				CExpression *pexprSubquery,
				CExpression **ppexprNewSubquery, // output argument for new scalar subquery
				CExpression **ppexprNewScalar   // output argument for new scalar expression
				);

			// create the Gb operator to be pushed below a join
			static
			CLogicalGbAgg *PopGbAggPushableBelowJoin
				(
				IMemoryPool *memory_pool,
				CLogicalGbAgg *popGbAggOld,
				CColRefSet *prcsOutput,
				CColRefSet *pcrsGrpCols
				);

			// check if the preconditions for pushing down Group by through join are satisfied
			static
			BOOL FCanPushGbAggBelowJoin
				(
				CColRefSet *pcrsGrpCols,
				CColRefSet *pcrsJoinOuterChildOutput,
				CColRefSet *pcrsJoinScalarUsedFromOuter,
				CColRefSet *pcrsGrpByOutput,
				CColRefSet *pcrsGrpByUsed,
				CColRefSet *pcrsFKey
				);

			// construct an expression representing a new access path using the given functors for
			// operator constructors and rewritten access path
			static
			CExpression *PexprBuildIndexPlan
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				CExpression *pexprGet,
				ULONG ulOriginOpId,
				ExpressionArray *pdrgpexprConds,
				CColRefSet *pcrsReqd,
				CColRefSet *pcrsScalarExpr,
				CColRefSet *outer_refs,
				const IMDIndex *pmdindex,
				const IMDRelation *pmdrel,
				BOOL fAllowPartialIndex,
				CPartConstraint *ppcForPartialIndexes,
				IMDIndex::EmdindexType emdindtype,
				PDynamicIndexOpConstructor pdiopc,
				PStaticIndexOpConstructor psiopc,
				PRewrittenIndexPath prip
				);

			// create a dynamic operator for a btree index plan
			static
			CLogical *
			PopDynamicBtreeIndexOpConstructor
				(
				IMemoryPool *memory_pool,
				const IMDIndex *pmdindex,
				CTableDescriptor *ptabdesc,
				ULONG ulOriginOpId,
				CName *pname,
				ULONG ulPartIndex,
				ColRefArray *pdrgpcrOutput,
				ColRefArrays *pdrgpdrgpcrPart,
				ULONG ulSecondaryPartIndexId,
				CPartConstraint *ppartcnstr,
				CPartConstraint *ppartcnstrRel
				)
			{
				return GPOS_NEW(memory_pool) CLogicalDynamicIndexGet
						(
						memory_pool,
						pmdindex,
						ptabdesc,
						ulOriginOpId,
						pname,
						ulPartIndex,
						pdrgpcrOutput,
						pdrgpdrgpcrPart,
						ulSecondaryPartIndexId,
						ppartcnstr,
						ppartcnstrRel
						);
			}

			//	create a static operator for a btree index plan
			static
			CLogical *
			PopStaticBtreeIndexOpConstructor
				(
				IMemoryPool *memory_pool,
				const IMDIndex *pmdindex,
				CTableDescriptor *ptabdesc,
				ULONG ulOriginOpId,
				CName *pname,
				ColRefArray *pdrgpcrOutput
				)
			{
				return  GPOS_NEW(memory_pool) CLogicalIndexGet
						(
						memory_pool,
						pmdindex,
						ptabdesc,
						ulOriginOpId,
						pname,
						pdrgpcrOutput
						);
			}

			//	produce an expression representing a new btree index path
			static
			CExpression *
			PexprRewrittenBtreeIndexPath
				(
				IMemoryPool *memory_pool,
				CExpression *pexprIndexCond,
				CExpression *pexprResidualCond,
				const IMDIndex *,  // pmdindex
				CTableDescriptor *,  // ptabdesc
				COperator *popLogical
				)
			{
				// create the expression containing the logical index get operator
				return CUtils::PexprSafeSelect(memory_pool, GPOS_NEW(memory_pool) CExpression(memory_pool, popLogical, pexprIndexCond), pexprResidualCond);
			}

			// create a candidate dynamic get scan to suplement the partial index scans
			static
			SPartDynamicIndexGetInfo *PpartdigDynamicGet
				(
				IMemoryPool *memory_pool,
				ExpressionArray *pdrgpexprScalar,
				CPartConstraint *ppartcnstrCovered,
				CPartConstraint *ppartcnstrRel
				);

			// returns true iff the given expression is a Not operator whose child is a
			// scalar identifier
			static
			BOOL FNotIdent(CExpression *pexpr);

			// creates a condition of the form col = m_bytearray_value, where col is the given column
			static
			CExpression *PexprEqualityOnBoolColumn
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				BOOL fNegated,
				CColRef *colref
				);

			// construct a bitmap index path expression for the given predicate
			// out of the children of the given expression
			static
			CExpression *PexprBitmapFromChildren
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				CExpression *pexprOriginalPred,
				CExpression *pexprPred,
				CTableDescriptor *ptabdesc,
				const IMDRelation *pmdrel,
				ColRefArray *pdrgpcrOutput,
				CColRefSet *outer_refs,
				CColRefSet *pcrsReqd,
				BOOL fConjunction,
				CExpression **ppexprRecheck,
				CExpression **ppexprResidual
				);

			// returns the recheck condition to use in a bitmap
			// index scan computed out of the expression 'pexprPred' that
			// uses the bitmap index
			// fBoolColumn (and fNegatedColumn) say whether the predicate is a
			// (negated) boolean scalar identifier
			// caller takes ownership of the returned expression
			static
			CExpression *PexprBitmapCondToUse
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				CExpression *pexprPred,
				BOOL fBoolColumn,
				BOOL fNegatedBoolColumn,
				CColRefSet *pcrsScalar
				);
			
			// compute the residual predicate for a bitmap table scan
			static
			void ComputeBitmapTableScanResidualPredicate
				(
				IMemoryPool *memory_pool, 
				BOOL fConjunction,
				CExpression *pexprOriginalPred,
				CExpression **ppexprResidual,
				ExpressionArray *pdrgpexprResidualNew
				);

			// compute a disjunction of two part constraints
			static
			CPartConstraint *PpartcnstrDisjunction
				(
				IMemoryPool *memory_pool,
				CPartConstraint *ppartcnstrOld,
				CPartConstraint *ppartcnstrNew
				);
			
			// construct a bitmap index path expression for the given predicate coming
			// from a condition without outer references
			static
			CExpression *PexprBitmapForSelectCondition
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				CExpression *pexprPred,
				CTableDescriptor *ptabdesc,
				const IMDRelation *pmdrel,
				ColRefArray *pdrgpcrOutput,
				CColRefSet *pcrsReqd,
				CExpression **ppexprRecheck,
				BOOL fBoolColumn,
				BOOL fNegatedBoolColumn
				);

			// construct a bitmap index path expression for the given predicate coming
			// from a condition with outer references that could potentially become
			// an index lookup
			static
			CExpression *PexprBitmapForIndexLookup
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				CExpression *pexprPred,
				CTableDescriptor *ptabdesc,
				const IMDRelation *pmdrel,
				ColRefArray *pdrgpcrOutput,
				CColRefSet *outer_refs,
				CColRefSet *pcrsReqd,
				CExpression **ppexprRecheck
				);

			// if there is already an index probe in pdrgpexprBitmap on the same
			// index as the given pexprBitmap, modify the existing index probe and
			// the corresponding recheck conditions to subsume pexprBitmap and
			// pexprRecheck respectively
			static
			BOOL FMergeWithPreviousBitmapIndexProbe
				(
				IMemoryPool *memory_pool,
				CExpression *pexprBitmap,
				CExpression *pexprRecheck,
				ExpressionArray *pdrgpexprBitmap,
				ExpressionArray *pdrgpexprRecheck
				);

			// iterate over given hash map and return array of arrays of project elements sorted by the column id of the first entries
			static
			ExpressionArrays *PdrgpdrgpexprSortedPrjElemsArray(IMemoryPool *memory_pool, HMExprDrgPexpr *phmexprdrgpexpr);

			// comparator used in sorting arrays of project elements based on the column id of the first entry
			static
			INT ICmpPrjElemsArr(const void *pvFst, const void *pvSnd);

		public:

			// helper function for implementation xforms on binary operators
			// with predicates (e.g. joins)
			template<class T>
			static
			void TransformImplementBinaryOp
				(
				CXformContext *pxfctxt,
				CXformResult *pxfres,
				CExpression *pexpr
				);

			// helper function for implementation of hash joins
			template <class T>
			static
			void ImplementHashJoin
				(
				CXformContext *pxfctxt,
				CXformResult *pxfres,
				CExpression *pexpr,
				BOOL fAntiSemiJoin = false
				);

			// helper function for implementation of nested loops joins
			template <class T>
			static
			void ImplementNLJoin
				(
				CXformContext *pxfctxt,
				CXformResult *pxfres,
				CExpression *pexpr
				);

			// helper for removing IsNotFalse join predicate for GPDB anti-semi hash join
			static
			BOOL FProcessGPDBAntiSemiHashJoin(IMemoryPool *memory_pool, CExpression *pexpr, CExpression **ppexprResult);

			// check the applicability of logical join to physical join xform
			static
			CXform::EXformPromise ExfpLogicalJoin2PhysicalJoin(CExpressionHandle &exprhdl);

			// check the applicability of semi join to cross product
			static
			CXform::EXformPromise ExfpSemiJoin2CrossProduct(CExpressionHandle &exprhdl);

			// check the applicability of N-ary join expansion
			static
			CXform::EXformPromise ExfpExpandJoinOrder(CExpressionHandle &exprhdl);

			// extract foreign key
			static
			CColRefSet *PcrsFKey
				(
				IMemoryPool *memory_pool,
				CExpression *pexprOuter,
				CExpression *pexprInner,
				CExpression *pexprScalar
				);
			
			// compute a swap of the two given joins
			static
			CExpression *PexprSwapJoins
				(
				IMemoryPool *memory_pool,
				CExpression *pexprTopJoin,
				CExpression *pexprBottomJoin
				);

			// push a Gb, optionally with a having clause below the child join
			static
			CExpression *PexprPushGbBelowJoin
				(
				IMemoryPool *memory_pool,
				CExpression *pexpr
				);

			// check if the the array of aligned input columns are of the same type
			static
			BOOL FSameDatatype(ColRefArrays *pdrgpdrgpcrInput);

			// helper function to separate subquery predicates in a top Select node
			static
			CExpression *PexprSeparateSubqueryPreds(IMemoryPool *memory_pool, CExpression *pexpr);

			// helper for creating inverse predicate for unnesting subquery ALL
			static
			CExpression *PexprInversePred(IMemoryPool *memory_pool, CExpression *pexprSubquery);

			// helper for creating a null indicator expression
			static
			CExpression *PexprNullIndicator(IMemoryPool *memory_pool, CExpression *pexpr);

			// helper for creating a logical DML on top of a project
			static
			CExpression *PexprLogicalDMLOverProject
				(
				IMemoryPool *memory_pool,
				CExpression *pexprChild,
				CLogicalDML::EDMLOperator edmlop,
				CTableDescriptor *ptabdesc,
				ColRefArray *colref_array,
				CColRef *pcrCtid,
				CColRef *pcrSegmentId
				);

			// check whether there are any BEFORE or AFTER triggers on the
			// given table that match the given DML operation
			static
			BOOL FTriggersExist
				(
				CLogicalDML::EDMLOperator edmlop,
				CTableDescriptor *ptabdesc,
				BOOL fBefore
				);

			// does the given trigger type match the given logical DML type
			static
			BOOL FTriggerApplies
				(
				CLogicalDML::EDMLOperator edmlop,
				const IMDTrigger *pmdtrigger
				);

			// construct a trigger expression on top of the given expression
			static
			CExpression *PexprRowTrigger
				(
				IMemoryPool *memory_pool,
				CExpression *pexprChild,
				CLogicalDML::EDMLOperator edmlop,
				IMDId *rel_mdid,
				BOOL fBefore,
				ColRefArray *colref_array
				);

			// construct a trigger expression on top of the given expression
			static
			CExpression *PexprRowTrigger
				(
				IMemoryPool *memory_pool,
				CExpression *pexprChild,
				CLogicalDML::EDMLOperator edmlop,
				IMDId *rel_mdid,
				BOOL fBefore,
				ColRefArray *pdrgpcrOld,
				ColRefArray *pdrgpcrNew
				);

			// construct a logical partition selector for the given table descriptor on top
			// of the given child expression. The partition selection filters use columns
			// from the given column array
			static
			CExpression *PexprLogicalPartitionSelector
				(
				IMemoryPool *memory_pool,
				CTableDescriptor *ptabdesc,
				ColRefArray *colref_array,
				CExpression *pexprChild
				);

			// return partition filter expressions given a table
			// descriptor and the given column references
			static
			ExpressionArray *PdrgpexprPartEqFilters(IMemoryPool *memory_pool, CTableDescriptor *ptabdesc, ColRefArray *pdrgpcrSource);

			// helper for creating Agg expression equivalent to quantified subquery
			static
			void QuantifiedToAgg
				(
				IMemoryPool *memory_pool,
				CExpression *pexprSubquery,
				CExpression **ppexprNewSubquery,
				CExpression **ppexprNewScalar
				);

			// helper for creating Agg expression equivalent to existential subquery
			static
			void ExistentialToAgg
				(
				IMemoryPool *memory_pool,
				CExpression *pexprSubquery,
				CExpression **ppexprNewSubquery,
				CExpression **ppexprNewScalar
				);
			
			// create a logical assert for the check constraints on the given table
			static
			CExpression *PexprAssertConstraints
				(
				IMemoryPool *memory_pool,
				CExpression *pexprChild,
				CTableDescriptor *ptabdesc,
				ColRefArray *colref_array
				);
			
			// create a logical assert for checking cardinality of update values
			static
			CExpression *PexprAssertUpdateCardinality
				(
				IMemoryPool *memory_pool,
				CExpression *pexprDMLChild,
				CExpression *pexprDML,
				CColRef *pcrCtid,
				CColRef *pcrSegmentId
				);

			// return true if stats derivation is needed for this xform
			static
			BOOL FDeriveStatsBeforeXform(CXform *pxform);

			// return true if xform is a subquery decorrelation xform
			static
			BOOL FSubqueryDecorrelation(CXform *pxform);

			// return true if xform is a subquery unnesting xform
			static
			BOOL FSubqueryUnnesting(CXform *pxform);

			// return true if xform should be applied to the next binding
			static
			BOOL FApplyToNextBinding(CXform *pxform, CExpression *pexprLastBinding);

			// return a formatted error message for the given exception
			static
			CWStringConst *PstrErrorMessage(IMemoryPool *memory_pool, ULONG major, ULONG minor, ...);
			
			// return the array of key columns from the given array of columns which appear 
			// in the index key columns
			static
			ColRefArray *PdrgpcrIndexKeys
				(
				IMemoryPool *memory_pool,
				ColRefArray *colref_array,
				const IMDIndex *pmdindex,
				const IMDRelation *pmdrel
				);
						
			// return the set of key columns from the given array of columns which appear 
			// in the index key columns
			static
			CColRefSet *PcrsIndexKeys
				(
				IMemoryPool *memory_pool,
				ColRefArray *colref_array,
				const IMDIndex *pmdindex,
				const IMDRelation *pmdrel
				);
			
			// return the set of key columns from the given array of columns which appear 
			// in the index included columns
			static
			CColRefSet *PcrsIndexIncludedCols
				(
				IMemoryPool *memory_pool,
				ColRefArray *colref_array,
				const IMDIndex *pmdindex,
				const IMDRelation *pmdrel
				);
			
			// check if an index is applicable given the required, output and scalar
			// expression columns
			static
			BOOL FIndexApplicable
				(
				IMemoryPool *memory_pool, 
				const IMDIndex *pmdindex,
				const IMDRelation *pmdrel,
				ColRefArray *pdrgpcrOutput, 
				CColRefSet *pcrsReqd, 
				CColRefSet *pcrsScalar,
				IMDIndex::EmdindexType emdindtype
				);
			
			// check whether a CTE should be inlined
			static
			BOOL FInlinableCTE(ULONG ulCTEId);

			// return the column reference of the n-th project element
			static
			CColRef *PcrProjectElement(CExpression *pexpr, ULONG ulIdxProjElement);

			// create an expression with "row_number" window function
			static
			CExpression *PexprRowNumber(IMemoryPool *memory_pool);

			// create a logical sequence project with a "row_number" window function
			static
			CExpression *PexprWindowWithRowNumber
				(
				IMemoryPool *memory_pool,
				CExpression *pexprWindowChild,
				ColRefArray *pdrgpcrInput
				);

			// generate a logical Assert expression that errors out when more than one row is generated
			static
			CExpression *PexprAssertOneRow
				(
				IMemoryPool *memory_pool,
				CExpression *pexprChild
				);

			// helper for adding CTE producer to global CTE info structure
			static
			CExpression *PexprAddCTEProducer
				(
				IMemoryPool *memory_pool,
				ULONG ulCTEId,
				ColRefArray *colref_array,
				CExpression *pexpr
				);

			// does transformation generate an Apply expression
			static
			BOOL FGenerateApply
				(
				CXform::EXformId exfid
				)
			{
				return CXform::ExfSelect2Apply == exfid ||
						CXform::ExfProject2Apply == exfid ||
						CXform::ExfGbAgg2Apply == exfid ||
						CXform::ExfSubqJoin2Apply == exfid ||
						CXform::ExfSubqNAryJoin2Apply == exfid ||
						CXform::ExfSequenceProject2Apply == exfid;
			}

			// helper for creating IndexGet/DynamicIndexGet expression
			static
			CExpression *PexprLogicalIndexGet
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				CExpression *pexprGet,
				ULONG ulOriginOpId,
				ExpressionArray *pdrgpexprConds,
				CColRefSet *pcrsReqd,
				CColRefSet *pcrsScalarExpr,
				CColRefSet *outer_refs,
				const IMDIndex *pmdindex,
				const IMDRelation *pmdrel,
				BOOL fAllowPartialIndex,
				CPartConstraint *ppcartcnstrIndex
				)
			{
				return PexprBuildIndexPlan
						(
						memory_pool,
						md_accessor,
						pexprGet,
						ulOriginOpId,
						pdrgpexprConds,
						pcrsReqd,
						pcrsScalarExpr,
						outer_refs,
						pmdindex,
						pmdrel,
						fAllowPartialIndex,
						ppcartcnstrIndex,
						IMDIndex::EmdindBtree,
						PopDynamicBtreeIndexOpConstructor,
						PopStaticBtreeIndexOpConstructor,
						PexprRewrittenBtreeIndexPath
						);
			}
			
			// helper for creating bitmap bool op expressions
			static
			CExpression *PexprScalarBitmapBoolOp
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				CExpression *pexprOriginalPred,
				ExpressionArray *pdrgpexpr,
				CTableDescriptor *ptabdesc,
				const IMDRelation *pmdrel,
				ColRefArray *pdrgpcrOutput,
				CColRefSet *outer_refs,
				CColRefSet *pcrsReqd,
				BOOL fConjunction,
				CExpression **ppexprRecheck,
				CExpression **ppexprResidual
				);
			
			// construct a bitmap bool op given the left and right bitmap access
			// path expressions
			static
			CExpression *PexprBitmapBoolOp
				(
				IMemoryPool *memory_pool, 
				IMDId *pmdidBitmapType, 
				CExpression *pexprLeft,
				CExpression *pexprRight,
				BOOL fConjunction
				);
			
			// construct a bitmap index path expression for the given predicate
			static
			CExpression *PexprBitmap
				(
				IMemoryPool *memory_pool, 
				CMDAccessor *md_accessor,
				CExpression *pexprOriginalPred,
				CExpression *pexprPred, 
				CTableDescriptor *ptabdesc,
				const IMDRelation *pmdrel,
				ColRefArray *pdrgpcrOutput,
				CColRefSet *outer_refs,
				CColRefSet *pcrsReqd,
				BOOL fConjunction,
				CExpression **ppexprRecheck,
				CExpression **ppexprResidual
				);
			
			// given an array of predicate expressions, construct a bitmap access path
			// expression for each predicate and accumulate it in the pdrgpexprBitmap array
			static
			void CreateBitmapIndexProbeOps
				(
				IMemoryPool *memory_pool, 
				CMDAccessor *md_accessor,
				CExpression *pexprOriginalPred,
				ExpressionArray *pdrgpexpr, 
				CTableDescriptor *ptabdesc,
				const IMDRelation *pmdrel,
				ColRefArray *pdrgpcrOutput,
				CColRefSet *outer_refs,
				CColRefSet *pcrsReqd,
				BOOL fConjunction,
				ExpressionArray *pdrgpexprBitmap,
				ExpressionArray *pdrgpexprRecheck,
				ExpressionArray *pdrgpexprResidual
				);

			// check if expression has any scalar node with ambiguous return type
			static
			BOOL FHasAmbiguousType(CExpression *pexpr, CMDAccessor *md_accessor);
			
			// construct a Bitmap(Dynamic)TableGet over BitmapBoolOp for the given
			// logical operator if bitmap indexes exist
			static
			CExpression *PexprBitmapTableGet
				(
				IMemoryPool *memory_pool,
				CLogical *popGet,
				ULONG ulOriginOpId,
				CTableDescriptor *ptabdesc,
				CExpression *pexprScalar,
				CColRefSet *outer_refs,
				CColRefSet *pcrsReqd
				);

			// transform a Select over a (dynamic) table get into a bitmap table scan
			// over bitmap bool op
			static
			CExpression *PexprSelect2BitmapBoolOp
				(
				IMemoryPool *memory_pool,
				CExpression *pexpr
				);

			// find a set of partial index combinations
			static
			PartDynamicIndexGetInfoArrays *PdrgpdrgppartdigCandidates
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				ExpressionArray *pdrgpexprScalar,
				ColRefArrays *pdrgpdrgpcrPartKey,
				const IMDRelation *pmdrel,
				CPartConstraint *ppartcnstrRel,
				ColRefArray *pdrgpcrOutput,
				CColRefSet *pcrsReqd,
				CColRefSet *pcrsScalarExpr,
				CColRefSet *pcrsAcceptedOuterRefs // set of columns to be considered for index apply
				);

			// compute the newly covered part constraint based on the old covered part
			// constraint and the given part constraint
			static
			CPartConstraint *PpartcnstrUpdateCovered
				(
				IMemoryPool *memory_pool,
				CMDAccessor *md_accessor,
				ExpressionArray *pdrgpexprScalar,
				CPartConstraint *ppartcnstrCovered,
				CPartConstraint *ppartcnstr,
				ColRefArray *pdrgpcrOutput,
				ExpressionArray *pdrgpexprIndex,
				ExpressionArray *pdrgpexprResidual,
				const IMDRelation *pmdrel,
				const IMDIndex *pmdindex,
				CColRefSet *pcrsAcceptedOuterRefs
				);

			// remap the expression from the old columns to the new ones
			static
			CExpression *PexprRemapColumns
				(
				IMemoryPool *memory_pool,
				CExpression *pexprScalar,
				ColRefArray *pdrgpcrA,
				ColRefArray *pdrgpcrRemappedA,
				ColRefArray *pdrgpcrB,
				ColRefArray *pdrgpcrRemappedB
				);

			// construct a partial dynamic index get
			static
			CExpression *PexprPartialDynamicIndexGet
				(
				IMemoryPool *memory_pool,
				CLogicalDynamicGet *popGet,
				ULONG ulOriginOpId,
				ExpressionArray *pdrgpexprIndex,
				ExpressionArray *pdrgpexprResidual,
				ColRefArray *pdrgpcrDIG,
				const IMDIndex *pmdindex,
				const IMDRelation *pmdrel,
				CPartConstraint *ppartcnstr,
				CColRefSet *pcrsAcceptedOuterRefs,  // set of columns to be considered for index apply
				ColRefArray *pdrgpcrOuter,
				ColRefArray *pdrgpcrNewOuter
				);

			// create a new CTE consumer for the given CTE id
			static
			CExpression *PexprCTEConsumer(IMemoryPool *memory_pool, ULONG ulCTEId, ColRefArray *pdrgpcrConsumer);

			// return a new array containing the columns from the given column array 'colref_array'
			// at the positions indicated by the given ULONG array 'pdrgpulIndexesOfRefs'
			// e.g., colref_array = {col1, col2, col3}, pdrgpulIndexesOfRefs = {2, 1}
			// the result will be {col3, col2}
			static
			ColRefArray *PdrgpcrReorderedSubsequence
				(
				IMemoryPool *memory_pool,
				ColRefArray *colref_array,
				ULongPtrArray *pdrgpulIndexesOfRefs
				);

			// check if given xform is an Agg splitting xform
			static
			BOOL FSplitAggXform(CXform::EXformId exfid);

			// check if given expression is a multi-stage Agg based on origin xform
			static
			BOOL FMultiStageAgg(CExpression *pexprAgg);

			// check if expression handle is attached to a Join with a predicate that uses columns from only one child
			static
			BOOL FJoinPredOnSingleChild(IMemoryPool *memory_pool, CExpressionHandle &exprhdl);

			// add a redundant SELECT node on top of Dynamic (Bitmap) IndexGet to be able to use index
			// predicate in partition elimination
			static
			CExpression *PexprRedundantSelectForDynamicIndex(IMemoryPool *memory_pool, CExpression *pexpr);

			// convert an Agg window function into regular Agg
			static
			CExpression *PexprWinFuncAgg2ScalarAgg(IMemoryPool *memory_pool, CExpression *pexprWinFunc);

			// create a map from the argument of each Distinct Agg to the array of project elements that define Distinct Aggs on the same argument
			static
			void MapPrjElemsWithDistinctAggs(IMemoryPool *memory_pool, CExpression *pexprPrjList, HMExprDrgPexpr **pphmexprdrgpexpr, ULONG *pulDifferentDQAs);

			// convert GbAgg with distinct aggregates to a join
			static
			CExpression *PexprGbAggOnCTEConsumer2Join(IMemoryPool *memory_pool, CExpression *pexprGbAgg);

	}; // class CXformUtils


	//---------------------------------------------------------------------------
	//	@function:
	//		CXformUtils::TransformImplementBinaryOp
	//
	//	@doc:
	//		Helper function for implementation xforms on binary operators
	//		with predicates (e.g. joins)
	//
	//---------------------------------------------------------------------------
	template<class T>
	void
	CXformUtils::TransformImplementBinaryOp
		(
		CXformContext *pxfctxt,
		CXformResult *pxfres,
		CExpression *pexpr
		)
	{
		GPOS_ASSERT(NULL != pxfctxt);
		GPOS_ASSERT(NULL != pexpr);

		IMemoryPool *memory_pool = pxfctxt->Pmp();

		// extract components
		CExpression *pexprLeft = (*pexpr)[0];
		CExpression *pexprRight = (*pexpr)[1];
		CExpression *pexprScalar = (*pexpr)[2];

		// addref all children
		pexprLeft->AddRef();
		pexprRight->AddRef();
		pexprScalar->AddRef();

		// assemble physical operator
		CExpression *pexprBinary =
			GPOS_NEW(memory_pool) CExpression
				(
				memory_pool,
				GPOS_NEW(memory_pool) T(memory_pool),
				pexprLeft,
				pexprRight,
				pexprScalar
				);

#ifdef GPOS_DEBUG
		COperator::EOperatorId op_id = pexprBinary->Pop()->Eopid();
#endif // GPOS_DEBUG
		GPOS_ASSERT
			(
			COperator::EopPhysicalInnerNLJoin == op_id ||
			COperator::EopPhysicalLeftOuterNLJoin == op_id ||
			COperator::EopPhysicalLeftSemiNLJoin == op_id ||
			COperator::EopPhysicalLeftAntiSemiNLJoin == op_id ||
			COperator::EopPhysicalLeftAntiSemiNLJoinNotIn == op_id
			);

		// add alternative to results
		pxfres->Add(pexprBinary);
	}

	//---------------------------------------------------------------------------
	//	@function:
	//		CXformUtils::AddHashJoinAlternative
	//
	//	@doc:
	//		Helper function for adding hash join alternative to given xform
	///		results
	//
	//---------------------------------------------------------------------------
	template <class T>
	void
	CXformUtils::AddHashJoinAlternative
		(
		IMemoryPool *memory_pool,
		CExpression *pexprJoin,
		ExpressionArray *pdrgpexprOuter,
		ExpressionArray *pdrgpexprInner,
		CXformResult *pxfres
		)
	{
		GPOS_ASSERT(CUtils::FLogicalJoin(pexprJoin->Pop()));
		GPOS_ASSERT(3 == pexprJoin->Arity());
		GPOS_ASSERT(NULL != pdrgpexprOuter);
		GPOS_ASSERT(NULL != pdrgpexprInner);
		GPOS_ASSERT(NULL != pxfres);

		for (ULONG ul = 0; ul < 3; ul++)
		{
			(*pexprJoin)[ul]->AddRef();
		}
		CExpression *pexprResult = GPOS_NEW(memory_pool) CExpression(memory_pool,
														GPOS_NEW(memory_pool) T(memory_pool, pdrgpexprOuter, pdrgpexprInner),
														(*pexprJoin)[0],
														(*pexprJoin)[1],
														(*pexprJoin)[2]);
		pxfres->Add(pexprResult);
	}


	//---------------------------------------------------------------------------
	//	@function:
	//		CXformUtils::ImplementHashJoin
	//
	//	@doc:
	//		Helper function for implementation of hash joins
	//
	//---------------------------------------------------------------------------
	template <class T>
	void
	CXformUtils::ImplementHashJoin
		(
		CXformContext *pxfctxt,
		CXformResult *pxfres,
		CExpression *pexpr,
		BOOL fAntiSemiJoin // is the target hash join type an anti-semi join?
		)
	{
		GPOS_ASSERT(NULL != pxfctxt);

		// if there are outer references, then we cannot build a hash join
		if (CUtils::HasOuterRefs(pexpr))
		{
			return;
		}

		IMemoryPool *memory_pool = pxfctxt->Pmp();
		ExpressionArray *pdrgpexprOuter = NULL;
		ExpressionArray *pdrgpexprInner = NULL;

		// check if we have already computed hash join keys for the scalar child
		LookupHashJoinKeys(memory_pool, pexpr, &pdrgpexprOuter, &pdrgpexprInner);
		if (NULL != pdrgpexprOuter)
		{
			GPOS_ASSERT(NULL != pdrgpexprInner);
			if (0 == pdrgpexprOuter->Size())
			{
				GPOS_ASSERT(0 == pdrgpexprInner->Size());

				// we failed before to find hash join keys for scalar child,
				// no reason to try to do the same again
				pdrgpexprOuter->Release();
				pdrgpexprInner->Release();
			}
			else
			{
				// we have computed hash join keys on scalar child before, reuse them
				AddHashJoinAlternative<T>(memory_pool, pexpr, pdrgpexprOuter, pdrgpexprInner, pxfres);
			}

			return;
		}

		// first time to compute hash join keys on scalar child

		pdrgpexprOuter = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);
		pdrgpexprInner = GPOS_NEW(memory_pool) ExpressionArray(memory_pool);

		CExpression *pexprInnerJoin = NULL;
		BOOL fHashJoinPossible = CPhysicalJoin::FHashJoinPossible(memory_pool, pexpr, pdrgpexprOuter, pdrgpexprInner, &pexprInnerJoin);

		// cache hash join keys on scalar child group
		CacheHashJoinKeys(pexprInnerJoin, pdrgpexprOuter, pdrgpexprInner);

		if (fHashJoinPossible)
		{
			AddHashJoinAlternative<T>(memory_pool, pexprInnerJoin, pdrgpexprOuter, pdrgpexprInner, pxfres);
		}
		else
		{
			// clean up
			pdrgpexprOuter->Release();
			pdrgpexprInner->Release();
		}

		pexprInnerJoin->Release();

		if (!fHashJoinPossible && fAntiSemiJoin)
		{
			CExpression *pexprProcessed = NULL;
			if (FProcessGPDBAntiSemiHashJoin(memory_pool, pexpr, &pexprProcessed))
			{
				// try again after simplifying join predicate
				ImplementHashJoin<T>(pxfctxt, pxfres, pexprProcessed, false /*fAntiSemiJoin*/);
				pexprProcessed->Release();
			}
		}
	}

	//---------------------------------------------------------------------------
	//	@function:
	//		CXformUtils::ImplementNLJoin
	//
	//	@doc:
	//		Helper function for implementation of nested loops joins
	//
	//---------------------------------------------------------------------------
	template <class T>
	void
	CXformUtils::ImplementNLJoin
		(
		CXformContext *pxfctxt,
		CXformResult *pxfres,
		CExpression *pexpr
		)
	{
		GPOS_ASSERT(NULL != pxfctxt);

		IMemoryPool *memory_pool = pxfctxt->Pmp();

		ColRefArray *pdrgpcrOuter = GPOS_NEW(memory_pool) ColRefArray(memory_pool);
		ColRefArray *pdrgpcrInner = GPOS_NEW(memory_pool) ColRefArray(memory_pool);

		TransformImplementBinaryOp<T>(pxfctxt, pxfres, pexpr);

		// clean up
		pdrgpcrOuter->Release();
		pdrgpcrInner->Release();
	}
}

#endif // !GPOPT_CXformUtils_H

// EOF
