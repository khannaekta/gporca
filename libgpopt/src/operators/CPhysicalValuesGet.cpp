//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2017 Pivotal Software, Inc.
//
//	@filename:
//		CPhysicalValuesGet.cpp
//
//	@doc:
//		Implementation of physical values get operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpecUniversal.h"
#include "gpopt/base/CUtils.h"
#include "gpopt/base/CCTEMap.h"
#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CPhysicalValuesGet.h"
#include "gpopt/operators/CExpressionHandle.h"


using namespace gpopt;


// Ctor
CPhysicalValuesGet::CPhysicalValuesGet
	(
	IMemoryPool *pmp,
	DrgPcoldesc *pdrgpcoldesc,
	DrgPdrgPdatum *pdrgpdrgpdatum,
	DrgPcr *pdrgpcrOutput
	)
	:
	CPhysical(pmp),
	m_pdrgpcoldesc(pdrgpcoldesc),
	m_pdrgpdrgpdatum(pdrgpdrgpdatum),
	m_pdrgpcrOutput(pdrgpcrOutput)
{
}


// Dtor
CPhysicalValuesGet::~CPhysicalValuesGet()
{
	m_pdrgpcoldesc->Release();
	m_pdrgpdrgpdatum->Release();
	m_pdrgpcrOutput->Release();
}


// Match operators
BOOL
CPhysicalValuesGet::FMatch
	(
	COperator *pop
	)
	const
{
	if (Eopid() == pop->Eopid())
	{
		CPhysicalValuesGet *popCTG = CPhysicalValuesGet::PopConvert(pop);
		return m_pdrgpcoldesc == popCTG->Pdrgpcoldesc() && 
				m_pdrgpdrgpdatum == popCTG->Pdrgpdrgpdatum() && 
				m_pdrgpcrOutput == popCTG->PdrgpcrOutput();
	}
	
	return false;
}


// Compute required columns of the n-th child;
// we only compute required columns for the relational child;
CColRefSet *
CPhysicalValuesGet::PcrsRequired
	(
	IMemoryPool *, // pmp,
	CExpressionHandle &, // exprhdl,
	CColRefSet *, // pcrsRequired,
	ULONG , // ulChildIndex,
	DrgPdp *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
{
	GPOS_ASSERT(!"CPhysicalValuesGet has no children");
	return NULL;
}


// Compute required sort order of the n-th child
COrderSpec *
CPhysicalValuesGet::PosRequired
	(
	IMemoryPool *, // pmp,
	CExpressionHandle &, // exprhdl,
	COrderSpec *, // posRequired,
	ULONG ,// ulChildIndex,
	DrgPdp *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(!"CPhysicalValuesGet has no children");
	return NULL;
}

// Compute required distribution of the n-th child
CDistributionSpec *
CPhysicalValuesGet::PdsRequired
	(
	IMemoryPool *, // pmp,
	CExpressionHandle &, // exprhdl,
	CDistributionSpec *, // pdsRequired,
	ULONG , //ulChildIndex
	DrgPdp *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(!"CPhysicalValuesGet has no children");
	return NULL;
}


// Compute required rewindability of the n-th child
CRewindabilitySpec *
CPhysicalValuesGet::PrsRequired
	(
	IMemoryPool *, // pmp,
	CExpressionHandle &, // exprhdl,
	CRewindabilitySpec *, // prsRequired,
	ULONG , // ulChildIndex,
	DrgPdp *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(!"CPhysicalValuesGet has no children");
	return NULL;
}

// Compute required CTE map of the n-th child
CCTEReq *
CPhysicalValuesGet::PcteRequired
	(
	IMemoryPool *, //pmp,
	CExpressionHandle &, //exprhdl,
	CCTEReq *, //pcter,
	ULONG , //ulChildIndex,
	DrgPdp *, //pdrgpdpCtxt,
	ULONG //ulOptReq
	)
	const
{
	GPOS_ASSERT(!"CPhysicalValuesGet has no children");
	return NULL;
}

// Check if required columns are included in output columns
BOOL
CPhysicalValuesGet::FProvidesReqdCols
	(
	CExpressionHandle &, // exprhdl,
	CColRefSet *pcrsRequired,
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(NULL != pcrsRequired);
	
	CColRefSet *pcrs = GPOS_NEW(m_pmp) CColRefSet(m_pmp);
	pcrs->Include(m_pdrgpcrOutput);

	BOOL fResult = pcrs->FSubset(pcrsRequired);
	
	pcrs->Release();
	
	return fResult;
}


// Derive sort order
COrderSpec *
CPhysicalValuesGet::PosDerive
	(
	IMemoryPool *pmp,
	CExpressionHandle & // exprhdl
	)
	const
{
	return GPOS_NEW(pmp) COrderSpec(pmp);
}


// Derive distribution
CDistributionSpec *
CPhysicalValuesGet::PdsDerive
	(
	IMemoryPool *pmp,
	CExpressionHandle & // exprhdl
	)
	const
{
	return GPOS_NEW(pmp) CDistributionSpecUniversal();
}


// Derive rewindability
CRewindabilitySpec *
CPhysicalValuesGet::PrsDerive
	(
	IMemoryPool *pmp, 
	CExpressionHandle & // exprhdl
	)
	const
{
	return GPOS_NEW(pmp) CRewindabilitySpec(CRewindabilitySpec::ErtGeneral /*ert*/);
}


// Derive cte map
CCTEMap *
CPhysicalValuesGet::PcmDerive
	(
	IMemoryPool *pmp,
	CExpressionHandle & //exprhdl
	)
	const
{
	return GPOS_NEW(pmp) CCTEMap(pmp);
}

// Return the enforcing type for order property based on this operator
CEnfdProp::EPropEnforcingType
CPhysicalValuesGet::EpetOrder
	(
	CExpressionHandle &, // exprhdl
	const CEnfdOrder *
#ifdef GPOS_DEBUG
	peo
#endif // GPOS_DEBUG
	)
	const
{
	GPOS_ASSERT(NULL != peo);
	GPOS_ASSERT(!peo->PosRequired()->FEmpty());
	
	return CEnfdProp::EpetRequired;
}


//  Return the enforcing type for distribution property based on this operator
CEnfdProp::EPropEnforcingType
CPhysicalValuesGet::EpetDistribution
	(
	CExpressionHandle &exprhdl,
	const CEnfdDistribution *ped
	)
	const
{
	GPOS_ASSERT(NULL != ped);

	CDistributionSpec *pds = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Pds();
	
	if (ped->FCompatible(pds))
	{
		return CEnfdProp::EpetUnnecessary;
	}

	return CEnfdProp::EpetRequired;
}


// Return the enforcing type for rewindability property based on this operator
CEnfdProp::EPropEnforcingType
CPhysicalValuesGet::EpetRewindability
	(
	CExpressionHandle &, // exprhdl
	const CEnfdRewindability * // per
	)
	const
{
	// rewindability is already provided
	return CEnfdProp::EpetUnnecessary;
}


// EOF

