//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalRowTrigger.cpp
//
//	@doc:
//		Implementation of Physical row-level trigger operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/COptCtxt.h"
#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalRowTrigger.h"
#include "gpopt/base/CDistributionSpecAny.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::CPhysicalRowTrigger
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalRowTrigger::CPhysicalRowTrigger
	(
	IMemoryPool *memory_pool,
	IMDId *rel_mdid,
	INT type,
	DrgPcr *pdrgpcrOld,
	DrgPcr *pdrgpcrNew
	)
	:
	CPhysical(memory_pool),
	m_rel_mdid(rel_mdid),
	m_type(type),
	m_pdrgpcrOld(pdrgpcrOld),
	m_pdrgpcrNew(pdrgpcrNew),
	m_pcrsRequiredLocal(NULL)
{
	GPOS_ASSERT(rel_mdid->IsValid());
	GPOS_ASSERT(0 != type);
	GPOS_ASSERT(NULL != pdrgpcrNew || NULL != pdrgpcrOld);
	GPOS_ASSERT_IMP(NULL != pdrgpcrNew && NULL != pdrgpcrOld,
			pdrgpcrNew->Size() == pdrgpcrOld->Size());

	m_pcrsRequiredLocal = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	if (NULL != m_pdrgpcrOld)
	{
		m_pcrsRequiredLocal->Include(m_pdrgpcrOld);
	}

	if (NULL != m_pdrgpcrNew)
	{
		m_pcrsRequiredLocal->Include(m_pdrgpcrNew);
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::~CPhysicalRowTrigger
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalRowTrigger::~CPhysicalRowTrigger()
{
	m_rel_mdid->Release();
	CRefCount::SafeRelease(m_pdrgpcrOld);
	CRefCount::SafeRelease(m_pdrgpcrNew);
	m_pcrsRequiredLocal->Release();
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::PosRequired
//
//	@doc:
//		Compute required sort columns of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalRowTrigger::PosRequired
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &, //exprhdl,
	COrderSpec *, //posRequired,
	ULONG
#ifdef GPOS_DEBUG
	child_index
#endif
	,
	DrgPdp *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(0 == child_index);

	return GPOS_NEW(memory_pool) COrderSpec(memory_pool);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::PosDerive
//
//	@doc:
//		Derive sort order
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalRowTrigger::PosDerive
	(
	IMemoryPool *memory_pool,
	CExpressionHandle & //exprhdl
	)
	const
{
	return GPOS_NEW(memory_pool) COrderSpec(memory_pool);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalRowTrigger::EpetOrder
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
	GPOS_ASSERT(!peo->PosRequired()->IsEmpty());

	return CEnfdProp::EpetRequired;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::PcrsRequired
//
//	@doc:
//		Compute required columns of the n-th child;
//		we only compute required columns for the relational child;
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalRowTrigger::PcrsRequired
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &, // exprhdl,
	CColRefSet *pcrsRequired,
	ULONG
#ifdef GPOS_DEBUG
	child_index
#endif // GPOS_DEBUG
	,
	DrgPdp *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
{
	GPOS_ASSERT(0 == child_index);

	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool, *m_pcrsRequiredLocal);
	pcrs->Union(pcrsRequired);

	return pcrs;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::PdsRequired
//
//	@doc:
//		Compute required distribution of the n-th child
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalRowTrigger::PdsRequired
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	CDistributionSpec *pdsInput,
	ULONG child_index,
	DrgPdp *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(0 == child_index);

	// if expression has to execute on master then we need a gather
	if (exprhdl.FMasterOnly())
	{
		return PdsEnforceMaster(memory_pool, exprhdl, pdsInput, child_index);
	}

	return GPOS_NEW(memory_pool) CDistributionSpecAny(this->Eopid());
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::PrsRequired
//
//	@doc:
//		Compute required rewindability of the n-th child
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalRowTrigger::PrsRequired
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	CRewindabilitySpec *prsRequired,
	ULONG child_index,
	DrgPdp *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
	const
{
	GPOS_ASSERT(0 == child_index);

	return PrsPassThru(memory_pool, exprhdl, prsRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::PppsRequired
//
//	@doc:
//		Compute required partition propagation of the n-th child
//
//---------------------------------------------------------------------------
CPartitionPropagationSpec *
CPhysicalRowTrigger::PppsRequired
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	CPartitionPropagationSpec *pppsRequired,
	ULONG child_index,
	DrgPdp *, //pdrgpdpCtxt,
	ULONG //ulOptReq
	)
{
	GPOS_ASSERT(0 == child_index);
	GPOS_ASSERT(NULL != pppsRequired);

	return CPhysical::PppsRequiredPushThru(memory_pool, exprhdl, pppsRequired, child_index);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::PcteRequired
//
//	@doc:
//		Compute required CTE map of the n-th child
//
//---------------------------------------------------------------------------
CCTEReq *
CPhysicalRowTrigger::PcteRequired
	(
	IMemoryPool *, //memory_pool,
	CExpressionHandle &, //exprhdl,
	CCTEReq *pcter,
	ULONG
#ifdef GPOS_DEBUG
	child_index
#endif
	,
	DrgPdp *, //pdrgpdpCtxt,
	ULONG //ulOptReq
	)
	const
{
	GPOS_ASSERT(0 == child_index);
	return PcterPushThru(pcter);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalRowTrigger::FProvidesReqdCols
	(
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsRequired,
	ULONG // ulOptReq
	)
	const
{
	return FUnaryProvidesReqdCols(exprhdl, pcrsRequired);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::PdsDerive
//
//	@doc:
//		Derive distribution
//
//---------------------------------------------------------------------------
CDistributionSpec *
CPhysicalRowTrigger::PdsDerive
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	return PdsDerivePassThruOuter(exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::PrsDerive
//
//	@doc:
//		Derive rewindability
//
//---------------------------------------------------------------------------
CRewindabilitySpec *
CPhysicalRowTrigger::PrsDerive
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	return PrsDerivePassThruOuter(exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CPhysicalRowTrigger::HashValue() const
{
	ULONG ulHash = gpos::CombineHashes(COperator::HashValue(), m_rel_mdid->HashValue());
	ulHash = gpos::CombineHashes(ulHash, gpos::HashValue<INT>(&m_type));

	if(NULL != m_pdrgpcrOld)
	{
		ulHash = gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrOld));
	}

	if(NULL != m_pdrgpcrNew)
	{
		ulHash = gpos::CombineHashes(ulHash, CUtils::UlHashColArray(m_pdrgpcrNew));
	}

	return ulHash;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::Matches
//
//	@doc:
//		Match operator
//
//---------------------------------------------------------------------------
BOOL
CPhysicalRowTrigger::Matches
	(
	COperator *pop
	)
	const
{
	if (pop->Eopid() != Eopid())
	{
		return false;
	}

	CPhysicalRowTrigger *popRowTrigger = CPhysicalRowTrigger::PopConvert(pop);

	DrgPcr *pdrgpcrOld = popRowTrigger->PdrgpcrOld();
	DrgPcr *pdrgpcrNew = popRowTrigger->PdrgpcrNew();

	return m_rel_mdid->Equals(popRowTrigger->GetRelMdId()) &&
			m_type == popRowTrigger->GetType() &&
			CUtils::Equals(m_pdrgpcrOld, pdrgpcrOld) &&
			CUtils::Equals(m_pdrgpcrNew, pdrgpcrNew);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::EpetRewindability
//
//	@doc:
//		Return the enforcing type for rewindability property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalRowTrigger::EpetRewindability
	(
	CExpressionHandle &exprhdl,
	const CEnfdRewindability *per
	)
	const
{
	CRewindabilitySpec *prs = CDrvdPropPlan::Pdpplan(exprhdl.Pdp())->Prs();
	if (per->FCompatible(prs))
	{
		 // required rewindability is already provided
		 return CEnfdProp::EpetUnnecessary;
	}

	// always force spool to be on top of trigger
	return CEnfdProp::EpetRequired;
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalRowTrigger::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CPhysicalRowTrigger::OsPrint
	(
	IOstream &os
	)
	const
{
	if (m_fPattern)
	{
		return COperator::OsPrint(os);
	}

	os << SzId() << " (Type: " << m_type << ")";

	if (NULL != m_pdrgpcrOld)
	{
		os << ", Old Columns: [";
		CUtils::OsPrintDrgPcr(os, m_pdrgpcrOld);
		os << "]";
	}

	if (NULL != m_pdrgpcrNew)
	{
		os << ", New Columns: [";
		CUtils::OsPrintDrgPcr(os, m_pdrgpcrNew);
		os << "]";
	}

	return os;
}


// EOF
