//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPhysicalMotionRandom.cpp
//
//	@doc:
//		Implementation of random motion operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CPhysicalMotionRandom.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionRandom::CPhysicalMotionRandom
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CPhysicalMotionRandom::CPhysicalMotionRandom
	(
	IMemoryPool *memory_pool,
	CDistributionSpecRandom *pdsRandom
	)
	:
	CPhysicalMotion(memory_pool),
	m_pdsRandom(pdsRandom)
{
	GPOS_ASSERT(NULL != pdsRandom);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionRandom::~CPhysicalMotionRandom
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CPhysicalMotionRandom::~CPhysicalMotionRandom()
{
	m_pdsRandom->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionRandom::Matches
//
//	@doc:
//		Match operators
//
//---------------------------------------------------------------------------
BOOL
CPhysicalMotionRandom::Matches
	(
	COperator *pop
	)
	const
{
	return Eopid() == pop->Eopid();
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionRandom::PcrsRequired
//
//	@doc:
//		Compute required columns of the n-th child;
//
//---------------------------------------------------------------------------
CColRefSet *
CPhysicalMotionRandom::PcrsRequired
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsRequired,
	ULONG child_index,
	DrgPdp *, // pdrgpdpCtxt
	ULONG // ulOptReq
	)
{
	GPOS_ASSERT(0 == child_index);

	return PcrsChildReqd(memory_pool, exprhdl, pcrsRequired, child_index,
						 gpos::ulong_max);
}

//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionRandom::FProvidesReqdCols
//
//	@doc:
//		Check if required columns are included in output columns
//
//---------------------------------------------------------------------------
BOOL
CPhysicalMotionRandom::FProvidesReqdCols
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
//		CPhysicalMotionRandom::EpetOrder
//
//	@doc:
//		Return the enforcing type for order property based on this operator
//
//---------------------------------------------------------------------------
CEnfdProp::EPropEnforcingType
CPhysicalMotionRandom::EpetOrder
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
//		CPhysicalMotionRandom::PosRequired
//
//	@doc:
//		Compute required sort order of the n-th child
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalMotionRandom::PosRequired
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &, //exprhdl,
	COrderSpec *,//posInput,
	ULONG
#ifdef GPOS_DEBUG
	child_index
#endif // GPOS_DEBUG
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
//		CPhysicalMotionRandom::PosDerive
//
//	@doc:
//		Derive sort order
//
//---------------------------------------------------------------------------
COrderSpec *
CPhysicalMotionRandom::PosDerive
	(
	IMemoryPool *memory_pool,
	CExpressionHandle & // exprhdl
	)
	const
{
	return GPOS_NEW(memory_pool) COrderSpec(memory_pool);
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionRandom::OsPrint
//
//	@doc:
//		Debug print
//
//---------------------------------------------------------------------------
IOstream &
CPhysicalMotionRandom::OsPrint
	(
	IOstream &os
	)
	const
{
	os	<< SzId();
		
	return os;
}


//---------------------------------------------------------------------------
//	@function:
//		CPhysicalMotionRandom::PopConvert
//
//	@doc:
//		Conversion function
//
//---------------------------------------------------------------------------
CPhysicalMotionRandom *
CPhysicalMotionRandom::PopConvert
	(
	COperator *pop
	)
{
	GPOS_ASSERT(NULL != pop);
	GPOS_ASSERT(EopPhysicalMotionRandom == pop->Eopid());
	
	return dynamic_cast<CPhysicalMotionRandom*>(pop);
}			

// EOF

