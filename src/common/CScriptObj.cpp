#include "../graysvr/graysvr.h"
#ifdef _WIN32
	#include <process.h>
#else
	#include <errno.h>
#endif

///////////////////////////////////////////////////////////
// CTextConsole

CChar *CTextConsole::GetChar() const
{
	ADDTOCALLSTACK("CTextConsole::GetChar");
	if (m_pChar) return m_pChar;

	return const_cast<CChar *>(dynamic_cast<const CChar *>(this));
}

void CTextConsole::SetChar(CChar* pChar)
{
	ADDTOCALLSTACK("CTextConsole::SetChar");
	m_pChar = pChar;
}

int CTextConsole::OnConsoleKey(CGString &sText, TCHAR szChar, bool fEcho)
{
	ADDTOCALLSTACK("CTextConsole::OnConsoleKey");
	// Eventaully we should call OnConsoleCmd
	// RETURN:
	//  0 = dump this connection
	//  1 = keep processing
	//  2 = process this

	if ( sText.GetLength() >= SCRIPT_MAX_LINE_LEN )
	{
	commandtoolong:
		SysMessage("Command too long\n");
		sText.Empty();
		return 0;
	}

	if ( (szChar == '\r') || (szChar == '\n') )
	{
		// Ignore the character if we have no text stored
		if ( !sText.GetLength() )
			return 1;
		if ( fEcho )
			SysMessage("\n");
		return 2;
	}
	else if ( szChar == 9 )			// TAB (auto-completion)
	{
		// Extract up to start of the word
		LPCTSTR pszArgs = sText.GetPtr() + sText.GetLength();
		while ( (pszArgs >= sText.GetPtr()) && (*pszArgs != '.') && (*pszArgs != ' ') && (*pszArgs != '/') && (*pszArgs != '=') )
			--pszArgs;
		++pszArgs;
		size_t inputLen = strlen(pszArgs);

		// Search in the auto-complete list for starting on P, and save coords of first/last match
		CGStringListRec *psFirstMatch = NULL;
		CGStringListRec *psLastMatch = NULL;
		CGStringListRec *psCurMatch = NULL;		// the one that should be set
		for ( psCurMatch = g_AutoComplete.GetHead(); psCurMatch != NULL; psCurMatch = psCurMatch->GetNext() )
		{
			if ( !strnicmp(psCurMatch->GetPtr(), pszArgs, inputLen) )	// matched
			{
				if ( !psFirstMatch )
					psFirstMatch = psLastMatch = psCurMatch;
				else
					psLastMatch = psCurMatch;
			}
			else if ( psLastMatch )
				break;		// if no longer matches - save time by instant quit
		}

		LPCTSTR pszTemp = NULL;
		bool fMatch = false;
		if ( psFirstMatch && (psFirstMatch == psLastMatch) )	// there IS a match and the ONLY
		{
			pszTemp = psFirstMatch->GetPtr() + inputLen;
			fMatch = true;
		}
		else if ( psFirstMatch )		// also make SE (if SERV/SERVER in dic) to become SERV
		{
			pszArgs = pszTemp = psFirstMatch->GetPtr();
			pszTemp += inputLen;
			inputLen = strlen(pszArgs);
			fMatch = true;
			for ( psCurMatch = psFirstMatch->GetNext(); psCurMatch != psLastMatch->GetNext(); psCurMatch = psCurMatch->GetNext() )
			{
				if ( strnicmp(psCurMatch->GetPtr(), pszArgs, inputLen) != 0 )	// mismatched
				{
					fMatch = false;
					break;
				}
			}
		}

		if ( fMatch )
		{
			if ( fEcho )
				SysMessage(pszTemp);

			sText += pszTemp;
			if ( sText.GetLength() > SCRIPT_MAX_LINE_LEN )
				goto commandtoolong;
		}
		return 1;
	}

	if ( fEcho )
	{
		TCHAR szTmp[2];
		szTmp[0] = szChar;
		szTmp[1] = '\0';
		SysMessage(szTmp);
	}

	if ( szChar == 8 )
	{
		if ( sText.GetLength() )	// back key
			sText.SetLength(sText.GetLength() - 1);
		return 1;
	}

	sText += szChar;
	return 1;
}

///////////////////////////////////////////////////////////
// CScriptObj

TRIGRET_TYPE CScriptObj::OnTriggerForLoop(CScript &s, int iType, CTextConsole *pSrc, CScriptTriggerArgs *pArgs, CGString *psResult)
{
	ADDTOCALLSTACK("CScriptObj::OnTriggerForLoop");
	// Loop from start here to the ENDFOR
	// See WebPageScriptList for dealing with arrays

	CScriptLineContext StartContext = s.GetContext();
	CScriptLineContext EndContext = StartContext;
	int iLoopsMade = 0;

	if ( iType & 8 )		// WHILE
	{
		TCHAR *pszCond;
		CGString sOrig;
		TemporaryString pszTemp;
		int i = 0;
		CExpression expr(pArgs, pSrc, this);
		LPCTSTR pszArg = s.GetArgStr();
		if (*(pszArg - 1) == '(')
			pszArg--;

		sOrig.Copy(pszArg);
		for (;;)
		{
			++iLoopsMade;
			if ( g_Cfg.m_iMaxLoopTimes && (iLoopsMade >= g_Cfg.m_iMaxLoopTimes) )
				goto toomanyloops;

			pArgs->m_VarsLocal.SetNum("_WHILE", i++, false);
			strcpy(pszTemp, sOrig.GetPtr());
			pszCond = pszTemp;
			ParseText(pszCond, pSrc, 0, pArgs);
			if ( !expr.GetVal(pszCond) )
				break;

			TRIGRET_TYPE iRet = OnTriggerRun(s, TRIGRUN_SECTION_TRUE, pSrc, pArgs, psResult);
			if ( iRet == TRIGRET_BREAK )
			{
				EndContext = StartContext;
				break;
			}
			if ( (iRet != TRIGRET_ENDIF) && (iRet != TRIGRET_CONTINUE) )
				return iRet;
			if ( iRet == TRIGRET_CONTINUE )
				EndContext = StartContext;
			else
				EndContext = s.GetContext();
			s.SeekContext(StartContext);
		}
	}
	else
		ParseText(s.GetArgStr(), pSrc, 0, pArgs);

	if ( iType & 4 )	// FOR
	{
		int iMin = 0;
		int iMax = 0;
		TCHAR *ppArgs[3];
		size_t iQty = Str_ParseCmds(s.GetArgStr(), ppArgs, COUNTOF(ppArgs), ", ");
		CGString sLoopVar = "_FOR";

		switch ( iQty )
		{
			case 1:		// FOR max
			{
				iMin = 1;
				iMax = Exp_GetSingle(ppArgs[0]);
				break;
			}
			case 2:		// FOR min max, FOR name max
			{
				if ( IsDigit(*ppArgs[0]) || ((*ppArgs[0] == '-') && IsDigit(*(ppArgs[0] + 1))) )
				{
					iMin = Exp_GetSingle(ppArgs[0]);
					iMax = Exp_GetSingle(ppArgs[1]);
				}
				else
				{
					sLoopVar = ppArgs[0];
					iMin = 1;
					iMax = Exp_GetSingle(ppArgs[1]);
				}
				break;
			}
			case 3:		// FOR name min max
			{
				sLoopVar = ppArgs[0];
				iMin = Exp_GetSingle(ppArgs[1]);
				iMax = Exp_GetSingle(ppArgs[2]);
				break;
			}
			default:
			{
				iMin = iMax = 1;
				break;
			}
		}

		if ( iMin > iMax )
		{
			for ( int i = iMin; i >= iMax; --i )
			{
				++iLoopsMade;
				if ( g_Cfg.m_iMaxLoopTimes && (iLoopsMade >= g_Cfg.m_iMaxLoopTimes) )
					goto toomanyloops;

				pArgs->m_VarsLocal.SetNum(sLoopVar, i, false, pArgs, pSrc, this);
				TRIGRET_TYPE iRet = OnTriggerRun(s, TRIGRUN_SECTION_TRUE, pSrc, pArgs, psResult);
				if ( iRet == TRIGRET_BREAK )
				{
					EndContext = StartContext;
					break;
				}
				if ( (iRet != TRIGRET_ENDIF) && (iRet != TRIGRET_CONTINUE) )
					return iRet;
				if ( iRet == TRIGRET_CONTINUE )
					EndContext = StartContext;
				else
					EndContext = s.GetContext();
				s.SeekContext(StartContext);
			}
		}
		else
		{
			for ( int i = iMin; i <= iMax; ++i )
			{
				++iLoopsMade;
				if ( g_Cfg.m_iMaxLoopTimes && (iLoopsMade >= g_Cfg.m_iMaxLoopTimes) )
					goto toomanyloops;

				pArgs->m_VarsLocal.SetNum(sLoopVar, i, false, pArgs, pSrc, this);
				TRIGRET_TYPE iRet = OnTriggerRun(s, TRIGRUN_SECTION_TRUE, pSrc, pArgs, psResult);
				if ( iRet == TRIGRET_BREAK )
				{
					EndContext = StartContext;
					break;
				}
				if ( (iRet != TRIGRET_ENDIF) && (iRet != TRIGRET_CONTINUE) )
					return iRet;
				if ( iRet == TRIGRET_CONTINUE )
					EndContext = StartContext;
				else
					EndContext = s.GetContext();
				s.SeekContext(StartContext);
			}
		}
	}

	if ( (iType & 1) || (iType & 2) )
	{
		CObjBaseTemplate *pObj = dynamic_cast<CObjBaseTemplate *>(this);
		if ( !pObj )
		{
			iType = 0;
			DEBUG_ERR(("FOR Loop trigger on non-world object '%s'\n", GetName()));
		}
		else
		{
			CObjBaseTemplate *pObjTop = pObj->GetTopLevelObj();
			CPointMap pt = pObjTop->GetTopPoint();
			int iDist = s.HasArgs() ? s.GetArgVal() : UO_MAP_VIEW_SIZE;

			if ( iType & 1 )		// FORITEM, FOROBJ
			{
				CWorldSearch AreaItems(pt, iDist);
				for (;;)
				{
					++iLoopsMade;
					if ( g_Cfg.m_iMaxLoopTimes && (iLoopsMade >= g_Cfg.m_iMaxLoopTimes) )
						goto toomanyloops;

					CItem *pItem = AreaItems.GetItem();
					if ( !pItem )
						break;
					TRIGRET_TYPE iRet = pItem->OnTriggerRun(s, TRIGRUN_SECTION_TRUE, pSrc, pArgs, psResult);
					if ( iRet == TRIGRET_BREAK )
					{
						EndContext = StartContext;
						break;
					}
					if ( (iRet != TRIGRET_ENDIF) && (iRet != TRIGRET_CONTINUE) )
						return iRet;
					if ( iRet == TRIGRET_CONTINUE )
						EndContext = StartContext;
					else
						EndContext = s.GetContext();
					s.SeekContext(StartContext);
				}
			}
			if ( iType & 2 )		// FORCHAR, FOROBJ
			{
				CWorldSearch AreaChars(pt, iDist);
				AreaChars.SetAllShow(iType & 0x20);
				for (;;)
				{
					++iLoopsMade;
					if ( g_Cfg.m_iMaxLoopTimes && (iLoopsMade >= g_Cfg.m_iMaxLoopTimes) )
						goto toomanyloops;

					CChar *pChar = AreaChars.GetChar();
					if ( !pChar )
						break;
					if ( (iType & 0x10) && !pChar->m_pClient )	// FORCLIENTS
						continue;
					if ( (iType & 0x20) && !pChar->m_pPlayer )	// FORPLAYERS
						continue;
					TRIGRET_TYPE iRet = pChar->OnTriggerRun(s, TRIGRUN_SECTION_TRUE, pSrc, pArgs, psResult);
					if ( iRet == TRIGRET_BREAK )
					{
						EndContext = StartContext;
						break;
					}
					if ( (iRet != TRIGRET_ENDIF) && (iRet != TRIGRET_CONTINUE) )
						return iRet;
					if ( iRet == TRIGRET_CONTINUE )
						EndContext = StartContext;
					else
						EndContext = s.GetContext();
					s.SeekContext(StartContext);
				}
			}
		}
	}

	if ( iType & 0x40 )		// FORINSTANCES
	{
		RESOURCE_ID rid;
		TCHAR *ppArgs[2];
		if ( Str_ParseCmds(s.GetArgStr(), ppArgs, COUNTOF(ppArgs), " \t,") >= 1 )
			rid = g_Cfg.ResourceGetID(RES_UNKNOWN, const_cast<LPCTSTR &>(static_cast<LPTSTR &>(ppArgs[0])));
		else
		{
			const CObjBase *pObj = dynamic_cast<CObjBase *>(this);
			if ( pObj && pObj->Base_GetDef() )
				rid = pObj->Base_GetDef()->GetResourceID();
		}

		// No need to loop if there is no valid resource id
		if ( rid.IsValidUID() )
		{
			DWORD dwTotalInstances = 0;		// will acquire the correct value for this during the loop
			DWORD dwUID = 0;
			DWORD dwTotal = g_World.GetUIDCount();
			DWORD dwCount = dwTotal - 1;
			DWORD dwFound = 0;

			while ( dwCount-- )
			{
				if ( ++dwUID >= dwTotal )
					break;

				CObjBase *pObj = g_World.FindUID(dwUID);
				if ( !pObj || (pObj->Base_GetDef()->GetResourceID() != rid) )
					continue;

				++iLoopsMade;
				if ( g_Cfg.m_iMaxLoopTimes && (iLoopsMade >= g_Cfg.m_iMaxLoopTimes) )
					goto toomanyloops;

				TRIGRET_TYPE iRet = pObj->OnTriggerRun(s, TRIGRUN_SECTION_TRUE, pSrc, pArgs, psResult);
				if ( iRet == TRIGRET_BREAK )
				{
					EndContext = StartContext;
					break;
				}
				if ( (iRet != TRIGRET_ENDIF) && (iRet != TRIGRET_CONTINUE) )
					return iRet;
				if ( iRet == TRIGRET_CONTINUE )
					EndContext = StartContext;
				else
					EndContext = s.GetContext();
				s.SeekContext(StartContext);

				if ( (dwTotalInstances == 0) && pObj->Base_GetDef() )
					dwTotalInstances = pObj->Base_GetDef()->GetRefInstances();

				++dwFound;
				if ( (dwTotalInstances > 0) && (dwFound >= dwTotalInstances) )
					break;
			}
		}
	}

	if ( iType & 0x100 )	// FORTIMERF
	{
		TCHAR *ppArgs[2];
		if ( Str_ParseCmds(s.GetArgStr(), ppArgs, COUNTOF(ppArgs), " \t,") >= 1 )
		{
			char chFunctionName[1024];
			strncpy(chFunctionName, ppArgs[0], sizeof(chFunctionName) - 1);

			TRIGRET_TYPE iRet = g_World.m_TimedFunctions.Loop(chFunctionName, iLoopsMade, StartContext, s, pSrc, pArgs, psResult);
			if ( (iRet != TRIGRET_ENDIF) && (iRet != TRIGRET_CONTINUE) )
				return iRet;
		}
	}

	if ( g_Cfg.m_iMaxLoopTimes )
	{
	toomanyloops:
		if ( iLoopsMade >= g_Cfg.m_iMaxLoopTimes )
			g_Log.EventError("Terminating loop cycle since it seems being dead-locked (%d iterations already passed)\n", iLoopsMade);
	}

	if ( EndContext.m_lOffset <= StartContext.m_lOffset )
	{
		// Just skip to the end
		TRIGRET_TYPE iRet = OnTriggerRun(s, TRIGRUN_SECTION_FALSE, pSrc, pArgs, psResult);
		if ( iRet != TRIGRET_ENDIF )
			return iRet;
	}
	else
		s.SeekContext(EndContext);

	return TRIGRET_ENDIF;
}

TRIGRET_TYPE CScriptObj::OnTriggerScript(CScript &s, LPCTSTR pszTrigName, CTextConsole *pSrc, CScriptTriggerArgs *pArgs)
{
	ADDTOCALLSTACK("CScriptObj::OnTriggerScript");
	// Look for exact trigger matches
	if ( !OnTriggerFind(s, pszTrigName) )
		return TRIGRET_RET_DEFAULT;

	ProfileTask scriptsTask(PROFILE_SCRIPTS);

	TScriptProfiler::TScriptProfilerTrigger *pTrigger = NULL;
	ULONGLONG llTicksStart, llTicksEnd;

	if ( IsSetEF(EF_Script_Profiler) )
	{
		// Lowercase for speed
		TCHAR *pszName = Str_GetTemp();
		strncpy(pszName, pszTrigName, sizeof(pTrigger->name) - 1);
		pszName[sizeof(pTrigger->name) - 1] = '\0';
		_strlwr(pszName);

		if ( g_profiler.initstate != 0xF1 )		// profiler is not initialized
		{
			memset(&g_profiler, 0, sizeof(g_profiler));
			g_profiler.initstate = static_cast<BYTE>(0xF1);		// ''
		}

		for ( pTrigger = g_profiler.TriggersHead; pTrigger != NULL; pTrigger = pTrigger->next )
		{
			if ( !strcmp(pTrigger->name, pszName) )
				break;
		}
		if ( !pTrigger )
		{
			// First time that the trigger is called, so create its record
			pTrigger = new TScriptProfiler::TScriptProfilerTrigger;
			memset(pTrigger, 0, sizeof(TScriptProfiler::TScriptProfilerTrigger));
			strncpy(pTrigger->name, pszName, sizeof(pTrigger->name) - 1);
			if ( g_profiler.TriggersTail )
				g_profiler.TriggersTail->next = pTrigger;
			else
				g_profiler.TriggersHead = pTrigger;
			g_profiler.TriggersTail = pTrigger;
		}

		++pTrigger->called;
		++g_profiler.called;
		TIME_PROFILE_START;
	}

	TRIGRET_TYPE iRet = OnTriggerRunVal(s, TRIGRUN_SECTION_TRUE, pSrc, pArgs);

	if ( IsSetEF(EF_Script_Profiler) && pTrigger )
	{
		TIME_PROFILE_END;
		llTicksStart = llTicksEnd - llTicksStart;
		pTrigger->total += llTicksStart;
		pTrigger->average = pTrigger->total / pTrigger->called;
		if ( pTrigger->max < llTicksStart )
			pTrigger->max = llTicksStart;
		if ( (pTrigger->min > llTicksStart) || !pTrigger->min )
			pTrigger->min = llTicksStart;
		g_profiler.total += llTicksStart;
	}

	return iRet;
}

bool CScriptObj::OnTriggerFind(CScript &s, LPCTSTR pszTrigName)
{
	ADDTOCALLSTACK("CScriptObj::OnTriggerFind");
	while ( s.ReadKey(false) )
	{
		if ( strnicmp(s.GetKey(), "ON", 2) != 0 )
			continue;

		s.ParseKeyLate();
		if ( strcmpi(s.GetArgRaw(), pszTrigName) == 0 )
			return true;
	}
	return false;
}

enum SK_TYPE
{
	SK_BEGIN,
	SK_BREAK,
	SK_CONTINUE,
	SK_DORAND,
	SK_DOSWITCH,
	SK_ELIF,
	SK_ELSE,
	SK_ELSEIF,
	SK_END,
	SK_ENDDO,
	SK_ENDFOR,
	SK_ENDIF,
	SK_ENDRAND,
	SK_ENDSWITCH,
	SK_ENDWHILE,
	SK_FOR,
	SK_FORCHARLAYER,
	SK_FORCHARMEMORYTYPE,
	SK_FORCHAR,
	SK_FORCLIENTS,
	SK_FORCONT,
	SK_FORCONTID,
	SK_FORCONTTYPE,
	SK_FORINSTANCE,
	SK_FORITEM,
	SK_FOROBJ,
	SK_FORPLAYERS,
	SK_FORTIMERF,
	SK_IF,
	SK_RETURN,
	SK_WHILE,
	SK_QTY
};

LPCTSTR const CScriptObj::sm_szScriptKeys[SK_QTY + 1] =
{
	"BEGIN",
	"BREAK",
	"CONTINUE",
	"DORAND",
	"DOSWITCH",
	"ELIF",
	"ELSE",
	"ELSEIF",
	"END",
	"ENDDO",
	"ENDFOR",
	"ENDIF",
	"ENDRAND",
	"ENDSWITCH",
	"ENDWHILE",
	"FOR",
	"FORCHARLAYER",
	"FORCHARMEMORYTYPE",
	"FORCHARS",
	"FORCLIENTS",
	"FORCONT",
	"FORCONTID",
	"FORCONTTYPE",
	"FORINSTANCES",
	"FORITEMS",
	"FOROBJS",
	"FORPLAYERS",
	"FORTIMERF",
	"IF",
	"RETURN",
	"WHILE",
	NULL
};

TRIGRET_TYPE CScriptObj::OnTriggerRun(CScript &s, TRIGRUN_TYPE trigger, CTextConsole *pSrc, CScriptTriggerArgs *pArgs, CGString *psResult)
{
	ADDTOCALLSTACK("CScriptObj::OnTriggerRun");
	// ARGS:
	//  TRIGRUN_SECTION_SINGLE = just this 1 line
	// RETURN:
	//  TRIGRET_RET_FALSE = return and continue processing
	//  TRIGRET_RET_TRUE = return and handled (halt further processing)
	//  TRIGRET_RET_DEFAULT = if process returns nothing specifically

	//CScriptFileContext set g_Log.m_pObjectContext is the current context (we assume)
	//DEBUGCHECK(this == g_Log.m_pObjectContext);

	// All scripts should have args for locals to work
	CScriptTriggerArgs argsEmpty;
	if ( !pArgs )
		pArgs = &argsEmpty;

	int keyLength = 0;
	TemporaryString tempKey;

	// Script execution is always not threaded action
	EXC_TRY("TriggerRun");

	LPCTSTR pszKey;

	bool fSectionFalse = ((trigger == TRIGRUN_SECTION_FALSE) || (trigger == TRIGRUN_SINGLE_FALSE));
	if ((trigger == TRIGRUN_SECTION_EXEC) || (trigger == TRIGRUN_SINGLE_EXEC))	// header was already read in
	{
		pszKey = s.GetKey();
		goto jump_in;
	}

	EXC_SET("parsing");
	while (s.ReadKey())
	{
		// Hit the end of the next trigger
		if ( s.IsKeyHead("ON", 2) )		// done with this section
			break;
		LPCTSTR peekedKey = s.GetKey();
		GETNONWHITESPACE(peekedKey);
		LPCTSTR peekedKeyWithoutWhiteSpace = peekedKey;
		SKIP_IDENTIFIERSTRING(peekedKey);
		if (*peekedKey == '(')
		{
			keyLength = peekedKey - peekedKeyWithoutWhiteSpace;
		}
		else
		{
			keyLength = 0;
		}

		SK_TYPE index;

		pszKey = s.GetKey();
		GETNONWHITESPACE(pszKey);
		index = static_cast<SK_TYPE>(FindTableHeadSorted(pszKey, sm_szScriptKeys, COUNTOF(sm_szScriptKeys) - 1));
		if (index == SK_RETURN)
		{
			strncpy(tempKey, pszKey, 6);
			tempKey.setAt(7, '\0');
			s.SetArgRaw(const_cast<TCHAR*>(pszKey + 6));
			pszKey = tempKey;
		}
		else if (keyLength > 0)
		{
			switch (index)
			{
				case SK_IF:
				case SK_RETURN:
				case SK_WHILE:
					s.SetArgRaw(const_cast<TCHAR*>(peekedKey));
					strncpy(tempKey, pszKey, keyLength);
					tempKey.setAt(keyLength, '\0');
					pszKey = tempKey;
					break;
				default:
					s.ReadKeyParse(false);
					break;
			}
		}
		else
			s.ReadKeyParse(false);

	jump_in:
		TRIGRET_TYPE iRet = TRIGRET_RET_DEFAULT;

		switch ( index )
		{
			case SK_ENDIF:
			case SK_END:
			case SK_ENDDO:
			case SK_ENDFOR:
			case SK_ENDRAND:
			case SK_ENDSWITCH:
			case SK_ENDWHILE:
				return TRIGRET_ENDIF;
			case SK_ELIF:
			case SK_ELSEIF:
				return TRIGRET_ELSEIF;
			case SK_ELSE:
				return TRIGRET_ELSE;
			default:
				break;
		}

		if ( fSectionFalse )
		{
			// Ignoring this whole section, don't bother parsing it
			switch ( index )
			{
				case SK_IF:
				{
					EXC_SET("if statement");
					do
					{
						iRet = OnTriggerRun(s, TRIGRUN_SECTION_FALSE, pSrc, pArgs, psResult);
					} while ( (iRet == TRIGRET_ELSEIF) || (iRet == TRIGRET_ELSE) );
					break;
				}
				case SK_WHILE:
				case SK_FOR:
				case SK_FORCHARLAYER:
				case SK_FORCHARMEMORYTYPE:
				case SK_FORCHAR:
				case SK_FORCLIENTS:
				case SK_FORCONT:
				case SK_FORCONTID:
				case SK_FORCONTTYPE:
				case SK_FORINSTANCE:
				case SK_FORITEM:
				case SK_FOROBJ:
				case SK_FORPLAYERS:
				case SK_FORTIMERF:
				case SK_DORAND:
				case SK_DOSWITCH:
				case SK_BEGIN:
					EXC_SET("begin/loop cycle");
					iRet = OnTriggerRun(s, TRIGRUN_SECTION_FALSE, pSrc, pArgs, psResult);
					break;
				default:
					break;
			}
			if ( trigger >= TRIGRUN_SINGLE_EXEC )
				return TRIGRET_RET_DEFAULT;
			continue;	// just ignore it
		}

		switch ( index )
		{
			case SK_BREAK:
				return TRIGRET_BREAK;
			case SK_CONTINUE:
				return TRIGRET_CONTINUE;
			case SK_FORITEM:
				EXC_SET("foritem");
				iRet = OnTriggerForLoop(s, 1, pSrc, pArgs, psResult);
				break;
			case SK_FORCHAR:
				EXC_SET("forchar");
				iRet = OnTriggerForLoop(s, 2, pSrc, pArgs, psResult);
				break;
			case SK_FORCLIENTS:
				EXC_SET("forclients");
				iRet = OnTriggerForLoop(s, 0x12, pSrc, pArgs, psResult);
				break;
			case SK_FOROBJ:
				EXC_SET("forobjs");
				iRet = OnTriggerForLoop(s, 3, pSrc, pArgs, psResult);
				break;
			case SK_FORPLAYERS:
				EXC_SET("forplayers");
				iRet = OnTriggerForLoop(s, 0x22, pSrc, pArgs, psResult);
				break;
			case SK_FOR:
				EXC_SET("for");
				iRet = OnTriggerForLoop(s, 4, pSrc, pArgs, psResult);
				break;
			case SK_WHILE:
				EXC_SET("while");
				iRet = OnTriggerForLoop(s, 8, pSrc, pArgs, psResult);
				break;
			case SK_FORINSTANCE:
				EXC_SET("forinstance");
				iRet = OnTriggerForLoop(s, 0x40, pSrc, pArgs, psResult);
				break;
			case SK_FORTIMERF:
				EXC_SET("fortimerf");
				iRet = OnTriggerForLoop(s, 0x100, pSrc, pArgs, psResult);
				break;
			case SK_FORCHARLAYER:
			case SK_FORCHARMEMORYTYPE:
			{
				EXC_SET("forchar[layer/memorytype]");
				if ( !s.HasArgs() )
				{
					DEBUG_ERR(("%s called without arguments\n", sm_szScriptKeys[index]));
					iRet = OnTriggerRun(s, TRIGRUN_SECTION_FALSE, pSrc, pArgs, psResult);
					break;
				}

				CChar *pChar = dynamic_cast<CChar *>(this);
				if ( !pChar )
				{
					DEBUG_ERR(("%s called on non-char object '%s'\n", sm_szScriptKeys[index], GetName()));
					iRet = OnTriggerRun(s, TRIGRUN_SECTION_FALSE, pSrc, pArgs, psResult);
					break;
				}

				ParseText(s.GetArgRaw(), pSrc, 0, pArgs);
				if ( index == SK_FORCHARLAYER )
					iRet = pChar->OnCharTrigForLayerLoop(s, pSrc, pArgs, psResult, static_cast<LAYER_TYPE>(s.GetArgVal()));
				else
					iRet = pChar->OnCharTrigForMemTypeLoop(s, pSrc, pArgs, psResult, static_cast<WORD>(s.GetArgVal()));
				break;
			}
			case SK_FORCONT:
			{
				EXC_SET("forcont");
				if ( !s.HasArgs() )
				{
					DEBUG_ERR(("%s called without arguments\n", sm_szScriptKeys[index]));
					iRet = OnTriggerRun(s, TRIGRUN_SECTION_FALSE, pSrc, pArgs, psResult);
					break;
				}

				TCHAR *ppArgs[2];
				size_t iArgsQty = Str_ParseCmds(const_cast<TCHAR *>(s.GetArgRaw()), ppArgs, COUNTOF(ppArgs), " \t,");

				TemporaryString pszTemp;
				strcpy(pszTemp, ppArgs[0]);
				TCHAR *pszTempPoint = pszTemp;
				ParseText(pszTempPoint, pSrc, 0, pArgs);

				CGrayUID uid = static_cast<CGrayUID>(Exp_GetLLVal(pszTempPoint));
				if ( !uid.IsValidUID() )
				{
					DEBUG_ERR(("%s called on invalid uid '0%" FMTDWORDH "'\n", sm_szScriptKeys[index], uid.GetObjUID()));
					iRet = OnTriggerRun(s, TRIGRUN_SECTION_FALSE, pSrc, pArgs, psResult);
					break;
				}

				CContainer *pCont = dynamic_cast<CContainer *>(uid.ObjFind());
				if ( !pCont )
				{
					DEBUG_ERR(("%s called on non-container uid '0%" FMTDWORDH "'\n", sm_szScriptKeys[index], uid.GetObjUID()));
					iRet = OnTriggerRun(s, TRIGRUN_SECTION_FALSE, pSrc, pArgs, psResult);
					break;
				}

				CScriptLineContext StartContext = s.GetContext();
				CScriptLineContext EndContext = StartContext;
				iRet = pCont->OnGenericContTriggerForLoop(s, pSrc, pArgs, psResult, StartContext, EndContext, (iArgsQty >= 2) ? Exp_GetVal(ppArgs[1]) : 255);
				break;
			}
			case SK_FORCONTID:
			case SK_FORCONTTYPE:
			{
				EXC_SET("forcont[id/type]");
				if ( !s.HasArgs() )
				{
					DEBUG_ERR(("%s called without arguments\n", sm_szScriptKeys[index]));
					iRet = OnTriggerRun(s, TRIGRUN_SECTION_FALSE, pSrc, pArgs, psResult);
					break;
				}

				CContainer *pCont = dynamic_cast<CContainer *>(this);
				if ( !pCont )
				{
					DEBUG_ERR(("%s called on non-container object '%s'\n", sm_szScriptKeys[index], GetName()));
					iRet = OnTriggerRun(s, TRIGRUN_SECTION_FALSE, pSrc, pArgs, psResult);
					break;
				}

				LPCTSTR pszKey = s.GetArgRaw();
				SKIP_SEPARATORS(pszKey);

				TCHAR *ppArgs[2];
				size_t iArgsQty = Str_ParseCmds(const_cast<TCHAR *>(pszKey), ppArgs, COUNTOF(ppArgs), " \t,");

				TemporaryString pszTemp;
				strcpy(pszTemp, ppArgs[0]);
				ParseText(pszTemp, pSrc, 0, pArgs);

				CScriptLineContext StartContext = s.GetContext();
				CScriptLineContext EndContext = StartContext;
				iRet = pCont->OnContTriggerForLoop(s, pSrc, pArgs, psResult, StartContext, EndContext, g_Cfg.ResourceGetID((index == SK_FORCONTID) ? RES_ITEMDEF : RES_TYPEDEF, static_cast<LPCTSTR &>(pszTemp)), 0, (iArgsQty >= 2) ? Exp_GetVal(ppArgs[1]) : 255);
				break;
			}
			default:
			{
				// Parse out any variables in it (may act like a verb sometimes?)
				EXC_SET("parsing");
				if ( strchr(pszKey, '<') )
				{
					EXC_SET("parsing <> in a key");
					TemporaryString pszBuffer;
					strcpy(pszBuffer, pszKey);
					strcat(pszBuffer, " ");
					strcat(pszBuffer, s.GetArgRaw());
					ParseText(pszBuffer, pSrc, 0, pArgs);
					s.ParseKey(pszBuffer);
					pszKey = s.GetKey();
				}
				else
					ParseText(s.GetArgRaw(), pSrc, 0, pArgs);
			}
		}

		if (!strnicmp(pszKey, "RETURN", 6))
		{
			EXC_SET("return");
			if (psResult)
			{
				LPCTSTR pszArgs = s.GetArgStr();
				if (*pszArgs)
				{
					if (IsSimpleNumberString(pszArgs))
					{
						CExpression expr(pArgs, pSrc, this);
						INT64 result = expr.GetVal(pszArgs);
						psResult->FormatLLVal(result);
					}
					else
						psResult->Copy(pszArgs);
					return TRIGRET_RET_TRUE;
				}
			}
			return static_cast<TRIGRET_TYPE>(s.GetArgVal());
		}

		switch ( index )
		{
			case SK_FORITEM:
			case SK_FORCHAR:
			case SK_FORCHARLAYER:
			case SK_FORCHARMEMORYTYPE:
			case SK_FORCLIENTS:
			case SK_FORCONT:
			case SK_FORCONTID:
			case SK_FORCONTTYPE:
			case SK_FOROBJ:
			case SK_FORPLAYERS:
			case SK_FORINSTANCE:
			case SK_FORTIMERF:
			case SK_FOR:
			case SK_WHILE:
			{
				if ( iRet != TRIGRET_ENDIF )
					return iRet;
				break;
			}
			case SK_DORAND:
			case SK_DOSWITCH:
			{
				EXC_SET("dorand/doswitch");
				INT64 iVal = s.GetArgLLVal(pArgs, pSrc, this);
				if ( index == SK_DORAND )
					iVal = Calc_GetRandLLVal(iVal);
				for ( ; ; --iVal )
				{
					iRet = OnTriggerRun(s, (iVal == 0) ? TRIGRUN_SINGLE_TRUE : TRIGRUN_SINGLE_FALSE, pSrc, pArgs, psResult);
					if ( iRet == TRIGRET_RET_DEFAULT )
						continue;
					if ( iRet == TRIGRET_ENDIF )
						break;
					return iRet;
				}
				break;
			}
			case SK_IF:
			{
				EXC_SET("if statement");
				bool fTrigger = (s.GetArgLLVal(pArgs, pSrc, this) != 0);
				bool fBeenTrue = false;
				for (;;)
				{
					iRet = OnTriggerRun(s, fTrigger ? TRIGRUN_SECTION_TRUE : TRIGRUN_SECTION_FALSE, pSrc, pArgs, psResult);
					if ( (iRet < TRIGRET_ENDIF) || (iRet >= TRIGRET_RET_HALFBAKED) )
						return iRet;
					if ( iRet == TRIGRET_ENDIF )
						break;
					fBeenTrue |= fTrigger;
					if ( fBeenTrue )
						fTrigger = false;
					else if ( iRet == TRIGRET_ELSE )
						fTrigger = true;
					else if ( iRet == TRIGRET_ELSEIF )
					{
						ParseText(s.GetArgStr(), pSrc, 0, pArgs);
						fTrigger = (s.GetArgLLVal(pArgs, pSrc, this) != 0);
					}
				}
				break;
			}
			case SK_BEGIN:
			{
				EXC_SET("begin/loop cycle");
				iRet = OnTriggerRun(s, TRIGRUN_SECTION_TRUE, pSrc, pArgs, psResult);
				if ( iRet != TRIGRET_ENDIF )
					return iRet;
				break;
			}
			default:
			{
				EXC_SET("parsing");
				bool isSafe = false;
				CScript *pSafeScript = NULL;
				if (!strnicmp(pszKey, "SAFE", 4))
				{
					pszKey += 4;
					if (*pszKey == '.')
						pszKey++;

					LPCTSTR safeScriptKey;
					LPCTSTR safeArgStr;
					if (!strlen(pszKey))
					{
						safeScriptKey = s.GetArgStr();
						safeArgStr = "";
						pSafeScript = new CScript(safeScriptKey, safeArgStr);
						pSafeScript->ReadKeyParse(false);
					}
					else
					{
						safeScriptKey = pszKey;
						safeArgStr = s.GetArgStr();
						pSafeScript = new CScript(safeScriptKey, safeArgStr);
					}
					isSafe = true;
				}

				CScript* pScript = pSafeScript != NULL ? pSafeScript : &s;
				if ( !pArgs->r_Verb(*pScript, pSrc, this) )
				{
					EXC_SET("verb");
					bool fRes = r_Verb(*pScript, pSrc, pArgs);
					if (!fRes)
						fRes = r_VerbGlobal(*pScript, pSrc, pArgs);

					if (!fRes && !isSafe)
					{
						DEBUG_ERR(("Undefined keyword '%s'\n", pScript->GetKey()));
						DEBUG_MSG(("WARNING: Trigger Bad Verb '%s','%s'\n", pszKey, pScript->GetArgStr()));
					}
				}
				if (pSafeScript != NULL)
					delete pSafeScript;
				break;
			}
		}

		if ( trigger >= TRIGRUN_SINGLE_EXEC )
			return TRIGRET_RET_DEFAULT;
	}
	EXC_CATCH;

	EXC_DEBUG_START;
	g_Log.EventDebug("key '%s' runtype '%d' pargs '%p' ret '%s' [%p]\n", s.GetKey(), trigger, static_cast<void *>(pArgs), psResult ? psResult->GetPtr() : "", static_cast<void *>(pSrc));
	EXC_DEBUG_END;
	return TRIGRET_RET_DEFAULT;
}

TRIGRET_TYPE CScriptObj::OnTriggerRunVal(CScript &s, TRIGRUN_TYPE trigger, CTextConsole *pSrc, CScriptTriggerArgs *pArgs)
{
	// Get the TRIGRET_TYPE that is returned by the script
	// This should be used instead of OnTriggerRun() when pReturn is not used
	ADDTOCALLSTACK("CScriptObj::OnTriggerRunVal");

	CGString sVal;
	static_cast<void>(OnTriggerRun(s, trigger, pSrc, pArgs, &sVal));

	LPCTSTR pszVal = sVal.GetPtr();
	if ( pszVal && *pszVal )
		return static_cast<TRIGRET_TYPE>(Exp_GetLLVal(pszVal));
	return TRIGRET_RET_DEFAULT;
}

size_t CScriptObj::ParseText(TCHAR* pszResponse, CTextConsole* pSrc, int iFlags, CScriptTriggerArgs* pArgs)
{
	bool bEscaped = false;
	return ParseText(pszResponse, pSrc, iFlags, pArgs, bEscaped);
}

size_t CScriptObj::ParseText(TCHAR *pszResponse, CTextConsole *pSrc, int iFlags, CScriptTriggerArgs *pArgs, bool &bEscaped)
{
	ADDTOCALLSTACK("CScriptObj::ParseText");
	// Take in a line of text that may have fields that can be replaced with operators here
	// ARGS:
	//  iFlags = 1=Use HTML %% as delimiters / 2=Allow recusive bracket count
	// NOTE:
	//  HTML will have opening <script language="SPHERE_FILE"> and then closing </script>
	// RETURN:
	//  New length of the string

	LPCTSTR pszKey;	// temporary, set below
	bool fRes;

	static int sm_iReentrant = 0;
	static bool sm_fBrackets = false;	// allowed to span multi lines

	if ( (iFlags & 2) == 0 )
		sm_fBrackets = false;

	size_t iBegin = 0;
	TCHAR chBegin = '<';
	TCHAR chEnd = '>';

	bool fHTML = (iFlags & 1);
	if ( fHTML )
	{
		chBegin = '%';
		chEnd = '%';
	}

	size_t i = 0;
	EXC_TRY("ParseText");
	bool inEscapedMacro = false;
	for ( i = 0; pszResponse[i]; ++i )
	{
		TCHAR ch = pszResponse[i];

		if ( !sm_fBrackets )	// not in brackets
		{
			if ( ch == chBegin )	// found the start
			{
				bool ignore = bEscaped;
				if ((pszResponse[i + 1] == '?'))
				{
					i++;
					inEscapedMacro = true;
					ignore = false;
				}
				else
					inEscapedMacro = false;

				if (!ignore)
				{
					if (!(isalnum(pszResponse[i + 1]) || (pszResponse[i + 1] == '<')))
						continue;

					iBegin = i;
					sm_fBrackets = true;
				}
			}
			continue;
		}

		if ( ch == '<' )	// recursive brackets
		{
			if (!(pszResponse[i + 1] == '?') && (!(isalnum(pszResponse[i + 1]) || (pszResponse[i + 1] == '<'))) )
				continue;

			if ( sm_iReentrant > 32 )
			{
				EXC_SET("reentrant limit");
				ASSERT(sm_iReentrant < 32);
			}
			++sm_iReentrant;
			sm_fBrackets = false;
			bool nestedEscaped = false;
			size_t iLen = ParseText(pszResponse + i, pSrc, 2, pArgs, nestedEscaped);
			if (nestedEscaped)
				bEscaped = true;
			sm_fBrackets = true;
			--sm_iReentrant;
			i += iLen;
			continue;
		}

		if ( ch == chEnd )
		{
			if ((inEscapedMacro || bEscaped) && ch == '>' && *(pszResponse + i - 1) != '?')
				continue;
			if (inEscapedMacro)
				bEscaped = true;

			if (!sm_fBrackets)
				continue;
			sm_fBrackets = false;
			if (inEscapedMacro)
			{
				pszResponse[i - 1] = '\0';
				pszKey = static_cast<LPCTSTR>(pszResponse) + iBegin + 1;
			}
			else
			{
				pszResponse[i] = '\0';
				pszKey = static_cast<LPCTSTR>(pszResponse) + iBegin + 1;
			}
			inEscapedMacro = false;

			EXC_SET("writeval");
			CGString sVal;
			fRes = false;

			bool fSafe = false;
			TemporaryString* safeArguments = NULL;
			if (!strnicmp(pszKey, "safe", 4))
			{
				pszKey += 4;
				if (*pszKey == ' ' || *pszKey == '.')
				{
					pszKey++;
					fSafe = true;
				}
				else if (*pszKey == '(')
				{
					safeArguments = new TemporaryString();
					Str_ParseArgumentList(pszKey, *safeArguments);
					pszKey = *safeArguments;
				}
				else if (*pszKey == '\0')
					fSafe = true;
				else
					pszKey -= 4;
			}

			if (pArgs && pArgs->r_WriteVarVal(pszKey, sVal, pSrc, this))
				fRes = true;
			
			if (!fRes)
			{
				fRes = r_WriteVal(pszKey, sVal, pSrc, pArgs);
				if (!fRes)
				{
					EXC_SET("writeval");
					if (pArgs && pArgs->r_WriteVal(pszKey, sVal, pSrc))
						fRes = true;
				}
			}

			if (!fRes && !fSafe)
			{
				DEBUG_ERR(("Can't resolve <%s>\n", pszKey));
				// Just in case this really is a <= operator?
				pszResponse[i] = chEnd;
			}
			if (safeArguments != NULL)
				delete safeArguments;

			if ( sVal.IsEmpty() && fHTML )
				sVal = "&nbsp";

			EXC_SET("mem shifting");
			size_t iLen = sVal.GetLength();
			if (bEscaped)
			{
				memmove(pszResponse + iBegin + iLen - 1, pszResponse + i + 1, strlen(pszResponse + i + 1) + 1);
				memcpy(pszResponse + iBegin - 1, static_cast<LPCTSTR>(sVal), iLen);
				i = iBegin + iLen - 2;
			}
			else
			{
				memmove(pszResponse + iBegin + iLen, pszResponse + i + 1, strlen(pszResponse + i + 1) + 1);
				memcpy(pszResponse + iBegin, static_cast<LPCTSTR>(sVal), iLen);
				i = iBegin + iLen - 1;
			}
			if ( (iFlags & 2) != 0 )	// just do this one then bail out
				return i;
		}
	}
	EXC_CATCH;

	EXC_DEBUG_START;
	g_Log.EventDebug("response '%s' source addr '0%p' flags '%d' args '%p'\n", pszResponse, static_cast<void *>(pSrc), iFlags, static_cast<void *>(pArgs));
	EXC_DEBUG_END;
	return i;
}

enum SSC_TYPE
{
	#define ADD(a,b) SSC_##a,
	#include "../tables/CScriptObj_functions.tbl"
	#undef ADD
	SSC_QTY
};

LPCTSTR const CScriptObj::sm_szLoadKeys[SSC_QTY + 1] =
{
	#define ADD(a,b) b,
	#include "../tables/CScriptObj_functions.tbl"
	#undef ADD
	NULL
};

enum SSV_TYPE
{
	SSV_NEW,
	SSV_NEWDUPE,
	SSV_NEWITEM,
	SSV_NEWNPC,
	SSV_OBJ,
	SSV_SHOW,
	SSV_QTY
};

LPCTSTR const CScriptObj::sm_szVerbKeys[SSV_QTY + 1] =
{
	"NEW",
	"NEWDUPE",
	"NEWITEM",
	"NEWNPC",
	"OBJ",
	"SHOW",
	NULL
};

static void StringFunction(int iFunc, LPCTSTR pszKey, CGString &sVal)
{
	GETNONWHITESPACE(pszKey);
	if ( *pszKey == '(' )
		++pszKey;

	TCHAR *ppCmd[4];
	size_t iCount = Str_ParseCmds(const_cast<TCHAR *>(pszKey), ppCmd, COUNTOF(ppCmd), ")");
	if ( iCount <= 0 )
	{
		DEBUG_ERR(("Bad string function usage. Missing ')'\n"));
		return;
	}

	switch ( iFunc )
	{
		case SSC_CHR:
			sVal.Format("%c", Exp_GetSingle(ppCmd[0]));
			return;
		case SSC_StrReverse:
			sVal = ppCmd[0];	// strreverse(str) = reverse the string
			sVal.Reverse();
			return;
		case SSC_StrToLower:	// strlower(str) = lower case the string
			sVal = ppCmd[0];
			sVal.MakeLower();
			return;
		case SSC_StrToUpper:	// strupper(str) = upper case the string
			sVal = ppCmd[0];
			sVal.MakeUpper();
			return;
	}
}

bool CScriptObj::r_GetRefNew(LPCTSTR& pszKey, CScriptObj*& pRef, LPCTSTR pszRawArgs, CScriptTriggerArgs* pArgs, CTextConsole* pSrc)
{
	ADDTOCALLSTACK("CScriptObj::r_GetRefNew");
	
	if (!strnicmp(pszKey, "FINDUID", 7))
	{
		pszKey += 7;
		
		if (*pszKey == '\0')
			pszKey = pszRawArgs;
		TemporaryString pszArg;
		if (Str_ParseArgumentList(pszKey, pszArg))
		{
			CExpression expr(pArgs, pSrc, this);
			pRef = static_cast<CGrayUID>(expr.GetVal(pszArg)).ObjFind();
			if (*pszKey == '.')
				pszKey++;
			return true;
		}
	}
	else if (!strnicmp(pszKey, "FINDRES(", 8)) {
		pszKey += 8;
		TCHAR* ppArgs[3];
		ppArgs[0] = const_cast<TCHAR*>(pszKey);
		Str_Parse(ppArgs[0], &(ppArgs[1]), ",");

		LPCTSTR pszArg2Start = ppArgs[1];
		LPCTSTR pszArg2End = pszArg2Start;
		Str_SkipArgumentList(pszArg2End);

		TemporaryString arg2;
		strncpy(arg2, pszArg2Start, pszArg2End - pszArg2Start);
		arg2.setAt(pszArg2End - pszArg2Start - 1, '\0');

		pszKey = pszArg2End;
		if (*pszKey == ')')
			pszKey++;
		if (*pszKey == '.')
			pszKey++;

		CGString resName;
		if (pArgs)
		{
			CExpression expr(pArgs, pSrc, this);
			INT64 resId = expr.GetVal(arg2);
			resName.FormatLLVal(resId);
			pRef = g_Cfg.ResourceGetDefByName(ppArgs[0], resName);
			if (pRef)
				return true;

			if (g_Cfg.r_GetRef(ppArgs[0], resName, pRef))
			{
				if (pRef)
					return true;
			}
		}

		if (g_Cfg.r_GetRef(ppArgs[0], arg2.toBuffer(), pRef))
		{
			return true;
		}

		return false;
	}

	return false;
}

bool CScriptObj::r_GetRef(LPCTSTR &pszKey, CScriptObj *&pRef)
{
	ADDTOCALLSTACK("CScriptObj::r_GetRef");
	// A key name that just links to another object

	if ( !strnicmp(pszKey, "SERV.", 5) )
	{
		pszKey += 5;
		pRef = &g_Serv;
		return true;
	}
	else if ( !strnicmp(pszKey, "UID.", 4) )
	{
		pszKey += 4;
		pRef = static_cast<CGrayUID>(Exp_GetLLVal(pszKey)).ObjFind();
		SKIP_SEPARATORS(pszKey);
		return true;
	}
	else if ( !strnicmp(pszKey, "OBJ.", 4) )
	{
		pszKey += 4;
		pRef = g_World.m_uidObj.ObjFind();
		return true;
	}
	else if (!strnicmp(pszKey, "LASTNEWCHAR", 11))
	{
		pszKey += 11;
		if (*pszKey == '.')
			pszKey++;
		pRef = g_World.m_uidLastNewChar.CharFind();
		return true;
	}
	else if ( !strnicmp(pszKey, "LASTNEW", 7) )
	{
		pszKey += 7;
		if (*pszKey == '.')
			pszKey++;
		pRef = g_World.m_uidNew.ObjFind();
		return true;
	}
	else if ( !strnicmp(pszKey, "I.", 2) )
	{
		pszKey += 2;
		pRef = this;
		return true;
	}
	else if ( IsSetOF(OF_FileCommands) && !strnicmp(pszKey, "FILE.", 5) )
	{
		pszKey += 5;
		pRef = &g_Serv.fhFile;
		return true;
	}
	else if ( !strnicmp(pszKey, "DB.", 3) )
	{
		pszKey += 3;
		pRef = &g_Serv.m_hdb;
		return true;
	}
	else if ( !strnicmp(pszKey, "LDB.", 4) )
	{
		pszKey += 4;
		pRef = &g_Serv.m_hldb;
		return true;
	}
	return false;
}

bool CScriptObj::r_LoadVal(CScript &s, CScriptTriggerArgs* pArgs, CTextConsole* pSrc)
{
	ADDTOCALLSTACK("CScriptObj::r_LoadVal");
	EXC_TRY("LoadVal");

	LPCTSTR pszKey = s.GetKey();
	if ( !strnicmp(pszKey, "CLEARVARS", 9) )
	{
		pszKey = s.GetArgStr();
		SKIP_SEPARATORS(pszKey);
		g_Exp.m_VarGlobals.ClearKeys(pszKey);
		return true;
	}

	int index = FindTableHeadSorted(pszKey, sm_szLoadKeys, COUNTOF(sm_szLoadKeys) - 1);
	if ( index < 0 )
		return false;

	switch ( index )
	{
		case SSC_VAR0:
		{
			bool fQuoted = false;
			g_Exp.m_VarGlobals.SetStr(pszKey + 5, fQuoted, s.GetArgStr(&fQuoted), true, pArgs, pSrc, this);
			return true;
		}
		case SSC_LIST:
		{
			if ( !g_Exp.m_ListGlobals.r_LoadVal(pszKey + 5, s) )
				DEBUG_ERR(("%s: unknown command '%s'\n", sm_szLoadKeys[index], pszKey));
			return true;
		}
		case SSC_DEFMSG:
		{
			pszKey += 7;
			for ( size_t i = 0; i < DEFMSG_QTY; ++i )
			{
				if ( !strcmpi(pszKey, g_Exp.sm_szMsgNames[i]) )
				{
					bool fQuoted = false;
					TCHAR *pszArgs = s.GetArgStr(&fQuoted);
					strncpy(g_Exp.sm_szMessages[i], pszArgs, EXPRESSION_MAX_KEY_LEN - 1);
					return true;
				}
			}
			g_Log.Event(LOGL_ERROR, "Unknown message '%s'\n", pszKey);
			return false;
		}
	}
	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_SCRIPT;
	EXC_DEBUG_END;
	return false;
}

bool CScriptObj::r_Load(CScript &s)
{
	ADDTOCALLSTACK("CScriptObj::r_Load");
	while ( s.ReadKeyParse() )
	{
		if ( s.IsKeyHead("ON", 2) )		// trigger scripting marks the end
			break;
		r_LoadVal(s, NULL, NULL);
	}
	return true;
}

bool CScriptObj::r_WriteValChained(LPCTSTR pszKey, CGString& sVal, CTextConsole* pSrc, CScriptTriggerArgs* pArgs)
{
	if (*pszKey != '.')
		return true;
	pszKey++;
	
	LPCTSTR pszId = sVal;
	if (*pszId != '#')
		return false;
	pszId++;

	CExpression expr(pArgs, pSrc, this);
	CScriptObj* pRef = static_cast<CGrayUID>(expr.GetVal(pszId)).ObjFind();
	if (pRef)
	{
		if (*pszKey == '.')
			pszKey++;
		sVal.Empty();
		return pRef->r_WriteVal(pszKey, sVal, pSrc, pArgs);
	}

	return true;
}

bool CScriptObj::r_WriteVal(LPCTSTR pszKey, CGString &sVal, CTextConsole *pSrc, CScriptTriggerArgs *pArgs)
{
	ADDTOCALLSTACK("CScriptObj::r_WriteVal");
	EXC_TRY("WriteVal");
	CObjBase *pObj;
	CScriptObj *pRef = NULL;
	bool fGetRef = r_GetRef(pszKey, pRef);
	LPCTSTR pszOriginalKey = pszKey;

	if ( !strnicmp(pszKey, "GetRefType", 10) )
	{
		CScriptObj *pRefTemp = pRef ? pRef : pSrc->GetChar();
		if ( pRefTemp == &g_Serv )
			sVal.FormatHex(0x1);
		else if ( pRefTemp == &g_Serv.fhFile )
			sVal.FormatHex(0x2);
		else if ( (pRefTemp == &g_Serv.m_hdb) || dynamic_cast<CDataBase *>(pRefTemp) )
			sVal.FormatHex(0x8);
		else if ( dynamic_cast<CResourceDef *>(pRefTemp) )
			sVal.FormatHex(0x10);
		else if ( dynamic_cast<CResourceBase *>(pRefTemp) )
			sVal.FormatHex(0x20);
		else if ( dynamic_cast<CScriptTriggerArgs *>(pRefTemp) )
			sVal.FormatHex(0x40);
		else if ( dynamic_cast<CFileObj *>(pRefTemp) )
			sVal.FormatHex(0x80);
		else if ( dynamic_cast<CFileObjContainer *>(pRefTemp) )
			sVal.FormatHex(0x100);
		else if ( dynamic_cast<CAccount *>(pRefTemp) )
			sVal.FormatHex(0x200);
		else if ( (pRefTemp == &g_Serv.m_hldb) || dynamic_cast<CSQLite *>(pRefTemp) )	//else if ( dynamic_cast<CPartyDef *>(pRefTemp) )
			sVal.FormatHex(0x400);
		else if ( dynamic_cast<CStoneMember *>(pRefTemp) )
			sVal.FormatHex(0x800);
		else if ( dynamic_cast<CServerDef *>(pRefTemp) )
			sVal.FormatHex(0x1000);
		else if ( dynamic_cast<CSector *>(pRefTemp) )
			sVal.FormatHex(0x2000);
		else if ( dynamic_cast<CWorld *>(pRefTemp) )
			sVal.FormatHex(0x4000);
		else if ( dynamic_cast<CGMPage *>(pRefTemp) )
			sVal.FormatHex(0x8000);
		else if ( dynamic_cast<CClient *>(pRefTemp) )
			sVal.FormatHex(0x10000);
		else if ( (pObj = dynamic_cast<CObjBase *>(pRefTemp)) != NULL )
		{
			if ( dynamic_cast<CChar *>(pObj) )
				sVal.FormatHex(0x40000);
			else if ( dynamic_cast<CItem *>(pObj) )
				sVal.FormatHex(0x80000);
			else
				sVal.FormatHex(0x20000);
		}
		else
			sVal.FormatHex(0x1);
		return true;
	}

	if ( !fGetRef )
		fGetRef = r_GetRefNew(pszKey, pRef, NULL, pArgs, pSrc);

	if ( fGetRef )
	{
		if ( !pRef )	// good command but bad link
		{
			sVal.Empty();
			return true;
		}
		if ( pszKey[0] == '\0' )	// just testing the ref
		{
			pObj = dynamic_cast<CObjBase *>(pRef);
			if (pObj)
				sVal.FormatUid(static_cast<DWORD>(pObj->GetUID()));
			else
			{
				CRegionBase* pRegion = dynamic_cast<CRegionBase*>(pRef);
				if (pRegion)
					sVal.FormatUid(static_cast<DWORD>(pRegion->GetResourceID()));
				else
				{
					CResourceDef* pResDef = dynamic_cast<CResourceDef*>(pRef);
					if (pResDef)
						sVal.Format("%s", pResDef->GetResourceName());
					else
						sVal.FormatUid(1);
				}
			}

			return true;
		}
		return pRef->r_WriteVal(pszKey, sVal, pSrc, pArgs);
	}

	int index = FindTableHeadSorted(pszKey, sm_szLoadKeys, COUNTOF(sm_szLoadKeys) - 1);
	if ( index < 0 )
	{
		TemporaryString varName;
		if (Str_ParseVariableName(pszKey, varName))
		{
			CVarDefCont* pVar = g_Exp.m_VarGlobals.GetKey(varName, pArgs, pSrc, this);
			if (pVar)
			{
				sVal = pVar->GetValStr();
				return r_WriteValChained(pszKey, sVal, pSrc, pArgs);
			}
			CVarDefCont* pDef = g_Exp.m_VarDefs.GetKey(varName, pArgs, pSrc, this);
			if (pDef)
			{
				if (*pszKey == '.')
				{
					INT64 num = pDef->GetValNum();
					if (num > 0)
					{
						RES_TYPE resType = static_cast<RES_TYPE>(RES_GET_TYPE(num));
						RESOURCE_ID resId(resType, RES_GET_INDEX(num));
						switch (resType)
						{
							case RES_ITEMDEF:
							{
								CItemBase* itemBase = CItemBase::FindItemBase(static_cast<ITEMID_TYPE>(RES_GET_INDEX(num)));
								if (itemBase)
								{
									pszKey++;
									return itemBase->r_WriteVal(pszKey, sVal, pSrc, pArgs);
								}
								break;
							}
							case RES_CHARDEF:
							{
								CCharBase* charBase = CCharBase::FindCharBase(static_cast<CREID_TYPE>(RES_GET_INDEX(num)));
								if (charBase)
								{
									pszKey++;
									return charBase->r_WriteVal(pszKey, sVal, pSrc, pArgs);
								}
								break;
							}
							case RES_SKILL:
							{
								CResourceDef* pDef = g_Cfg.ResourceGetDef(resId);
								if (pDef)
								{
									CSkillDef* pSkillDef = g_Cfg.GetSkillDef(static_cast<SKILL_TYPE>(resId.GetResIndex()));
									if (pSkillDef)
									{
										pszKey++;
										return pSkillDef->r_WriteVal(pszKey, sVal, pSrc, pArgs);
									}
								}
								break;
							}
							case RES_SPELL:
							{
								CResourceDef* pDef = g_Cfg.ResourceGetDef(resId);
								if (pDef)
								{
									CSpellDef* pSpellDef = g_Cfg.GetSpellDef(static_cast<SPELL_TYPE>(resId.GetResIndex()));
									if (pSpellDef)
									{
										pszKey++;
										return pSpellDef->r_WriteVal(pszKey, sVal, pSrc, pArgs);
									}
								}
								break;
							}
						}
					}
				}
				sVal = pDef->GetValStr();
				return r_WriteValChained(pszKey, sVal, pSrc, pArgs);
			}
		}
		pszKey = pszOriginalKey;

		if ( (*pszKey == 'd') || (*pszKey == 'D') )
		{
			// <dSOMEVAL> same as <eval <SOMEVAL>> to get dec from the val
			LPCTSTR pszArg = pszKey + 1;
			if ( r_WriteVal(pszArg, sVal, pSrc, pArgs) )
			{
				if ( *sVal != '-' )
					sVal.FormatLLVal(ahextoi64(sVal));
				return true;
			}
		}
		else if ( (*pszKey == 'r') || (*pszKey == 'R') )
		{
			// <R>, <R15>, <R3,15> are shortcuts to rand(), rand(15) and rand(3,15)
			pszKey += 1;
			if ( *pszKey && ((*pszKey < '0') || (*pszKey > '9')) && (*pszKey != '-') )
				goto badcmd;

			INT64 iMin = 1000, iMax = LLONG_MIN;

			if ( *pszKey )
			{
				iMin = Exp_GetLLVal(pszKey);
				SKIP_ARGSEP(pszKey);
			}
			if ( *pszKey )
			{
				iMax = Exp_GetLLVal(pszKey);
				SKIP_ARGSEP(pszKey);
			}

			if ( iMax == LLONG_MIN )
			{
				iMax = iMin - 1;
				iMin = 0;
			}

			if ( iMin >= iMax )
				sVal.FormatLLVal(iMin);
			else
				sVal.FormatLLVal(Calc_GetRandLLVal(iMin, iMax));
			return true;
		}
	badcmd:
		return false;
	}

	pszKey += strlen(sm_szLoadKeys[index]);
	GETNONWHITESPACE(pszKey);

	switch (index)
	{
		case SSC_EVAL:
		{
			CExpression expr(pArgs, pSrc, this);
			sVal.FormatLLVal(expr.GetVal(pszKey));
			return true;
		}
		case SSC_HVAL:
		{
			CExpression expr(pArgs, pSrc, this);
			sVal.FormatULLLowerHex(expr.GetVal(pszKey));
			return true;
		}
	}

	SKIP_SEPARATORS(pszKey);
	bool fZero = false;

	switch ( index )
	{
		case SSC_BETWEEN:
		case SSC_BETWEEN2:
		{
			INT64 iMin = Exp_GetLLVal(pszKey);
			SKIP_ARGSEP(pszKey);
			INT64 iMax = Exp_GetLLVal(pszKey);
			SKIP_ARGSEP(pszKey);
			INT64 iCurrent = Exp_GetLLVal(pszKey);
			SKIP_ARGSEP(pszKey);
			INT64 iAbsMax = Exp_GetLLVal(pszKey);
			SKIP_ARGSEP(pszKey);
			if ( index == SSC_BETWEEN2 )
				iCurrent = iAbsMax - iCurrent;

			if ( (iMin >= iMax) || (iAbsMax <= 0) || (iCurrent <= 0) )
				sVal.FormatLLVal(iMin);
			else if ( iCurrent >= iAbsMax )
				sVal.FormatLLVal(iMax);
			else
				sVal.FormatLLVal((iCurrent * (iMax - iMin)) / iAbsMax + iMin);
			break;
		}
		case SSC_LISTCOL:
			sVal = (CWebPageDef::sm_iListIndex & 1) ? "bgcolor=\"#E8E8E8\"" : "";	// alternating color
			return true;
		case SSC_OBJ:
			if ( !g_World.m_uidObj.ObjFind() )
				g_World.m_uidObj = static_cast<CGrayUID>(UID_CLEAR);
			sVal.FormatHex(static_cast<DWORD>(g_World.m_uidObj));
			return true;
		case SSC_NEW:
			if ( !g_World.m_uidNew.ObjFind() )
				g_World.m_uidNew = static_cast<CGrayUID>(UID_CLEAR);
			sVal.FormatHex(static_cast<DWORD>(g_World.m_uidNew));
			return true;
		case SSC_SRC:
		{
			if ( !pSrc )
				pRef = NULL;
			else
			{
				pRef = pSrc->GetChar();
				if ( !pRef )
					pRef = dynamic_cast<CScriptObj *>(pSrc);
			}
			if ( !pRef )
			{
				sVal.FormatHex(UID_CLEAR);
				return true;
			}
			if ( !*pszKey )
			{
				pObj = dynamic_cast<CObjBase *>(pRef);
				sVal.FormatUid(pObj ? static_cast<DWORD>(pObj->GetUID()) : UID_CLEAR);
				return true;
			}
			return pRef->r_WriteVal(pszKey, sVal, pSrc, pArgs);
		}
		case SSC_VAR0:
			fZero = true;
			// fall through
		case SSC_VAR:
		{
			TemporaryString varName;
			if (Str_ParseArgumentStart(pszKey, false))
			{
				if (Str_ParseVariableName(pszKey, varName))
				{
					if (Str_ParseArgumentEnd(pszKey, false))
					{
						CVarDefCont* pVar = g_Exp.m_VarGlobals.GetKey(varName, pArgs, pSrc, this);
						if (pVar)
							sVal = pVar->GetValStr();
						else if (fZero)
							sVal.FormatVal(0);
						if (sVal.IsEmpty())
							return true;
						return r_WriteValChained(pszKey, sVal, pSrc, pArgs);
					}
				}
			}
			pszKey = pszOriginalKey;
		}
		case SSC_DEFLIST:
			g_Exp.m_ListInternals.r_Write(pSrc, pszKey, sVal);
			return true;
		case SSC_LIST:
			g_Exp.m_ListGlobals.r_Write(pSrc, pszKey, sVal);
			return true;
		case SSC_DEF0:
			fZero = true;
		case SSC_DEF:
		{
			CVarDefCont *pVar = g_Exp.m_VarDefs.GetKey(pszKey, pArgs, pSrc, this);
			if ( pVar )
				sVal = pVar->GetValStr();
			else if ( fZero )
				sVal.FormatVal(0);
			return true;
		}
		case SSC_DEFMSG:
			sVal = g_Cfg.GetDefaultMsg(pszKey);
			return true;
		case SSC_UVAL:
			sVal.FormatULLVal(static_cast<unsigned long long>(Exp_GetLLVal(pszKey)));
			return true;
		case SSC_FVAL:
		{
			INT64 iVal = Exp_GetLLVal(pszKey);
			sVal.Format("%lld.%lld", iVal / 10, llabs(iVal % 10));
			return true;
		}
		case SSC_FEVAL:		// Float EVAL
			sVal.FormatVal(ATOI(pszKey));
			break;
		case SSC_FHVAL:		// Float HVAL
			sVal.FormatHex(ATOI(pszKey));
			break;
		case SSC_FLOATVAL:	// Float math
			sVal = CVarFloat::FloatMath(pszKey);
			break;
		case SSC_QVAL:
		{
			TCHAR * ppArgs[3];
			pszKey = Str_TrimEnd(const_cast<TCHAR*>(pszKey), ")");
			Str_ParseExpressionArgument(const_cast<TCHAR*>(pszKey), ppArgs, ",");

			CExpression expr(pArgs, pSrc, this);
			INT64 conditionValue = expr.GetVal(pszKey);

			Str_Parse(ppArgs[0], &(ppArgs[1]), ",");

			if (conditionValue)
				sVal = Str_TrimDoublequotes(ppArgs[0]);
			else
				sVal = Str_TrimDoublequotes(ppArgs[1]);
			return true;
		}
		case SSC_ISBIT:
		case SSC_SETBIT:
		case SSC_CLRBIT:
		{
			GETNONWHITESPACE(pszKey);
			if ( !IsDigit(pszKey[0]) )
				return false;

			UINT32 iVal = Exp_GetLLVal(pszKey);
			SKIP_ARGSEP(pszKey);
			UINT32 iBit = Exp_GetLLVal(pszKey);
			if ( iBit < 0 )
			{
				g_Log.EventWarn("%s(%lld,%lld): Can't shift bit by negative position\n", sm_szLoadKeys[index], iVal, iBit);
				iBit = 0;
			}

			if ( index == SSC_ISBIT )
				sVal.FormatLLVal((iVal & (static_cast<UINT32>(1) << iBit)) != 0);
			else if ( index == SSC_SETBIT )
				sVal.FormatLLVal(iVal | (static_cast<UINT32>(1) << iBit));
			else
				sVal.FormatLLVal(iVal & ~(static_cast<UINT32>(1) << iBit));
			break;
		}
		case SSC_ISEMPTY:
			sVal.FormatVal(IsStrEmpty(pszKey));
			return true;
		case SSC_ISNUM:
		{
			GETNONWHITESPACE(pszKey);
			if ( *pszKey == '-' )
				++pszKey;
			sVal.FormatVal(IsStrNumeric(pszKey));
			return true;
		}
		case SSC_StrPos:
		{
			TCHAR *ppArgs[3];
			size_t iQty = Str_ParseCmds(const_cast<TCHAR *>(pszKey), ppArgs, COUNTOF(ppArgs));
			if ( iQty < 3 )
				return false;

			INT64 iPos = Exp_GetLLVal(ppArgs[0]);
			INT64 iLen = strlen(ppArgs[2]);
			if ( iPos < 0 )
				iPos = maximum(0, iPos + iLen);
			else if ( iPos > iLen )
				iPos = iLen;

			TCHAR *pszPos = const_cast<TCHAR *>(strstr(ppArgs[2] + iPos, ppArgs[1]));

#ifdef _DEBUG
			if ( g_Cfg.m_wDebugFlags & DEBUGF_SCRIPTS )
				g_Log.EventDebug("SCRIPT: %s(%lld,'%s','%s') -> '%ld'\n", sm_szLoadKeys[index], iPos, ppArgs[1], ppArgs[2], pszPos ? static_cast<long>(pszPos - ppArgs[2]) : -1);
#endif

			sVal.FormatVal(pszPos ? static_cast<long>(pszPos - ppArgs[2]) : -1);
			return true;
		}
		case SSC_StrSub:
		{
			TCHAR *ppArgs[3];
			size_t iQty = Str_ParseCmds(const_cast<TCHAR *>(pszKey), ppArgs, COUNTOF(ppArgs));
			if ( iQty < 3 )
				return false;

			INT64 iPos = Exp_GetLLVal(ppArgs[0]);
			INT64 iLen = strlen(ppArgs[2]);
			if ( iPos < 0 )
				iPos = maximum(0, iPos + iLen);
			else if ( iPos > iLen )
				iPos = iLen;

			INT64 iCnt = Exp_GetLLVal(ppArgs[1]);
			if ( (iCnt <= 0) || (iPos + iCnt > iLen) )
				iCnt = iLen - iPos;

			TCHAR *pszBuffer = Str_GetTemp();
			strncpy(pszBuffer, ppArgs[2] + iPos, static_cast<size_t>(iCnt));
			pszBuffer[iCnt] = '\0';

#ifdef _DEBUG
			if ( g_Cfg.m_wDebugFlags & DEBUGF_SCRIPTS )
				g_Log.EventDebug("SCRIPT: %s(%lld,%lld,'%s') -> '%s'\n", sm_szLoadKeys[index], iPos, iCnt, ppArgs[2], pszBuffer);
#endif

			sVal = pszBuffer;
			return true;
		}
		case SSC_StrArg:
		{
			TCHAR *pszBuffer = Str_GetTemp();
			GETNONWHITESPACE(pszKey);
			if ( *pszKey == '"' )
				++pszKey;

			size_t iLen = 0;
			while ( *pszKey && !IsSpace(*pszKey) && (*pszKey != ',') )
			{
				pszBuffer[iLen] = *pszKey;
				++pszKey;
				++iLen;
			}
			pszBuffer[iLen] = '\0';
			sVal = pszBuffer;
			return true;
		}
		case SSC_StrCmp:
		{
			int iResult = 0;
			TCHAR* ppArgs[2];
			size_t iCount = Str_ParseCmds(const_cast<TCHAR*>(pszKey), ppArgs, COUNTOF(ppArgs), ",");
			if (iCount < 2)
				iResult = 1;
			else
			{
				if (*ppArgs[0] == '"')
				{
					ppArgs[0]++;
					ppArgs[0] = Str_TrimEnd(ppArgs[0], "\"");
				}
				ppArgs[1] = Str_TrimEnd(ppArgs[1], ")");
				if (*ppArgs[1] == '"')
				{
					ppArgs[1]++;
					ppArgs[1] = Str_TrimEnd(ppArgs[1], "\"");
				}
				iResult = strcmp(ppArgs[0], ppArgs[1]);
			}
			sVal.FormatLLVal(iResult);
			return true;
		}
		case SSC_StrCmpI:
		{
			int iResult = 0;
			TCHAR* ppArgs[2];
			size_t iCount = Str_ParseCmds(const_cast<TCHAR*>(pszKey), ppArgs, COUNTOF(ppArgs), ",");
			if (iCount < 2)
				iResult = 1;
			else
			{
				if (*ppArgs[0] == '"')
				{
					ppArgs[0]++;
					ppArgs[0] = Str_TrimEnd(ppArgs[0], "\"");
				}
				ppArgs[1] = Str_TrimEnd(ppArgs[1], ")");
				if (*ppArgs[1] == '"')
				{
					ppArgs[1]++;
					ppArgs[1] = Str_TrimEnd(ppArgs[1], "\"");
				}
				iResult = strcmpi(ppArgs[0], ppArgs[1]);
			}
			sVal.FormatLLVal(iResult);
			return true;
		}
		case SSC_StrEat:
		{
			GETNONWHITESPACE(pszKey);
			while ( *pszKey && !IsSpace(*pszKey) && (*pszKey != ',') )
				++pszKey;

			SKIP_ARGSEP(pszKey);
			sVal = pszKey;
			return true;
		}
		case SSC_StrLen:
			if (*pszKey)
			{
				if (*pszKey == '"')
					pszKey++;

				pszKey = Str_TrimEnd(const_cast<TCHAR*>(pszKey), ")");
				pszKey = Str_TrimEnd(const_cast<TCHAR*>(pszKey), "\"");
				sVal.FormatLLVal(strlen(pszKey));
			}
			else
				sVal = "0";
			return true;
		case SSC_StrTrim:
			sVal = *pszKey ? Str_TrimWhitespace(const_cast<TCHAR *>(pszKey)) : "";
			return true;
		case SSC_ASC:
		{
			REMOVE_QUOTES(pszKey);
			sVal.FormatULLHex(*pszKey);

			TCHAR *pszBuffer = Str_GetTemp();
			strncpy(pszBuffer, sVal, SCRIPT_MAX_LINE_LEN - 1);

			while ( *(++pszKey) )
			{
				if ( *pszKey == '"' )
					break;
				sVal.FormatULLHex(*pszKey);
				strcat(pszBuffer, " ");
				strncat(pszBuffer, sVal, SCRIPT_MAX_LINE_LEN - 1);
			}
			sVal = pszBuffer;
			return true;
		}
		case SSC_ASCPAD:
		{
			TCHAR *ppArgs[2];
			size_t iQty = Str_ParseCmds(const_cast<TCHAR *>(pszKey), ppArgs, COUNTOF(ppArgs));
			if ( iQty < 2 )
				return false;

			INT64 iPad = Exp_GetLLVal(ppArgs[0]);
			if ( iPad < 0 )
				return false;

			REMOVE_QUOTES(ppArgs[1]);
			sVal.FormatULLHex(*ppArgs[1]);

			TCHAR *pszBuffer = Str_GetTemp();
			strncpy(pszBuffer, sVal, SCRIPT_MAX_LINE_LEN - 1);

			while ( --iPad )
			{
				if ( *ppArgs[1] == '"' )
					continue;
				if ( *ppArgs[1] )
				{
					++ppArgs[1];
					sVal.FormatULLHex(*ppArgs[1]);
				}
				else
					sVal.FormatULLHex('\0');

				strcat(pszBuffer, " ");
				strncat(pszBuffer, sVal, SCRIPT_MAX_LINE_LEN - 1);
			}
			sVal = pszBuffer;
			return true;
		}
		case SSC_SYSCMD:
		case SSC_SYSSPAWN:
		{
			if ( !IsSetOF(OF_FileCommands) )
				return false;

			GETNONWHITESPACE(pszKey);
			TCHAR *pszBuffer = Str_GetTemp();
			strncpy(pszBuffer, pszKey, SCRIPT_MAX_LINE_LEN - 1);

			TCHAR *ppCmd[10];	// limit to 10 arguments
			size_t iQty = Str_ParseCmds(pszBuffer, ppCmd, COUNTOF(ppCmd));
			if ( iQty < 1 )
				return false;

			bool fWait = (index == SSC_SYSCMD);
#ifdef _WIN32
			_spawnl(fWait ? _P_WAIT : _P_NOWAIT, ppCmd[0], ppCmd[0], ppCmd[1], ppCmd[2], ppCmd[3], ppCmd[4], ppCmd[5], ppCmd[6], ppCmd[7], ppCmd[8], ppCmd[9], NULL);
#else

			// I think fork will cause problems.. we'll see.. if yes new thread + execlp is required.
			pid_t child_pid = vfork();
			if ( child_pid < 0 )
			{
				g_Log.EventError("%s failed when executing '%s'\n", sm_szLoadKeys[index], pszKey);
				return false;
			}
			else if ( child_pid == 0 )
			{
				// Don't touch this :P
				execlp(ppCmd[0], ppCmd[0], ppCmd[1], ppCmd[2], ppCmd[3], ppCmd[4], ppCmd[5], ppCmd[6], ppCmd[7], ppCmd[8], ppCmd[9], NULL);

				g_Log.EventError("%s failed with error %d (\"%s\") when executing '%s'\n", sm_szLoadKeys[index], errno, strerror(errno), pszKey);
				raise(SIGKILL);
				g_Log.EventError("%s failed to handle error. Server is UNSTABLE\n", sm_szLoadKeys[index]);

				while ( true )
				{
					// Do NOT leave until the process receives SIGKILL, otherwise it will free up resources it inherited
					// from the main process, which will mess everything up. Normally this point should never be reached
				}
			}
			else if ( fWait )	// parent process here (do we have to wait?)
			{
				int status;
				do
				{
					if ( waitpid(child_pid, &status, 0) )
						break;
				} while ( !WIFSIGNALED(status) && !WIFEXITED(status) );
				sVal.FormatULLHex(WEXITSTATUS(status));
			}
#endif
			g_Log.EventDebug("%s: process execution finished\n", sm_szLoadKeys[index]);
			return true;
		}
		case SSC_StrMid:
		{
			TCHAR* ppArgs[3];
			ppArgs[0] = const_cast<TCHAR*>(pszKey);
			if (!Str_Parse(ppArgs[0], &(ppArgs[1]), ","))
				return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]), ","))
				return false;
			TCHAR* pStr = Str_TrimDoublequotes(ppArgs[0]);
			CExpression expr(pArgs, pSrc, this);
			int startIndex = expr.GetVal(ppArgs[1]);
			if (startIndex < 0)
				return true;

			int subLength = expr.GetVal(ppArgs[2]);
			int strLength = strlen(pStr);
			if (startIndex + subLength > strLength)
				subLength = strLength - startIndex;
			if (subLength < 0)
				return true;

			sVal = (pStr + startIndex);
			sVal.SetAt(subLength, '\0');

			return true;
		}
		case SSC_StrFirstCap:
		{
			TCHAR* ppArgs[1];
			pszKey = Str_TrimEnd(const_cast<TCHAR*>(pszKey), ")");
			size_t iQty = Str_ParseCmds(const_cast<TCHAR*>(pszKey), ppArgs, COUNTOF(ppArgs));
			if (iQty < 1)
				return false;
			TCHAR* pArg = Str_TrimDoublequotes(ppArgs[0]);
			*pArg = toupper(*pArg);
			sVal = pArg;
			return true;
		}
		case SSC_StrGetAscii:
		{
			TCHAR* ppArgs[2];
			pszKey = Str_TrimEnd(const_cast<TCHAR*>(pszKey), ")");
			size_t iQty = Str_ParseCmds(const_cast<TCHAR*>(pszKey), ppArgs, COUNTOF(ppArgs));
			if (iQty < 2)
				return false;
			TCHAR* pStr = Str_TrimDoublequotes(ppArgs[0]);
			CExpression expr(pArgs, pSrc, this);
			int index = expr.GetVal(ppArgs[1]);

			if (index < 0 || index > strlen(pStr))
			{
				sVal.FormatLLVal(0);
				return true;
			}

			sVal.FormatLLVal(*(pStr + index));
			return true;
		}
		case SSC_StrGetTok:
		{
			TCHAR* ppArgs[3];
			ppArgs[0] = const_cast<TCHAR*>(pszKey);
			pszKey = Str_TrimEnd(ppArgs[0], ")");
			if (!Str_Parse(ppArgs[0], &(ppArgs[1]), ","))
				return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]), ","))
				return false;
			
			TCHAR* pStr = Str_TrimDoublequotes(ppArgs[0]);
			CExpression expr(pArgs, pSrc, this);
			int requestedIndex = expr.GetVal(ppArgs[1]);
			TCHAR* pDelimiter = Str_TrimDoublequotes(ppArgs[2]);

			TCHAR* pNext;
			int currentIndex = 0;
			TCHAR* pStart = pStr;
			while (pStart != NULL && *pStart != '\0')
			{
				if (ISWHITESPACE(*pDelimiter))
				{
					GETNONWHITESPACE(pStart);
				}
				pNext = strchr(pStart, *pDelimiter);
				if (pNext != NULL)
					pNext++;
				if (requestedIndex == currentIndex)
				{
					int len = pNext != NULL ? static_cast<int>(pNext - pStart) : strlen(pStart) + 1;

					sVal.SetLength(len);
					if (len)
						strcpylen(const_cast<TCHAR*>(sVal.GetPtr()), pStart, len);
					sVal.SetAt(len - 1, '\0');
					return true;
				}

				currentIndex++;
				pStart = pNext;
			}

			return true;
		}
		case SSC_EXPLODE:
		{
			GETNONWHITESPACE(pszKey);
			char chSeparators[16];
			strcpylen(chSeparators, pszKey, 16);
			{
				char *p = chSeparators;
				while ( *p && (*p != ',') )
					++p;
				*p = 0;
			}

			const char *p = pszKey + strlen(chSeparators) + 1;
			sVal = "";
			if ( (p > pszKey) && *p )		// list of accessible separators 
			{
				TCHAR *ppCmd[255];
				TCHAR *z = Str_GetTemp();
				strncpy(z, p, SCRIPT_MAX_LINE_LEN - 1);
				size_t iCount = Str_ParseCmds(z, ppCmd, COUNTOF(ppCmd), chSeparators);
				if ( iCount > 0 )
				{
					sVal.Add(ppCmd[0]);
					for ( size_t i = 1; i < iCount; ++i )
					{
						sVal.Add(',');
						sVal.Add(ppCmd[i]);
					}
				}
			}
			return true;
		}
		case SSC_MD5HASH:
		{
			GETNONWHITESPACE(pszKey);
			char digest[33];
			CMD5::fastDigest(digest, pszKey);
			sVal.Format("%s", digest);
			return true;
		}
		case SSC_MULDIV:
		{
			INT64 iNum = Exp_GetLLVal(pszKey);
			SKIP_ARGSEP(pszKey);
			INT64 iMul = Exp_GetLLVal(pszKey);
			SKIP_ARGSEP(pszKey);
			INT64 iDiv = Exp_GetLLVal(pszKey);
			INT64 iRes = 0;

			if ( iDiv == 0 )
				DEBUG_ERR(("%s: can't divide by 0\n", sm_szLoadKeys[index]));
			else
				iRes = IMULDIV(iNum, iMul, iDiv);

#ifdef _DEBUG
			if ( g_Cfg.m_wDebugFlags & DEBUGF_SCRIPTS )
				g_Log.EventDebug("SCRIPT: %s(%lld,%lld,%lld) -> %lld\n", sm_szLoadKeys[index], iNum, iMul, iDiv, iRes);
#endif

			sVal.FormatLLVal(iRes);
			return true;
		}
		default:
			StringFunction(index, pszKey, sVal);
			return true;
	}
	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_KEYRET(pSrc);
	EXC_DEBUG_END;
	return false;
}

bool CScriptObj::r_VerbChained(CScript &s, CGString& sVal, CTextConsole* pSrc, CScriptTriggerArgs* pArgs)
{
	LPCTSTR pszKey = s.GetKey();
	if (*pszKey != '.')
		return true;
	pszKey++;

	LPCTSTR pszId = sVal;
	if (*pszId != '#')
		return false;
	pszId++;

	CExpression expr(NULL, pSrc, this);
	CScriptObj* pRef = static_cast<CGrayUID>(expr.GetVal(pszId)).ObjFind();
	if (pRef)
	{
		if (*pszKey == '.')
			pszKey++;
		sVal.Empty();
		CScript chainedScript(pszKey, s.GetArgStr());
		return pRef->r_Verb(chainedScript, pSrc, pArgs);
	}

	return true;
}

bool CScriptObj::r_VerbGlobal(CScript& s, CTextConsole* pSrc, CScriptTriggerArgs* pArgs)
{

	LPCTSTR pszKey = s.GetKey();
	CGString sVal;
	TemporaryString varName;
	if (Str_ParseVariableName(pszKey, varName))
	{
		CVarDefCont* pVar = g_Exp.m_VarGlobals.GetKey(varName, pArgs, pSrc, this);
		if (pVar)
		{
			sVal = pVar->GetValStr();
			CScript chainedScript(pszKey, s.GetArgStr());
			return r_VerbChained(chainedScript, sVal, pSrc, pArgs);
		}

		if (pArgs)
		{
			CVarDefCont* pLocalVar = pArgs->m_VarsLocal.GetKey(varName, pArgs, pSrc, this);
			if (pLocalVar)
			{
				sVal = pLocalVar->GetValStr();
				CScript chainedScript(pszKey, s.GetArgStr());
				return r_VerbChained(chainedScript, sVal, pSrc, pArgs);
			}
		}

		CVarDefCont* pDef = g_Exp.m_VarDefs.GetKey(varName, pArgs, pSrc, this);
		if (pDef)
		{
			if (*pszKey == '.')
			{
				INT64 num = pDef->GetValNum();
				if (num > 0)
				{
					RES_TYPE resType = static_cast<RES_TYPE>(RES_GET_TYPE(num));
					RESOURCE_ID resId(resType, RES_GET_INDEX(num));
					switch (resType)
					{
					case RES_ITEMDEF:
					{
						CItemBase* itemBase = CItemBase::FindItemBase(static_cast<ITEMID_TYPE>(RES_GET_INDEX(num)));
						if (itemBase)
						{
							pszKey++;
							CScript chainedScript(pszKey, s.GetArgStr());
							return itemBase->r_Verb(chainedScript, pSrc, pArgs);
						}
						break;
					}
					case RES_CHARDEF:
					{
						CCharBase* charBase = CCharBase::FindCharBase(static_cast<CREID_TYPE>(RES_GET_INDEX(num)));
						if (charBase)
						{
							pszKey++;
							CScript chainedScript(pszKey, s.GetArgStr());
							return charBase->r_Verb(chainedScript, pSrc, pArgs);
						}
						break;
					}
					case RES_SKILL:
					{
						CResourceDef* pDef = g_Cfg.ResourceGetDef(resId);
						if (pDef)
						{
							CSkillDef* pSkillDef = g_Cfg.GetSkillDef(static_cast<SKILL_TYPE>(resId.GetResIndex()));
							if (pSkillDef)
							{
								pszKey++;
								CScript chainedScript(pszKey, s.GetArgStr());
								return pSkillDef->r_Verb(chainedScript, pSrc, pArgs);
							}
						}
						break;
					}
					case RES_SPELL:
					{
						CResourceDef* pDef = g_Cfg.ResourceGetDef(resId);
						if (pDef)
						{
							CSpellDef* pSpellDef = g_Cfg.GetSpellDef(static_cast<SPELL_TYPE>(resId.GetResIndex()));
							if (pSpellDef)
							{
								pszKey++;
								CScript chainedScript(pszKey, s.GetArgStr());
								return pSpellDef->r_Verb(chainedScript, pSrc, pArgs);
							}
						}
						break;
					}
					}
				}
			}
			sVal = pDef->GetValStr();
			CScript chainedScript(pszKey, s.GetArgStr());
			return r_VerbChained(chainedScript, sVal, pSrc, pArgs);
		}
	}

	return false;
}

bool CScriptObj::r_Verb(CScript &s, CTextConsole *pSrc, CScriptTriggerArgs* pArgs)
{
	ADDTOCALLSTACK("CScriptObj::r_Verb");
	// Execute command from script

	EXC_TRY("Verb");
	ASSERT(pSrc);
	LPCTSTR pszKey = s.GetKey();
	CScriptObj *pRef = NULL;
	if ( r_GetRef(pszKey, pRef) )
	{
		if ( pszKey[0] )
		{
			if ( !pRef )
				return true;
			CScript script(pszKey, s.GetArgStr());
			return pRef->r_Verb(script, pSrc, pArgs);
		}
		// else just fall through. as they seem to be setting the pointer !?
	}
	if (r_GetRefNew(pszKey, pRef, s.GetArgRaw(), pArgs, pSrc))
	{
		if (pszKey[0])
		{
			if (!pRef)
				return true;
			CScript script(pszKey);
			return pRef->r_Verb(script, pSrc, pArgs);
		}
	}

	if ( s.IsKeyHead("SRC.", 4) )
	{
		pszKey += 4;
		pRef = dynamic_cast<CScriptObj *>(pSrc->GetChar());
		if ( !pRef )
		{
			pRef = dynamic_cast<CScriptObj *>(pSrc);
			if ( !pRef )
				return false;
		}
		CScript script(pszKey, s.GetArgStr());
		return pRef->r_Verb(script, pSrc, pArgs);
	}

	if (!strcmpi(pszKey, "SRC"))
	{
		CExpression expr(pArgs, pSrc, this, false);
		LPCTSTR pszArg = s.GetArgStr();
		CChar* pChar = static_cast<CGrayUID>(expr.GetVal(const_cast<LPCTSTR>(pszArg))).CharFind();
		pSrc->SetChar(pChar);
	}

	SSV_TYPE index = static_cast<SSV_TYPE>(FindTableSorted(s.GetKey(), sm_szVerbKeys, COUNTOF(sm_szVerbKeys) - 1));

	switch ( index )
	{
		case SSV_OBJ:
		{
			g_World.m_uidObj = static_cast<CGrayUID>(s.GetArgLLVal());
			if ( !g_World.m_uidObj.ObjFind() )
				g_World.m_uidObj = static_cast<CGrayUID>(UID_CLEAR);
			break;
		}
		case SSV_NEW:
		{
			g_World.m_uidNew = static_cast<CGrayUID>(s.GetArgLLVal());
			if ( !g_World.m_uidNew.ObjFind() )
				g_World.m_uidNew = static_cast<CGrayUID>(UID_CLEAR);
			break;
		}
		case SSV_NEWDUPE:
		{
			CGrayUID uid = static_cast<CGrayUID>(s.GetArgLLVal());
			CObjBase *pObj = uid.ObjFind();
			if ( !pObj )
			{
				g_World.m_uidNew = static_cast<CGrayUID>(UID_CLEAR);
				return false;
			}

			g_World.m_uidNew = uid;
			CScript script("DUPE");
			bool fRes = pObj->r_Verb(script, pSrc, pArgs);

			if ( this != &g_Serv )
			{
				CChar *pChar = dynamic_cast<CChar *>(this);
				if ( pChar )
					pChar->m_Act_Targ = g_World.m_uidNew;
				else
				{
					CClient *pClient = dynamic_cast<CClient *>(this);
					if ( pClient && pClient->GetChar() )
						pClient->GetChar()->m_Act_Targ = g_World.m_uidNew;
				}
			}
			return fRes;
		}
		case SSV_NEWITEM:
		{
			// Just create the item but don't put it anyplace yet
			TCHAR *ppCmd[4];
			size_t iQty = Str_ParseCmds(s.GetArgRaw(), ppCmd, COUNTOF(ppCmd), ",");
			if ( iQty < 1 )
				return false;

			CItem *pItem = CItem::CreateHeader(ppCmd[0], NULL, false, pSrc->GetChar());
			if ( !pItem )
			{
				g_World.m_uidNew = static_cast<CGrayUID>(UID_CLEAR);
				return false;
			}

			if ( ppCmd[1] )
				pItem->SetAmount(static_cast<WORD>(Exp_GetLLVal(ppCmd[1])));

			if ( ppCmd[2] )
			{
				CGrayUID uidEquipper = static_cast<CGrayUID>(Exp_GetLLVal(ppCmd[2]));
				bool fTriggerEquip = ppCmd[3] ? (Exp_GetLLVal(ppCmd[3]) != 0) : false;

				if ( !fTriggerEquip || uidEquipper.IsItem() )
					pItem->LoadSetContainer(uidEquipper, LAYER_NONE);
				else
				{
					if ( fTriggerEquip )
					{
						CChar *pCharEquipper = uidEquipper.CharFind();
						if ( pCharEquipper )
							pCharEquipper->ItemEquip(pItem);
					}
				}
			}

			g_World.m_uidNew = pItem->GetUID();

			if ( this != &g_Serv )
			{
				CChar *pChar = dynamic_cast<CChar *>(this);
				if ( pChar )
					pChar->m_Act_Targ = g_World.m_uidNew;
				else
				{
					CClient *pClient = dynamic_cast<CClient *>(this);
					if ( pClient && pClient->GetChar() )
						pClient->GetChar()->m_Act_Targ = g_World.m_uidNew;
				}
			}
			break;
		}
		case SSV_NEWNPC:
		{
			CREID_TYPE id = static_cast<CREID_TYPE>(g_Cfg.ResourceGetIndexType(RES_CHARDEF, s.GetArgRaw()));
			CChar *pChar = CChar::CreateNPC(id);
			if ( !pChar )
			{
				g_World.m_uidNew = static_cast<CGrayUID>(UID_CLEAR);
				return false;
			}

			g_World.m_uidNew = pChar->GetUID();

			if ( this != &g_Serv )
			{
				pChar = dynamic_cast<CChar *>(this);
				if ( pChar )
					pChar->m_Act_Targ = g_World.m_uidNew;
				else
				{
					const CClient *pClient = dynamic_cast<CClient *>(this);
					if ( pClient && pClient->GetChar() )
						pClient->GetChar()->m_Act_Targ = g_World.m_uidNew;
				}
			}
			break;
		}
		case SSV_SHOW:
		{
			CGString sVal;
			if ( !r_WriteVal(s.GetArgStr(), sVal, pSrc, pArgs) )
				return false;
			TCHAR *pszMsg = Str_GetTemp();
			sprintf(pszMsg, "'%s' for '%s' is '%s'\n", static_cast<LPCTSTR>(s.GetArgStr()), GetName(), static_cast<LPCTSTR>(sVal));
			pSrc->SysMessage(pszMsg);
			break;
		}
		default:
		{
			if (!strnicmp(pszKey, "VAR", 3))
			{
				if (*(pszKey + 3) == '.')
				{
					bool fQuoted = false;
					TCHAR* args = s.GetArgStr(&fQuoted);
					if (*args == '#')
					{
						TemporaryString pszBuffer;
						strcpy(pszBuffer, pszKey + 4);
						strcat(pszBuffer, args + 1);
						int iValue = Exp_GetVal(pszBuffer);
						g_Exp.m_VarGlobals.SetNum(pszKey + 4, iValue, false, pArgs, pSrc, this);
					}
					else
					{
						g_Exp.m_VarGlobals.SetStr(pszKey + 4, fQuoted, s.GetArgStr(&fQuoted), false, pArgs, pSrc, this);
					}
					return true;
				}
				else if (pszKey[3] == '(' || strlen(pszKey) == 3)
				{
					bool fQuoted = false;
					TCHAR* ppArgs[2];
					size_t iCount;
					LPCTSTR pszRawArgs = s.GetArgRaw();
					TemporaryString varArgList;
					Str_ParseArgumentList(pszRawArgs, varArgList);
					Str_ParseArgumentEnd(pszRawArgs, false);
					iCount = Str_ParseCmds(varArgList, ppArgs, COUNTOF(ppArgs), ",", false);
					if (iCount > 1)
					{
						TCHAR* pszVarName = Str_TrimWhitespace(ppArgs[0]);
						TCHAR* pszValue = iCount == 1 ? s.GetArgStr(&fQuoted) : ppArgs[1];

						if (*pszValue == '"')
						{
							pszValue++;
							pszValue = Str_TrimEnd(pszValue, "\"");
							fQuoted = true;
						}

						if (*pszValue == '#')
						{
							LPCTSTR ppArgs1 = ppArgs[1] + 1;
							if (!IsStrNumeric(ppArgs1))
							{
								LPCTSTR sVal = g_Exp.m_VarGlobals.GetKeyStr(pszVarName, false, pArgs, pSrc, this);

								TemporaryString pszBuffer;
								strcpy(pszBuffer, sVal);
								strcat(pszBuffer, pszValue + 1);
								int iValue = Exp_GetVal(pszBuffer);
								g_Exp.m_VarGlobals.SetNum(pszVarName, iValue, false, pArgs, pSrc, this);
							}
							else
							{
								g_Exp.m_VarGlobals.SetStr(pszVarName, fQuoted, ppArgs[1], false, pArgs, pSrc, this);
							}
						}
						else
						{
							g_Exp.m_VarGlobals.SetStr(pszVarName, fQuoted, Str_TrimDoublequotes(ppArgs[1]), false, pArgs, pSrc, this);
						}
					}
					else
					{
						if (*pszRawArgs == '.')
						{
							pszRawArgs++;
							CObjBase* pObj = static_cast<CGrayUID>(g_Exp.m_VarGlobals.GetKeyNum(varArgList)).ObjFind();
							if (pObj)
							{
								CScript subS(pszRawArgs);
								return pObj->r_Verb(subS, pSrc, pArgs);
							}
						}
					}
					return true;
				}
			}

			return r_LoadVal(s, pArgs, pSrc);		// default to loading values
		}
	}
	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_SCRIPTSRC;
	EXC_DEBUG_END;
	return false;
}

bool CScriptObj::r_CallRaw(LPCTSTR pszKey, CTextConsole* pSrc,  CScriptTriggerArgs* pArgs, CGString* psVal, TRIGRET_TYPE* piRet)
{
	LPCTSTR pszArgs = strchr(pszKey, '(');
	if (pszArgs)
	{
		LPCTSTR pszArgsStart = pszArgs;
		Str_SkipArgumentList(pszArgs);
		TemporaryString sArgsTmp;
		strncpy(sArgsTmp, pszArgsStart, pszArgs - pszArgsStart - 1);
		sArgsTmp.setAt(pszArgs - pszArgsStart - 1, '\0');
		pszArgs = sArgsTmp;
		pszArgs++;
	}
	else
	{
		pszArgs = strchr(pszKey, ' ');
		if (pszArgs)
			SKIP_SEPARATORS(pszArgs);
	}


	CScriptTriggerArgs Args(pszArgs ? pszArgs : "");
	if (pArgs)
		Args.m_pO1 = pArgs->m_pO1;
	if (r_Call(pszKey, pSrc, &Args, psVal))
		return true;

	return false;
}

bool CScriptObj::r_Call(LPCTSTR pszFunction, CTextConsole *pSrc, CScriptTriggerArgs *pArgs, CGString *psVal, TRIGRET_TYPE *piRet)
{
	ADDTOCALLSTACK("CScriptObj::r_Call");
	GETNONWHITESPACE(pszFunction);

	int iCompareRes = -1;
	size_t index = g_Cfg.m_Functions.FindKeyNear(pszFunction, iCompareRes, true);
	if ( (iCompareRes != 0) || (index == g_Cfg.m_Functions.BadIndex()) )
		return false;

	CResourceNamed *pFunction = static_cast<CResourceNamed *>(g_Cfg.m_Functions[index]);
	ASSERT(pFunction);
	CResourceLock sFunction;
	if ( pFunction->ResourceLock(sFunction) )
	{
		TScriptProfiler::TScriptProfilerFunction *pFun = NULL;
		ULONGLONG llTicksStart, llTicksEnd;

		if ( IsSetEF(EF_Script_Profiler) )
		{
			// Lowercase for speed, and strip arguments
			char *pchName = Str_GetTemp();
			char *pchSpace;
			strncpy(pchName, pszFunction, sizeof(pFun->name) - 1);
			pchName[sizeof(pFun->name) - 1] = '\0';
			if ( (pchSpace = strchr(pchName, ' ')) != NULL )
				*pchSpace = '\0';
			_strlwr(pchName);

			if ( g_profiler.initstate != 0xF1 )		// profiler is not initialized
			{
				memset(&g_profiler, 0, sizeof(g_profiler));
				g_profiler.initstate = static_cast<BYTE>(0xF1);		// ''
			}

			for ( pFun = g_profiler.FunctionsHead; pFun != NULL; pFun = pFun->next )
			{
				if ( !strcmp(pFun->name, pchName) )
					break;
			}
			if ( !pFun )
			{
				// First time that the function is called, so create its record
				pFun = new TScriptProfiler::TScriptProfilerFunction;
				memset(pFun, 0, sizeof(TScriptProfiler::TScriptProfilerFunction));
				strncpy(pFun->name, pchName, sizeof(pFun->name) - 1);
				if ( g_profiler.FunctionsTail )
					g_profiler.FunctionsTail->next = pFun;
				else
					g_profiler.FunctionsHead = pFun;
				g_profiler.FunctionsTail = pFun;
			}

			++pFun->called;
			++g_profiler.called;
			TIME_PROFILE_START;
		}

		TRIGRET_TYPE iRet = OnTriggerRun(sFunction, TRIGRUN_SECTION_TRUE, pSrc, pArgs, psVal);

		if ( IsSetEF(EF_Script_Profiler) )
		{
			TIME_PROFILE_END;
			llTicksStart = llTicksEnd - llTicksStart;
			pFun->total += llTicksStart;
			pFun->average = pFun->total / pFun->called;
			if ( pFun->max < llTicksStart )
				pFun->max = llTicksStart;
			if ( (pFun->min > llTicksStart) || !pFun->min )
				pFun->min = llTicksStart;
			g_profiler.total += llTicksStart;
		}

		if ( piRet )
			*piRet = iRet;
	}
	return true;
}

///////////////////////////////////////////////////////////
// CScriptTriggerArgs

CScriptTriggerArgs::CScriptTriggerArgs(LPCTSTR pszStr)
{
	Init(pszStr);
}

void CScriptTriggerArgs::Init(LPCTSTR pszStr)
{
	ADDTOCALLSTACK_INTENSIVE("CScriptTriggerArgs::Init");
	m_iN1 = 0;
	m_iN2 = 0;
	m_iN3 = 0;
	m_pO1 = NULL;

	if ( !pszStr )
		pszStr = "";

	bool fParentheses = false;
	if (*pszStr == '(')
	{
		fParentheses = true;
	}

	// Raw is left untouched for now - it'll be split the 1st time argv is accessed
	Str_TrimEndWhitespace(const_cast<TCHAR*>(pszStr), strlen(pszStr));
	m_s1_raw = pszStr;
	bool fQuote = false;
	if ( *pszStr == '"' )
	{
		fQuote = true;
		++pszStr;
	}

	m_s1 = pszStr;

	// Take quote if present
	if ( fQuote )
	{
		TCHAR *str = const_cast<TCHAR *>(strchr(m_s1.GetPtr(), '"'));
		if ( str )
			*str = '\0';
	}

	// Attempt to parse this
	if ( IsDigit(*pszStr) || ((*pszStr == '-') && IsDigit(*(pszStr + 1))) )
	{
		m_iN1 = Exp_GetLLSingle(pszStr);
		SKIP_ARGSEP(pszStr);
		if ( IsDigit(*pszStr) || ((*pszStr == '-') && IsDigit(*(pszStr + 1))) )
		{
			m_iN2 = Exp_GetLLSingle(pszStr);
			SKIP_ARGSEP(pszStr);
			if ( IsDigit(*pszStr) || ((*pszStr == '-') && IsDigit(*(pszStr + 1))) )
				m_iN3 = Exp_GetLLSingle(pszStr);
		}
	}

	// ensure argv will be recalculated next time it is accessed
	m_v.SetCount(0);
}

enum AGC_TYPE
{
	AGC_N,
	AGC_N1,
	AGC_N2,
	AGC_N3,
	AGC_O,
	AGC_V,
	AGC_LVAR,
	AGC_SAFE,
	AGC_TRY,
	AGC_TRYSRV,
	AGC_QTY
};

LPCTSTR const CScriptTriggerArgs::sm_szLoadKeys[AGC_QTY + 1] =
{
	"ARGN",
	"ARGN1",
	"ARGN2",
	"ARGN3",
	"ARGO",
	"ARGV",
	"LOCAL",
	"SAFE",
	"TRY",
	"TRYSRV",
	NULL
};

bool CScriptTriggerArgs::r_GetRef(LPCTSTR &pszKey, CScriptObj *&pRef)
{
	ADDTOCALLSTACK("CScriptTriggerArgs::r_GetRef");

	if ( !strnicmp(pszKey, "ARGO.", 5) )
	{
		pszKey += 5;
		if ( *pszKey == '1' )
			++pszKey;
		pRef = m_pO1;
		return true;
	}
	else if ( !strnicmp(pszKey, "REF", 3) )
	{
		LPCTSTR pszTemp = pszKey;
		pszTemp += 3;
		if ( *pszTemp && IsDigit(*pszTemp) )
		{
			char *pchEnd;
			WORD wNumber = static_cast<WORD>(strtol(pszTemp, &pchEnd, 10));
			if ( wNumber > 0 )	// can only use 1 to 65535 as REFx
			{
				// Make sure REFx or REFx.KEY is being used
				pszTemp = pchEnd;
				if ( !*pszTemp || (*pszTemp == '.') )
				{
					if ( *pszTemp == '.' )
						++pszTemp;

					pRef = m_VarObjs.Get(wNumber);
					pszKey = pszTemp;
					return true;
				}
			}
		}
	}
	return false;
}

bool CScriptTriggerArgs::r_LoadVal(CScript &s, CTextConsole* pSrc)
{
	ADDTOCALLSTACK("CScriptTriggerArgs::r_LoadVal");

	if (s.IsKeyHead("ARGV(", 4))
	{

		return true;
	}

	return false;
}

size_t CScriptTriggerArgs::getArgumentsCount()
{
	size_t iQty = m_v.GetCount();
	if (iQty > 0)
		return iQty;

	// Parse it here
	TCHAR* pszArgs = const_cast<TCHAR*>(m_s1_raw.GetPtr());
	TCHAR* s = pszArgs;
	bool fQuotes = false;
	bool fInerQuotes = false;
	bool fCommaPrev = false;
	while (*s)
	{
		if ((*s == ',') && !fQuotes)	// add empty arguments if they are provided
		{
			fCommaPrev = true;
			m_v.Add(NULL);
			++s;
			continue;
		}
		fCommaPrev = false;
		if (*s == '"')	// check to see if the argument is quoted (in case it contains commas)
		{
			++s;
			fQuotes = true;
			fInerQuotes = false;
		}

		pszArgs = s;	// arg starts here
		if (fQuotes && *s != '"')
			++s;

		while (*s)
		{
			if ((*s == '"') && fQuotes)
			{
				*s = '\0';
				fQuotes = false;
			}
			else if ((*s == '"'))
				fInerQuotes = !fInerQuotes;

			if (!fQuotes && !fInerQuotes && (*s == ','))
			{
				*s = '\0';
				++s;
				break;
			}
			++s;
		}
		m_v.Add(pszArgs);
	}

	if (fCommaPrev)
		m_v.Add(NULL);

	return m_v.GetCount();
}

LPCTSTR CScriptTriggerArgs::GetArgV(int iKey)
{
	size_t iQty = getArgumentsCount();
	return m_v.GetAt(static_cast<size_t>(iKey));
}

bool CScriptTriggerArgs::r_WriteVarVal(LPCTSTR pszKey, CGString& sVal, CTextConsole* pSrc, CScriptObj *pObj)
{
	ADDTOCALLSTACK("CScriptTriggerArgs::r_WriteVarVal");
	EXC_TRY("WriteVarVal");

	TemporaryString varName;
	LPCTSTR pszOriginalKey = pszKey;
	if (Str_ParseVariableName(pszKey, varName))
	{
		CVarDefCont* pVar = m_VarsLocal.GetKey(varName, this, pSrc, pObj);
		if (pVar)
		{
			sVal = pVar->GetValStr();
			return r_WriteValChained(pszKey, sVal, pSrc, this);
		}
	}
	pszKey = pszOriginalKey;

	if (!strnicmp("arg(", pszKey, 4) || !strnicmp("arg.", pszKey, 4))
	{
		EXC_SET("localarg");
		pszKey += 3;

		if (*pszKey == '.')
		{
			pszKey++;
			sVal = m_VarsLocal.GetKeyStr(pszKey, false, this, pSrc, this);
			return true;
		}
		else
		{
			if (Str_ParseArgumentStart(pszKey, true))
			{
				TemporaryString varName;
				if (Str_ParseVariableName(pszKey, varName))
				{
					Str_ParseArgumentEnd(pszKey, true);
					sVal = m_VarsLocal.GetKeyStr(varName, false, this, pSrc, pObj);
					if (sVal.IsEmpty())
						return true;
					return r_WriteValChained(pszKey, sVal, pSrc, this);
				}
			}
		}
	}

	return false;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_KEYRET(pSrc);
	EXC_DEBUG_END;
	return false;
}

bool CScriptTriggerArgs::r_WriteVal(LPCTSTR pszKey, CGString &sVal, CTextConsole *pSrc)
{
	ADDTOCALLSTACK("CScriptTriggerArgs::r_WriteVal");
	EXC_TRY("WriteVal");
	if ( !strnicmp("LOCAL.", pszKey, 6) )
	{
		EXC_SET("local");
		pszKey += 6;
		sVal = m_VarsLocal.GetKeyStr(pszKey, true, this, pSrc);
		return true;
	}
	else if ( !strnicmp("FLOAT.", pszKey, 6) )
	{
		EXC_SET("float");
		pszKey += 6;
		sVal = m_VarsFloat.Get(pszKey);
		return true;
	}
	else if (!strnicmp(pszKey, "ARGVCOUNT", 9))
	{
		EXC_SET("argvcount");
		pszKey += 9;
		SKIP_SEPARATORS(pszKey);
		size_t iQty = getArgumentsCount();
		sVal.FormatLLVal(iQty);
		return true;
	}
	else if ( !strnicmp(pszKey, "ARGV", 4) )
	{
		EXC_SET("argv");
		pszKey += 4;

		INT64 iKey;
		CExpression expr(this, pSrc, NULL);
		if (*pszKey == '.')
		{
			pszKey++;
			iKey = 0;
		}
		else
		{
			TemporaryString tmpArgs;
			if (!Str_ParseArgumentList(pszKey, tmpArgs))
				return false;
			iKey = expr.GetVal(tmpArgs);
		}

		getArgumentsCount();
		if ( (iKey < 0) || !m_v.IsValidIndex(static_cast<size_t>(iKey)) )
		{
			sVal = "";
			return true;
		}

		sVal = m_v.GetAt(static_cast<size_t>(iKey));
		if (sVal.GetLength() > 0 && sVal.GetAt(0) == '#')
		{
			LPCTSTR pszVal = sVal;
			pszVal++;
			if (IsStrNumeric(pszVal))
				sVal.MakeUpper();
		}
		return r_WriteValChained(pszKey, sVal, pSrc, this);
	}
	else if (!strnicmp(pszKey, "ARGS", 4)) {
		if (m_s1 && strlen(m_s1))
		{
			sVal = m_s1;
			pszKey += 4;
			return r_WriteValChained(pszKey, sVal, pSrc, this);
		}
		else
			return true;
	}

	EXC_SET("generic");
	AGC_TYPE index = static_cast<AGC_TYPE>(FindTableSorted(pszKey, sm_szLoadKeys, COUNTOF(sm_szLoadKeys) - 1));
	switch ( index )
	{
		case AGC_N:
		case AGC_N1:
			sVal.FormatLLVal(m_iN1);
			break;
		case AGC_N2:
			sVal.FormatLLVal(m_iN2);
			break;
		case AGC_N3:
			sVal.FormatLLVal(m_iN3);
			break;
		case AGC_O:
		{
			CObjBase *pObj = dynamic_cast<CObjBase *>(m_pO1);
			sVal.FormatHex(pObj ? static_cast<DWORD>(pObj->GetUID()) : UID_CLEAR);
			break;
		}
		default:
			CVarDefCont* pVar = m_VarsLocal.GetKey(pszKey, this, pSrc, this);
			if (pVar)
			{
				EXC_SET("nonspecificvar");
				sVal = pVar->GetValStr();
				return true;
			}

			return CScriptObj::r_WriteVal(pszKey, sVal, pSrc, this);
	}
	CVarDefCont* pVar = m_VarsLocal.GetKey(pszKey);
	if (pVar)
	{
		EXC_SET("nonspecificvar");
		sVal = pVar->GetValStr();
		return true;
	}
	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_KEYRET(pSrc);
	EXC_DEBUG_END;
	return false;
}

bool CScriptTriggerArgs::r_Verb(CScript &s, CTextConsole *pSrc, CScriptObj* pObj)
{
	ADDTOCALLSTACK("CScriptTriggerArgs::r_Verb");
	EXC_TRY("Verb");
	LPCTSTR pszKey = s.GetKey();
	int index = -1;

	TemporaryString chainedPrefix;
	LPCTSTR pszOrigKey = pszKey;
	if (Str_ParseChained(pszKey, chainedPrefix))
	{
		CObjBase* pObj = static_cast<CGrayUID>(m_VarsLocal.GetKeyNum(chainedPrefix)).ObjFind();
		if (pObj)
		{
			pszKey++;
			CScript subS(pszKey, s.GetArgStr());
			return pObj->r_Verb(subS, pSrc, this);
		}
	}
	pszKey = pszOrigKey;

	if ( !strnicmp("FLOAT.", pszKey, 6) )
		return m_VarsFloat.Insert(pszKey + 6, s.GetArgStr(), true);
	else if ( !strnicmp("LOCAL.", pszKey, 6) )
	{
		bool fQuoted = false;
		m_VarsLocal.SetStr(s.GetKey() + 6, fQuoted, s.GetArgStr(&fQuoted), false, this, pSrc, pObj);
		return true;
	}
	else if (!stricmp("argv", pszKey))
	{
		LPCTSTR pszOrigKey = pszKey;
		pszKey = s.GetArgRaw();
		TemporaryString argumentIndexStr;
		Str_ParseArgumentList(pszKey, argumentIndexStr);
		Str_ParseArgumentEnd(pszKey, true);
		if (*pszKey == '.')
		{
			pszKey++;
			CExpression expr(this, pSrc);
			INT64 argumentIndex = expr.GetVal(argumentIndexStr);
			LPCTSTR lpszArgumentValue = GetArgV(argumentIndex);
			if (*lpszArgumentValue == '#')
			{
				lpszArgumentValue++;
				CObjBase* pObj = static_cast<CGrayUID>(expr.GetVal(lpszArgumentValue)).ObjFind();
				if (pObj)
				{
					CScript subS(pszKey);
					return pObj->r_Verb(subS, pSrc, this);
				}
			}
		}

		pszKey = pszOrigKey;
	}
	else if (!stricmp("arg", pszKey) || !strnicmp("arg.", pszKey, 4))
	{
		if (*(pszKey + 3) == '.')
		{
			bool fQuoted = false;
			TCHAR* args = s.GetArgStr(&fQuoted);
			if (*args == '#')
			{
				TemporaryString pszBuffer;
				strcpy(pszBuffer, m_VarsLocal.GetKeyStr(pszKey + 4, false, this, pSrc));
				strcat(pszBuffer, args + 1);
				CExpression expr(this, pSrc, NULL);
				int iValue = expr.GetVal(pszBuffer);
				m_VarsLocal.SetNum(pszKey + 4, iValue, false, this, pSrc, pObj);
			}
			else
			{
				m_VarsLocal.SetStr(pszKey + 4, fQuoted, s.GetArgStr(&fQuoted), false, this, pSrc, pObj);
			}
			return true;
		}
		else
		{
			bool fQuoted = false;
			TCHAR* ppArgs[2];
			size_t iCount;
			LPCTSTR pszRawArgs = s.GetArgRaw();
			TemporaryString varArgList;
			Str_ParseArgumentList(pszRawArgs, varArgList);
			Str_ParseArgumentEnd(pszRawArgs, false);
			iCount = Str_ParseCmds(varArgList, ppArgs, COUNTOF(ppArgs), ",", false);
			if (iCount > 1)
			{
				TCHAR* pszVarName = Str_TrimWhitespace(ppArgs[0]);
				TCHAR* pszValue = iCount == 1 ? s.GetArgStr(&fQuoted) : ppArgs[1];

				if (*pszValue == '"')
				{
					pszValue++;
					pszValue = Str_TrimEnd(pszValue, "\"");
					fQuoted = true;
				}

				if (*pszValue == '#')
				{
					LPCTSTR ppArgs1 = ppArgs[1] + 1;
					if (!IsStrNumeric(ppArgs1))
					{
						LPCTSTR sVal = m_VarsLocal.GetKeyStr(pszVarName, false, this, pSrc, pObj);

						TemporaryString pszBuffer;
						strcpy(pszBuffer, sVal);
						strcat(pszBuffer, pszValue + 1);
						int iValue = Exp_GetVal(pszBuffer);
						m_VarsLocal.SetNum(pszVarName, iValue, false, this, pSrc, pObj);
					}
					else
					{
						m_VarsLocal.SetStr(pszVarName, fQuoted, ppArgs[1], false, this, pSrc, pObj);
					}
				}
				else
				{
					m_VarsLocal.SetStr(pszVarName, fQuoted, pszValue, false, this, pSrc, pObj);
				}
			}
			else
			{
				if (*pszRawArgs == '.')
				{
					pszRawArgs++;
					CObjBase* pObj = static_cast<CGrayUID>(m_VarsLocal.GetKeyNum(varArgList)).ObjFind();
					if (pObj)
					{
						CScript subS(pszRawArgs);
						return pObj->r_Verb(subS, pSrc, this);
					}
				}
			}
		}

		return true;
	}
	else if ( !strnicmp("REF", pszKey, 3) )
	{
		LPCTSTR pszTemp = pszKey;
		pszTemp += 3;
		if ( *pszTemp && IsDigit(*pszTemp) )
		{
			char *pchEnd;
			WORD wNumber = static_cast<WORD>(strtol(pszTemp, &pchEnd, 10));
			if ( wNumber > 0 )	// can only use 1 to 65535 as REFs
			{
				pszTemp = pchEnd;
				if ( !*pszTemp )	// setting REFx to a new object
				{
					m_VarObjs.Insert(wNumber, static_cast<CGrayUID>(s.GetArgLLVal()).ObjFind(), true);
					pszKey = pszTemp;
					return true;
				}
				else if ( *pszTemp == '.' )	// accessing REFx object
				{
					pszKey = ++pszTemp;
					CObjBase *pObj = m_VarObjs.Get(wNumber);
					if ( !pObj )
						return false;

					CScript script(pszKey, s.GetArgStr());
					return pObj->r_Verb(script, pSrc, this);
				}
			}
		}
	}
	else if ( !strnicmp(pszKey, "ARGO", 4) )
	{
		pszKey += 4;
		if ( !strnicmp(pszKey, ".", 1) )
			index = AGC_O;
		else
		{
			++pszKey;
			m_pO1 = static_cast<CGrayUID>(Exp_GetSingle(pszKey)).ObjFind();
			return true;
		}
	}
	else if (!strnicmp(pszKey, "ARGS", 4))
	{
		Init(s.GetArgStr());
		return true;
	}
	else
		index = FindTableSorted(s.GetKey(), sm_szLoadKeys, COUNTOF(sm_szLoadKeys) - 1);

	switch ( index )
	{
		case AGC_N:
		case AGC_N1:
			m_iN1 = s.GetArgLLVal();
			return true;
		case AGC_N2:
			m_iN2 = s.GetArgLLVal();
			return true;
		case AGC_N3:
			m_iN3 = s.GetArgLLVal();
			return true;
		case AGC_O:
		{
			LPCTSTR pszTemp = s.GetKey() + 4;
			if ( *pszTemp == '.' )
			{
				++pszTemp;
				if ( !m_pO1 )
					return false;

				CScript script(pszTemp, s.GetArgStr());
				return m_pO1->r_Verb(script, pSrc, this);
			}
			return false;
		}
		case AGC_TRY:
		case AGC_TRYSRV:
		{
			CScript try_script(s.GetArgStr());
			if ( r_Verb(try_script, pSrc, this) )
				return true;
		}
		case AGC_V:
		{
			TemporaryString varIndexStr;
			LPCTSTR pszCmd = const_cast<LPCTSTR>(s.GetArgStr());
			Str_ParseArgumentList(pszCmd, varIndexStr);
			Str_ParseArgumentEnd(pszCmd, true);
			if (*pszCmd == '.')
			{
				pszCmd++;
				CExpression expr(this, pSrc, pObj);
				long varIndex = expr.GetVal(varIndexStr);
				LPCTSTR varValue = this->GetArgV(varIndex);
				CObjBase* pObj = static_cast<CGrayUID>(expr.GetVal(varValue)).ObjFind();
				if (pObj)
				{
					CScript subS(pszCmd);
					return pObj->r_LoadVal(subS, this, pSrc);
				}
			}
			break;
		}
		default:
			return false;
	}
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_SCRIPTSRC;
	EXC_DEBUG_END;
	return false;
}

///////////////////////////////////////////////////////////
// CFileObj

CFileObj::CFileObj()
{
	m_pFile = new CFileText();
	m_pszBuffer = new TCHAR[SCRIPT_MAX_LINE_LEN];
	m_psWriteBuffer = new CGString();
	SetDefaultMode();
}

CFileObj::~CFileObj()
{
	if ( m_pFile->IsFileOpen() )
		m_pFile->Close();

	delete m_psWriteBuffer;
	delete[] m_pszBuffer;
	delete m_pFile;
}

void CFileObj::SetDefaultMode()
{
	ADDTOCALLSTACK("CFileObj::SetDefaultMode");
	m_fAppend = true;
	m_fCreate = false;
	m_fRead = true;
	m_fWrite = true;
}

bool CFileObj::FileOpen(LPCTSTR pszPath)
{
	ADDTOCALLSTACK("CFileObj::FileOpen");
	if ( m_pFile->IsFileOpen() )
		return false;

	UINT uMode = OF_SHARE_DENY_NONE|OF_TEXT;
	if ( m_fCreate )	// if we create, we can't append or read
		uMode |= OF_CREATE;
	else if ( (m_fRead && m_fWrite) || m_fAppend )
		uMode |= OF_READWRITE;
	else if ( m_fRead )
		uMode |= OF_READ;
	else if ( m_fWrite )
		uMode |= OF_WRITE;

	return m_pFile->Open(pszPath, uMode);
}

TCHAR *CFileObj::GetReadBuffer(bool fDelete)
{
	ADDTOCALLSTACK("CFileObj::GetReadBuffer");
	if ( fDelete )
		memset(m_pszBuffer, 0, SCRIPT_MAX_LINE_LEN);
	else
		*m_pszBuffer = 0;
	return m_pszBuffer;
}

CGString *CFileObj::GetWriteBuffer()
{
	ADDTOCALLSTACK("CFileObj::GetWriteBuffer");
	if ( !m_psWriteBuffer )
		m_psWriteBuffer = new CGString();

	m_psWriteBuffer->Empty(m_psWriteBuffer->GetLength() > SCRIPT_MAX_LINE_LEN / 4);
	return m_psWriteBuffer;
}

bool CFileObj::OnTick()
{
	ADDTOCALLSTACK("CFileObj::OnTick");
	return true;
}

int CFileObj::FixWeirdness()
{
	ADDTOCALLSTACK("CFileObj::FixWeirdness");
	return 0;
}

bool CFileObj::IsInUse()
{
	ADDTOCALLSTACK("CFileObj::IsInUse");
	return m_pFile->IsFileOpen();
}

void CFileObj::FlushAndClose()
{
	ADDTOCALLSTACK("CFileObj::FlushAndClose");
	if ( m_pFile->IsFileOpen() )
	{
		m_pFile->Flush();
		m_pFile->Close();
	}
}

enum FO_TYPE
{
	#define ADD(a,b) FO_##a,
	#include "../tables/CFile_props.tbl"
	#undef ADD
	FO_QTY
};

LPCTSTR const CFileObj::sm_szLoadKeys[FO_QTY + 1] =
{
	#define ADD(a,b) b,
	#include "../tables/CFile_props.tbl"
	#undef ADD
	NULL
};

enum FOV_TYPE
{
	#define ADD(a,b) FOV_##a,
	#include "../tables/CFile_functions.tbl"
	#undef ADD
	FOV_QTY
};

LPCTSTR const CFileObj::sm_szVerbKeys[FOV_QTY + 1] =
{
	#define ADD(a,b) b,
	#include "../tables/CFile_functions.tbl"
	#undef ADD
	NULL
};

bool CFileObj::r_GetRef(LPCTSTR &pszKey, CScriptObj *&pRef)
{
	UNREFERENCED_PARAMETER(pszKey);
	UNREFERENCED_PARAMETER(pRef);
	return false;
}

bool CFileObj::r_LoadVal(CScript &s, CScriptTriggerArgs* pArgs, CTextConsole* pSrc)
{
	ADDTOCALLSTACK("CFileObj::r_LoadVal");
	EXC_TRY("LoadVal");
	LPCTSTR pszKey = s.GetKey();

	if ( !strnicmp("MODE.", pszKey, 5) )
	{
		pszKey += 5;
		if ( !m_pFile->IsFileOpen() )
		{
			if ( !strnicmp("APPEND", pszKey, 6) )
			{
				m_fAppend = (s.GetArgVal() != 0);
				m_fCreate = false;
			}
			else if ( !strnicmp("CREATE", pszKey, 6) )
			{
				m_fCreate = (s.GetArgVal() != 0);
				m_fAppend = false;
			}
			else if ( !strnicmp("READFLAG", pszKey, 8) )
				m_fRead = (s.GetArgVal() != 0);
			else if ( !strnicmp("WRITEFLAG", pszKey, 9) )
				m_fWrite = (s.GetArgVal() != 0);
			else if ( !strnicmp("SETDEFAULT", pszKey, 7) )
				SetDefaultMode();
			else
				return false;

			return true;
		}
		else
			g_Log.Event(LOGL_ERROR, "FILE (%s): Cannot set mode after file opening\n", static_cast<LPCTSTR>(m_pFile->GetFilePath()));
		return false;
	}

	FO_TYPE index = static_cast<FO_TYPE>(FindTableSorted(pszKey, sm_szLoadKeys, COUNTOF(sm_szLoadKeys) - 1));
	switch ( index )
	{
		case FO_WRITE:
		case FO_WRITECHR:
		case FO_WRITELINE:
		{
			if ( !m_pFile->IsFileOpen() )
			{
				g_Log.Event(LOGL_ERROR, "FILE: Cannot write content. Open the file first\n");
				return false;
			}

			if ( !s.HasArgs() )
				return false;

			CGString *psArgs = GetWriteBuffer();
			if ( index == FO_WRITELINE )
			{
				psArgs->Copy(s.GetArgStr());
#ifdef _WIN32
				psArgs->Add("\r\n");
#else
				psArgs->Add("\n");
#endif
			}
			else if ( index == FO_WRITECHR )
				psArgs->Format("%c", static_cast<TCHAR>(s.GetArgVal()));
			else
				psArgs->Copy(s.GetArgStr());

			bool fSuccess;
			if ( index == FO_WRITECHR )
				fSuccess = m_pFile->Write(psArgs->GetPtr(), 1);
			else
				fSuccess = m_pFile->WriteString(psArgs->GetPtr());

			if ( !fSuccess )
			{
				g_Log.Event(LOGL_ERROR, "FILE: Failed writing to \"%s\"\n", static_cast<LPCTSTR>(m_pFile->GetFilePath()));
				return false;
			}
			break;
		}
		default:
			return false;
	}

	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_SCRIPT;
	EXC_DEBUG_END;
	return false;
}

bool CFileObj::r_WriteVal(LPCTSTR pszKey, CGString &sVal, CTextConsole *pSrc, CScriptTriggerArgs* pArgs)
{
	ADDTOCALLSTACK("CFileObj::r_WriteVal");
	EXC_TRY("WriteVal");
	ASSERT(pszKey);

	if ( !strnicmp("MODE.", pszKey, 5) )
	{
		pszKey += 5;
		if ( !strnicmp("APPEND", pszKey, 6) )
			sVal.FormatVal(m_fAppend);
		else if ( !strnicmp("CREATE", pszKey, 6) )
			sVal.FormatVal(m_fCreate);
		else if ( !strnicmp("READFLAG", pszKey, 8) )
			sVal.FormatVal(m_fRead);
		else if ( !strnicmp("WRITEFLAG", pszKey, 9) )
			sVal.FormatVal(m_fWrite);
		else
			return false;

		return true;
	}

	int index = FindTableHeadSorted(pszKey, sm_szLoadKeys, COUNTOF(sm_szLoadKeys) - 1);
	switch ( index )
	{
		case FO_FILEEXIST:
		{
			pszKey += 9;
			GETNONWHITESPACE(pszKey);

			TCHAR *ppCmd = Str_TrimWhitespace(const_cast<TCHAR *>(pszKey));
			if ( !(ppCmd && strlen(ppCmd)) )
				return false;

			CFile *pFile = new CFile();
			sVal.FormatVal(pFile->Open(ppCmd));

			delete pFile;
			break;
		}
		case FO_FILELINES:
		{
			pszKey += 9;
			GETNONWHITESPACE(pszKey);

			TCHAR *ppCmd = Str_TrimWhitespace(const_cast<TCHAR *>(pszKey));
			if ( !(ppCmd && strlen(ppCmd)) )
				return false;

			CFileText *pFile = new CFileText();
			if ( !pFile->Open(ppCmd, OF_READ|OF_TEXT) )
			{
				delete pFile;
				return false;
			}

			long lLines = 0;
			while ( !pFile->IsEOF() )
			{
				pFile->ReadString(GetReadBuffer(), SCRIPT_MAX_LINE_LEN);
				++lLines;
			}
			pFile->Close();
			sVal.FormatVal(lLines);

			delete pFile;
			break;
		}
		case FO_FILEPATH:
			sVal.Format("%s", m_pFile->IsFileOpen() ? static_cast<LPCTSTR>(m_pFile->GetFilePath()) : "");
			break;
		case FO_INUSE:
			sVal.FormatVal(m_pFile->IsFileOpen());
			break;
		case FO_ISEOF:
			sVal.FormatVal(m_pFile->IsEOF());
			break;
		case FO_LENGTH:
			sVal.FormatVal(m_pFile->IsFileOpen() ? m_pFile->GetLength() : -1);
			break;
		case FO_OPEN:
		{
			pszKey += 4;
			GETNONWHITESPACE(pszKey);

			TCHAR *pszFilename = Str_TrimWhitespace(const_cast<TCHAR *>(pszKey));
			if ( !(pszFilename && strlen(pszFilename)) )
				return false;

			if ( m_pFile->IsFileOpen() )
			{
				g_Log.Event(LOGL_ERROR, "FILE: Cannot open file (%s). First close \"%s\"\n", pszFilename, static_cast<LPCTSTR>(m_pFile->GetFilePath()));
				return false;
			}

			sVal.FormatVal(FileOpen(pszFilename));
			break;
		}
		case FO_POSITION:
			sVal.FormatUVal(m_pFile->GetPosition());
			break;
		case FO_READBYTE:
		case FO_READCHAR:
		{
			size_t iRead = 1;
			if ( index != FO_READCHAR )
			{
				pszKey += 8;
				GETNONWHITESPACE(pszKey);

				iRead = Exp_GetVal(pszKey);
				if ( (iRead <= 0) || (iRead >= SCRIPT_MAX_LINE_LEN) )
					return false;
			}

			if ( (m_pFile->GetPosition() + iRead > m_pFile->GetLength()) || m_pFile->IsEOF() )
			{
				g_Log.Event(LOGL_ERROR, "FILE: Failed reading %" FMTSIZE_T " byte from \"%s\". Too near to EOF\n", iRead, static_cast<LPCTSTR>(m_pFile->GetFilePath()));
				return false;
			}

			TCHAR *pszBuffer = GetReadBuffer(true);
			if ( iRead != m_pFile->Read(pszBuffer, iRead) )
			{
				g_Log.Event(LOGL_ERROR, "FILE: Failed reading %" FMTSIZE_T " byte from \"%s\"\n", iRead, static_cast<LPCTSTR>(m_pFile->GetFilePath()));
				return false;
			}

			if ( index == FO_READCHAR )
				sVal.FormatVal(*pszBuffer);
			else
				sVal.Format("%s", pszBuffer);
			break;
		}
		case FO_READLINE:
		{
			pszKey += 8;
			GETNONWHITESPACE(pszKey);

			TCHAR *pszBuffer = GetReadBuffer();
			ASSERT(pszBuffer);

			INT64 iLines = Exp_GetLLVal(pszKey);
			if ( iLines < 0 )
				return false;

			DWORD dwSeek = m_pFile->GetPosition();
			m_pFile->SeekToBegin();

			if ( iLines == 0 )
			{
				while ( !m_pFile->IsEOF() )
					m_pFile->ReadString(pszBuffer, SCRIPT_MAX_LINE_LEN);
			}
			else
			{
				for ( INT64 i = 1; i <= iLines; ++i )
				{
					if ( m_pFile->IsEOF() )
						break;

					pszBuffer = GetReadBuffer();
					m_pFile->ReadString(pszBuffer, SCRIPT_MAX_LINE_LEN);
				}
			}

			m_pFile->Seek(dwSeek);

			size_t iLineLen = strlen(pszBuffer);
			while ( iLineLen > 0 )
			{
				--iLineLen;
				if ( isgraph(pszBuffer[iLineLen]) || (pszBuffer[iLineLen] == 0x20) || (pszBuffer[iLineLen] == '\t') )
				{
					++iLineLen;
					pszBuffer[iLineLen] = '\0';
					break;
				}
			}

			sVal.Format("%s", pszBuffer);
			break;
		}
		case FO_SEEK:
		{
			pszKey += 4;
			GETNONWHITESPACE(pszKey);

			if ( pszKey[0] == '\0' )
				return false;

			if ( strcmpi("BEGIN", pszKey) == 0 )
				sVal.FormatVal(m_pFile->Seek(0, SEEK_SET));
			else if ( strcmpi("END", pszKey) == 0 )
				sVal.FormatVal(m_pFile->Seek(0, SEEK_END));
			else
				sVal.FormatVal(m_pFile->Seek(Exp_GetVal(pszKey), SEEK_SET));
			break;
		}
		default:
			return false;
	}

	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_KEYRET(pSrc);
	EXC_DEBUG_END;
	return false;
}

bool CFileObj::r_Verb(CScript &s, CTextConsole *pSrc, CScriptTriggerArgs* pArgs)
{
	ADDTOCALLSTACK("CFileObj::r_Verb");
	EXC_TRY("Verb");
	ASSERT(pSrc);

	LPCTSTR pszKey = s.GetKey();
	int index = FindTableSorted(pszKey, sm_szVerbKeys, COUNTOF(sm_szVerbKeys) - 1);
	if ( index < 0 )
		return r_LoadVal(s, pArgs, pSrc);

	switch ( index )
	{
		case FOV_CLOSE:
		{
			if ( m_pFile->IsFileOpen() )
				m_pFile->Close();
			break;
		}
		case FOV_DELETEFILE:
		{
			if ( !s.GetArgStr() )
				return false;
			if ( m_pFile->IsFileOpen() && !strcmp(s.GetArgStr(), m_pFile->GetFileTitle()) )
				return false;
			STDFUNC_UNLINK(s.GetArgRaw());
			break;
		}
		case FOV_FLUSH:
		{
			if ( m_pFile->IsFileOpen() )
				m_pFile->Flush();
			break;
		}
		default:
			return false;
	}

	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_SCRIPTSRC;
	EXC_DEBUG_END;
	return false;
}

///////////////////////////////////////////////////////////
// CFileObjContainer

CFileObjContainer::CFileObjContainer()
{
	m_iGlobalTimeout = m_iCurrentTick = 0;
	SetFileNumber(0);
}

CFileObjContainer::~CFileObjContainer()
{
	ResizeContainer(0);
	m_FileList.clear();
}

void CFileObjContainer::ResizeContainer(size_t iNewRange)
{
	ADDTOCALLSTACK("CFileObjContainer::ResizeContainer");
	if ( iNewRange == m_FileList.size() )
		return;

	if ( iNewRange < m_FileList.size() )
	{
		if ( m_FileList.empty() )
			return;

		CFileObj *pObj = NULL;
		size_t iDiff = m_FileList.size() - iNewRange;
		for ( size_t i = m_FileList.size() - 1; iDiff > 0; --iDiff, --i )
		{
			pObj = m_FileList.at(i);
			m_FileList.pop_back();

			if ( pObj )
				delete pObj;
		}
	}
	else
	{
		size_t iDiff = iNewRange - m_FileList.size();
		for ( size_t i = 0; i < iDiff; ++i )
			m_FileList.push_back(new CFileObj());
	}
}

int CFileObjContainer::GetFileNumber()
{
	ADDTOCALLSTACK("CFileObjContainer::GetFilenumber");
	return m_iFileNumber;
}

void CFileObjContainer::SetFileNumber(int iNewRange)
{
	ADDTOCALLSTACK("CFileObjContainer::SetFilenumber");
	ResizeContainer(iNewRange);
	m_iFileNumber = iNewRange;
}

bool CFileObjContainer::OnTick()
{
	ADDTOCALLSTACK("CFileObjContainer::OnTick");
	EXC_TRY("Tick");

	if ( !m_iGlobalTimeout )
		return true;

	if ( ++m_iCurrentTick >= m_iGlobalTimeout )
	{
		m_iCurrentTick = 0;
		for ( std::vector<CFileObj *>::iterator i = m_FileList.begin(); i != m_FileList.end(); ++i )
		{
			if ( !(*i)->OnTick() )
			{
				// Error and fixWeirdness
			}
		}
	}

	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_DEBUG_END;
	return false;
}

int CFileObjContainer::FixWeirdness()
{
	ADDTOCALLSTACK("CFileObjContainer::FixWeirdness");
	return 0;
}

enum CFO_TYPE
{
	#define ADD(a,b) CFO_##a,
	#include "../tables/CFileObjContainer_props.tbl"
	#undef ADD
	CFO_QTY
};

LPCTSTR const CFileObjContainer::sm_szLoadKeys[CFO_QTY + 1] =
{
	#define ADD(a,b) b,
	#include "../tables/CFileObjContainer_props.tbl"
	#undef ADD
	NULL
};

enum CFOV_TYPE
{
	#define ADD(a,b) CFOV_##a,
	#include "../tables/CFileObjContainer_functions.tbl"
	#undef ADD
	CFOV_QTY
};

LPCTSTR const CFileObjContainer::sm_szVerbKeys[CFOV_QTY + 1] =
{
	#define ADD(a,b) b,
	#include "../tables/CFileObjContainer_functions.tbl"
	#undef ADD
	NULL
};

bool CFileObjContainer::r_GetRef(LPCTSTR &pszKey, CScriptObj *&pRef)
{
	ADDTOCALLSTACK("CFileObjContainer::r_GetRef");
	if ( !strnicmp("FIRSTUSED.", pszKey, 10) )
	{
		pszKey += 10;
		for ( std::vector<CFileObj *>::iterator i = m_FileList.begin(); i != m_FileList.end(); ++i )
		{
			if ( (*i)->IsInUse() )
			{
				pRef = (*i);
				return true;
			}
		}
		return false;
	}

	size_t iNumber = static_cast<size_t>(Exp_GetLLVal(pszKey));
	SKIP_SEPARATORS(pszKey);

	if ( iNumber >= m_FileList.size() )
		return false;

	CFileObj *pFile = m_FileList.at(iNumber);
	if ( pFile )
	{
		pRef = pFile;
		return true;
	}
	return false;
}

bool CFileObjContainer::r_LoadVal(CScript &s, CScriptTriggerArgs* pArgs, CTextConsole* pSrc)
{
	ADDTOCALLSTACK("CFileObjContainer::r_LoadVal");
	EXC_TRY("LoadVal");
	LPCTSTR pszKey = s.GetKey();

	CFO_TYPE index = static_cast<CFO_TYPE>(FindTableSorted(pszKey, sm_szLoadKeys, COUNTOF(sm_szLoadKeys) - 1));
	switch ( index )
	{
		case CFO_OBJECTPOOL:
			SetFileNumber(s.GetArgVal());
			break;
		case CFO_GLOBALTIMEOUT:
			m_iGlobalTimeout = labs(s.GetArgVal() * TICK_PER_SEC);
			break;
		default:
			return false;
	}

	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_SCRIPT;
	EXC_DEBUG_END;
	return false;
}

bool CFileObjContainer::r_WriteVal(LPCTSTR pszKey, CGString &sVal, CTextConsole *pSrc, CScriptTriggerArgs* pArgs)
{
	ADDTOCALLSTACK("CFileObjContainer::r_WriteVal");
	EXC_TRY("WriteVal");

	if ( !strnicmp("FIRSTUSED.", pszKey, 10) )
	{
		pszKey += 10;
		for ( std::vector<CFileObj *>::iterator i = m_FileList.begin(); i != m_FileList.end(); ++i )
		{
			if ( (*i)->IsInUse() )
				return static_cast<CScriptObj *>(*i)->r_WriteVal(pszKey, sVal, pSrc, pArgs);
		}
		return false;
	}

	int index = FindTableHeadSorted(pszKey, sm_szLoadKeys, COUNTOF(sm_szLoadKeys) - 1);
	if ( index < 0 )
	{
		size_t iNumber = static_cast<size_t>(Exp_GetLLVal(pszKey));
		SKIP_SEPARATORS(pszKey);

		if ( iNumber >= m_FileList.size() )
			return false;

		CFileObj *pFile = m_FileList.at(iNumber);
		if ( pFile )
		{
			CScriptObj *pObj = dynamic_cast<CScriptObj *>(pFile);
			if ( pObj )
				return pObj->r_WriteVal(pszKey, sVal, pSrc, pArgs);
		}
		return false;
	}

	switch ( index )
	{
		case CFO_OBJECTPOOL:
			sVal.FormatVal(GetFileNumber());
			break;
		case CFO_GLOBALTIMEOUT:
			sVal.FormatVal(m_iGlobalTimeout / TICK_PER_SEC);
			break;
		default:
			return false;
	}

	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_KEYRET(pSrc);
	EXC_DEBUG_END;
	return false;
}

bool CFileObjContainer::r_Verb(CScript &s, CTextConsole *pSrc, CScriptTriggerArgs* pArgs)
{
	ADDTOCALLSTACK("CFileObjContainer::r_Verb");
	EXC_TRY("Verb");
	ASSERT(pSrc);
	LPCTSTR pszKey = s.GetKey();

	if ( !strnicmp("FIRSTUSED.", pszKey, 10) )
	{
		pszKey += 10;
		for ( std::vector<CFileObj *>::iterator i = m_FileList.begin(); i != m_FileList.end(); ++i )
		{
			if ( (*i)->IsInUse() )
				return static_cast<CScriptObj *>(*i)->r_Verb(s, pSrc, pArgs);
		}
		return false;
	}

	int index = FindTableSorted(pszKey, sm_szVerbKeys, COUNTOF(sm_szVerbKeys) - 1);
	if ( index < 0 )
	{
		if ( strchr(pszKey, '.') )	// 0.blah format
		{
			size_t iNumber = static_cast<size_t>(Exp_GetLLVal(pszKey));
			if ( iNumber < m_FileList.size() )
			{
				SKIP_SEPARATORS(pszKey);
				CFileObj *pFile = m_FileList.at(iNumber);
				if ( pFile )
				{
					CScriptObj *pObj = dynamic_cast<CScriptObj *>(pFile);
					if ( pObj )
					{
						CScript psContinue(pszKey, s.GetArgStr());
						return pObj->r_Verb(psContinue, pSrc, pArgs);
					}
				}
				return false;
			}
		}
		return r_LoadVal(s, pArgs, pSrc);
	}

	switch ( index )
	{
		case CFOV_CLOSEOBJECT:
		case CFOV_RESETOBJECT:
		{
			if ( s.HasArgs() )
			{
				size_t iNumber = static_cast<size_t>(s.GetArgVal());
				if ( iNumber >= m_FileList.size() )
					return false;

				CFileObj *pObj = m_FileList.at(iNumber);
				if ( index == CFOV_RESETOBJECT )
				{
					delete pObj;
					m_FileList.at(iNumber) = new CFileObj();
				}
				else
					pObj->FlushAndClose();
			}
			break;
		}
		default:
			return false;
	}

	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_SCRIPTSRC;
	EXC_DEBUG_END;
	return false;
}
