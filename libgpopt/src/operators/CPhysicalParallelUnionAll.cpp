//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#include "gpopt/operators/CPhysicalParallelUnionAll.h"
#include "gpopt/base/CDistributionSpecRandom.h"
#include "gpopt/base/CDistributionSpecStrictHashed.h"
#include "gpopt/base/CDistributionSpecHashedNoOp.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CStrictHashedDistributions.h"

namespace gpopt
{
	CPhysicalParallelUnionAll::CPhysicalParallelUnionAll
		(
			IMemoryPool *memory_pool,
			DrgPcr *pdrgpcrOutput,
			DrgDrgPcr *pdrgpdrgpcrInput,
			ULONG ulScanIdPartialIndex
		) : CPhysicalUnionAll
		(
			memory_pool,
			pdrgpcrOutput,
			pdrgpdrgpcrInput,
			ulScanIdPartialIndex
		),
			m_pdrgpds(GPOS_NEW(memory_pool) CStrictHashedDistributions(memory_pool, pdrgpcrOutput, pdrgpdrgpcrInput))
	{
		// ParallelUnionAll creates two distribution requests to enforce distribution of its children:
		// (1) (StrictHashed, StrictHashed, ...): used to force redistribute motions that mirror the
		//     output columns
		// (2) (HashedNoOp, HashedNoOp, ...): used to force redistribution motions that mirror the
		//     underlying distribution of each relational child

		SetDistrRequests(2);
	}

	COperator::EOperatorId CPhysicalParallelUnionAll::Eopid() const
	{
		return EopPhysicalParallelUnionAll;
	}

	const CHAR *CPhysicalParallelUnionAll::SzId() const
	{
		return "CPhysicalParallelUnionAll";
	}

	CDistributionSpec *
	CPhysicalParallelUnionAll::PdsRequired
		(
			IMemoryPool *memory_pool,
			CExpressionHandle &,
			CDistributionSpec *,
			ULONG child_index,
			DrgPdp *,
			ULONG ulOptReq
		)
	const
	{
		if (0 == ulOptReq)
		{
			CDistributionSpec *pdsChild = (*m_pdrgpds)[child_index];
			pdsChild->AddRef();
			return pdsChild;
		}
		else
		{
			DrgPcr *pdrgpcrChildInputColumns = (*PdrgpdrgpcrInput())[child_index];
			DrgPexpr *pdrgpexprFakeRequestedColumns = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);

			CColRef *pcrFirstColumn = (*pdrgpcrChildInputColumns)[0];
			CExpression *pexprScalarIdent = CUtils::PexprScalarIdent(memory_pool, pcrFirstColumn);
			pdrgpexprFakeRequestedColumns->Append(pexprScalarIdent);

			return GPOS_NEW(memory_pool) CDistributionSpecHashedNoOp(pdrgpexprFakeRequestedColumns);
		}
	}

	CEnfdDistribution::EDistributionMatching
	CPhysicalParallelUnionAll::Edm
		(
		CReqdPropPlan *, // prppInput
		ULONG,  // child_index
		DrgPdp *, //pdrgpdpCtxt
		ULONG // ulOptReq
		)
	{
		return CEnfdDistribution::EdmExact;
	}

	CPhysicalParallelUnionAll::~CPhysicalParallelUnionAll()
	{
		m_pdrgpds->Release();
	}
}
