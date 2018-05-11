//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2011 EMC Corp.
//
//	@filename:
//		CLogicalLimit.cpp
//
//	@doc:
//		Implementation of logical limit operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "naucrates/base/IDatumInt8.h"
#include "naucrates/md/IMDTypeInt8.h"

#include "gpopt/base/CUtils.h"
#include "gpopt/base/CColRefSet.h"

#include "gpopt/metadata/CTableDescriptor.h"
#include "gpopt/metadata/CName.h"

#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalLimit.h"

#include "naucrates/statistics/CLimitStatsProcessor.h"

using namespace gpopt;


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::CLogicalLimit
//
//	@doc:
//		Ctor - for pattern
//
//---------------------------------------------------------------------------
CLogicalLimit::CLogicalLimit
	(
	IMemoryPool *memory_pool
	)
	:
	CLogical(memory_pool),
	m_pos(NULL),
	m_fGlobal(true),
	m_fHasCount(false),
	m_top_limit_under_dml(false)
{
	m_fPattern = true;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::CLogicalLimit
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalLimit::CLogicalLimit
	(
	IMemoryPool *memory_pool,
	COrderSpec *pos,
	BOOL fGlobal,
	BOOL fHasCount,
	BOOL fTopLimitUnderDML
	)
	:
	CLogical(memory_pool),
	m_pos(pos),
	m_fGlobal(fGlobal),
	m_fHasCount(fHasCount),
	m_top_limit_under_dml(fTopLimitUnderDML)
{
	GPOS_ASSERT(NULL != pos);
	CColRefSet *pcrsSort = m_pos->PcrsUsed(memory_pool);
	m_pcrsLocalUsed->Include(pcrsSort);
	pcrsSort->Release();
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::~CLogicalLimit
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalLimit::~CLogicalLimit()
{
	CRefCount::SafeRelease(m_pos);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::HashValue
//
//	@doc:
//		Operator specific hash function
//
//---------------------------------------------------------------------------
ULONG
CLogicalLimit::HashValue() const
{
	return gpos::CombineHashes
			(
			gpos::CombineHashes(COperator::HashValue(), m_pos->HashValue()),
			gpos::CombineHashes(gpos::HashValue<BOOL>(&m_fGlobal), gpos::HashValue<BOOL>(&m_fHasCount))
			);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::Matches
//
//	@doc:
//		Match function on operator level
//
//---------------------------------------------------------------------------
BOOL
CLogicalLimit::Matches
	(
	COperator *pop
	)
	const
{
	if (pop->Eopid() == Eopid())
	{
		CLogicalLimit *popLimit = CLogicalLimit::PopConvert(pop);
		
		if (popLimit->FGlobal() == m_fGlobal &&
			popLimit->FHasCount() == m_fHasCount)
		{
			// match if order specs match
			return m_pos->Matches(popLimit->m_pos);
		}
	}
	
	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalLimit::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	COrderSpec *pos = m_pos->PosCopyWithRemappedColumns(memory_pool, colref_mapping, must_exist);

	return GPOS_NEW(memory_pool) CLogicalLimit(memory_pool, pos, m_fGlobal, m_fHasCount, m_top_limit_under_dml);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::PcrsDeriveOutput
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalLimit::PcrsDeriveOutput
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
{
	return PcrsDeriveOutputPassThru(exprhdl);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::PcrsDeriveOuter
//
//	@doc:
//		Derive outer references
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalLimit::PcrsDeriveOuter
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	CColRefSet *pcrsSort = m_pos->PcrsUsed(memory_pool);
	CColRefSet *outer_refs = CLogical::PcrsDeriveOuter(memory_pool, exprhdl, pcrsSort);
	pcrsSort->Release();

	return outer_refs;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalLimit::Maxcard
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	CExpression *pexprCount = exprhdl.PexprScalarChild(2 /*child_index*/);
	if (CUtils::FScalarConstInt<IMDTypeInt8>(pexprCount))
	{
		CScalarConst *popScalarConst = CScalarConst::PopConvert(pexprCount->Pop());
		IDatumInt8 *pdatumInt8 = dynamic_cast<IDatumInt8 *>(popScalarConst->GetDatum());

		return CMaxCard(pdatumInt8->Value());
	}

	// pass on max card of first child
	return exprhdl.GetRelationalProperties(0)->Maxcard();
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalLimit::PxfsCandidates
	(
	IMemoryPool *memory_pool
	) 
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	
	(void) xform_set->ExchangeSet(CXform::ExfImplementLimit);
	(void) xform_set->ExchangeSet(CXform::ExfSplitLimit);
	
	return xform_set;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::PcrsStat
//
//	@doc:
//		Compute required stat columns of the n-th child
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalLimit::PcrsStat
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	CColRefSet *pcrsInput,
	ULONG child_index
	)
	const
{
	GPOS_ASSERT(0 == child_index);

	CColRefSet *pcrsUsed = GPOS_NEW(memory_pool) CColRefSet(memory_pool);
	// add columns used by number of rows and offset scalar children
	pcrsUsed->Union(exprhdl.GetDrvdScalarProps(1)->PcrsUsed());
	pcrsUsed->Union(exprhdl.GetDrvdScalarProps(2)->PcrsUsed());

	CColRefSet *pcrsStat = PcrsReqdChildStats(memory_pool, exprhdl, pcrsInput, pcrsUsed, child_index);
	pcrsUsed->Release();

	return pcrsStat;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalLimit::PkcDeriveKeys
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	return PkcDeriveKeysPassThru(exprhdl, 0 /* ulChild */);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalLimit::OsPrint
	(
	IOstream &os
	)
	const
{
	os << SzId() << " " << (*m_pos) << " " << (m_fGlobal ? "global" : "local")
			<< (m_top_limit_under_dml ? " NonRemovableLimit" : "");
	
	return os;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalLimit::PstatsDerive
//
//	@doc:
//		Derive statistics based on limit
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalLimit::PstatsDerive
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	StatsArray * // not used
	)
	const
{
	GPOS_ASSERT(Esp(exprhdl) > EspNone);
	IStatistics *child_stats = exprhdl.Pstats(0);
	CMaxCard maxcard = this->Maxcard(memory_pool, exprhdl);
	CDouble dRowsMax = CDouble(maxcard.Ull());

	if (child_stats->Rows() <= dRowsMax)
	{
		child_stats->AddRef();
		return child_stats;
	}

	return CLimitStatsProcessor::CalcLimitStats(memory_pool, dynamic_cast<CStatistics*>(child_stats), dRowsMax);
}

// EOF
