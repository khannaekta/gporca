//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CLogicalSequenceProject.cpp
//
//	@doc:
//		Implementation of sequence project operator
//---------------------------------------------------------------------------

#include "gpos/base.h"

#include "gpopt/base/CDistributionSpec.h"
#include "gpopt/base/CDistributionSpecHashed.h"
#include "gpopt/base/CDistributionSpecSingleton.h"
#include "gpopt/base/CColRefSet.h"
#include "gpopt/base/CKeyCollection.h"

#include "gpopt/operators/CExpression.h"
#include "gpopt/operators/CExpressionHandle.h"
#include "gpopt/operators/CLogicalSequenceProject.h"

using namespace gpopt;

//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::CLogicalSequenceProject
//
//	@doc:
//		Ctor
//
//---------------------------------------------------------------------------
CLogicalSequenceProject::CLogicalSequenceProject
	(
	IMemoryPool *memory_pool,
	CDistributionSpec *pds,
	DrgPos *pdrgpos,
	DrgPwf *pdrgpwf
	)
	:
	CLogicalUnary(memory_pool),
	m_pds(pds),
	m_pdrgpos(pdrgpos),
	m_pdrgpwf(pdrgpwf),
	m_fHasOrderSpecs(false),
	m_fHasFrameSpecs(false)
{
	GPOS_ASSERT(NULL != pds);
	GPOS_ASSERT(NULL != pdrgpos);
	GPOS_ASSERT(NULL != pdrgpwf);
	GPOS_ASSERT(CDistributionSpec::EdtHashed == pds->Edt() ||
			CDistributionSpec::EdtSingleton == pds->Edt());

	// set flags indicating that current operator has non-empty order specs/frame specs
	SetHasOrderSpecs(memory_pool);
	SetHasFrameSpecs(memory_pool);

	// include columns used by Partition By, Order By, and window frame edges
	CColRefSet *pcrsSort = COrderSpec::GetColRefSet(memory_pool, m_pdrgpos);
	m_pcrsLocalUsed->Include(pcrsSort);
	pcrsSort->Release();

	if (CDistributionSpec::EdtHashed == m_pds->Edt())
	{
		CColRefSet *pcrsHashed = CDistributionSpecHashed::PdsConvert(m_pds)->PcrsUsed(memory_pool);
		m_pcrsLocalUsed->Include(pcrsHashed);
		pcrsHashed->Release();
	}

	const ULONG ulFrames = m_pdrgpwf->Size();
	for (ULONG ul = 0; ul < ulFrames; ul++)
	{
		CWindowFrame *pwf = (*m_pdrgpwf)[ul];
		if (!CWindowFrame::IsEmpty(pwf))
		{
			m_pcrsLocalUsed->Include(pwf->PcrsUsed());
		}
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::CLogicalSequenceProject
//
//	@doc:
//		Ctor for patterns
//
//---------------------------------------------------------------------------
CLogicalSequenceProject::CLogicalSequenceProject
	(
	IMemoryPool *memory_pool
	)
	:
	CLogicalUnary(memory_pool),
	m_pds(NULL),
	m_pdrgpos(NULL),
	m_pdrgpwf(NULL),
	m_fHasOrderSpecs(false),
	m_fHasFrameSpecs(false)
{
	m_fPattern = true;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::~CLogicalSequenceProject
//
//	@doc:
//		Dtor
//
//---------------------------------------------------------------------------
CLogicalSequenceProject::~CLogicalSequenceProject()
{
	CRefCount::SafeRelease(m_pds);
	CRefCount::SafeRelease(m_pdrgpos);
	CRefCount::SafeRelease(m_pdrgpwf);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::PopCopyWithRemappedColumns
//
//	@doc:
//		Return a copy of the operator with remapped columns
//
//---------------------------------------------------------------------------
COperator *
CLogicalSequenceProject::PopCopyWithRemappedColumns
	(
	IMemoryPool *memory_pool,
	UlongColRefHashMap *colref_mapping,
	BOOL must_exist
	)
{
	CDistributionSpec *pds = m_pds->PdsCopyWithRemappedColumns(memory_pool, colref_mapping, must_exist);

	DrgPos *pdrgpos = GPOS_NEW(memory_pool) DrgPos(memory_pool);
	const ULONG ulOrderSpec = m_pdrgpos->Size();
	for (ULONG ul = 0; ul < ulOrderSpec; ul++)
	{
		COrderSpec *pos = ((*m_pdrgpos)[ul])->PosCopyWithRemappedColumns(memory_pool, colref_mapping, must_exist);
		pdrgpos->Append(pos);
	}

	DrgPwf *pdrgpwf = GPOS_NEW(memory_pool) DrgPwf(memory_pool);
	const ULONG ulWindowFrames = m_pdrgpwf->Size();
	for (ULONG ul = 0; ul < ulWindowFrames; ul++)
	{
		CWindowFrame *pwf = ((*m_pdrgpwf)[ul])->PwfCopyWithRemappedColumns(memory_pool, colref_mapping, must_exist);
		pdrgpwf->Append(pwf);
	}

	return GPOS_NEW(memory_pool) CLogicalSequenceProject(memory_pool, pds, pdrgpos, pdrgpwf);
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::SetHasOrderSpecs
//
//	@doc:
//		Set the flag indicating that SeqPrj has specified order specs
//
//---------------------------------------------------------------------------
void
CLogicalSequenceProject::SetHasOrderSpecs
	(
	IMemoryPool *memory_pool
	)
{
	GPOS_ASSERT(NULL != m_pdrgpos);

	const ULONG ulOrderSpecs = m_pdrgpos->Size();
	if (0 == ulOrderSpecs)
	{
		// if no order specs are given, we add one empty order spec
		m_pdrgpos->Append(GPOS_NEW(memory_pool) COrderSpec(memory_pool));
	}
	BOOL fHasOrderSpecs = false;
	for (ULONG ul = 0; !fHasOrderSpecs && ul < ulOrderSpecs; ul++)
	{
		fHasOrderSpecs = !(*m_pdrgpos)[ul]->IsEmpty();
	}
	m_fHasOrderSpecs = fHasOrderSpecs;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::SetHasFrameSpecs
//
//	@doc:
//		Set the flag indicating that SeqPrj has specified frame specs
//
//---------------------------------------------------------------------------
void
CLogicalSequenceProject::SetHasFrameSpecs
	(
	IMemoryPool * // memory_pool
	)
{
	GPOS_ASSERT(NULL != m_pdrgpwf);

	const ULONG ulFrameSpecs = m_pdrgpwf->Size();
	if (0 == ulFrameSpecs)
	{
		// if no frame specs are given, we add one empty frame
		CWindowFrame *pwf = const_cast<CWindowFrame *>(CWindowFrame::PwfEmpty());
		pwf->AddRef();
		m_pdrgpwf->Append(pwf);
	}
	BOOL fHasFrameSpecs = false;
	for (ULONG ul = 0; !fHasFrameSpecs && ul < ulFrameSpecs; ul++)
	{
		fHasFrameSpecs = !CWindowFrame::IsEmpty((*m_pdrgpwf)[ul]);
	}
	m_fHasFrameSpecs = fHasFrameSpecs;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::PcrsDeriveOutput
//
//	@doc:
//		Derive output columns
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalSequenceProject::PcrsDeriveOutput
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	GPOS_ASSERT(2 == exprhdl.Arity());

	CColRefSet *pcrs = GPOS_NEW(memory_pool) CColRefSet(memory_pool);

	pcrs->Union(exprhdl.GetRelationalProperties(0)->PcrsOutput());

	// the scalar child defines additional columns
	pcrs->Union(exprhdl.GetDrvdScalarProps(1)->PcrsDefined());

	return pcrs;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::PcrsDeriveOuter
//
//	@doc:
//		Derive outer references
//
//---------------------------------------------------------------------------
CColRefSet *
CLogicalSequenceProject::PcrsDeriveOuter
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	CColRefSet *outer_refs = CLogical::PcrsDeriveOuter(memory_pool, exprhdl, m_pcrsLocalUsed);

	return outer_refs;
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::FHasLocalOuterRefs
//
//	@doc:
//		Return true if outer references are included in Partition/Order,
//		or window frame edges
//
//---------------------------------------------------------------------------
BOOL
CLogicalSequenceProject::FHasLocalOuterRefs
	(
	CExpressionHandle &exprhdl
	)
	const
{
	GPOS_ASSERT(this == exprhdl.Pop());

	CColRefSet *outer_refs = CDrvdPropRelational::GetRelationalProperties(exprhdl.Pdp())->PcrsOuter();

	return !(outer_refs->IsDisjoint(m_pcrsLocalUsed));
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::PkcDeriveKeys
//
//	@doc:
//		Derive key collection
//
//---------------------------------------------------------------------------
CKeyCollection *
CLogicalSequenceProject::PkcDeriveKeys
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
//		CLogicalSequenceProject::Maxcard
//
//	@doc:
//		Derive max card
//
//---------------------------------------------------------------------------
CMaxCard
CLogicalSequenceProject::Maxcard
	(
	IMemoryPool *, // memory_pool
	CExpressionHandle &exprhdl
	)
	const
{
	// pass on max card of first child
	return exprhdl.GetRelationalProperties(0)->Maxcard();
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::Matches
//
//	@doc:
//		Matching function
//
//---------------------------------------------------------------------------
BOOL
CLogicalSequenceProject::Matches
	(
	COperator *pop
	)
	const
{
	GPOS_ASSERT(NULL != pop);
	if (Eopid() == pop->Eopid())
	{
		CLogicalSequenceProject *popLogicalSequenceProject = CLogicalSequenceProject::PopConvert(pop);
		return
			m_pds->Matches(popLogicalSequenceProject->Pds()) &&
			CWindowFrame::Equals(m_pdrgpwf, popLogicalSequenceProject->Pdrgpwf()) &&
			COrderSpec::Equals(m_pdrgpos, popLogicalSequenceProject->Pdrgpos());
	}

	return false;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::HashValue
//
//	@doc:
//		Hashing function
//
//---------------------------------------------------------------------------
ULONG
CLogicalSequenceProject::HashValue() const
{
	ULONG ulHash = 0;
	ulHash = gpos::CombineHashes(ulHash, m_pds->HashValue());
	ulHash = gpos::CombineHashes(ulHash, CWindowFrame::HashValue(m_pdrgpwf, 3 /*ulMaxSize*/));
	ulHash = gpos::CombineHashes(ulHash, COrderSpec::HashValue(m_pdrgpos, 3 /*ulMaxSize*/));

	return ulHash;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::PxfsCandidates
//
//	@doc:
//		Get candidate xforms
//
//---------------------------------------------------------------------------
CXformSet *
CLogicalSequenceProject::PxfsCandidates
	(
	IMemoryPool *memory_pool
	)
	const
{
	CXformSet *xform_set = GPOS_NEW(memory_pool) CXformSet(memory_pool);
	(void) xform_set->ExchangeSet(CXform::ExfSequenceProject2Apply);
	(void) xform_set->ExchangeSet(CXform::ExfImplementSequenceProject);

	return xform_set;
}


//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::PstatsDerive
//
//	@doc:
//		Derive statistics based on filter predicates
//
//---------------------------------------------------------------------------
IStatistics *
CLogicalSequenceProject::PstatsDerive
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl,
	StatsArray * // stats_ctxt
	)
	const
{
	return PstatsDeriveProject(memory_pool, exprhdl);
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::OsPrint
//
//	@doc:
//		debug print
//
//---------------------------------------------------------------------------
IOstream &
CLogicalSequenceProject::OsPrint
	(
	IOstream &os
	)
	const
{
	os	<< SzId() << " (";
	os << "Partition By Keys:";
	(void) m_pds->OsPrint(os);
	os	<< ", ";
	os << "Order Spec:";
	(void) COrderSpec::OsPrint(os, m_pdrgpos);
	os	<< ", ";
	os << "WindowFrame Spec:";
	(void) CWindowFrame::OsPrint(os, m_pdrgpwf);

	return os << ")";
}

//---------------------------------------------------------------------------
//	@function:
//		CLogicalSequenceProject::PopRemoveLocalOuterRefs
//
//	@doc:
//		Filter out outer references from Order By/ Partition By
//		clauses, and return a new operator
//
//---------------------------------------------------------------------------
CLogicalSequenceProject *
CLogicalSequenceProject::PopRemoveLocalOuterRefs
	(
	IMemoryPool *memory_pool,
	CExpressionHandle &exprhdl
	)
{
	GPOS_ASSERT(this == exprhdl.Pop());

	CColRefSet *outer_refs = exprhdl.GetRelationalProperties()->PcrsOuter();
	CDistributionSpec *pds = m_pds;
	if (CDistributionSpec::EdtHashed == m_pds->Edt())
	{
		pds = CDistributionSpecHashed::PdsConvert(m_pds)->PdshashedExcludeColumns(memory_pool, outer_refs);
		if (NULL == pds)
		{
			// eliminate Partition clause
			pds = GPOS_NEW(memory_pool) CDistributionSpecSingleton(CDistributionSpecSingleton::EstMaster);
		}
	}
	else
	{
		pds->AddRef();
	}

	DrgPos *pdrgpos = COrderSpec::PdrgposExclude(memory_pool, m_pdrgpos, outer_refs);

	// for window frame edges, outer references cannot be removed since this can change
	// the semantics of frame edge from delayed-bounding to unbounded,
	// we re-use the frame edges without changing here
	m_pdrgpwf->AddRef();

	return GPOS_NEW(memory_pool) CLogicalSequenceProject(memory_pool, pds, pdrgpos, m_pdrgpwf);
}

// EOF

