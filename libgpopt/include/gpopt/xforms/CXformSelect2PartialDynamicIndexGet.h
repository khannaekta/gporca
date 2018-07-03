//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CXformSelect2PartialDynamicIndexGet.h
//
//	@doc:
//		Transform select over partitioned table into a union all of dynamic 
//		index gets
//---------------------------------------------------------------------------
#ifndef GPOPT_CXformSelect2PartialDynamicIndexGet_H
#define GPOPT_CXformSelect2PartialDynamicIndexGet_H

#include "gpos/base.h"
#include "gpos/common/CDynamicPtrArray.h"

#include "gpopt/xforms/CXformExploration.h"
#include "gpopt/xforms/CXformUtils.h"

namespace gpopt
{
	using namespace gpos;
	
	//---------------------------------------------------------------------------
	//	@class:
	//		CXformSelect2PartialDynamicIndexGet
	//
	//	@doc:
	//		Transform select over a partitioned table into a union all of 
	//		dynamic index get
	//
	//---------------------------------------------------------------------------
	class CXformSelect2PartialDynamicIndexGet : public CXformExploration
	{
		private:
			// return the column reference set of included / key columns
			CColRefSet *GetColRefSet
				(
				IMemoryPool *memory_pool,
				CLogicalGet *popGet,
				const IMDIndex *pmdindex,
				BOOL fIncludedColumns
				)
				const;

			// create an index get plan when applicable
			void CreatePartialIndexGetPlan
					(
					IMemoryPool *memory_pool,
					CExpression *pexpr,
					PartDynamicIndexGetInfoArray *pdrgppartdig,
					const IMDRelation *pmdrel,
					CXformResult *pxfres
					)
					const;
			
			// private copy ctor
			CXformSelect2PartialDynamicIndexGet(const CXformSelect2PartialDynamicIndexGet &);

			// create a partial dynamic get expression with a select on top
			static
			CExpression *PexprSelectOverDynamicGet
				(
				IMemoryPool *memory_pool,
				CLogicalDynamicGet *popGet,
				CExpression *pexprScalar,
				ColRefArray *pdrgpcrDGet,
				CPartConstraint *ppartcnstr
				);
			
		public:

			// ctor
			explicit
			CXformSelect2PartialDynamicIndexGet(IMemoryPool *memory_pool);

			// dtor
			virtual
			~CXformSelect2PartialDynamicIndexGet()
			{}

			// ident accessors
			virtual
			EXformId Exfid() const
			{
				return ExfSelect2PartialDynamicIndexGet;
			}

			// xform name
			virtual
			const CHAR *SzId() const
			{
				return "CXformSelect2PartialDynamicIndexGet";
			}

			// compute xform promise for a given expression handle
			virtual
			EXformPromise Exfp(CExpressionHandle &exprhdl) const;

			// actual transform
			void Transform(CXformContext *pxfctxt, CXformResult *pxfres, CExpression *pexpr) const;


	}; // class CXformSelect2PartialDynamicIndexGet
	

}

#endif // !GPOPT_CXformSelect2PartialDynamicIndexGet_H

// EOF
