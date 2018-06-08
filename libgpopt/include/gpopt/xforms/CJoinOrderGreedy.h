//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2018 Pivotal Software Inc.
//
//	@filename:
//		CJoinOrderGreedy.h
//
//	@doc:
//		Cardinality-based join order generation with delayed cross joins
//---------------------------------------------------------------------------
#ifndef GPOPT_CJoinOrderGreedy_H
#define GPOPT_CJoinOrderGreedy_H

#include "gpos/base.h"
#include "gpos/io/IOstream.h"
#include "gpopt/xforms/CJoinOrder.h"

namespace gpopt
{
	using namespace gpos;

	//---------------------------------------------------------------------------
	//	@class:
	//		CJoinOrderGreedy
	//
	//	@doc:
	//		Helper class for creating join orders based on cardinality of results
	//
	//---------------------------------------------------------------------------
	class CJoinOrderGreedy : public CJoinOrder
	{

		private:

			// result component
			SComponent *m_pcompResult;

			ULONG m_ulNumUsedEdges;

			// derive stats on a given component
			static
			void DeriveStats(IMemoryPool *pmp, SComponent *pcomp);

			// combine the two given components using applicable edges
			SComponent *PcompCombine(SComponent *pcompOuter, SComponent *pcompInner);

			// mark edges used by result component
			void MarkUsedEdges();

			// returns starting joins with minimal cardinality
			SComponent *GetStartingJoins();

		public:

			// ctor
			CJoinOrderGreedy
				(
				IMemoryPool *pmp,
				DrgPexpr *pdrgpexprComponents,
				DrgPexpr *pdrgpexprConjuncts
				);

			// dtor
			virtual
			~CJoinOrderGreedy();

			// main handler
			virtual
			CExpression *PexprExpand();

			// print function
			virtual
			IOstream &OsPrint(IOstream &) const;

	}; // class CJoinOrderGreedy

}

#endif // !GPOPT_CJoinOrderGreedy_H

// EOF
