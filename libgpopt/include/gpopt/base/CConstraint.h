//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2012 EMC Corp.
//
//	@filename:
//		CConstraint.h
//
//	@doc:
//		Base class for representing constraints
//---------------------------------------------------------------------------
#ifndef GPOPT_CConstraint_H
#define GPOPT_CConstraint_H

#include "gpos/base.h"
#include "gpos/types.h"
#include "gpos/common/CRefCount.h"

#include "gpopt/base/CRange.h"

namespace gpopt
{
	using namespace gpos;

	// fwd declaration
	class CExpression;
	class CConstraint;
	class CRange;

	// constraint array
	typedef CDynamicPtrArray<CConstraint, CleanupRelease> ConstraintArray;

	// hash map mapping CColRef -> ConstraintArray
	typedef CHashMap<CColRef, ConstraintArray, gpos::HashValue<CColRef>, gpos::Equals<CColRef>,
					CleanupNULL<CColRef>, CleanupRelease<ConstraintArray> > ColRefToConstraintArrayMap;

	// mapping CConstraint -> BOOL to cache previous containment queries,
	// we use pointer equality here for fast map lookup -- since we do shallow comparison, we do not take ownership
	// of pointer values
	typedef CHashMap<CConstraint, BOOL, gpos::HashPtr<CConstraint>, gpos::EqualPtr<CConstraint>,
					CleanupNULL<CConstraint>, CleanupNULL<BOOL> > ConstraintContainmentMap;

	// hash map mapping ULONG -> CConstraint
	typedef CHashMap<ULONG, CConstraint, gpos::HashValue<ULONG>, gpos::Equals<ULONG>,
					CleanupDelete<ULONG>, CleanupRelease<CConstraint> > UlongToConstraintMap;

	//---------------------------------------------------------------------------
	//	@class:
	//		CConstraint
	//
	//	@doc:
	//		Base class for representing constraints
	//
	//---------------------------------------------------------------------------
	class CConstraint : public CRefCount
	{
		public:

			enum EConstraintType
			{
				EctInterval, // a single interval on a single columns
				EctConjunction, // a set of ANDed constraints
				EctDisjunction, // a set of ORed constraints
				EctNegation // a negated constraint
			};

		private:

			// containment map
			ConstraintContainmentMap *m_phmcontain;

			// constant true
			static
			BOOL m_fTrue;

			// constant false
			static
			BOOL m_fFalse;

			// hidden copy ctor
			CConstraint(const CConstraint&);

			// return address of static BOOL constant based on passed BOOL m_bytearray_value
			static
			BOOL *PfVal
				(
				BOOL value
				)
			{
				if (value)
				{
					return &m_fTrue;
				}

				return &m_fFalse;
			}

			// add column as a new equivalence class, if it is not already in one of the
			// existing equivalence classes
			static
			void AddColumnToEquivClasses(IMemoryPool *mp, const CColRef *colref, ColRefSetArray **ppdrgpcrs);

			// create constraint from scalar comparison
			static
			CConstraint *PcnstrFromScalarCmp(IMemoryPool *mp, CExpression *pexpr, ColRefSetArray **ppdrgpcrs);

			// create constraint from scalar boolean expression
			static
			CConstraint *PcnstrFromScalarBoolOp(IMemoryPool *mp, CExpression *pexpr, ColRefSetArray **ppdrgpcrs);

			// create conjunction/disjunction from array of constraints
			static
			CConstraint *PcnstrConjDisj(IMemoryPool *mp, ConstraintArray *pdrgpcnstr, BOOL fConj);

		protected:

			// memory pool -- used for local computations
			IMemoryPool *m_mp;

			// columns used in this constraint
			CColRefSet *m_pcrsUsed;

			// equivalent scalar expression
			CExpression *m_pexprScalar;

			// print
			IOstream &PrintConjunctionDisjunction
						(
						IOstream &os,
						ConstraintArray *pdrgpcnstr
						)
						const;

			// construct a conjunction or disjunction scalar expression from an
			// array of constraints
			CExpression *PexprScalarConjDisj(IMemoryPool *mp, ConstraintArray *pdrgpcnstr, BOOL fConj) const;

			// flatten an array of constraints to be used as constraint children
			ConstraintArray *PdrgpcnstrFlatten(IMemoryPool *mp, ConstraintArray *pdrgpcnstr, EConstraintType ect) const;

			// combine any two or more constraints that reference only one particular column
			ConstraintArray *PdrgpcnstrDeduplicate(IMemoryPool *mp, ConstraintArray *pdrgpcnstr, EConstraintType ect) const;

			// mapping between columns and arrays of constraints
			ColRefToConstraintArrayMap *Phmcolconstr(IMemoryPool *mp, CColRefSet *pcrs, ConstraintArray *pdrgpcnstr) const;

			// return a copy of the conjunction/disjunction constraint for a different column
			CConstraint *PcnstrConjDisjRemapForColumn
							(
							IMemoryPool *mp,
							CColRef *colref,
							ConstraintArray *pdrgpcnstr,
							BOOL fConj
							)
							const;

			// create constraint from scalar array comparison expression originally generated for
			// "scalar op ANY/ALL (array)" construct
			static
			CConstraint *PcnstrFromScalarArrayCmp(IMemoryPool *mp, CExpression *pexpr, CColRef *colref);

		public:

			// ctor
			explicit
			CConstraint(IMemoryPool *mp);

			// dtor
			virtual
			~CConstraint();

			// constraint type accessor
			virtual
			EConstraintType Ect() const = 0;

			// is this constraint a contradiction
			virtual
			BOOL FContradiction() const = 0;

			// is this constraint unbounded
			virtual
			BOOL IsConstraintUnbounded() const
			{
				return false;
			}
			
			// does the current constraint contain the given one
			virtual
			BOOL Contains(CConstraint *pcnstr);

			// equality function
			virtual
			BOOL Equals(CConstraint *pcnstr);

			// columns in this constraint
			virtual
			CColRefSet *PcrsUsed() const
			{
				return m_pcrsUsed;
			}

			// scalar expression
			virtual
			CExpression *PexprScalar(IMemoryPool *mp) = 0;

			// check if there is a constraint on the given column
			virtual
			BOOL FConstraint(const CColRef *colref) const = 0;

			// return a copy of the constraint with remapped columns
			virtual
			CConstraint *PcnstrCopyWithRemappedColumns(IMemoryPool *mp, UlongToColRefMap *colref_mapping, BOOL must_exist) = 0;

			// return constraint on a given column
			virtual
			CConstraint *Pcnstr
							(
							IMemoryPool *, //mp,
							const CColRef * //colref
							)
			{
				return NULL;
			}

			// return constraint on a given set of columns
			virtual
			CConstraint *Pcnstr
							(
							IMemoryPool *, //mp,
							CColRefSet * //pcrs
							)
			{
				return NULL;
			}

			// return a clone of the constraint for a different column
			virtual
			CConstraint *PcnstrRemapForColumn(IMemoryPool *mp, CColRef *colref) const = 0;

			// print
			virtual
			IOstream &OsPrint
						(
						IOstream &os
						)
						const = 0;

			// create constraint from scalar expression and pass back any discovered
			// equivalence classes
			static
			CConstraint *PcnstrFromScalarExpr
							(
							IMemoryPool *mp,
							CExpression *pexpr,
							ColRefSetArray **ppdrgpcrs
							);

			// create conjunction from array of constraints
			static
			CConstraint *PcnstrConjunction(IMemoryPool *mp, ConstraintArray *pdrgpcnstr);

			// create disjunction from array of constraints
			static
			CConstraint *PcnstrDisjunction(IMemoryPool *mp, ConstraintArray *pdrgpcnstr);

			// merge equivalence classes coming from children of a bool op
			static
			ColRefSetArray *PdrgpcrsMergeFromBoolOp(IMemoryPool *mp, CExpression *pexpr, ColRefSetArray *pdrgpcrsFst, ColRefSetArray *pdrgpcrsSnd);

			// subset of the given constraints, which reference the given column
			static
			ConstraintArray *PdrgpcnstrOnColumn(IMemoryPool *mp, ConstraintArray *pdrgpcnstr, CColRef *colref, BOOL fExclusive);
#ifdef GPOS_DEBUG
			void DbgPrint() const;
#endif  // GPOS_DEBUG

	}; // class CConstraint

	// shorthand for printing, pointer.
	inline
	IOstream &operator << (IOstream &os, const CConstraint *cnstr)
	{
		return cnstr->OsPrint(os);
	}
	// shorthand for printing
	inline
	IOstream &operator << (IOstream &os, const CConstraint &cnstr)
	{
		return cnstr.OsPrint(os);
	}
}

#endif // !GPOPT_CConstraint_H

// EOF
