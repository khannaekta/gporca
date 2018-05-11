//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.

#include "gpopt/operators/CHashedDistributions.h"

using namespace gpopt;
CHashedDistributions::CHashedDistributions
		(
		IMemoryPool *memory_pool,
		DrgPcr *pdrgpcrOutput,
		DrgDrgPcr *pdrgpdrgpcrInput
		)
		:
		DrgPds(memory_pool)
{
	const ULONG num_cols = pdrgpcrOutput->Size();
	const ULONG arity = pdrgpdrgpcrInput->Size();
	for (ULONG ulChild = 0; ulChild < arity; ulChild++)
	{
		DrgPcr *colref_array = (*pdrgpdrgpcrInput)[ulChild];
		DrgPexpr *pdrgpexpr = GPOS_NEW(memory_pool) DrgPexpr(memory_pool);
		for (ULONG ulCol = 0; ulCol < num_cols; ulCol++)
		{
			CColRef *colref = (*colref_array)[ulCol];
			CExpression *pexpr = CUtils::PexprScalarIdent(memory_pool, colref);
			pdrgpexpr->Append(pexpr);
		}

		// create a hashed distribution on input columns of the current child
		BOOL fNullsColocated = true;
		CDistributionSpec *pdshashed = GPOS_NEW(memory_pool) CDistributionSpecHashed(pdrgpexpr, fNullsColocated);
		Append(pdshashed);
	}
}
