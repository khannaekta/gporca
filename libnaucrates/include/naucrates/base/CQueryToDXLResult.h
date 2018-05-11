//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CQueryToDXLResult.h
//
//	@doc:
//		Class representing the result of the Query to DXL translation
//		
//---------------------------------------------------------------------------

#ifndef GPDXL_CTranslatorQueryToDXLOutput_H
#define GPDXL_CTranslatorQueryToDXLOutput_H

#include "gpos/base.h"
#include "naucrates/dxl/operators/CDXLNode.h"

namespace gpdxl
{

	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CQueryToDXLResult
	//
	//	@doc:
	//		Class representing the result of the Query to DXL translation
	//
	//---------------------------------------------------------------------------
	class CQueryToDXLResult
	{
		private:

			// DXL representing the Query
			CDXLNode *m_query_dxl;

			// array of DXL nodes that represent the query output
			DXLNodeArray *m_query_output;
			
			// CTE list
			DXLNodeArray *m_cte_producers;

		public:
			// ctor
			CQueryToDXLResult(CDXLNode *query, DXLNodeArray *query_output, DXLNodeArray *cte_producers);

			// dtor
			~CQueryToDXLResult();

			// return the DXL representation of the query
			const CDXLNode *CreateDXLNode() const;

			// return the array of output columns
			const DXLNodeArray *GetOutputColumnsDXLArray() const;
			
			// return the array of CTEs
			const DXLNodeArray *GetCTEProducerDXLArray() const;
	};
}

#endif // !GPDXL_CTranslatorQueryToDXLOutput_H

// EOF
