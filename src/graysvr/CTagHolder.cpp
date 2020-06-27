#include "graysvr.h"	// predef header.

bool CTagHolder::r_Verb(CScript& s, CTextConsole* pSrc, CScriptTriggerArgs* pArgs, CScriptObj *pObj)
{
	LPCTSTR pszKey = s.GetKey();

	if (!stricmp(pszKey, "TAG.REMOVE"))
	{
		LPCTSTR pszArgs = s.GetArgStr();
		m_TagDefs.DeleteKey(pszArgs);
		return true;
	}
	else if (!stricmp(pszKey, "TAG"))
	{
		bool fQuoted = false;
		TCHAR* ppArgs[2];
		size_t iCount;
		LPCTSTR pszRawArgs = s.GetArgRaw();
		TemporaryString varArgList;
		Str_ParseArgumentList(pszRawArgs, varArgList);
		Str_ParseArgumentEnd(pszRawArgs, false);
		iCount = Str_ParseCmds(varArgList, ppArgs, COUNTOF(ppArgs), ",", false);
		TCHAR* pszVarName = Str_TrimWhitespace(ppArgs[0]);
		if (iCount > 1)
		{
			TCHAR* pszValue = iCount == 1 ? s.GetArgStr(&fQuoted) : ppArgs[1];

			if (*pszValue == '"')
			{
				pszValue++;
				pszValue = Str_TrimEnd(pszValue, "\"");
				fQuoted = true;
			}

			if (*ppArgs[1] == '#')
			{
				LPCTSTR ppArgs2 = ppArgs[1] + 1;
				if (!IsStrNumeric(ppArgs2))
				{
					LPCTSTR sVal = m_TagDefs.GetKeyStr(pszVarName, false, pArgs, pSrc, pObj);

					TemporaryString pszBuffer;
					strcpy(pszBuffer, sVal);
					strcat(pszBuffer, ppArgs[1] + 1);
					int iValue = Exp_GetVal(pszBuffer);
					m_TagDefs.SetNum(pszVarName, iValue, false, pArgs, pSrc);
				}
				else
				{
					m_TagDefs.SetStr(pszVarName, fQuoted, pszValue, false, pArgs, pSrc, pObj);
				}
			}
			else
			{
				m_TagDefs.SetStr(pszVarName, fQuoted, pszValue, false, pArgs, pSrc, pObj);
			}
		}
		else
		{
			if (*pszRawArgs == '.')
			{
				pszRawArgs++;
				CObjBase* pObj = static_cast<CGrayUID>(m_TagDefs.GetKeyNum(varArgList)).ObjFind();
				if (pObj)
				{
					CScript subS(pszRawArgs);
					return pObj->r_Verb(subS, pSrc, pArgs);
				}
			}
		}
		return true;
	}

	return false;
}

bool CTagHolder::r_WriteVal(LPCTSTR pszKey, CGString& sVal, CTextConsole* pSrc, CScriptTriggerArgs* pArgs, CScriptObj *pObj)
{
	if (!strnicmp(pszKey, "TAG", 3))
	{
		if (pszKey[3] != '.' && pszKey[3] != '(')
			return false;
		if (pszKey[3] == '.')
		{
			pszKey += 4;
			TCHAR* pszTagName = Str_TrimWhitespace(const_cast<CHAR*>(pszKey));
			pszTagName = Str_TrimEnd(pszTagName, ") \t");
			CVarDefCont* pVarKey = m_TagDefs.GetKey(pszTagName);
			if (!pVarKey)
				return false;
			else
				sVal = pVarKey->GetValStr();
			return true;
		}
		else
		{
			pszKey += 3;
			if (Str_ParseArgumentStart(pszKey, true))
			{
				TemporaryString tagName;
				if (Str_ParseVariableName(pszKey, tagName))
				{
					if (Str_ParseArgumentEnd(pszKey, true))
					{
						CVarDefCont* pVarKey = m_TagDefs.GetKey(tagName, pArgs, pSrc, pObj);
						if (!pVarKey)
							return false;
						else
							sVal = pVarKey->GetValStr();
						return r_WriteValChained(pszKey, sVal, pSrc, pArgs, pObj);
					}
				}
			}
		}
	}

	return false;
}

bool CTagHolder::r_WriteValChained(LPCTSTR pszKey, CGString& sVal, CTextConsole* pSrc, CScriptTriggerArgs* pArgs, CScriptObj *pObj)
{
	if (*pszKey != '.')
		return true;
	pszKey++;

	LPCTSTR pszId = sVal;
	if (*pszId != '#')
		return false;
	pszId++;

	CExpression expr(pArgs, pSrc, pObj);
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
