//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CParseHandlerManagerTest.cpp
//
//	@doc:
//		Tests parsing DXL documents into DXL trees.
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/io/COstreamString.h"
#include "gpos/string/CWStringDynamic.h"
#include "gpos/memory/CAutoMemoryPool.h"
#include "gpos/task/CAutoTraceFlag.h"
#include "gpos/test/CUnittest.h"

#include "naucrates/dxl/operators/CDXLNode.h"
#include "naucrates/dxl/parser/CParseHandlerHashJoin.h"
#include "naucrates/dxl/operators/CDXLPhysicalTableScan.h"
#include "naucrates/dxl/parser/CParseHandlerPlan.h"
#include "naucrates/dxl/parser/CParseHandlerFactory.h"
#include "naucrates/dxl/parser/CParseHandlerManager.h"
#include "naucrates/dxl/xml/CDXLMemoryManager.h"

#include <xercesc/sax2/XMLReaderFactory.hpp>

#include "unittest/dxl/CParseHandlerManagerTest.h"

using namespace gpos;
using namespace gpdxl;

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerManagerTest::EresUnittest
//
//	@doc:
//		Unittest for activating and deactivating DXL parse handlers
//
//---------------------------------------------------------------------------
GPOS_RESULT
CParseHandlerManagerTest::EresUnittest()
{
	CUnittest rgut[] =
		{
		GPOS_UNITTEST_FUNC(CParseHandlerManagerTest::EresUnittest_Basic)
		};

	return CUnittest::EresExecute(rgut, GPOS_ARRAY_SIZE(rgut));
}

//---------------------------------------------------------------------------
//	@function:
//		CParseHandlerManagerTest::EresUnittest_Basic
//
//	@doc:
//		Testing activation and deactivation of parse handlers
//
//---------------------------------------------------------------------------
GPOS_RESULT
CParseHandlerManagerTest::EresUnittest_Basic()
{
	// create memory pool
	CAutoMemoryPool amp(CAutoMemoryPool::ElcNone);
	IMemoryPool *memory_pool = amp.Pmp();
		
	// create XML reader and a parse handler manager for it
	CDXLMemoryManager *memory_manager_dxl = GPOS_NEW(memory_pool) CDXLMemoryManager(memory_pool);

	SAX2XMLReader* parser = NULL;
	{
		CAutoTraceFlag atf(EtraceSimulateOOM, false);
		parser = XMLReaderFactory::createXMLReader(memory_manager_dxl);
	}

	CParseHandlerManager *parse_handler_mgr = GPOS_NEW(memory_pool) CParseHandlerManager(memory_manager_dxl, parser);
	
	// create some parse handlers
	CParseHandlerPlan *pphPlan = GPOS_NEW(memory_pool) CParseHandlerPlan(memory_pool, parse_handler_mgr, NULL);
	CParseHandlerHashJoin *pphHJ = GPOS_NEW(memory_pool) CParseHandlerHashJoin(memory_pool, parse_handler_mgr, pphPlan);
	
	parse_handler_mgr->ActivateParseHandler(pphPlan);
	GPOS_ASSERT(pphPlan == parse_handler_mgr->GetCurrentParseHandler());
	GPOS_ASSERT(pphPlan == parser->getContentHandler());

	parse_handler_mgr->ActivateParseHandler(pphHJ);
	GPOS_ASSERT(pphHJ == parse_handler_mgr->GetCurrentParseHandler());
	GPOS_ASSERT(pphHJ == parser->getContentHandler());


	parse_handler_mgr->DeactivateHandler();
	GPOS_ASSERT(pphPlan == parse_handler_mgr->GetCurrentParseHandler());
	GPOS_ASSERT(pphPlan == parser->getContentHandler());
	
	parse_handler_mgr->DeactivateHandler();
	// no more parse handlers
	GPOS_ASSERT(NULL == parse_handler_mgr->GetCurrentParseHandler());
	GPOS_ASSERT(NULL == parser->getContentHandler());

	// cleanup
	GPOS_DELETE(parse_handler_mgr);
	delete parser;
	GPOS_DELETE(memory_manager_dxl);
	GPOS_DELETE(pphPlan);
	GPOS_DELETE(pphHJ);

	return GPOS_OK;
}





// EOF
