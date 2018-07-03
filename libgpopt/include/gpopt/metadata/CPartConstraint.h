//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CPartConstraint.h
//
//	@doc:
//		Part constraints for partitioned tables
//---------------------------------------------------------------------------
#ifndef GPOPT_CPartConstraint_H
#define GPOPT_CPartConstraint_H

#include "gpos/base.h"

#include "gpos/common/CHashMap.h"
#include "gpos/common/CHashMapIter.h"

#include "gpopt/base/CColRef.h"
#include "gpopt/base/CConstraint.h"

namespace gpopt
{
	using namespace gpos;	
	
	// fwd decl
	class CColRef;
	class CPartConstraint;

	// hash maps of part constraints indexed by part index id
	typedef CHashMap<ULONG, CPartConstraint, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
					CleanupDelete<ULONG>, CleanupRelease<CPartConstraint> > PartCnstrMap;
	
	// map iterator
	typedef CHashMapIter<ULONG, CPartConstraint, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
		CleanupDelete<ULONG>, CleanupRelease<CPartConstraint> > PartCnstrMapIter;

	//---------------------------------------------------------------------------
	//	@class:
	//		CPartConstraint
	//
	//	@doc:
	//		metadata abstraction for tables
	//
	//---------------------------------------------------------------------------
	class CPartConstraint : public CRefCount
	{
		private:
			
			// constraints for different levels
			UlongToConstraintMap *m_phmulcnstr;
			
			// levels at which the default partitions are included
			CBitSet *m_pbsDefaultParts;
			
			// number of levels;
			ULONG m_num_of_part_levels;

			// is constraint unbounded
			BOOL m_is_unbounded;
			
			// is a dummy (not to be used) constraint
			BOOL m_fUninterpreted;

			// partition keys
			ColRefArrays *m_pdrgpdrgpcr;

			// combined constraint
			CConstraint *m_pcnstrCombined;

			// private copy ctor
			CPartConstraint(const CPartConstraint &);

#ifdef GPOS_DEBUG
			// are all default partitions on all levels included
			BOOL FAllDefaultPartsIncluded();
#endif //GPOS_DEBUG

			// does the current constraint overlap with given one at the given level
			BOOL FOverlapLevel(IMemoryPool *mp, const CPartConstraint *ppartcnstr, ULONG ulLevel) const;

			// check whether or not the current part constraint can be negated. A part
			// constraint can be negated only if it has constraints on the first level
			// since negation destroys the independence between the levels
			BOOL FCanNegate() const;

			// construct the combined constraint
			CConstraint *PcnstrBuildCombined(IMemoryPool *mp);

			// return the remaining part of the first constraint that is not covered by
			// the second constraint
			CConstraint *PcnstrRemaining(IMemoryPool *mp, CConstraint *pcnstrFst, CConstraint *pcnstrSnd);

			// check if two constaint maps have the same constraints
			static
			BOOL FEqualConstrMaps(UlongToConstraintMap *phmulcnstrFst, UlongToConstraintMap *phmulcnstrSnd, ULONG ulLevels);

			// check if it is possible to produce a disjunction of the two given part
			// constraints. This is possible if the first ulLevels-1 have the same
			// constraints and default flags for both part constraints
			static
			BOOL FDisjunctionPossible(CPartConstraint *ppartcnstrFst, CPartConstraint *ppartcnstrSnd);

		public:

			// ctors
			CPartConstraint(IMemoryPool *mp, UlongToConstraintMap *phmulcnstr, CBitSet *pbsDefaultParts, BOOL is_unbounded, ColRefArrays *pdrgpdrgpcr);
			CPartConstraint(IMemoryPool *mp, CConstraint *pcnstr, BOOL fDefaultPartition, BOOL is_unbounded);
				
			CPartConstraint(BOOL fUninterpreted);

			// dtor
			virtual
			~CPartConstraint();
	
			// constraint at given level
			CConstraint *Pcnstr(ULONG ulLevel) const;

			// combined constraint
			CConstraint *PcnstrCombined() const
			{
				return m_pcnstrCombined;
			}

			// is default partition included on the given level
			BOOL IsDefaultPartition(ULONG ulLevel) const
			{
				return m_pbsDefaultParts->Get(ulLevel);
			}

			// partition keys
			ColRefArrays *Pdrgpdrgpcr() const
			{
				return m_pdrgpdrgpcr;
			}

			// is constraint unbounded
			BOOL IsConstraintUnbounded() const;

			// is constraint uninterpreted
			BOOL FUninterpreted() const
			{
				return m_fUninterpreted;
			}

			// are constraints equivalent
			BOOL FEquivalent(const CPartConstraint *ppartcnstr) const;
			
			// does constraint overlap with given constraint
			BOOL FOverlap(IMemoryPool *mp, const CPartConstraint *ppartcnstr) const;
			
			// does constraint subsume given one
			BOOL FSubsume(const CPartConstraint *ppartcnstr) const;

			// return what remains of the current part constraint after taking out
			// the given part constraint. Returns NULL is the difference cannot be
			// performed
			CPartConstraint *PpartcnstrRemaining(IMemoryPool *mp, CPartConstraint *ppartcnstr);

			// return a copy of the part constraint with remapped columns
			CPartConstraint *PpartcnstrCopyWithRemappedColumns(IMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist);

			// print
			IOstream &OsPrint(IOstream &) const;
			
			// construct a disjunction of the two constraints
			static
			CPartConstraint *PpartcnstrDisjunction(IMemoryPool *mp, CPartConstraint *ppartcnstrFst, CPartConstraint *ppartcnstrSnd);

			// combine the two given part constraint maps and return the result
			static
			PartCnstrMap *PpartcnstrmapCombine
				(
				IMemoryPool *mp,
				PartCnstrMap *ppartcnstrmapFst,
				PartCnstrMap *ppartcnstrmapSnd
				);

			// copy the part constraints from the source map into the destination map
			static
			void CopyPartConstraints(IMemoryPool *mp, PartCnstrMap *ppartcnstrmapDest, PartCnstrMap *ppartcnstrmapSource);

	}; // class CPartConstraint

}

#endif // !GPOPT_CPartConstraint_H

// EOF
