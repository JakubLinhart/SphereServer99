// CChar is either an NPC or a Player
#include "graysvr.h"	// predef header.
#include <cmath>

///////////////////////////////////////////////////////////
// Stats

void CChar::Stat_AddMod(STAT_TYPE stat, int iVal)
{
	ADDTOCALLSTACK("CChar::Stat_AddMod");
	if ( (stat < STAT_STR) || (stat >= STAT_QTY) )
		return;

	m_Stat[stat].m_mod += iVal;

	int iMaxValue = Stat_GetMax(stat);		// make sure the current value is not higher than new max value
	if ( m_Stat[stat].m_val > iMaxValue )
		m_Stat[stat].m_val = iMaxValue;

	UpdateStatsFlag();
}

void CChar::Stat_SetMod(STAT_TYPE stat, int iVal)
{
	ADDTOCALLSTACK("CChar::Stat_SetMod");
	if ( (stat < STAT_STR) || (stat >= STAT_QTY) )
		return;
	if ( iVal > WORD_MAX )
		iVal = WORD_MAX;
	else if ( iVal < -WORD_MAX )
		iVal = -WORD_MAX;

	int iStatVal = Stat_GetMod(stat);
	if ( IsTrigUsed(TRIGGER_STATCHANGE) && !IsTriggerActive("CREATE") )
	{
		if ( (stat >= STAT_STR) && (stat <= STAT_DEX) )
		{
			CScriptTriggerArgs args;
			args.m_iN1 = static_cast<INT64>(stat) + 8;		// shift by 8 to indicate modSTR, modINT, modDEX
			args.m_iN2 = iStatVal;
			args.m_iN3 = iVal;
			if ( OnTrigger(CTRIG_StatChange, this, &args) == TRIGRET_RET_TRUE )
				return;
			// do not restore argn1 to i, bad things will happen! leave i untouched. (matex)
			iVal = static_cast<int>(args.m_iN3);
		}
	}

	m_Stat[stat].m_mod = iVal;

	if ( (stat == STAT_STR) && (iVal < iStatVal) )
	{
		// ModSTR is being decreased, so check if the char still have enough STR to use current equipped items
		CItem *pItemNext = NULL;
		for ( CItem *pItem = GetContentHead(); pItem != NULL; pItem = pItemNext )
		{
			pItemNext = pItem->GetNext();
			if ( !CanEquipStr(pItem) )
			{
				SysMessagef("%s %s.", g_Cfg.GetDefaultMsg(DEFMSG_EQUIP_NOT_STRONG_ENOUGH), pItem->GetName());
				ItemBounce(pItem, false);
			}
		}
	}

	int iMaxValue = Stat_GetMax(stat);		// make sure the current value is not higher than new max value
	if ( m_Stat[stat].m_val > iMaxValue )
		m_Stat[stat].m_val = iMaxValue;

	UpdateStatsFlag();
}

int CChar::Stat_GetMod(STAT_TYPE stat) const
{
	ADDTOCALLSTACK("CChar::Stat_GetMod");
	if ( (stat < STAT_STR) || (stat >= STAT_QTY) )
		return 0;

	return m_Stat[stat].m_mod;
}

void CChar::Stat_SetVal(STAT_TYPE stat, int iVal)
{
	ADDTOCALLSTACK("CChar::Stat_SetVal");
	if ( (stat < STAT_STR) || (stat >= STAT_QTY) )
		return;
	if ( iVal > WORD_MAX )
		iVal = WORD_MAX;
	else if ( iVal < -WORD_MAX )
		iVal = -WORD_MAX;

	if ( stat >= STAT_BASE_QTY )	// food must trigger @StatChange, redirect to base value
	{
		Stat_SetBase(stat, iVal);
		return;
	}
	m_Stat[stat].m_val = iVal;
}

int CChar::Stat_GetVal(STAT_TYPE stat) const
{
	ADDTOCALLSTACK("CChar::Stat_GetVal");
	if ( (stat < STAT_STR) || (stat >= STAT_QTY) )
		return 0;
	if ( stat >= STAT_BASE_QTY )	// food must trigger @StatChange, redirect to base value
		return Stat_GetBase(stat);
	return m_Stat[stat].m_val;
}

void CChar::Stat_SetMax(STAT_TYPE stat, int iVal)
{
	ADDTOCALLSTACK("CChar::Stat_SetMax");
	if ( (stat < STAT_STR) || (stat >= STAT_QTY) )
		return;
	if ( iVal > WORD_MAX )
		iVal = WORD_MAX;
	else if ( iVal < -WORD_MAX )
		iVal = -WORD_MAX;

	if ( g_Cfg.m_iStatFlag && ((g_Cfg.m_iStatFlag & STAT_FLAG_DENYMAX) || (m_pPlayer && (g_Cfg.m_iStatFlag & STAT_FLAG_DENYMAXP)) || (m_pNPC && (g_Cfg.m_iStatFlag & STAT_FLAG_DENYMAXN))) )
		m_Stat[stat].m_max = 0;
	else
	{
		if ( IsTrigUsed(TRIGGER_STATCHANGE) && !IsTriggerActive("CREATE") )
		{
			if ( (stat >= STAT_STR) && (stat <= STAT_FOOD) )		// only STR, DEX, INT, FOOD fire MaxHits, MaxMana, MaxStam, MaxFood for @StatChange
			{
				CScriptTriggerArgs args;
				args.m_iN1 = static_cast<INT64>(stat) + 4;		// shift by 4 to indicate MaxHits, etc..
				args.m_iN2 = Stat_GetMax(stat);
				args.m_iN3 = iVal;
				if ( OnTrigger(CTRIG_StatChange, this, &args) == TRIGRET_RET_TRUE )
					return;
				// do not restore argn1 to i, bad things will happen! leave it untouched. (matex)
				iVal = static_cast<int>(args.m_iN3);
			}
		}
		m_Stat[stat].m_max = iVal;

		int iMaxValue = Stat_GetMax(stat);		// make sure the current value is not higher than new max value
		if ( m_Stat[stat].m_val > iMaxValue )
			m_Stat[stat].m_val = iMaxValue;

		switch ( stat )
		{
			case STAT_STR:
				UpdateHitsFlag();
				break;
			case STAT_INT:
				UpdateManaFlag();
				break;
			case STAT_DEX:
				UpdateStamFlag();
				break;
		}
	}
}

int CChar::Stat_GetMax(STAT_TYPE stat) const
{
	ADDTOCALLSTACK("CChar::Stat_GetMax");
	if ( (stat < STAT_STR) || (stat >= STAT_QTY) )
		return 0;

	int iVal;
	if ( m_Stat[stat].m_max < 1 )
	{
		if ( stat == STAT_FOOD )
		{
			CCharBase *pCharDef = Char_GetDef();
			ASSERT(pCharDef);
			iVal = pCharDef->m_MaxFood;
		}
		else
			iVal = Stat_GetAdjusted(stat);

		if ( stat == STAT_INT )
		{
			if ( (g_Cfg.m_iRacialFlags & RACIALF_ELF_WISDOM) && IsElf() )
				iVal += 20;		// elves always have +20 max mana (Wisdom racial trait)
		}

		if ( iVal < 0 )
			return m_pPlayer ? 1 : 0;
		return iVal;
	}

	iVal = m_Stat[stat].m_max;
	if ( stat >= STAT_BASE_QTY )
		iVal += m_Stat[stat].m_mod;

	if ( iVal < 0 )
		return m_pPlayer ? 1 : 0;
	return iVal;
}

int CChar::Stat_GetSum() const
{
	ADDTOCALLSTACK("CChar::Stat_GetSum");
	int iStatSum = 0;
	for ( STAT_TYPE i = STAT_STR; i < STAT_BASE_QTY; i = static_cast<STAT_TYPE>(i + 1) )
		iStatSum += Stat_GetBase(i);

	return iStatSum;
}

int CChar::Stat_GetAdjusted(STAT_TYPE stat) const
{
	ADDTOCALLSTACK("CChar::Stat_GetAdjusted");
	int iVal = Stat_GetBase(stat) + Stat_GetMod(stat);
	if ( stat == STAT_KARMA )
		iVal = maximum(g_Cfg.m_iMinKarma, minimum(g_Cfg.m_iMaxKarma, iVal));
	else if ( stat == STAT_FAME )
		iVal = maximum(0, minimum(g_Cfg.m_iMaxFame, iVal));

	return iVal;
}

int CChar::Stat_GetBase(STAT_TYPE stat) const
{
	ADDTOCALLSTACK("CChar::Stat_GetBase");
	if ( (stat < STAT_STR) || (stat >= STAT_QTY) )
		return 0;
	if ( (stat == STAT_FAME) && (m_Stat[stat].m_base < 0) )		// fame can't be negative
		return 0;
	return m_Stat[stat].m_base;
}

void CChar::Stat_SetBase(STAT_TYPE stat, int iVal)
{
	ADDTOCALLSTACK("CChar::Stat_SetBase");
	if ( (stat < STAT_STR) || (stat >= STAT_QTY) )
		return;
	if ( iVal > WORD_MAX )
		iVal = WORD_MAX;
	else if ( iVal < -WORD_MAX )
		iVal = -WORD_MAX;

	int iStatVal = Stat_GetBase(stat);
	if ( IsTrigUsed(TRIGGER_STATCHANGE) && !g_Serv.IsLoading() && !IsTriggerActive("CREATE") )
	{
		// Only Str, Dex, Int, Food fire @StatChange here
		if ( (stat >= STAT_STR) && (stat <= STAT_FOOD) )
		{
			CScriptTriggerArgs args;
			args.m_iN1 = stat;
			args.m_iN2 = iStatVal;
			args.m_iN3 = iVal;
			if ( OnTrigger(CTRIG_StatChange, this, &args) == TRIGRET_RET_TRUE )
				return;
			// do not restore argn1 to i, bad things will happen! leave i untouched. (matex)
			iVal = static_cast<int>(args.m_iN3);

			if ( (stat != STAT_FOOD) && (m_Stat[stat].m_max < 1) ) // MaxFood cannot depend on something, otherwise if the Stat depends on STR, INT, DEX, fire MaxHits, MaxMana, MaxStam
			{
				args.m_iN1 = static_cast<INT64>(stat) + 4; // Shift by 4 to indicate MaxHits, MaxMana, MaxStam
				args.m_iN2 = iStatVal;
				args.m_iN3 = iVal;
				if ( OnTrigger(CTRIG_StatChange, this, &args) == TRIGRET_RET_TRUE )
					return;
				// do not restore argn1 to i, bad things will happen! leave i untouched. (matex)
				iVal = static_cast<int>(args.m_iN3);
			}
		}
	}
	switch ( stat )
	{
		case STAT_STR:
		{
			CCharBase *pCharDef = Char_GetDef();
			if ( pCharDef && !pCharDef->m_Str )
				pCharDef->m_Str = iVal;
			break;
		}
		case STAT_INT:
		{
			CCharBase *pCharDef = Char_GetDef();
			if ( pCharDef && !pCharDef->m_Int )
				pCharDef->m_Int = iVal;
			break;
		}
		case STAT_DEX:
		{
			CCharBase *pCharDef = Char_GetDef();
			if ( pCharDef && !pCharDef->m_Dex )
				pCharDef->m_Dex = iVal;
			break;
		}
		case STAT_FOOD:
			break;
		case STAT_KARMA:
			iVal = maximum(g_Cfg.m_iMinKarma, minimum(g_Cfg.m_iMaxKarma, iVal));
			break;
		case STAT_FAME:
			iVal = maximum(0, minimum(g_Cfg.m_iMaxFame, iVal));
			break;
		default:
			throw CGrayError(LOGL_CRIT, 0, "Stat_SetBase: index out of range");
	}

	m_Stat[stat].m_base = iVal;

	if ( (stat == STAT_STR) && (iVal < iStatVal) )
	{
		// STR is being decreased, so check if the char still have enough STR to use current equipped items
		CItem *pItemNext = NULL;
		for ( CItem *pItem = GetContentHead(); pItem != NULL; pItem = pItemNext )
		{
			pItemNext = pItem->GetNext();
			if ( !CanEquipStr(pItem) )
			{
				SysMessagef("%s %s.", g_Cfg.GetDefaultMsg(DEFMSG_EQUIP_NOT_STRONG_ENOUGH), pItem->GetName());
				ItemBounce(pItem, false);
			}
		}
	}

	int iMaxValue = Stat_GetMax(stat);		// make sure the current value is not higher than new max value
	if ( m_Stat[stat].m_val > iMaxValue )
		m_Stat[stat].m_val = iMaxValue;

	UpdateStatsFlag();
	if ( !g_Serv.IsLoading() && (stat == STAT_KARMA) )
		NotoSave_Update();
}

int CChar::Stat_GetLimit(STAT_TYPE stat) const
{
	ADDTOCALLSTACK("CChar::Stat_GetLimit");
	const CVarDefCont *pTagStorage = NULL;
	TemporaryString sStatName;

	if ( m_pPlayer )
	{
		const CSkillClassDef *pSkillClass = m_pPlayer->GetSkillClass();
		ASSERT(pSkillClass);
		if ( stat == STAT_QTY )
		{
			if ( (pTagStorage = GetKey("OVERRIDE.STATSUM", true)) != NULL )
				return static_cast<int>(pTagStorage->GetValNum());

			return pSkillClass->m_StatSumMax;
		}
		ASSERT((stat >= STAT_STR) && (stat < STAT_BASE_QTY));

		sprintf(sStatName, "OVERRIDE.STATCAP_%d", static_cast<int>(stat));
		int iStatMax;
		if ( (pTagStorage = GetKey(sStatName, true)) != NULL )
			iStatMax = static_cast<int>(pTagStorage->GetValNum());
		else
			iStatMax = pSkillClass->m_StatMax[stat];

		if ( m_pPlayer->Stat_GetLock(stat) >= SKILLLOCK_DOWN )
		{
			int iStatLevel = Stat_GetBase(stat);
			if ( iStatLevel < iStatMax )
				iStatMax = iStatLevel;
		}

		return iStatMax;
	}
	else
	{
		if ( stat == STAT_QTY )
		{
			if ( (pTagStorage = GetKey("OVERRIDE.STATSUM", true)) != NULL )
				return static_cast<int>(pTagStorage->GetValNum());

			return 300;
		}

		sprintf(sStatName, "OVERRIDE.STATCAP_%d", static_cast<int>(stat));
		if ( (pTagStorage = GetKey(sStatName, true)) != NULL )
			return static_cast<int>(pTagStorage->GetValNum());

		return 100;
	}
}

///////////////////////////////////////////////////////////
// Skills

SKILL_TYPE CChar::Skill_GetBest(size_t uRank) const
{
	ADDTOCALLSTACK("CChar::Skill_GetBest");
	// Get the top n best skills.

	if ( uRank >= g_Cfg.m_iMaxSkill )
		uRank = 0;

	DWORD *pdwSkills = new DWORD[uRank + 1];
	ASSERT(pdwSkills);
	memset(pdwSkills, 0, (uRank + 1) * sizeof(DWORD));

	DWORD dwSkillTmp;
	for ( unsigned int i = 0; i < g_Cfg.m_iMaxSkill; ++i )
	{
		if ( !g_Cfg.m_SkillIndexDefs.IsValidIndex(i) )
			continue;

		dwSkillTmp = MAKEDWORD(i, Skill_GetBase(static_cast<SKILL_TYPE>(i)));
		for ( size_t j = 0; j <= uRank; ++j )
		{
			if ( HIWORD(dwSkillTmp) >= HIWORD(pdwSkills[j]) )
			{
				memmove(&pdwSkills[j + 1], &pdwSkills[j], (uRank - j) * sizeof(DWORD));
				pdwSkills[j] = dwSkillTmp;
				break;
			}
		}
	}

	dwSkillTmp = pdwSkills[uRank];
	delete[] pdwSkills;
	return static_cast<SKILL_TYPE>(LOWORD(dwSkillTmp));
}

SKILL_TYPE CChar::Skill_GetMagicRandom(WORD wMinValue)
{
	ADDTOCALLSTACK("CChar::Skill_GetMagicRandom");
	SKILL_TYPE skills[SKILL_QTY];
	int iCount = 0;
	for ( unsigned int i = 0; i < g_Cfg.m_iMaxSkill; ++i )
	{
		SKILL_TYPE skill = static_cast<SKILL_TYPE>(i);
		if ( !g_Cfg.IsSkillFlag(skill, SKF_MAGIC) )
			continue;
		if ( Skill_GetBase(skill) < wMinValue )
			continue;

		skills[iCount] = skill;
		++iCount;
	}
	if ( iCount )
		return skills[Calc_GetRandVal(iCount)];

	return SKILL_NONE;
}

bool CChar::Skill_CanUse(SKILL_TYPE skill)
{
	ADDTOCALLSTACK("CChar::Skill_CanUse");
	if ( g_Cfg.IsSkillFlag(skill, SKF_DISABLED) )	// skill disabled
	{
		SysMessageDefault(DEFMSG_SKILL_NOSKILL);
		return false;
	}

	if ( IsStatFlag(STATF_Ridden) )		// ridden mounts can't use skills
		return false;

	// Expansion checks? different flags for NPCs/Players?
	return true;
}

WORD CChar::Skill_GetAdjusted(SKILL_TYPE skill) const
{
	ADDTOCALLSTACK("CChar::Skill_GetAdjusted");
	// Get the skill adjusted for str,dex,int = 0-1000

	// m_SkillStat is used to figure out how much
	// of the total bonus comes from the stats
	// so if it's 80, then 20% (100% - 80%) comes from
	// the stat (str,int,dex) bonuses

	// example:

	// These are the cchar's stats:
	// m_Skill[x] = 50.0
	// m_Stat[str] = 50, m_Stat[int] = 30, m_Stat[dex] = 20

	// these are the skill "defs":
	// m_SkillStat = 80
	// m_StatBonus[str] = 50
	// m_StatBonus[int] = 50
	// m_StatBonus[dex] = 0

	// Pure bonus is:
	// 50% of str (25) + 50% of int (15) = 40

	// Percent of pure bonus to apply to raw skill is
	// 20% = 100% - m_SkillStat = 100 - 80

	// adjusted bonus is: 8 (40 * 0.2)

	// so the effective skill is 50 (the raw) + 8 (the bonus)
	// which is 58 in total.

	ASSERT(IsSkillBase(skill));
	const CSkillDef *pSkillDef = g_Cfg.GetSkillDef(skill);
	WORD wAdjSkill = 0;

	if ( pSkillDef )
	{
		int iPureBonus = (pSkillDef->m_StatBonus[STAT_STR] * maximum(0, Stat_GetAdjusted(STAT_STR))) + (pSkillDef->m_StatBonus[STAT_INT] * maximum(0, Stat_GetAdjusted(STAT_INT))) + (pSkillDef->m_StatBonus[STAT_DEX] * maximum(0, Stat_GetAdjusted(STAT_DEX)));
		wAdjSkill = static_cast<WORD>(IMULDIV(pSkillDef->m_StatPercent, iPureBonus, 10000));
	}

	return Skill_GetBase(skill) + wAdjSkill;
}

void CChar::Skill_SetBase(SKILL_TYPE skill, WORD wValue)
{
	ADDTOCALLSTACK("CChar::Skill_SetBase");
	ASSERT(IsSkillBase(skill));

	if ( IsTrigUsed(TRIGGER_SKILLCHANGE) )
	{
		CScriptTriggerArgs args;
		args.m_iN1 = static_cast<INT64>(skill);
		args.m_iN2 = static_cast<INT64>(wValue);
		if ( OnTrigger(CTRIG_SkillChange, this, &args) == TRIGRET_RET_TRUE )
			return;

		wValue = static_cast<WORD>(args.m_iN2);
	}
	m_Skill[skill] = wValue;

	if ( m_pClient )
		m_pClient->addSkillWindow(skill);	// update the skills list

	if ( g_Cfg.m_iCombatDamageEra )
	{
		if ( (skill == SKILL_ANATOMY) || (skill == SKILL_TACTICS) || (skill == SKILL_LUMBERJACKING) )
			UpdateStatsFlag();		// those skills are used to calculate the char damage bonus, so we must update the client status gump
	}
}

WORD CChar::Skill_GetMax(SKILL_TYPE skill, bool fIgnoreLock) const
{
	ADDTOCALLSTACK("CChar::Skill_GetMax");
	const CVarDefCont *pTagStorage = NULL;
	TemporaryString sSkillName;

	if ( m_pPlayer )
	{
		ASSERT(IsSkillBase(skill));
		sprintf(sSkillName, "OVERRIDE.SKILLCAP_%d", static_cast<int>(skill));

		WORD wSkillMax;
		if ( (pTagStorage = GetKey(sSkillName, true)) != NULL )
			wSkillMax = static_cast<WORD>(pTagStorage->GetValNum());
		else
		{
			const CSkillClassDef *pSkillClass = m_pPlayer->GetSkillClass();
			ASSERT(pSkillClass);
			wSkillMax = pSkillClass->m_SkillLevelMax[skill];
		}

		if ( !fIgnoreLock )
		{
			if ( m_pPlayer->Skill_GetLock(skill) >= SKILLLOCK_DOWN )
			{
				WORD wSkillLevel = Skill_GetBase(skill);
				if ( wSkillLevel < wSkillMax )
					wSkillMax = wSkillLevel;
			}
		}

		return wSkillMax;
	}
	else
	{
		if ( skill == static_cast<SKILL_TYPE>(g_Cfg.m_iMaxSkill) )
		{
			if ( (pTagStorage = GetKey("OVERRIDE.SKILLSUM", true)) != NULL )
				return static_cast<WORD>(pTagStorage->GetValNum());

			return static_cast<WORD>(g_Cfg.m_iMaxSkill) * 500;
		}

		sprintf(sSkillName, "OVERRIDE.SKILLCAP_%d", static_cast<int>(skill));
		if ( (pTagStorage = GetKey(sSkillName, true)) != NULL )
			return static_cast<WORD>(pTagStorage->GetValNum());

		return 1000;
	}
}

DWORD CChar::Skill_GetSumMax() const
{
	ADDTOCALLSTACK("CChar::Skill_GetSumMax");
	const CVarDefCont *pTagStorage = GetKey("OVERRIDE.SKILLSUM", true);
	if ( pTagStorage )
		return static_cast<DWORD>(pTagStorage->GetValNum());

	const CSkillClassDef *pSkillClass = m_pPlayer->GetSkillClass();
	if ( pSkillClass )
		return pSkillClass->m_SkillSumMax;

	return 0;
}

void CChar::Skill_Decay()
{
	ADDTOCALLSTACK("CChar::Skill_Decay");
	// Decay the character's skill levels.

	SKILL_TYPE skillDeduct = SKILL_NONE;
	WORD wSkillLevel = 0;

	// Look for a skill to deduct from
	for ( unsigned int i = 0; i < g_Cfg.m_iMaxSkill; ++i )
	{
		if ( !g_Cfg.m_SkillIndexDefs.IsValidIndex(i) )
			continue;

		// Check that the skill is set to decrease and that it is not already at 0
		if ( (Skill_GetLock(static_cast<SKILL_TYPE>(i)) != SKILLLOCK_DOWN) || (Skill_GetBase(static_cast<SKILL_TYPE>(i)) <= 0) )
			continue;

		// Prefer to deduct from lesser skills
		if ( (skillDeduct != SKILL_NONE) && (wSkillLevel > Skill_GetBase(static_cast<SKILL_TYPE>(i))) )
			continue;

		skillDeduct = static_cast<SKILL_TYPE>(i);
		wSkillLevel = Skill_GetBase(skillDeduct);
	}

	// deduct a point from the chosen skill
	if ( skillDeduct != SKILL_NONE )
	{
		--wSkillLevel;
		Skill_SetBase(skillDeduct, wSkillLevel);
	}
}

bool CChar::Skill_Degrade(SKILL_TYPE skillused)
{
	// Degrade skills that are over the cap !
	// RETURN:
	//  false = give no credit for ne skil use.
	//  true = give credit. ok

	if (!m_pPlayer)
		return(true);

	int iSkillSum = 0;
	int iSkillSumMax = Skill_GetMax(SKILL_QTY);

	int i;
	for (i = SKILL_First; i < SKILL_QTY; i++)
	{
		iSkillSum += Skill_GetBase((SKILL_TYPE)i);
	}

	// Check for stats degrade first !

	if (iSkillSum < iSkillSumMax)
		return(true);

	// degrade a random skill
	// NOTE: Take skills over the class cap first ???

	int skillrand = Calc_GetRandVal(SKILL_QTY);
	for (i = SKILL_First; true; i++)
	{
		if (i >= SKILL_QTY)
		{
			// If we cannot decrease a skill then give no more credit !
			return(false);
		}
		if (skillrand >= SKILL_QTY)
			skillrand = 0;
		if (skillrand == skillused)	// never degrade the skill i just used !
			continue;
		if (m_pPlayer->Skill_GetLock((SKILL_TYPE)skillrand) != SKILLLOCK_DOWN)
			continue;
		int iSkillLevel = Skill_GetBase((SKILL_TYPE)skillrand);
		if (!iSkillLevel)
			continue;

		// reduce the skill.
		Skill_SetBase((SKILL_TYPE)skillrand, iSkillLevel - 1);
		return(true);
	}
}

void CChar::Skill_Experience(SKILL_TYPE skill, int iDifficulty)
{
	ADDTOCALLSTACK("CChar::Skill_Experience");
	// Give the char credit for using the skill.
	// More credit for the more difficult. or none if too easy
	// ARGS:
	//  iDifficulty = skill target from 0-100

	if ( !IsSkillBase(skill) || !g_Cfg.m_SkillIndexDefs.IsValidIndex(skill) )
		return;
	if ( m_pArea && m_pArea->IsFlag(REGION_FLAG_SAFE) )	// skills don't advance in safe areas.
		return;

	const CSkillDef *pSkillDef = g_Cfg.GetSkillDef(skill);
	if ( !pSkillDef )
		return;

	iDifficulty *= 10;
	if ( iDifficulty < 1 )
		iDifficulty = 1;
	else if ( iDifficulty > 1000 )
		iDifficulty = 1000;

	if ( m_pPlayer && (GetSkillTotal() >= Skill_GetSumMax()) )
		iDifficulty = 0;

	// ex. ADV_RATE=2000,500,25 -> easy
	// ex. ADV_RATE=8000,2000,100 -> hard
	// Assume 100 = a 1 for 1 gain
	// ex: 8000 = we must use it 80 times to gain .1
	// Higher the number = the less probable to advance.
	// Extrapolate a place in the range.

	// give a bonus or a penalty if the task was too hard or too easy.
	// no gain at all if it was WAY TOO easy
	WORD wSkillLevel = Skill_GetBase(skill);
	WORD wSkillLevelFixed = maximum(50, wSkillLevel);
	int iGainRadius = pSkillDef->m_GainRadius;
	if ( (iGainRadius > 0) && ((iDifficulty + iGainRadius) < wSkillLevelFixed) )
	{
		if ( GetKeyNum("NOSKILLMSG") )
			SysMessage(g_Cfg.GetDefaultMsg(DEFMSG_GAINRADIUS_NOT_MET));
		return;
	}

	INT64 iChance = pSkillDef->m_AdvRate.GetChancePercent(wSkillLevel);
	INT64 iSkillMax = Skill_GetMax(skill);	// max advance for this skill.

	CScriptTriggerArgs pArgs(0, iChance, iSkillMax);
	if ( IsTrigUsed(TRIGGER_SKILLGAIN) )
	{
		if ( Skill_OnCharTrigger(skill, CTRIG_SkillGain, &pArgs) == TRIGRET_RET_TRUE )
			return;
	}
	if ( IsTrigUsed(TRIGGER_GAIN) )
	{
		if ( Skill_OnTrigger(skill, SKTRIG_GAIN, &pArgs) == TRIGRET_RET_TRUE )
			return;
	}
	pArgs.getArgNs(0, &iChance, &iSkillMax);

	if ( iChance <= 0 )
		return;

	if ( wSkillLevelFixed < iSkillMax )	// are we in position to gain skill ?
	{
		// slightly more chance of decay than gain
		INT64 iRoll = Calc_GetRandVal(1000);
		if ( iRoll * 3 <= iChance * 4 )
			Skill_Decay();

		if ( iDifficulty > 0 )
		{
#ifdef _DEBUG
			if ( IsPriv(PRIV_DETAIL) && (GetPrivLevel() >= PLEVEL_GM) && (g_Cfg.m_wDebugFlags & DEBUGF_ADVANCE_STATS) )
				SysMessagef("%s=%hu.%hu Difficult=%d Gain Chance=%lld.%lld%% Roll=%lld%%", pSkillDef->GetKey(), wSkillLevel / 10, wSkillLevel % 10, iDifficulty / 10, iChance / 10, iChance % 10, iRoll / 10);
#endif
			if ( iRoll <= iChance )
			{
				++wSkillLevel;
				Skill_SetBase(skill, wSkillLevel);
			}
		}
	}

	// Dish out any stat gains - even for failures
	int iStatSum = Stat_GetSum();
	int iStatCap = Stat_GetLimit(STAT_QTY);

	// Stat effects are unrelated to advance in skill !
	for ( STAT_TYPE i = STAT_STR; i < STAT_BASE_QTY; i = static_cast<STAT_TYPE>(i + 1) )
	{
		// Can't gain STR or DEX if morphed.
		if ( IsStatFlag(STATF_Polymorph) && (i != STAT_INT) )
			continue;

		if ( Stat_GetLock(i) != SKILLLOCK_UP )
			continue;

		int iStatVal = Stat_GetBase(i);
		if ( iStatVal <= 0 )	// some odd condition
			continue;

		if ( iStatSum > iStatCap )	// stat cap already reached
			break;

		int iStatMax = Stat_GetLimit(i);
		if ( iStatVal >= iStatMax )
			continue;	// nothing grows past this. (even for NPC's)

		// You will tend toward these stat vals if you use this skill a lot
		BYTE iStatTarg = pSkillDef->m_Stat[i];
		if ( iStatVal >= iStatTarg )
			continue;	// you've got higher stats than this skill is good for

		// Adjust the chance by the percent of this that the skill uses
		iDifficulty = IMULDIV(iStatVal, 1000, iStatTarg);
		iChance = g_Cfg.m_StatAdv[i].GetChancePercent(iDifficulty);
		if ( pSkillDef->m_StatPercent )
			iChance = (iChance * pSkillDef->m_StatBonus[i] * pSkillDef->m_StatPercent) / 10000;

		if ( iChance == 0 )
			continue;

		if ( Stat_Decrease(i, skill) )
		{
			iStatSum = Stat_GetSum();
			if ( iChance > Calc_GetRandVal(1000) )
			{
				Stat_SetBase(i, iStatVal + 1);
				break;
			}
		}
	}
}

bool CChar::Stats_Regen(INT64 iTimeDiff)
{
	ADDTOCALLSTACK("CChar::Stats_Regen");
	// Calling regens in all stats and checking REGEN%s/REGEN%sVAL where %s is hits/stam... to check values/delays
	// Food decay called here too.
	// calling @RegenStat for each stat if proceed.
	// iTimeDiff is the next tick the stats are going to regen.

	int iHitsHungerLoss = g_Cfg.m_iHitsHungerLoss ? g_Cfg.m_iHitsHungerLoss : 0;

	for ( STAT_TYPE i = STAT_STR; i <= STAT_FOOD; i = static_cast<STAT_TYPE>(i + 1) )
	{
		if ( g_Cfg.m_iRegenRate[i] <= 0 )
			continue;

		WORD wRate = Stats_GetRegenVal(i, true);
		if ( wRate <= 0 )
			continue;

		m_Stat[i].m_regen += static_cast<WORD>(iTimeDiff);
		if ( m_Stat[i].m_regen < wRate )
			continue;

		m_Stat[i].m_regen = 0;

		int iMod = Stats_GetRegenVal(i, false);
		if ( (i == STAT_STR) && (g_Cfg.m_iRacialFlags & RACIALF_HUMAN_TOUGH) && IsHuman() )
			iMod += 2;		// humans always have +2 hitpoint regeneration (Tough racial trait)

		int iStatLimit = Stat_GetMax(i);

		if ( IsTrigUsed(TRIGGER_REGENSTAT) )
		{
			CScriptTriggerArgs Args;
			Args.m_VarsLocal.SetNum("StatID", i, true);		// read-only
			Args.m_VarsLocal.SetNum("Value", iMod, true);
			Args.m_VarsLocal.SetNum("StatLimit", iStatLimit, true);
			if ( i == STAT_FOOD )
				Args.m_VarsLocal.SetNum("HitsHungerLoss", iHitsHungerLoss);

			if ( OnTrigger(CTRIG_RegenStat, this, &Args) == TRIGRET_RET_TRUE )
				continue;

			iMod = static_cast<int>(Args.m_VarsLocal.GetKeyNum("Value"));
			iStatLimit = static_cast<int>(Args.m_VarsLocal.GetKeyNum("StatLimit"));
			if ( i == STAT_FOOD )
				iHitsHungerLoss = static_cast<int>(Args.m_VarsLocal.GetKeyNum("HitsHungerLoss"));
		}
		if ( iMod == 0 )
			continue;

		if ( i == STAT_FOOD )
			OnTickFood(iMod, iHitsHungerLoss);
		else
			UpdateStatVal(i, iMod, iStatLimit);
	}
	return true;
}

WORD CChar::Stats_GetRegenVal(STAT_TYPE stat, bool fGetTicks)
{
	ADDTOCALLSTACK("CChar::Stats_GetRegenVal");
	// Return regen rates and regen val for the given stat.
	// fGetTicks = true returns the regen ticks
	// fGetTicks = false returns the values of regeneration.

	LPCTSTR pszStat = NULL;
	switch ( stat )
	{
		case STAT_STR:
			pszStat = "HITS";
			break;
		case STAT_INT:
			pszStat = "MANA";
			break;
		case STAT_DEX:
			pszStat = "STAM";
			break;
		case STAT_FOOD:
			pszStat = "FOOD";
			break;
		default:
			break;
	}

	if ( stat <= STAT_FOOD )
	{
		char chRegen[14];
		if ( fGetTicks )
		{
			sprintf(chRegen, "REGEN%s", pszStat);
			WORD wRate = static_cast<WORD>(maximum(0, GetDefNum(chRegen)));
			if ( wRate )
				return wRate * TICK_PER_SEC;

			return static_cast<WORD>(maximum(0, g_Cfg.m_iRegenRate[stat]));
		}
		else
		{
			sprintf(chRegen, "REGENVAL%s", pszStat);
			return static_cast<WORD>(maximum(1, GetDefNum(chRegen)));
		}
	}
	return 0;
}

bool CChar::Stat_Decrease(STAT_TYPE stat, SKILL_TYPE skill)
{
	// Stat to decrease
	// Skill = is this being called from Skill_Gain? if so we check this skill's bonuses.
	if ( !m_pPlayer )
		return false;

	// Check for stats degrade.
	int iStatSumAvg = Stat_GetLimit(STAT_QTY);
	int iStatSum = Stat_GetSum() + 1;	// +1 here assuming we are going to have +1 stat at some point thus we are calling this function
	if ( iStatSum <= iStatSumAvg )		// no need to lower any stat
		return true;

	int iMinVal;
	if ( skill )
		iMinVal = Stat_GetMax(stat);
	else
		iMinVal = Stat_GetAdjusted(stat);

	// We are at a point where our skills can degrade a bit.
	int iStatSumMax = iStatSumAvg + iStatSumAvg / 4;
	int iChanceForLoss = Calc_GetSCurve(iStatSumMax - iStatSum, (iStatSumMax - iStatSumAvg) / 4);
	if ( iChanceForLoss > Calc_GetRandVal(1000) )
	{
		// Find the stat that was used least recently and degrade it.
		STAT_TYPE iMin = STAT_NONE;
		int iVal = 0;
		for ( STAT_TYPE i = STAT_STR; i < STAT_BASE_QTY; i = static_cast<STAT_TYPE>(i + 1) )
		{
			if ( i == stat )
				continue;
			if ( Stat_GetLock(i) != SKILLLOCK_DOWN )
				continue;

			if ( skill )
			{
				const CSkillDef *pSkillDef = g_Cfg.GetSkillDef(skill);
				iVal = pSkillDef->m_StatBonus[i];
			}
			else
				iVal = Stat_GetBase(i);

			if ( iMinVal > iVal )
			{
				iMin = i;
				iMinVal = iVal;
			}
		}

		if ( iMin == STAT_NONE )
			return false;

		int iStatVal = Stat_GetBase(iMin);
		if ( iStatVal > 10 )
		{
			Stat_SetBase(iMin, iStatVal - 1);
			return true;
		}
	}
	return false;
}

bool CChar::Skill_CheckSuccess(SKILL_TYPE skill, int iDifficulty, bool fUseBellCurve) const
{
	ADDTOCALLSTACK("CChar::Skill_CheckSuccess");
	// PURPOSE:
	//  Check a skill for success or fail.
	//  DO NOT give experience here.
	// ARGS:
	//  iDifficulty		= 0-100 = The point at which the equiv skill level has a 50% chance of success.
	//  fUseBellCurve	= check skill success chance using bell curve or a simple percent check?
	// RETURN:
	//  true = success in skill.

	if ( IsPriv(PRIV_GM) && (skill != SKILL_PARRYING) )		// GM's can't always succeed Parrying or they won't receive any damage on combat even without STATF_Invul set
		return true;
	if ( !IsSkillBase(skill) || (iDifficulty < 0) )	// auto failure
		return false;

	iDifficulty *= 10;
	if ( fUseBellCurve )
		iDifficulty = Calc_GetSCurve(Skill_GetAdjusted(skill) - iDifficulty, SKILL_VARIANCE);

	return (iDifficulty >= Calc_GetRandVal(1000));
}

bool CChar::Skill_UseQuick(SKILL_TYPE skill, int iDifficulty, bool fAllowGain, bool fUseBellCurve)
{
	ADDTOCALLSTACK("CChar::Skill_UseQuick");
	// ARGS:
	//  skill			= skill to use
	//  iDifficulty		= 0-100
	//  fAllowGain		= can gain skill from this?
	//  fUseBellCurve	= check skill success chance using bell curve or a simple percent check?
	// Use a skill instantly. No wait at all.
	// No interference with other skills.

	if ( g_Cfg.IsSkillFlag(skill, SKF_SCRIPTED) )
		return false;

	INT64 iResult = Skill_CheckSuccess(skill, iDifficulty, fUseBellCurve);
	INT64 iDiff = iDifficulty;
	CScriptTriggerArgs pArgs(0, iDiff, iResult);
	TRIGRET_TYPE ret = TRIGRET_RET_DEFAULT;

	if ( IsTrigUsed(TRIGGER_SKILLUSEQUICK) )
	{
		ret = Skill_OnCharTrigger(skill, CTRIG_SkillUseQuick, &pArgs);

		if ( ret == TRIGRET_RET_TRUE )
			return true;
		if ( ret == TRIGRET_RET_FALSE )
			return false;
		pArgs.getArgNs(0, &iDiff, &iResult);
	}
	if ( IsTrigUsed(TRIGGER_USEQUICK) )
	{
		ret = Skill_OnTrigger(skill, SKTRIG_USEQUICK, &pArgs);

		if ( ret == TRIGRET_RET_TRUE )
			return true;
		if ( ret == TRIGRET_RET_FALSE )
			return false;
		pArgs.getArgNs(0, &iDiff, &iResult);
	}

	if ( iResult > 0 )	// success
	{
		if ( fAllowGain )
			Skill_Experience(skill, static_cast<int>(iDiff));
		return true;
	}
	else				// fail
	{
		if ( fAllowGain )
			Skill_Experience(skill, static_cast<int>(-iDiff));
		return false;
	}
}

void CChar::Skill_Cleanup()
{
	ADDTOCALLSTACK("CChar::Skill_Cleanup");
	// We are done with the skill (succeeded / failed / aborted)
	m_Act_Difficulty = 0;
	m_Act_SkillCurrent = SKILL_NONE;
	SetTimeout(m_pPlayer ? -1 : TICK_PER_SEC);	// we should get a brain tick next time
}

LPCTSTR CChar::Skill_GetName(bool fUse) const
{
	ADDTOCALLSTACK("CChar::Skill_GetName");
	// Name the current skill we are doing.

	SKILL_TYPE skill = Skill_GetActive();
	if ( IsSkillBase(skill) )
	{
		if ( !fUse )
			return g_Cfg.GetSkillKey(skill);

		TCHAR *pszText = Str_GetTemp();
		sprintf(pszText, "%s %s", g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_USING), g_Cfg.GetSkillKey(skill));
		return pszText;
	}

	switch ( skill )
	{
		case NPCACT_FOLLOW_TARG:	return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_FOLLOWING);
		case NPCACT_STAY:			return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_STAYING);
		case NPCACT_GOTO:			return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_GOINGTO);
		case NPCACT_WANDER:			return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_WANDERING);
		case NPCACT_LOOKING:		return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_LOOKING);
		case NPCACT_FLEE:			return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_FLEEING);
		case NPCACT_TALK:			return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_TALKING);
		case NPCACT_TALK_FOLLOW:	return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_TALKFOLLOW);
		case NPCACT_GUARD_TARG:		return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_GUARDING);
		case NPCACT_GO_HOME:		return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_GOINGHOME);
		case NPCACT_BREATH:			return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_BREATHING);
		case NPCACT_RIDDEN:			return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_RIDDEN);
		case NPCACT_THROWING:		return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_THROWING);
		case NPCACT_TRAINING:		return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_TRAINING);
		case NPCACT_FOOD:			return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_SEARCHINGFOOD);
		case NPCACT_RUNTO:			return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_RUNNINGTO);
		default:					return g_Cfg.GetDefaultMsg(DEFMSG_SKILLACT_IDLING);
	}
}

void CChar::Skill_SetTimeout()
{
	ADDTOCALLSTACK("CChar::Skill_SetTimeout");
	SKILL_TYPE skill = Skill_GetActive();
	ASSERT(IsSkillBase(skill));

	const CSkillDef *pSkillDef = g_Cfg.GetSkillDef(skill);
	if ( !pSkillDef )
		return;

	SetTimeout(pSkillDef->m_Delay.GetLinear(Skill_GetBase(skill)));
}

INT64 CChar::Skill_GetTimeout()
{
	ADDTOCALLSTACK("CChar::Skill_SetTimeout");
	SKILL_TYPE skill = Skill_GetActive();
	ASSERT(IsSkillBase(skill));

	const CSkillDef *pSkillDef = g_Cfg.GetSkillDef(skill);
	if ( !pSkillDef )
		return 0;

	return pSkillDef->m_Delay.GetLinear(Skill_GetBase(skill));
}

int CChar::SkillResourceTest(const CResourceQtyArray *pResources)
{
	ADDTOCALLSTACK("CChar::SkillResourceTest");
	return pResources->IsResourceMatchAll(this);
}

bool CChar::Skill_MakeItem(ITEMID_TYPE id, CSphereUID uidTarg, CSkillDef::T_TYPE_ stage)
{
	//ADDTOCALLSTACK("CChar::Skill_MakeItem");
	//// "MAKEITEM"
	////
	//// SKILL_ALCHEMY
	//// SKILL_BLACKSMITHING
	//// SKILL_BOWCRAFT
	//// SKILL_CARPENTRY
	//// SKILL_INSCRIPTION
	//// SKILL_TAILORING:
	//// SKILL_TINKERING,
	////
	//// Confer the new item.
	//// Test for consumable items.
	//// on CSkillDef::T_Fail do a partial consume of the resources.
	////
	//// ARGS:
	////  uidTarg = item targetted to try to make this . (this item should be used to make somehow)
	////  skill = Skill_GetActive()
	////
	//// RETURN:
	////   true = success.
	////

	//if (id <= ITEMID_NOTHING)
	//	return(true);

	//CItemDefPtr pItemDef = g_Cfg.FindItemDef(id);
	//if (pItemDef == NULL)
	//	return(false);

	//// Trigger Target item for creating the new item.
	//CItemPtr pItemTarg = g_World.ItemFind(uidTarg);
	//if (pItemTarg && stage == CSkillDef::T_Select)
	//{
	//	if (pItemDef->m_SkillMake.FindResourceMatch(pItemTarg) < 0 &&
	//		pItemDef->m_BaseResources.FindResourceMatch(pItemTarg) < 0)
	//	{
	//		// Not intersect with the specified item
	//		return(false);
	//	}
	//}

	//int iReplicationQty = 1;
	//if (pItemDef->Can(CAN_I_REPLICATE))
	//{
	//	// For arrows/bolts, how many do they want ?
	//	// Set the quantity that they want to make.
	//	if (pItemTarg != NULL)
	//	{
	//		iReplicationQty = pItemTarg->GetAmount();
	//	}
	//}

	//// Test the hypothetical required skills and tools
	//if (!pItemDef->m_SkillMake.IsResourceMatchAll(this))
	//{
	//	if (stage == CSkillDef::T_Start)
	//	{
	//		WriteString("You cannot make this");
	//	}
	//	return(false);
	//}

	//// test or consume the needed resources.
	//if (stage == CSkillDef::T_Fail)
	//{
	//	// If fail only consume part of them.
	//	ResourceConsumePart(&(pItemDef->m_BaseResources), iReplicationQty, Calc_GetRandVal(50));
	//	return(false);
	//}

	//// How many do i actually have resource for?
	//iReplicationQty = ResourceConsume(&(pItemDef->m_BaseResources), iReplicationQty, stage != CSkillDef::T_Success);
	//if (!iReplicationQty)
	//{
	//	if (stage == CSkillDef::T_Start)
	//	{
	//		WriteString("You lack the resources to make this");
	//	}
	//	return(false);
	//}

	//if (stage == CSkillDef::T_Start)
	//{
	//	// Start the skill.
	//	// Find the primary skill required.

	//	int i = pItemDef->m_SkillMake.FindResourceType(RES_Skill);
	//	if (i < 0)
	//	{
	//		// Weird.
	//		if (stage == CSkillDef::T_Start)
	//		{
	//			WriteString("You cannot figure this out");
	//		}
	//		return(false);
	//	}

	//	CResourceQty RetMainSkill = pItemDef->m_SkillMake[i];

	//	m_Act_Targ = uidTarg;	// targetted item to start the make process.
	//	m_Act.m_atCreate.m_ItemID = id;
	//	m_Act.m_atCreate.m_Amount = iReplicationQty;

	//	return Skill_Start((SKILL_TYPE)RetMainSkill.GetResIndex(), RetMainSkill.GetResQty() / 10);
	//}

	//if (stage == CSkillDef::T_Success)
	//{
	//	return(Skill_MakeItem_Success(iReplicationQty));
	//}

	//return(true);
}

bool CChar::Skill_MakeItem_Success(int iQty)
{
	//ADDTOCALLSTACK("CChar::Skill_MakeItem_Success");
	//// deliver the goods.
	//if (iQty <= 0)
	//	return true;

	//CItemVendablePtr pItem = REF_CAST(CItemVendable, CItem::CreateTemplate(m_Act.m_atCreate.m_ItemID, NULL, this));
	//if (pItem == NULL)
	//	return(false);

	//CGString sMakeMsg;
	//int iSkillLevel = Skill_GetBase(Skill_GetActive());	// primary skill value.

	//if (iQty != 1)	// m_Act.m_atCreate.m_Amount
	//{
	//	// Some item with the REPLICATE flag ?
	//	pItem->SetAmount(m_Act.m_atCreate.m_Amount); // Set the quantity if we are making bolts, arrows or shafts
	//}
	//else if (pItem->IsType(IT_SCROLL))
	//{
	//	// scrolls have the skill level of the inscriber ?
	//	pItem->m_itSpell.m_spelllevel = iSkillLevel;
	//}
	//else if (pItem->IsType(IT_POTION))
	//{
	//	// Create the potion, set various properties,
	//	// put in pack
	//	Emote("pour the completed potion into a bottle");
	//	Sound(0x240);	// pouring noise.
	//}
	//else
	//{
	//	// Only set the quality on single items.
	//	int quality = IMULDIV(iSkillLevel, 2, 10);	// default value for quality.
	//	// Quality depends on the skill of the craftsman, and a random chance.
	//	// minimum quality is 1, maximum quality is 200.  100 is average.
	//	// How much variance?  This is the difference in quality levels from
	//	// what I can normally make.
	//	int variance = 2 - (int)log10(Calc_GetRandVal(250) + 1); // this should result in a value between 0 and 2.
	//	// Determine if lower or higher quality
	//	if (Calc_GetRandVal(2))
	//	{
	//		// Better than I can normally make
	//	}
	//	else
	//	{
	//		// Worse than I can normally make
	//		variance = -(variance);
	//	}
	//	// The quality levels are as follows:
	//	// 1 - 25 Shoddy
	//	// 26 - 50 Poor
	//	// 51 - 75 Below Average
	//	// 76 - 125 Average
	//	// 125 - 150 Above Average
	//	// 151 - 175 Excellent
	//	// 175 - 200 Superior
	//	// Determine which range I'm in
	//	int qualityBase;
	//	if (quality < 25)
	//		qualityBase = 0;
	//	else if (quality < 50)
	//		qualityBase = 1;
	//	else if (quality < 75)
	//		qualityBase = 2;
	//	else if (quality < 125)
	//		qualityBase = 3;
	//	else if (quality < 150)
	//		qualityBase = 4;
	//	else if (quality < 175)
	//		qualityBase = 5;
	//	else
	//		qualityBase = 6;
	//	qualityBase += variance;
	//	if (qualityBase < 0)
	//		qualityBase = 0;
	//	if (qualityBase > 6)
	//		qualityBase = 6;

	//	switch (qualityBase)
	//	{
	//	case 0:
	//		// Shoddy quality
	//		sMakeMsg.Format("Due to your poor skill, the item is of shoddy quality");
	//		quality = Calc_GetRandVal(25) + 1;
	//		break;
	//	case 1:
	//		// Poor quality
	//		sMakeMsg.Format("You were barely able to make this item.  It is of poor quality");
	//		quality = Calc_GetRandVal(25) + 26;
	//		break;
	//	case 2:
	//		// Below average quality
	//		sMakeMsg.Format("You make the item, but it is of below average quality");
	//		quality = Calc_GetRandVal(25) + 51;
	//		break;
	//	case 3:
	//		// Average quality
	//		quality = Calc_GetRandVal(50) + 76;
	//		break;
	//	case 4:
	//		// Above average quality
	//		sMakeMsg.Format("The item is of above average quality");
	//		quality = Calc_GetRandVal(25) + 126;
	//		break;
	//	case 5:
	//		// Excellent quality
	//		sMakeMsg.Format("The item is of excellent quality");
	//		quality = Calc_GetRandVal(25) + 151;
	//		break;
	//	case 6:
	//		// Superior quality
	//		sMakeMsg.Format("Due to your exceptional skill, the item is of superior quality");
	//		quality = Calc_GetRandVal(25) + 176;
	//		break;
	//	default:
	//		// How'd we get here?
	//		quality = 1000;
	//		break;
	//	}
	//	pItem->SetQuality(quality);
	//	if (iSkillLevel > 999 && (quality > 175))
	//	{
	//		// A GM made this, and it is of high quality
	//		CGString csNewName;
	//		csNewName.Format("%s crafted by %s", (LPCTSTR)pItem->GetName(), (LPCTSTR)GetName());
	//		pItem->SetName(csNewName);
	//	}
	//}

	//pItem->SetAttr(ATTR_MOVE_ALWAYS | ATTR_CAN_DECAY);	// Any made item is movable.

	//CSphereExpArgs execArgs(this, this, Skill_GetActive(), 0, pItem);
	//if (OnTrigger("@SkillMakeItem", execArgs) == TRIGRET_RET_VAL)
	//{
	//	pItem->DeleteThis();
	//	return(false);
	//}

	//if (!sMakeMsg.IsEmpty())
	//	WriteString(sMakeMsg);

	//ItemBounce(pItem);
	//return(true);
}

int CChar::Skill_NaturalResource_Setup(CItem *pResBit)
{
	ADDTOCALLSTACK("CChar::Skill_NaturalResource_Setup");
	// RETURN:
	//  difficulty = 0-100
	ASSERT(pResBit);

	// Find the resource type located here based on color.
	const CRegionResourceDef *pResourceDef = dynamic_cast<const CRegionResourceDef *>(g_Cfg.ResourceGetDef(pResBit->m_itResource.m_rid_res));
	if ( !pResourceDef )
		return -1;

	return pResourceDef->m_Skill.GetRandom() / 10;
}

CItem *CChar::Skill_NaturalResource_Create(CItem *pResBit, SKILL_TYPE skill)
{
	ADDTOCALLSTACK("CChar::Skill_NaturalResource_Create");
	// Create some natural resource item.
	// skill = Effects qty of items returned.
	// SKILL_MINING
	// SKILL_FISHING
	// SKILL_LUMBERJACKING
	ASSERT(pResBit);

	// Find the resource type located here based on color.
	CRegionResourceDef *pResourceDef = dynamic_cast<CRegionResourceDef *>(g_Cfg.ResourceGetDef(pResBit->m_itResource.m_rid_res));
	if ( !pResourceDef )
		return NULL;

	// Skill effects how much of the resource i can get all at once.
	if ( pResourceDef->m_ReapItem == ITEMID_NOTHING )
		return NULL;

	// Reap amount is semi-random
	WORD wAmount = static_cast<WORD>(pResourceDef->m_ReapAmount.GetRandomLinear(Skill_GetBase(skill)));
	if ( !wAmount )		// if REAPAMOUNT wasn't defined
	{
		wAmount = static_cast<WORD>(pResourceDef->m_Amount.GetRandomLinear(Skill_GetBase(skill)) / 2);
		WORD wMaxAmount = pResBit->GetAmount();
		if ( wAmount < 1 )
			wAmount = 1;
		if ( wAmount > wMaxAmount )
			wAmount = wMaxAmount;
	}

	// [Region]ResourceGather behavior
	CScriptTriggerArgs Args(0, 0, pResBit);
	Args.m_VarsLocal.SetNum("ResourceID", pResourceDef->m_ReapItem);
	Args.m_iN1 = wAmount;
	TRIGRET_TYPE tRet = TRIGRET_RET_DEFAULT;
	if ( IsTrigUsed(TRIGGER_REGIONRESOURCEGATHER) )
		tRet = OnTrigger(CTRIG_RegionResourceGather, this, &Args);
	if ( IsTrigUsed(TRIGGER_RESOURCEGATHER) )
		tRet = pResourceDef->OnTrigger("@ResourceGather", this, &Args);
	if ( tRet == TRIGRET_RET_TRUE )
		return NULL;

	wAmount = static_cast<WORD>(pResBit->ConsumeAmount(static_cast<DWORD>(Args.m_iN1)));	// amount i used up.
	if ( wAmount <= 0 )
		return NULL;

	// Create 'id' variable with the local given through->by the trigger(s) instead on top of method
	ITEMID_TYPE id = static_cast<ITEMID_TYPE>(RES_GET_INDEX(Args.m_VarsLocal.GetKeyNum("ResourceID")));
	CItem *pItem = CItem::CreateScript(id, this);
	ASSERT(pItem);
	pItem->SetAmount(wAmount);
	return pItem;
}

bool CChar::Skill_SmeltOre(CItem *pOre)
{
	ADDTOCALLSTACK("CChar::Skill_SmeltOre");
	// Smelt ores into ingots
	if ( !pOre )
		return false;

	m_Act_p = g_World.FindItemTypeNearby(GetTopPoint(), IT_FORGE, 2, false, true);
	if ( !m_Act_p.IsValidPoint() || !CanTouch(m_Act_p) )
	{
		SysMessageDefault(DEFMSG_SMELT_NOFORGE);
		return false;
	}

	const CItemBase *pIngotDef = CItemBase::FindItemBase(static_cast<ITEMID_TYPE>(RES_GET_INDEX(pOre->Item_GetDef()->m_ttOre.m_IngotID)));
	if ( !pIngotDef || !pIngotDef->IsType(IT_INGOT) || pOre->IsType(IT_INGOT) )
	{
		SysMessageDefault(DEFMSG_SMELT_CANT);
		return false;
	}

	if ( Skill_GetAdjusted(SKILL_MINING) < static_cast<WORD>(pIngotDef->m_ttIngot.m_iSkillReq) )
	{
		SysMessageDefault(DEFMSG_SMELT_NOSKILL);
		return false;
	}

	UpdateDir(m_Act_p);

	if ( !Skill_CheckSuccess(SKILL_MINING, pIngotDef->m_ttIngot.m_iSkillReq / 10) )
	{
		pOre->ConsumeAmount(maximum(1, (pOre->GetAmount() / 2)));
		SysMessageDefault(DEFMSG_SMELT_FAIL);
		return false;
	}

	CItem *pIngot = CItem::CreateScript(pIngotDef->GetID(), this);
	if ( !pIngot )
	{
		SysMessageDefault(DEFMSG_SMELT_CANT);
		return false;
	}

	pOre->Delete();
	pIngot->SetAmount(pOre->GetAmount());
	ItemBounce(pIngot, false);
	SysMessageDefault(DEFMSG_SMELT_SUCCESS);
	return true;
}

bool CChar::Skill_SmeltItem(CItem *pItem)
{
	ADDTOCALLSTACK("CChar::Skill_SmeltItem");
	// Smelt armors/weapons into ingots
	if ( !pItem )
		return false;

	m_Act_p = g_World.FindItemTypeNearby(GetTopPoint(), IT_FORGE, 2, false, true);
	if ( !m_Act_p.IsValidPoint() || !CanTouch(m_Act_p) )
	{
		SysMessageDefault(DEFMSG_SMELT_NOFORGE);
		return false;
	}

	m_Act_p = g_World.FindItemTypeNearby(GetTopPoint(), IT_ANVIL, 2, false, true);
	if ( !m_Act_p.IsValidPoint() || !CanTouch(m_Act_p) )
	{
		SysMessageDefault(DEFMSG_SMELT_NOANVIL);
		return false;
	}

	const CItemBase *pItemDef = pItem->Item_GetDef();
	const CItemBase *pIngotDef = NULL;
	WORD wAmount = 0;
	for ( size_t i = 0; i < pItemDef->m_BaseResources.GetCount(); ++i )
	{
		RESOURCE_ID rid = pItemDef->m_BaseResources[i].GetResourceID();
		if ( rid.GetResType() != RES_ITEMDEF )
			continue;

		const CItemBase *pResourceDef = CItemBase::FindItemBase(static_cast<ITEMID_TYPE>(rid.GetResIndex()));
		if ( !pResourceDef || !pResourceDef->IsType(IT_INGOT) )
			continue;

		pIngotDef = pResourceDef;
		wAmount = static_cast<WORD>((pItemDef->m_BaseResources[i].GetResQty() * 2) / 3);
		break;
	}

	if ( !pIngotDef || pItemDef->IsType(IT_INGOT) )
	{
		SysMessageDefault(DEFMSG_SMELT_CANT);
		return false;
	}

	if ( Skill_GetAdjusted(SKILL_BLACKSMITHING) < static_cast<WORD>(pIngotDef->m_ttIngot.m_iSkillReq) )
	{
		SysMessageDefault(DEFMSG_BLACKSMITHING_NOSKILL);
		return false;
	}

	UpdateDir(m_Act_p);

	CItem *pIngot = CItem::CreateScript(pIngotDef->GetID(), this);
	if ( !pIngot )
	{
		SysMessageDefault(DEFMSG_SMELT_CANT);
		return false;
	}

	pItem->Delete();
	pIngot->SetAmount(pItem->GetAmount() * wAmount);
	ItemBounce(pIngot, false);
	Sound(SOUND_DRIP3);
	Sound(SOUND_LIQUID);
	SysMessageDefault(DEFMSG_SMELT_ITEM_SUCCESS);
	return true;
}

bool CChar::Skill_Tracking(CGrayUID uidTarg, int iDistMax)
{
	ADDTOCALLSTACK("CChar::Skill_Tracking");
	// SKILL_TRACKING

	if ( !m_pClient )		// abort action if the client get disconnected
		return false;

	const CObjBaseTemplate *pObj = uidTarg.ObjFind();
	if ( !pObj )
		return false;

	const CChar *pChar = uidTarg.CharFind();
	if ( !pChar )
		return false;

	int iDist = GetTopDist3D(pObj);	// disconnected = SHRT_MAX
	if ( iDist > iDistMax )
		return false;

	// Prevent track hidden GMs
	if ( pChar->IsStatFlag(STATF_Insubstantial) && (pChar->GetPrivLevel() > GetPrivLevel()) )
		return false;

	DIR_TYPE dir = GetDir(pObj);
	ASSERT((dir > DIR_INVALID) && (static_cast<size_t>(dir) < COUNTOF(CPointBase::sm_szDirs)));

	// Select tracking message based on distance
	LPCTSTR pszDef;
	if ( iDist <= 0 )
		pszDef = g_Cfg.GetDefaultMsg(DEFMSG_TRACKING_RESULT_0);
	else if ( iDist < 16 )
		pszDef = g_Cfg.GetDefaultMsg(DEFMSG_TRACKING_RESULT_1);
	else if ( iDist < 32 )
		pszDef = g_Cfg.GetDefaultMsg(DEFMSG_TRACKING_RESULT_2);
	else if ( iDist < 100 )
		pszDef = g_Cfg.GetDefaultMsg(DEFMSG_TRACKING_RESULT_3);
	else
		pszDef = g_Cfg.GetDefaultMsg(DEFMSG_TRACKING_RESULT_4);

	if ( pszDef[0] )
	{
		TCHAR *pszMsg = Str_GetTemp();
		sprintf(pszMsg, pszDef, pChar->GetName(), pChar->IsDisconnected() ? g_Cfg.GetDefaultMsg(DEFMSG_TRACKING_RESULT_DISC) : CPointBase::sm_szDirs[dir]);
		ObjMessage(pszMsg, this);
	}
	return true;		// keep the skill active
}

///////////////////////////////////////////////////////////
// Skill handlers

int CChar::Skill_Tracking(SKTRIG_TYPE stage)
{
	ADDTOCALLSTACK("CChar::Skill_Tracking");
	// SKILL_TRACKING
	// m_Act_Targ = what am i tracking ?

	if ( stage == SKTRIG_START )
		return 0;	// already checked difficulty earlier

	if ( stage == SKTRIG_FAIL )
	{
		// This skill didn't fail, it just ended/went out of range, etc...
		ObjMessage(g_Cfg.GetDefaultMsg(DEFMSG_TRACKING_END), this);		// say this instead of the failure message
		return -SKTRIG_ABORT;
	}

	if ( stage == SKTRIG_STROKE )
	{
		if ( Skill_Stroke(false) == -SKTRIG_ABORT )
			return -SKTRIG_ABORT;

		WORD wSkillLevel = Skill_GetAdjusted(SKILL_TRACKING);
		if ( (g_Cfg.m_iRacialFlags & RACIALF_HUMAN_JACKOFTRADES) && IsHuman() )
			wSkillLevel = maximum(wSkillLevel, 200);			// humans always have a 20.0 minimum skill (racial traits)

		if ( !Skill_Tracking(m_Act_Targ, wSkillLevel / 10 + 10) )
			return -SKTRIG_ABORT;

		Skill_SetTimeout();			// next update
		return -SKTRIG_STROKE;		// keep it active
	}

	return -SKTRIG_ABORT;
}

int CChar::Skill_Alchemy(CSkillDef::T_TYPE_ stage)
{
	// SKILL_ALCHEMY
	// m_Act.m_atCreate.m_ItemID = potion we are making.
	// We consume resources on each stroke.
	// This was start in Skill_MakeItem()

	//CItemDefPtr pPotionDef = g_Cfg.FindItemDef(m_Act.m_atCreate.m_ItemID);
	//if (pPotionDef == NULL)
	//{
	//	WriteString("You have no clue how to make this potion.");
	//	return -CSkillDef::T_Abort;
	//}

	//if (stage == CSkillDef::T_Start)
	//{
	//	// See if skill allows a potion made out of targ'd reagent		// Sound( 0x243 );
	//	m_Act.m_atCreate.m_Stroke_Count = 0; // counts up.
	//	return(m_Act.m_Difficulty);
	//}
	//if (stage == CSkillDef::T_Fail)
	//{
	//	// resources have already been consumed.
	//	Emote("toss the failed mixture from the mortar");
	//	return(0);	// normal failure
	//}
	//if (stage == CSkillDef::T_Success)
	//{
	//	// Resources have already been consumed.
	//	// Now deliver the goods.
	//	Skill_MakeItem_Success(1);
	//	return(0);
	//}

	//ASSERT(stage == CSkillDef::T_Stroke);
	//if (stage != CSkillDef::T_Stroke)
	//	return(-CSkillDef::T_QTY);

	//if (m_Act.m_atCreate.m_Stroke_Count >= pPotionDef->m_BaseResources.GetSize())
	//{
	//	// done.
	//	return 0;
	//}

	//// Keep trying and grinding
	////  OK, we know potion being attempted and the bottle
	////  it's going in....do a loop for each reagent

	//CResourceQty item = pPotionDef->m_BaseResources[m_Act.m_atCreate.m_Stroke_Count];
	//CSphereUID rid = item.GetResourceID();

	//CItemDefPtr pReagDef = REF_CAST(CItemDef, g_Cfg.ResourceGetDef(rid));
	//if (pReagDef == NULL)
	//{
	//	return -CSkillDef::T_Abort;
	//}

	//if (pReagDef->IsType(IT_POTION_EMPTY) && m_Act.m_Difficulty < 0) // going to fail anyhow.
	//{
	//	// NOTE: Assume the bottle is ALWAYS LAST !
	//	// Don't consume the bottle.
	//	return -CSkillDef::T_Abort;
	//}

	//if (ContentConsume(rid, item.GetResQty()))
	//{
	//	Printf("Hmmm, you lack %s for this potion.", (LPCTSTR)pReagDef->GetName());
	//	return -CSkillDef::T_Abort;
	//}

	//if (GetTopSector()->GetCharComplexity() < 5 && pReagDef->IsType(IT_REAGENT))
	//{
	//	CGString sSpeak;
	//	sSpeak.Format((m_Act.m_atCreate.m_Stroke_Count == 0) ?
	//		"start grinding some %s in the mortar" :
	//		"add %s and continue grinding", (LPCTSTR)pReagDef->GetName());
	//	Emote(sSpeak);
	//}

	//Sound(0x242);
	//m_Act.m_atCreate.m_Stroke_Count++;
	//Skill_SetTimeout();
	//return -CSkillDef::T_Stroke;	// keep active.
	return 0;
}

int CChar::Skill_Mining(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Mining");
	// SKILL_MINING
	// m_Act.m_pt = the point we want to mine at.
	// m_Act.m_TargPrv = Shovel
	//
	// Test the chance of precious ore.
	// resource check  to IT_ORE. How much can we get ?
	// RETURN:
	//  Difficulty 0-100

	if (m_Act_p.m_x == 0xFFFF)
	{
		WriteString("Try mining in rock!");
		return(-CSkillDef::T_QTY);
	}

	// Verify so we have a line of sight.
	if (!CanSeeLOS(m_Act_p, NULL, 2))
	{
		if (GetTopPoint().GetDist(m_Act_p) > 2)
		{
			WriteString("That is too far away.");
		}
		else
		{
			WriteString("You have no line of sight to that location");
		}
		return(-CSkillDef::T_QTY);
	}

	// resource check
	CItemPtr pResBit = g_World.CheckNaturalResource(m_Act_p, IT_ROCK, stage == CSkillDef::T_Start);
	if (pResBit == NULL)
	{
		WriteString("Try mining in rock.");
		return(-CSkillDef::T_QTY);
	}
	if (pResBit->GetAmount() == 0)
	{
		WriteString("There is no ore here to mine.");
		return(-CSkillDef::T_QTY);
	}

	CItemPtr pShovel = g_World.ItemFind(m_Act_TargPrv);
	if (pShovel == NULL)
	{
		WriteString("You must use a shovel or pick.");
		return(-CSkillDef::T_Abort);
	}

	if (stage == CSkillDef::T_Fail)
		return 0;

	if (stage == CSkillDef::T_Start)
	{
		m_atResource.m_Stroke_Count = Calc_GetRandVal(5) + 2;

		pShovel->OnTakeDamage(1, this, DAMAGE_HIT_BLUNT);

		return(Skill_NaturalResource_Setup(pResBit));
	}

	if (stage == CSkillDef::T_Stroke)
	{
		// Pick a "mining" type sound
		Sound((Calc_GetRandVal(2)) ? 0x125 : 0x126);
		UpdateDir(m_Act_p);

		if (m_atResource.m_Stroke_Count)
		{
			// Keep trying and updating the animation
			m_atResource.m_Stroke_Count--;
			UpdateAnimate(ANIM_ATTACK_1H_DOWN);
			Skill_SetTimeout();
			return(-CSkillDef::T_Stroke);	// keep active.
		}

		return(0);
	}

	ASSERT(stage == CSkillDef::T_Success);

	CItemPtr pItem = Skill_NaturalResource_Create(pResBit, SKILL_MINING);
	if (pItem == NULL)
	{
		WriteString("There is no ore here to mine.");
		return(-CSkillDef::T_Fail);
	}

	ItemBounce(pItem);
	return(0);
}

int CChar::Skill_Fishing(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Fishing");
	// m_Act.m_pt = where to fish.
	// NOTE: don't check LOS else you can't fish off boats.
	// Check that we dont stand too far away
	// Make sure we aren't in a house
	// RETURN:
	//   difficulty = 0-100

	//CRegionPtr pRegion = GetTopRegion(REGION_TYPE_MULTI);
	//if (pRegion && !pRegion->IsFlag(REGION_FLAG_SHIP))
	//{
	//	// We are in a house ?
	//	WriteString("You can't fish from where you are standing.");
	//	return(-CSkillDef::T_QTY);
	//}

	//if (GetTopPoint().GetDist(m_Act_p) > 6)	// cast works for long distances.
	//{
	//	WriteString("That is too far away.");
	//	return(-CSkillDef::T_QTY);
	//}

	//if (stage == CSkillDef::T_Stroke)
	//{
	//	return 0;
	//}
	//if (stage == CSkillDef::T_Fail)
	//{
	//	return 0;
	//}

	//// resource check
	//CItemPtr pResBit = g_World.CheckNaturalResource(m_Act_p, IT_WATER, stage == CSkillDef::T_Start);
	//if (pResBit == NULL)
	//{
	//	WriteString("There are no fish here.");
	//	return(-CSkillDef::T_QTY);
	//}
	//if (pResBit->GetAmount() == 0)
	//{
	//	WriteString("There are no fish here.");
	//	return(-CSkillDef::T_QTY);
	//}

	//Sound(0x027);

	//// Create the little splash effect.
	//CItemPtr pItemFX = CItem::CreateBase(ITEMID_FX_SPLASH);
	//ASSERT(pItemFX);
	//pItemFX->SetType(IT_WATER_WASH);	// can't fish here.

	//if (stage == CSkillDef::T_Start)
	//{
	//	pItemFX->MoveToDecay(m_Act_p, 1 * TICKS_PER_SEC);

	//	UpdateAnimate(ANIM_ATTACK_2H_DOWN);
	//	return(Skill_NaturalResource_Setup(pResBit));
	//}

	//if (stage == CSkillDef::T_Success)
	//{
	//	pItemFX->MoveToDecay(m_Act_p, 3 * TICKS_PER_SEC);

	//	CItemPtr pFish = Skill_NaturalResource_Create(pResBit, SKILL_FISHING);
	//	if (pFish == NULL)
	//	{
	//		return(-CSkillDef::T_Abort);
	//	}

	//	Printf("You pull out a %s!", (LPCTSTR)pFish->GetName());
	//	pFish->MoveToCheck(GetTopPoint(), this);	// put at my feet.
	//	return(0);
	//}

	//ASSERT(0);
	//return(-CSkillDef::T_QTY);
}

int CChar::Skill_Lumberjack(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Lumberjack");
	// RETURN:
	//   difficulty = 0-100

	if (m_Act_p.m_x == 0xFFFF)
	{
		WriteString("Try chopping a tree.");
		return(-CSkillDef::T_QTY);
	}

	if (stage == CSkillDef::T_Fail)
	{
		return 0;
	}

	// 3D distance check and LOS
	if (!CanTouch(m_Act_p) || GetTopPoint().GetDist3D(m_Act_p) > 3)
	{
		if (GetTopPoint().GetDist(m_Act_p) > 3)
		{
			WriteString("That is too far away.");
		}
		else
		{
			WriteString("You have no line of sight to that location");
		}
		return(-CSkillDef::T_QTY);
	}

	// resource check
	CItemPtr pResBit = g_World.CheckNaturalResource(m_Act_p, IT_TREE, stage == CSkillDef::T_Start);
	if (pResBit == NULL)
	{
		WriteString("Try chopping a tree.");
		return(-CSkillDef::T_QTY);
	}
	if (pResBit->GetAmount() == 0)
	{
		WriteString("There are no logs here to chop.");
		return(-CSkillDef::T_QTY);
	}

	if (stage == CSkillDef::T_Start)
	{
		m_atResource.m_Stroke_Count = Calc_GetRandVal(5) + 2;
		return(Skill_NaturalResource_Setup(pResBit));
	}

	if (stage == CSkillDef::T_Stroke)
	{
		Sound(0x13e);	// 0135, 013e, 148, 14a
		UpdateDir(m_Act_p);
		if (m_atResource.m_Stroke_Count)
		{
			// Keep trying and updating the animation
			m_atResource.m_Stroke_Count--;
			UpdateAnimate(ANIM_ATTACK_WEAPON);
			Skill_SetTimeout();
			return(-CSkillDef::T_Stroke);	// keep active.
		}
		return 0;
	}

	ASSERT(stage == CSkillDef::T_Success);

	// resource check

	CItemPtr pItem = Skill_NaturalResource_Create(pResBit, SKILL_LUMBERJACKING);
	if (pItem == NULL)
		return(-CSkillDef::T_Fail);

	ItemBounce(pItem);
	return(0);
}

int CChar::Skill_DetectHidden(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_DetectHidden");
	// SKILL_DETECTINGHIDDEN
	// Look around for who is hiding.
	// Detect them based on skill diff.
	// ??? Hidden objects ?

	if (stage == CSkillDef::T_Start)
	{
		// Based on who is hiding ?
		return(10);
	}
	if (stage == CSkillDef::T_Fail)
	{
		return 0;
	}
	if (stage == CSkillDef::T_Stroke)
	{
		return 0;
	}
	if (stage != CSkillDef::T_Success)
	{
		ASSERT(0);
		return(-CSkillDef::T_QTY);
	}

	int iRadius = (Skill_GetAdjusted(SKILL_DETECTINGHIDDEN) / 8) + 1;
	CWorldSearch Area(GetTopPoint(), iRadius);
	bool fFound = false;
	for (;;)
	{
		CCharPtr pChar = Area.GetNextChar();
		if (pChar == NULL)
			break;
		if (pChar == this)
			continue;
		if (!pChar->IsStatFlag(STATF_Invisible | STATF_Hidden))
			continue;
		// Try to detect them.
		if (pChar->IsStatFlag(STATF_Hidden))
		{
			// If there hiding skill is much better than our detect then stay hidden
		}
		pChar->Reveal();
		Printf("You find %s", (LPCTSTR)pChar->GetName());
		fFound = true;
	}

	if (!fFound)
	{
		return(-CSkillDef::T_Fail);
	}

	return(0);
}

int CChar::Skill_Cartography(CSkillDef::T_TYPE_ stage)
{
	//// Selected a map type and now we are making it.
	//// m_Act_Cartography_Dist = the map distance.
	//// Find the blank map to write on first.

	//if (stage == CSkillDef::T_Stroke)
	//	return 0;

	//CPointMap pnt = GetTopPoint();
	//if (pnt.m_x >= pnt.GetMulMap()->m_iSizeXWrap)	// maps don't work out here !
	//{
	//	WriteString("You can't seem to figure out your surroundings.");
	//	return(-CSkillDef::T_QTY);
	//}

	//CItemPtr pItem = ContentFind(CSphereUID(RES_TypeDef, IT_MAP_BLANK), 0);
	//if (pItem == NULL)
	//{
	//	WriteString("You have no blank parchment to draw on");
	//	return(-CSkillDef::T_QTY);
	//}

	//if (!CanUse(pItem, true))
	//{
	//	Printf("You can't use the %s where it is.", (LPCTSTR)pItem->GetName());
	//	return(false);
	//}

	//m_Act_Targ = pItem->GetUID();

	//if (stage == CSkillDef::T_Start)
	//{
	//	Sound(0x249);

	//	// difficulty related to m_Act.m_atCartography.m_Dist ???

	//	return(Calc_GetRandVal(100));
	//}
	//if (stage == CSkillDef::T_Fail)
	//{
	//	// consume the map sometimes ?
	//	// pItem->ConsumeAmount( 1 );
	//	return 0;
	//}
	//if (stage == CSkillDef::T_Success)
	//{
	//	pItem->ConsumeAmount(1);

	//	// Get a valid region.
	//	CRectMap rect;
	//	rect.SetRect(pnt.m_x - m_Act.m_atCartography.m_Dist,
	//		pnt.m_y - m_Act.m_atCartography.m_Dist,
	//		pnt.m_x + m_Act.m_atCartography.m_Dist,
	//		pnt.m_y + m_Act.m_atCartography.m_Dist);

	//	// Now create the map
	//	pItem = CItem::CreateScript(ITEMID_MAP, this);
	//	pItem->m_itMap.m_top = rect.top;
	//	pItem->m_itMap.m_left = rect.left;
	//	pItem->m_itMap.m_bottom = rect.bottom;
	//	pItem->m_itMap.m_right = rect.right;
	//	ItemBounce(pItem);
	//	return(0);
	//}

	//ASSERT(0);
	return(-CSkillDef::T_QTY);
}


int CChar::Skill_Musicianship(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Musicianship");
	// m_Act.m_Targ = the intrument i targetted to play.
	if (stage == CSkillDef::T_Stroke)
		return 0;
	if (stage == CSkillDef::T_Start)
	{
		// no instrument fail immediate
		return Use_PlayMusic(g_World.ItemFind(m_Act_Targ), Calc_GetRandVal(90));;
	}

	return(0);
}

int CChar::Skill_Peacemaking(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Peacemaking");
	// try to make all those listening peacable.
	// General area effect.
	// make peace if possible. depends on who is listening/fighting.

	if (stage == CSkillDef::T_Stroke)
	{
		return 0;
	}
	if (stage == CSkillDef::T_Start)
	{
		// Find musical inst first.

		// Basic skill check.
		int iDifficulty = Use_PlayMusic(NULL, Calc_GetRandVal(40));
		if (iDifficulty < -1)	// no instrument fail immediate
			return(-CSkillDef::T_Fail);

		if (!iDifficulty)
		{
			iDifficulty = Calc_GetRandVal(40);	// Depend on evil of the creatures here.
		}

		// Who is fighting around us ? determines difficulty.
		return(iDifficulty);
	}

	if (stage == CSkillDef::T_Fail || stage == CSkillDef::T_Success)
	{
		// Failure just irritates.

		int iRadius = (Skill_GetAdjusted(SKILL_PEACEMAKING) / 8) + 1;
		CWorldSearch Area(GetTopPoint(), iRadius);
		for (;;)
		{
			CCharPtr pChar = Area.GetNextChar();
			if (pChar == NULL)
				return(-CSkillDef::T_Fail);
			if (pChar == this)
				continue;
			break;
		}
		return 0;
	}

	ASSERT(0);
	return(-CSkillDef::T_QTY);
}

int CChar::Skill_Enticement(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Enticement");
	//// m_Act.m_Targ = my target
	//// Just keep playing and trying to allure them til we can't
	//// Must have a musical instrument.

	//CCharPtr pChar = g_World.CharFind(m_Act_Targ);
	//if (pChar == NULL)
	//{
	//	return(-CSkillDef::T_QTY);
	//}
	//if (stage == CSkillDef::T_Fail)
	//{
	//	return 0;
	//}
	//if (stage == CSkillDef::T_Stroke)
	//{
	//	return 0;
	//}
	//if (stage == CSkillDef::T_Success)
	//{
	//	// Walk to me ?
	//	return 0;
	//}
	//if (stage == CSkillDef::T_Start)
	//{
	//	// Base music diff, (whole thing won't work if this fails)
	//	int iDifficulty = Use_PlayMusic(NULL, Calc_GetRandVal(55));
	//	if (iDifficulty < -1)	// no instrument fail immediate
	//		return(-CSkillDef::T_QTY);

	//	m_Act.m_atMusician.m_iMusicDifficulty = iDifficulty;

	//	// Based on the STAT_Int and will of the target.
	//	if (!iDifficulty)
	//	{
	//		return pChar->m_StatInt;
	//	}

	//	return(iDifficulty);
	//}

	//ASSERT(0);
	return(-CSkillDef::T_QTY);
}

int CChar::Skill_Provocation(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Provocation");
	// m_Act.m_TargPrv = provoke this person
	// m_Act.m_Targ = against this person.

	if (stage == CSkillDef::T_Stroke)
	{
		return 0;
	}

	CCharPtr pCharProv = g_World.CharFind(m_Act_TargPrv);
	CCharPtr pCharTarg = g_World.CharFind(m_Act_Targ);

	// If no provoker, then we fail (naturally!)
	if (pCharProv == NULL || pCharProv == this)
	{
		WriteString("You are really upset about this");
		return -CSkillDef::T_QTY;
	}

	if (stage == CSkillDef::T_Fail)
	{
		if (pCharProv->IsClient())
			CheckCrimeSeen(SKILL_NONE, pCharProv, pCharTarg, "provoking");

		// Might just attack you !
		pCharProv->Fight_Attack(this);
		return(0);
	}

	if (stage == CSkillDef::T_Start)
	{
		int iDifficulty = Use_PlayMusic(NULL, Calc_GetRandVal(40));
		if (iDifficulty < -1)	// no instrument fail immediate
			return(false);
		if (!iDifficulty)
		{
			iDifficulty = pCharProv->m_StatInt;	// Depend on evil of the creature.
		}

		return(iDifficulty);
	}

	if (stage != CSkillDef::T_Success)
	{
		ASSERT(0);
		return -CSkillDef::T_QTY;
	}

	if (pCharProv->IsClient())
	{
		CheckCrimeSeen(SKILL_NONE, pCharProv, pCharTarg, "provoking");
		return -CSkillDef::T_Fail;
	}

	// If out of range or something, then we might get attacked ourselves.
	if (pCharProv->Stat_Get(STAT_Karma) >= 10000)
	{
		// They are just too good for this.
		pCharProv->Emote("looks peaceful");
		return -CSkillDef::T_Abort;
	}

	pCharProv->Emote("looks furious");

	// If no target then skill fails
	if (pCharTarg == NULL)
	{
		return -CSkillDef::T_Fail;
	}

	// He realizes that you are the real bad guy as well.
	if (!pCharTarg->OnAttackedBy(this, 1, true))
	{
		return -CSkillDef::T_Abort;
	}

	pCharProv->Memory_AddObjTypes(this, MEMORY_AGGREIVED | MEMORY_IRRITATEDBY);

	// If out of range we might get attacked ourself.
	if (pCharProv->GetTopDist3D(pCharTarg) > SPHEREMAP_VIEW_SIGHT ||
		pCharProv->GetTopDist3D(this) > SPHEREMAP_VIEW_SIGHT)
	{
		// Check that only "evil" monsters attack provoker back
		if (pCharProv->Noto_IsEvil())
		{
			pCharProv->Fight_Attack(this);
		}
		return -CSkillDef::T_Abort;
	}

	// If we are provoking against a "good" PC/NPC and the provoked
	// NPC/PC is good, we are flagged criminal for it and guards
	// are called.
	if (pCharProv->Noto_GetFlag(this) == NOTO_GOOD)
	{
		// lose some karma for this.
		CheckCrimeSeen(SKILL_NONE, pCharProv, pCharTarg, "provoking");
		return -CSkillDef::T_Abort;
	}

	// If we provoke upon a good char we should go criminal for it
	// but skill still succeed.
	if (pCharTarg->Noto_GetFlag(this) == NOTO_GOOD)
	{
		CheckCrimeSeen(SKILL_NONE, pCharTarg, pCharProv, "provoking");
	}

	pCharProv->Fight_Attack(pCharTarg); // Make the actual provoking.
	return(0);
}

int CChar::Skill_Poisoning(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Poisoning");
	//// Act_TargPrv = poison this weapon/food
	//// Act_Targ = with this poison.
	//if (stage == CSkillDef::T_Stroke)
	//{
	//	return 0;
	//}

	//CItemPtr pPoison = g_World.ItemFind(m_Act.m_Targ);
	//if (pPoison == NULL ||
	//	!pPoison->IsType(IT_POTION))
	//{
	//	return(-CSkillDef::T_Abort);
	//}

	//if (stage == CSkillDef::T_Start)
	//{
	//	return Calc_GetRandVal(60);
	//}
	//if (stage == CSkillDef::T_Fail)
	//{
	//	// Lose the poison sometimes ?
	//	return(0);
	//}

	//if (RES_GET_INDEX(pPoison->m_itPotion.m_Type) != SPELL_Poison)
	//{
	//	return(-CSkillDef::T_Abort);
	//}

	//CItemPtr pItem = g_World.ItemFind(m_Act.m_TargPrv);
	//if (pItem == NULL)
	//{
	//	return(-CSkillDef::T_QTY);
	//}

	//if (stage != CSkillDef::T_Success)
	//{
	//	ASSERT(0);
	//	return(-CSkillDef::T_Abort);
	//}

	//Sound(0x247);	// powdering.

	//switch (pItem->GetType())
	//{

	//case IT_FRUIT:
	//case IT_FOOD:
	//case IT_FOOD_RAW:
	//case IT_MEAT_RAW:
	//	pItem->m_itFood.m_poison_skill = pPoison->m_itPotion.m_skillquality / 10;
	//	break;
	//case IT_WEAPON_MACE_SHARP:
	//case IT_WEAPON_SWORD:		// 13 =
	//case IT_WEAPON_FENCE:		// 14 = can't be used to chop trees. (make kindling)
	//	pItem->m_itWeapon.m_poison_skill = pPoison->m_itPotion.m_skillquality / 10;
	//	break;
	//default:
	//	WriteString("You can only poison food or piercing weapons.");
	//	return(-CSkillDef::T_QTY);
	//}
	//// skill + quality of the poison.
	//WriteString("You apply the poison.");
	//pPoison->ConsumeAmount();
	return(0);
}

int CChar::Skill_Cooking(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Cooking");
	//// m_Act.m_Targ = food object to cook.
	//// m_Act.m_pt = my fire.
	//// How hard to cook is this ?

	//if (stage == CSkillDef::T_Stroke)
	//{
	//	return 0;
	//}

	//CItemPtr pFoodRaw = g_World.ItemFind(m_Act_Targ);
	//if (pFoodRaw == NULL)
	//{
	//	return(-CSkillDef::T_QTY);
	//}
	//if (!pFoodRaw->IsType(IT_FOOD_RAW) && !pFoodRaw->IsType(IT_MEAT_RAW))
	//{
	//	return(-CSkillDef::T_Abort);
	//}

	//if (stage == CSkillDef::T_Start)
	//{
	//	return Calc_GetRandVal(50);
	//}

	//// Convert uncooked food to cooked food.
	//ITEMID_TYPE id = (ITEMID_TYPE)RES_GET_INDEX(pFoodRaw->m_itFood.m_cook_id);
	//if (!id)
	//{
	//	id = (ITEMID_TYPE)pFoodRaw->Item_GetDef()->m_ttFoodRaw.m_cook_id.GetResIndex();
	//	if (!id)	// does not cook into anything.
	//	{
	//		return(-CSkillDef::T_QTY);
	//	}
	//}

	//CItemPtr pFoodCooked;
	//if (stage == CSkillDef::T_Success)
	//{
	//	pFoodCooked = CItem::CreateTemplate(id, NULL, this);
	//	if (pFoodCooked)
	//	{
	//		WriteString("Mmm, smells good");
	//		pFoodCooked->m_itFood.m_MeatType = pFoodRaw->m_itFood.m_MeatType;
	//		ItemBounce(pFoodCooked);
	//	}
	//}
	//else	// CSkillDef::T_Fail
	//{
	//	// Burn food
	//}

	//pFoodRaw->ConsumeAmount();

	//if (pFoodCooked == NULL)
	//{
	//	return(-CSkillDef::T_QTY);
	//}

	return(0);
}

int CChar::Skill_Taming(CSkillDef::T_TYPE_ stage)
{
	//ADDTOCALLSTACK("CChar::Skill_Taming");
	//// m_Act.m_Targ = creature to tame.
	//// Check the min required skill for this creature.
	//// Related to INT ?

	//CCharPtr pChar = g_World.CharFind(m_Act_Targ);
	//if (pChar == NULL)
	//{
	//	return(-CSkillDef::T_QTY);
	//}
	//if (pChar == this)
	//{
	//	WriteString("You are your own master.");
	//	return(-CSkillDef::T_QTY);
	//}
	//if (pChar->m_pPlayer->IsValidNewObj())
	//{
	//	WriteString("You can't tame them.");
	//	return(-CSkillDef::T_QTY);
	//}
	//if (!CanTouch(pChar))
	//{
	//	WriteString("You are too far away");
	//	return -CSkillDef::T_QTY;
	//}

	//UpdateDir(pChar);

	//ASSERT(pChar->m_pNPC.IsValidNewObj());

	//int iTameBase = pChar->Skill_GetBase(SKILL_TAMING);
	//if (!IsGM()) // if its a gm doing it, just check that its not
	//{
	//	// Is it tamable ?
	//	if (pChar->IsStatFlag(STATF_Pet))
	//	{
	//		Printf("%s is already tame.", (LPCTSTR)pChar->GetName());
	//		return(-CSkillDef::T_QTY);
	//	}

	//	// Too smart or not an animal.
	//	if (!iTameBase || pChar->Skill_GetBase(SKILL_ANIMALLORE))
	//	{
	//		Printf("%s cannot be tamed.", (LPCTSTR)pChar->GetName());
	//		return(-CSkillDef::T_QTY);
	//	}

	//	// You shouldn't be able to tame creatures that are above your level
	//	if (iTameBase > Skill_GetBase(SKILL_TAMING))
	//	{
	//		Printf("You have no chance of taming %s.", (LPCTSTR)pChar->GetName());
	//		return(-CSkillDef::T_QTY);
	//	}
	//}

	//if (stage == CSkillDef::T_Start)
	//{
	//	// The difficulty should be based on the difference between your skill level
	//	// and the creature's base taming value
	//	// If TameBase == My taming, difficulty should be 100
	//	// If TameBase == 0, difficulty should be 0
	//	// Make it linear for now
	//	int iDifficulty = (iTameBase * 100) / Skill_GetBase(SKILL_TAMING);
	//	if (iDifficulty > 100)
	//		iDifficulty = 100;

	//	if (pChar->Memory_FindObjTypes(this, MEMORY_FIGHT | MEMORY_HARMEDBY | MEMORY_IRRITATEDBY | MEMORY_AGGREIVED))
	//	{
	//		// I've attacked it b4 ?
	//		iDifficulty += 50;
	//	}

	//	m_atTaming.m_Stroke_Count = Calc_GetRandVal(4) + 2;
	//	return(iDifficulty);
	//}

	//if (stage == CSkillDef::T_Fail)
	//{
	//	// chance of being attacked ?
	//	return(0);
	//}

	//if (stage == CSkillDef::T_Stroke)
	//{
	//	static LPCTSTR const sm_szTameSpeak[] =
	//	{
	//		"I won't hurt you.",
	//		"I always wanted a %s like you",
	//		"Good %s",
	//		"Here %s",
	//	};

	//	if (IsGM())
	//		return(0);
	//	if (m_atTaming.m_Stroke_Count <= 0)
	//		return(0);

	//	CGString sSpeak;
	//	sSpeak.Format(sm_szTameSpeak[Calc_GetRandVal(COUNTOF(sm_szTameSpeak))], (LPCTSTR)pChar->GetName());
	//	Speak(sSpeak);

	//	// Keep trying and updating the animation
	//	m_atTaming.m_Stroke_Count--;
	//	Skill_SetTimeout();
	//	return -CSkillDef::T_Stroke;
	//}

	//ASSERT(stage == CSkillDef::T_Success);

	//// Create the memory of being tamed to prevent lame macroers
	//CItemMemoryPtr pMemory = pChar->Memory_FindObjTypes(this, MEMORY_SPEAK);
	//if (pMemory &&
	//	pMemory->m_itEqMemory.m_Arg1 == NPC_MEM_ACT_TAMED)
	//{
	//	// See if I tamed it before
	//	// I did, no skill to tame it again
	//	CGString sSpeak;
	//	sSpeak.Format("The %s remembers you and accepts you once more as it's master.", (LPCTSTR)pChar->GetName());
	//	ObjMessage(sSpeak, pChar);

	//	pChar->NPC_PetSetOwner(this);
	//	// pChar->Stat_Set( STAT_Food, 50 );	// this is good for something.
	//	pChar->m_Act_Targ = GetUID();
	//	pChar->Skill_Start(NPCACT_FOLLOW_TARG);
	//	return -CSkillDef::T_QTY;	// no credit for this.
	//}

	//pChar->NPC_PetSetOwner(this);
	//pChar->Stat_Set(STAT_Food, 50);	// this is good for something.
	//pChar->m_Act_Targ = GetUID();
	//pChar->Skill_Start(NPCACT_FOLLOW_TARG);
	//WriteString("It seems to accept you as master");

	//// Create the memory of being tamed to prevent lame macroers
	//pMemory = pChar->Memory_AddObjTypes(this, MEMORY_SPEAK);
	//ASSERT(pMemory);
	//pMemory->m_itEqMemory.m_Arg1 = NPC_MEM_ACT_TAMED;
	return(0);
}

int CChar::Skill_Lockpicking(CSkillDef::T_TYPE_ stage)
{
	//ADDTOCALLSTACK("CChar::Skill_Lockpicking");
	//// m_Act.m_Targ = the item to be picked.
	//// m_Act.m_TargPrv = The pick.

	//if (stage == CSkillDef::T_Stroke)
	//{
	//	return 0;
	//}

	//CItemPtr pPick = g_World.ItemFind(m_Act.m_TargPrv);
	//if (pPick == NULL || !pPick->IsType(IT_LOCKPICK))
	//{
	//	WriteString("You need a lock pick.");
	//	return -CSkillDef::T_QTY;
	//}

	//CItemPtr pLock = g_World.ItemFind(m_Act.m_Targ);
	//if (pLock == NULL)
	//{
	//	WriteString("Use the lock pick on a lockable item.");
	//	return -CSkillDef::T_QTY;
	//}

	//if (pPick->GetTopLevelObj() != this)	// the pick is gone !
	//{
	//	WriteString("Your pick must be on your person.");
	//	return -CSkillDef::T_QTY;
	//}

	//if (stage == CSkillDef::T_Fail)
	//{
	//	// Damage my pick
	//	pPick->OnTakeDamage(1, this, DAMAGE_HIT_BLUNT);
	//	return(0);
	//}

	//if (!CanTouch(pLock))	// we moved too far from the lock.
	//{
	//	WriteString("You can't reach that.");
	//	return -CSkillDef::T_QTY;
	//}

	//if (stage == CSkillDef::T_Start)
	//{
	//	return(pLock->Use_LockPick(this, true, false));
	//}

	//ASSERT(stage == CSkillDef::T_Success);

	//if (pLock->Use_LockPick(this, false, false) < 0)
	//{
	//	return -CSkillDef::T_Fail;
	//}
	//return 0;
}

int CChar::Skill_Hiding(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Hiding");
	// SKILL_Stealth = move while already hidden !
	// SKILL_Hiding
	// Skill required varies with terrain and situation ?
	// if we are carrying a light source then this should not work.

#if 0
	// We shoud just stay in HIDING skill. ?
#else
	if (stage == CSkillDef::T_Stroke)
	{
		return 0;
	}
	if (stage == CSkillDef::T_Fail)
	{
		Reveal(STATF_Hidden);
		return 0;
	}

	if (stage == CSkillDef::T_Success)
	{
		if (IsStatFlag(STATF_Hidden))
		{
			// I was already hidden ? so un-hide.
			Reveal(STATF_Hidden);
			return(-CSkillDef::T_Abort);
		}

		ObjMessage("You have hidden yourself well", this);
		StatFlag_Set(STATF_Hidden);
		UpdateMode();
		return(0);
	}

	if (stage == CSkillDef::T_Start)
	{
		// Make sure i am not carrying a light ?

		CItemPtr pItem = GetContentHead();
		for (; pItem; pItem = pItem->GetNext())
		{
			LAYER_TYPE layer = pItem->GetEquipLayer();
			if (!CItemDef::IsVisibleLayer(layer))
				continue;
			if (pItem->IsType(IT_EQ_HORSE))
			{
				// Horses are hard to hide !
				WriteString("Your horse reveals you");
				return(-CSkillDef::T_QTY);
			}
			if (pItem->Item_GetDef()->Can(CAN_I_LIGHT))
			{
				WriteString("You are too well lit to hide");
				return(-CSkillDef::T_QTY);
			}
		}

		return Calc_GetRandVal(70);
	}
#endif

	ASSERT(0);
	return(-CSkillDef::T_QTY);
}

int CChar::Skill_Herding(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Herding");
	// m_Act.m_Targ = move this creature.
	// m_Act.m_pt = move to here.
	// How do I make them move fast ? or with proper speed ???

	//if (stage == CSkillDef::T_Stroke)
	//{
	//	return 0;
	//}

	//CCharPtr pChar = g_World.CharFind(m_Act_Targ);
	//if (pChar == NULL)
	//{
	//	WriteString("You lost your target!");
	//	return(-CSkillDef::T_QTY);
	//}
	//CItemPtr pCrook = g_World.ItemFind(m_Act_TargPrv);
	//if (pCrook == NULL)
	//{
	//	WriteString("You lost your crook!");
	//	return(-CSkillDef::T_QTY);
	//}

	//// special GM version to move to coordinates.
	//if (!IsGM())
	//{
	//	// Herdable ?
	//	if (pChar->m_pPlayer->IsValidNewObj() ||
	//		!pChar->m_pNPC->IsValidNewObj() ||
	//		pChar->m_pNPC->m_Brain != NPCBRAIN_ANIMAL)
	//	{
	//		WriteString("They look somewhat annoyed");
	//		return(-CSkillDef::T_QTY);
	//	}
	//}
	//else
	//{
	//	if (GetPrivLevel() < pChar->GetPrivLevel())
	//		return(-CSkillDef::T_QTY);
	//}

	//if (stage == CSkillDef::T_Start)
	//{
	//	UpdateAnimate(ANIM_ATTACK_WEAPON);
	//	int iIntVal = pChar->m_StatInt / 2;
	//	return(iIntVal + Calc_GetRandVal(iIntVal));
	//}
	//if (stage == CSkillDef::T_Fail)
	//{
	//	// Irritate the animal.
	//	return 0;
	//}

	////
	//// Try to make them walk there.
	//ASSERT(stage == CSkillDef::T_Success);

	//if (IsGM())
	//{
	//	pChar->Spell_Effect_Teleport(m_Act_p, true, false);
	//}
	//else
	//{
	//	pChar->m_Act_p = m_Act_p;
	//	pChar->Skill_Start(NPCACT_GOTO);
	//}

	//ObjMessage("The animal goes where it is instructed", pChar);
	return(0);
}

int CChar::Skill_SpiritSpeak(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_SpiritSpeak");
	if (stage == CSkillDef::T_Fail)
	{
		// bring ghosts ? hehe
		return 0;
	}
	if (stage == CSkillDef::T_Stroke)
	{
		return 0;
	}
	if (stage == CSkillDef::T_Start)
	{
		// difficulty based on spirits near ?
		return(Calc_GetRandVal(90));
	}
	if (stage == CSkillDef::T_Success)
	{
		if (IsStatFlag(STATF_SpiritSpeak))
			return(-CSkillDef::T_Abort);
		WriteString("You establish a connection to the netherworld.");
		Sound(0x24a);
		Spell_Equip_Create(SPELL_NONE, LAYER_FLAG_SpiritSpeak, m_Act_Difficulty * 10, 4 * 60 * TICKS_PER_SEC, this, false);
		return(0);
	}

	ASSERT(0);
	return(-CSkillDef::T_Abort);
}

int CChar::Skill_Meditation(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Meditation");
	// SKILL_MEDITATION
	// Try to regen your mana even faster than normal.
	// Give experience only when we max out.

	if (stage == CSkillDef::T_Fail || stage == CSkillDef::T_Abort)
	{
		return 0;
	}

	if (stage == CSkillDef::T_Start)
	{
		if (m_StatMana >= m_StatMaxMana)
		{
			WriteString("You are at peace.");
			return(-CSkillDef::T_QTY);
		}
		m_atTaming.m_Stroke_Count = 0;

		WriteString("You attempt a meditative trance.");

		return Calc_GetRandVal(100);	// how hard to get started ?
	}
	if (stage == CSkillDef::T_Stroke)
	{
		return 0;
	}
	if (stage == CSkillDef::T_Success)
	{
		if (m_StatMana >= m_StatMaxMana)
		{
			WriteString("You are at peace.");
			return(0);	// only give skill credit now.
		}

		if (m_atTaming.m_Stroke_Count == 0)
		{
			Sound(0x0f9);
		}
		m_atTaming.m_Stroke_Count++;

		Stat_Change(STAT_Mana, 1);

		// next update. (depends on skill)
		Skill_SetTimeout();

		// Set a new possibility for failure ?
		// iDifficulty = Calc_GetRandVal(100);
		return(-CSkillDef::T_Stroke);
	}

	DEBUG_CHECK(0);
	return(-CSkillDef::T_QTY);
}

int CChar::Skill_Healing(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Healing");
	//// SKILL_VETERINARY:
	//// SKILL_HEALING
	//// m_Act.m_TargPrv = bandages.
	//// m_Act.m_Targ = heal target.
	////
	//// should depend on the severity of the wounds ?
	//// should be just a fast regen over time ?
	//// RETURN:
	////  = -3 = failure.

	//if (stage == CSkillDef::T_Stroke)
	//{
	//	return 0;
	//}

	//CItemPtr pBandage = g_World.ItemFind(m_Act.m_TargPrv);
	//if (pBandage == NULL)
	//{
	//	WriteString("Where are your bandages?");
	//	return(-CSkillDef::T_QTY);
	//}
	//if (!pBandage->IsType(IT_BANDAGE))
	//{
	//	WriteString("Use a bandage");
	//	return(-CSkillDef::T_QTY);
	//}

	//CObjBasePtr pObj = g_World.ObjFind(m_Act.m_Targ);
	//if (!CanTouch(pObj))
	//{
	//	WriteString("You must be able to reach the target");
	//	return(-CSkillDef::T_QTY);
	//}

	//CItemCorpsePtr pCorpse;	// resurrect by corpse.
	//CCharPtr pChar;
	//if (pObj->IsItem())
	//{
	//	// Corpse ?
	//	pCorpse = REF_CAST(CItemCorpse, pObj);
	//	if (pCorpse == NULL)
	//	{
	//		WriteString("Try healing a creature.");
	//		return(-CSkillDef::T_QTY);
	//	}

	//	pChar = g_World.CharFind(pCorpse->m_uidLink);
	//}
	//else
	//{
	//	pCorpse = NULL;
	//	pChar = g_World.CharFind(m_Act.m_Targ);
	//}

	//if (pChar == NULL)
	//{
	//	WriteString("This creature is beyond help.");
	//	return(-CSkillDef::T_QTY);
	//}

	//if (GetDist(pObj) > 2)
	//{
	//	Printf("You are too far away to apply bandages on %s", (LPCTSTR)pObj->GetName());
	//	if (pChar != this)
	//	{
	//		pChar->Printf("%s is attempting to apply bandages to %s, but they are too far away!",
	//			(LPCTSTR)GetName(), (LPCTSTR)(pCorpse ? (pCorpse->GetName()) : "you"));
	//	}
	//	return(-CSkillDef::T_QTY);
	//}

	//if (pCorpse)
	//{
	//	if (!pCorpse->IsTopLevel())
	//	{
	//		WriteString("Put the corpse on the ground");
	//		return(-CSkillDef::T_QTY);
	//	}
	//	CRegionPtr pRegion = pCorpse->GetTopRegion(REGION_TYPE_AREA | REGION_TYPE_MULTI);
	//	if (pRegion == NULL)
	//	{
	//		return(-CSkillDef::T_QTY);
	//	}
	//	if (pRegion->IsFlag(REGION_ANTIMAGIC_ALL | REGION_ANTIMAGIC_RECALL_IN | REGION_ANTIMAGIC_TELEPORT))
	//	{
	//		WriteString("Your resurrection attempt is blocked by antimagic.");
	//		if (pChar != this)
	//		{
	//			pChar->Printf("%s is attempting to apply bandages to %s, but they are blocked by antimagic!",
	//				(LPCTSTR)GetName(), (LPCTSTR)pCorpse->GetName());
	//		}
	//		return(-CSkillDef::T_QTY);
	//	}
	//}
	//else if (pChar->IsStatFlag(STATF_DEAD))
	//{
	//	WriteString("You can't heal a ghost! Try healing their corpse.");
	//	return(-CSkillDef::T_QTY);
	//}

	//if (!pChar->IsStatFlag(STATF_Poisoned | STATF_DEAD) &&
	//	pChar->GetHealthPercent() >= 100)
	//{
	//	if (pChar == this)
	//	{
	//		Printf("You are healthy");
	//	}
	//	else
	//	{
	//		Printf("%s does not require you to heal or cure them!", (LPCTSTR)pChar->GetName());
	//	}
	//	return(-CSkillDef::T_QTY);
	//}

	//if (stage == CSkillDef::T_Fail)
	//{
	//	// just consume the bandage on fail and give some credit for trying.
	//	pBandage->ConsumeAmount();

	//	if (pChar != this)
	//	{
	//		pChar->Printf("%s is attempting to apply bandages to %s, but has failed",
	//			(LPCTSTR)GetName(), (LPCTSTR)pCorpse->GetName());
	//	}

	//	// Harm the creature ?
	//	return(-CSkillDef::T_Fail);
	//}
}

int CChar::Skill_RemoveTrap(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_RemoveTrap");
	// m_Act.m_Targ = trap
	// Is it a trap ?

	if (stage == CSkillDef::T_Stroke)
	{
		return 0;
	}
	CItemPtr pTrap = g_World.ItemFind(m_Act_Targ);
	if (pTrap == NULL || !pTrap->IsType(IT_TRAP))
	{
		WriteString("You should use this skill to disable traps");
		return(-CSkillDef::T_QTY);
	}
	if (!CanTouch(pTrap))
	{
		WriteString("You can't reach it.");
		return(-CSkillDef::T_QTY);
	}
	if (stage == CSkillDef::T_Start)
	{
		// How difficult ?
		return Calc_GetRandVal(95);
	}
	if (stage == CSkillDef::T_Fail)
	{
		Use_Item(pTrap);	// set it off ?
		return 0;
	}
	if (stage == CSkillDef::T_Success)
	{
		// disable it.
		pTrap->SetTrapState(IT_TRAP_INACTIVE, ITEMID_NOTHING, 5 * 60);
		return 0;
	}
	ASSERT(0);
	return(-CSkillDef::T_QTY);
}

int CChar::Skill_Begging(CSkillDef::T_TYPE_ stage)
{
	// m_Act.m_Targ = Our begging target..

	CCharPtr pChar = g_World.CharFind(m_Act_Targ);
	if (pChar == NULL || pChar == this)
	{
		return(-CSkillDef::T_QTY);
	}

	switch (stage)
	{
	case CSkillDef::T_Start:
		Printf("You grovel at %s's feet", (LPCTSTR)pChar->GetName());
		return(pChar->m_StatInt);
	case CSkillDef::T_Stroke:
		if (m_pNPC->IsValidNewObj())
			return -CSkillDef::T_Stroke;	// Keep it active.
		return(0);
	case CSkillDef::T_Fail:
		// Might they do something bad ?
		return(0);
	case CSkillDef::T_Success:
		// Now what ? Not sure how to make begging successful.
		// Give something from my inventory ?
		return(0);
	}

	ASSERT(0);
	return(-CSkillDef::T_QTY);
}

int CChar::Skill_Magery(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Magery");
	// SKILL_MAGERY
	//  m_Act.m_pt = location to cast to.
	//  m_Act.m_TargPrv = the source of the spell.
	//  m_Act.m_Targ = target for the spell.
	//  m_atMagery.m_Spell = the spell.

	switch (stage)
	{
	case CSkillDef::T_Start:
		// NOTE: this should call SetTimeout();
		return Spell_CastStart();
	case CSkillDef::T_Stroke:
		return(0);
	case CSkillDef::T_Fail:
		Spell_CastFail();
		return(0);
	case CSkillDef::T_Success:
		if (!Spell_CastDone())
		{
			return(-CSkillDef::T_Abort);
		}
		return(0);
	}

	ASSERT(0);
	return(-CSkillDef::T_Abort);
}

int CChar::Skill_Fighting(CSkillDef::T_TYPE_ stage)
{
	//ADDTOCALLSTACK("CChar::Skill_Fighting");
	//// SKILL_ARCHERY
	//// m_Act.m_Targ = attack target.
	//// RETURN:
	////  Difficulty against my skill.

	//if (stage == CSkillDef::T_Start)
	//{
	//	// When do we get our next shot?

	//	DEBUG_CHECK(IsStatFlag(STATF_War));

	//	m_atFight.m_War_Swing_State = WAR_SWING_EQUIPPING;

	//	int iDifficulty = g_Cfg.Calc_CombatChanceToHit(this, Skill_GetActive(),
	//		g_World.CharFind(m_Act_Targ),
	//		g_World.ItemFind(m_uidWeapon));

	//	// Set the swing timer.
	//	int iWaitTime = Fight_GetWeaponSwingTimer() / 2;	// start the anim immediately.
	//	if (Skill_GetActive() == SKILL_ARCHERY)	// anim is funny for archery
	//		iWaitTime /= 2;
	//	SetTimeout(iWaitTime);

	//	return(iDifficulty);
	//}

	//if (stage == CSkillDef::T_Stroke)
	//{
	//	// Hit or miss my current target.
	//	if (!IsStatFlag(STATF_War))
	//		return -CSkillDef::T_Abort;

	//	if (m_atFight.m_War_Swing_State != WAR_SWING_SWINGING)
	//	{
	//		m_atFight.m_War_Swing_State = WAR_SWING_READY;  // Waited my recoil time. So I'm ready.
	//	}

	//	Fight_HitTry();	// this cleans up itself.
	//	return -CSkillDef::T_Stroke;	// Stay in the skill till we hit.
	//}

	return -CSkillDef::T_QTY;
}

int CChar::Skill_MakeItem(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_MakeItem");
	//// SKILL_BLACKSMITHING:
	//// SKILL_BOWCRAFT:
	//// SKILL_CARPENTRY:
	//// SKILL_INSCRIPTION:
	//// SKILL_TAILORING:
	//// SKILL_TINKERING:
	////
	//// m_Act.m_Targ = the item we want to be part of this process.
	//// m_Act.m_atCreate.m_ItemID = new item we are making
	//// m_Act.m_atCreate.m_Amount = amount of said item.

	//if (stage == CSkillDef::T_Start)
	//{
	//	return m_Act_Difficulty;	// keep the already set difficulty
	//}
	//if (stage == CSkillDef::T_Stroke)
	//{
	//	return 0;
	//}
	//if (stage == CSkillDef::T_Success)
	//{
	//	if (!Skill_MakeItem(m_Act.m_atCreate.m_ItemID, m_Act.m_Targ, CSkillDef::T_Success))
	//		return(-CSkillDef::T_Abort);
	//	return 0;
	//}
	//if (stage == CSkillDef::T_Fail)
	//{
	//	Skill_MakeItem(m_Act.m_atCreate.m_ItemID, m_Act.m_Targ, CSkillDef::T_Fail);
	//	return(0);
	//}
	//ASSERT(0);
	return(-CSkillDef::T_QTY);
}

int CChar::Skill_Blacksmith(CSkillDef::T_TYPE_ stage)
{
	//ADDTOCALLSTACK("CChar::Skill_Blacksmith");
	//// m_Act.m_atCreate.m_ItemID = create this item
	//// m_Act.m_pt = the anvil.
	//// m_Act.m_Targ = the hammer.

	//m_Act_p = g_World.FindItemTypeNearby(GetTopPoint(), IT_FORGE, 3);
	//if (!m_Act_p.IsValidPoint())
	//{
	//	WriteString("You must be near a forge to smith");
	//	return(-CSkillDef::T_QTY);
	//}

	//UpdateDir(m_Act_p);	// toward the forge

	//if (stage == CSkillDef::T_Start)
	//{
	//	m_Act.m_atCreate.m_Stroke_Count = Calc_GetRandVal(4) + 2;
	//}

	//if (stage == CSkillDef::T_Stroke)
	//{
	//	Sound(0x02a);
	//	if (m_Act.m_atCreate.m_Stroke_Count <= 0)
	//		return 0;

	//	// Keep trying and updating the animation
	//	m_Act.m_atCreate.m_Stroke_Count--;
	//	UpdateAnimate(ANIM_ATTACK_WEAPON);	// ANIM_ATTACK_1H_DOWN
	//	Skill_SetTimeout();
	//	return(-CSkillDef::T_Stroke);	// keep active.
	//}

	return(Skill_MakeItem(stage));
}

int CChar::Skill_Tailoring(CSkillDef::T_TYPE_ stage)
{
	if (stage == CSkillDef::T_Success)
	{
		Sound(SOUND_SNIP);	// snip noise
	}

	return(Skill_MakeItem(stage));
}

int CChar::Skill_Inscription(CSkillDef::T_TYPE_ stage)
{
	//if (stage == CSkillDef::T_Start)
	//{
	//	// Can we even attempt to make this scroll ?
	//	// m_Act.m_atCreate.m_ItemID = create this item
	//	Sound(0x249);

	//	// Stratics says you loose mana regardless of success or failure
	//	for (DWORD dwSpell = SPELL_Clumsy; dwSpell < SPELL_BOOK_QTY; dwSpell++)
	//	{
	//		CSpellDefPtr pSpellDef = g_Cfg.GetSpellDef((SPELL_TYPE)dwSpell);
	//		if (pSpellDef == NULL)
	//			continue;
	//		if (pSpellDef->m_idScroll == m_Act.m_atCreate.m_ItemID)
	//		{
	//			// Consume mana.
	//			Stat_Change(STAT_Mana, -pSpellDef->m_wManaUse);
	//		}
	//	}
	//}

	//return(Skill_MakeItem(stage));
}

int CChar::Skill_Bowcraft(CSkillDef::T_TYPE_ stage)
{
	//// SKILL_BOWCRAFT
	//// m_Act.m_Targ = the item we want to be part of this process.
	//// m_Act.m_atCreate.m_ItemID = new item we are making
	//// m_Act.m_atCreate.m_Amount = amount of said item.

	//Sound(0x055);
	//UpdateAnimate(ANIM_SALUTE);

	//if (stage == CSkillDef::T_Start)
	//{
	//	// Might be based on how many arrows to make ???
	//	m_Act.m_atCreate.m_Stroke_Count = Calc_GetRandVal(2) + 1;
	//}

	return(Skill_MakeItem(stage));
}

int CChar::Skill_Carpentry(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Carpentry");
	//// m_Act.m_Targ = the item we want to be part of this process.
	//// m_Act.m_atCreate.m_ItemID = new item we are making
	//// m_Act.m_atCreate.m_Amount = amount of said item.

	//Sound(0x23d);

	//if (stage == CSkillDef::T_Start)
	//{
	//	// m_Act.m_atCreate.m_ItemID = create this item
	//	m_Act.m_atCreate.m_Stroke_Count = Calc_GetRandVal(3) + 2;
	//}

	//if (stage == CSkillDef::T_Stroke)
	//{
	//	if (m_Act.m_atCreate.m_Stroke_Count <= 0)
	//		return 0;

	//	// Keep trying and updating the animation
	//	m_Act.m_atCreate.m_Stroke_Count--;
	//	UpdateAnimate(ANIM_ATTACK_WEAPON);
	//	Skill_SetTimeout();
	//	return(-CSkillDef::T_Stroke);	// keep active.
	//}

	return(Skill_MakeItem(stage));
}

int CChar::Skill_Scripted(SKTRIG_TYPE stage)
{
	ADDTOCALLSTACK("CChar::Skill_Scripted");
	if ( (stage == SKTRIG_FAIL) || (stage == SKTRIG_START) || (stage == SKTRIG_STROKE) || (stage == SKTRIG_SUCCESS) )
		return 0;

	return -SKTRIG_QTY;	// something odd
}

int CChar::Skill_Information(CSkillDef::T_TYPE_ stage)
{
	// SKILL_ANIMALLORE:
	// SKILL_ARMSLORE:
	// SKILL_ANATOMY:
	// SKILL_ITEMID:
	// SKILL_EVALINT:
	// SKILL_FORENSICS:
	// SKILL_TASTEID:
	// Difficulty should depend on the target item !!!??
	// m_Act.m_Targ = target.

	if (!IsClient())	// purely informational
		return(-CSkillDef::T_QTY);

	if (stage == CSkillDef::T_Fail)
	{
		return 0;
	}
	if (stage == CSkillDef::T_Stroke)
	{
		return 0;
	}

	SKILL_TYPE skill = Skill_GetActive();
	int iSkillLevel = Skill_GetAdjusted(skill);

	if (stage == CSkillDef::T_Start)
	{
		return GetClient()->OnSkill_Info(skill, m_Act_Targ, iSkillLevel, true);
	}
	if (stage == CSkillDef::T_Success)
	{
		return GetClient()->OnSkill_Info(skill, m_Act_Targ, iSkillLevel, false);
	}

	ASSERT(0);
	return(-CSkillDef::T_QTY);
}

int CChar::Skill_Act_Napping(CSkillDef::T_TYPE_ stage)
{
	// NPCACT_Napping:
	// we are taking a small nap. keep napping til we wake. (or move)
	// AFK command

	if (stage == CSkillDef::T_Start)
	{
		// we are taking a small nap.
		SetTimeout(2 * TICKS_PER_SEC);
		return(0);
	}

	if (stage == CSkillDef::T_Stroke)
	{
		if (m_Act_p != GetTopPoint())
			return(-CSkillDef::T_QTY);	// we moved.
		SetTimeout(8 * TICKS_PER_SEC);
		Speak("z", HUE_WHITE, TALKMODE_WHISPER);
		return -CSkillDef::T_Stroke;	// Stay in the skill till we hit.
	}

	return(-CSkillDef::T_QTY);	// something odd
}

int CChar::Skill_Act_Breath(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Act_Breath");
	// NPCACT_BREATH
	// A Dragon I assume.
	// m_Act.m_Targ = my target.

	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Fail )
	{
		return 0;
	}
	if ( stage == CSkillDef::T_Stroke )
	{
		return 0;
	}

	CCharPtr pChar = g_World.CharFind(m_Act_Targ);
	if ( pChar == NULL )
	{
		return -CSkillDef::T_QTY;
	}

	m_Act_p = pChar->GetTopPoint();
	UpdateDir( m_Act_p );

	if ( stage == CSkillDef::T_Start )
	{
		Stat_Change( STAT_Stam, -10 );
		UpdateAnimate( ANIM_MON_Stomp, false );
		SetTimeout( 3*TICKS_PER_SEC );
		return 0;
	}

	ASSERT( stage == CSkillDef::T_Success );

	CPointMap pntMe = GetTopPoint();
	if ( pntMe.GetDist( m_Act_p ) > SPHEREMAP_VIEW_SIGHT )
	{
		m_Act_p.StepLinePath( pntMe, SPHEREMAP_VIEW_SIGHT );
	}

	Sound( 0x227 );
	int iDamage = m_StatStam/4 + Calc_GetRandVal( m_StatStam/4 );
	g_World.Explode( this, m_Act_p, 3, iDamage, DAMAGE_FIRE | DAMAGE_GENERAL );
	return( 0 );
}

int CChar::Skill_Act_Looting(CSkillDef::T_TYPE_ stage)
{
	//// NPCACT_LOOTING
	//// m_Act.m_Targ = the item i want.

	//if (stage == CSkillDef::T_Stroke)
	//{
	//	return 0;
	//}
	//if (stage == CSkillDef::T_Start)
	//{
	//	if (m_atLooting.m_iDistCurrent == 0)
	//	{
	//		CSphereExpArgs Args(this, this, g_World.ItemFind(m_Act_Targ));
	//		if (OnTrigger(CCharDef::T_NPCSeeWantItem, Args) == TRIGRET_RET_VAL)
	//			return(false);
	//	}
	//	SetTimeout(1 * TICKS_PER_SEC);
	//	return 0;
	//}

	return(-CSkillDef::T_QTY);
}

int CChar::Skill_Act_Throwing(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Act_Throwing");
	if (stage == CSkillDef::T_Stroke)
	{
		return 0;
	}
	CCharPtr pChar = g_World.CharFind(m_Act_Targ);
	if (pChar == NULL)
	{
		return(-CSkillDef::T_QTY);
	}

	m_Act_p = pChar->GetTopPoint();
	UpdateDir(m_Act_p);

	if (stage == CSkillDef::T_Start)
	{
		Stat_Change(STAT_Stam, -(4 + Calc_GetRandVal(6)));
		UpdateAnimate(ANIM_MON_Stomp);
		return 0;
	}

	if (stage != CSkillDef::T_Success)
	{
		return(-CSkillDef::T_QTY);
	}

	CPointMap pntMe = GetTopPoint();
	if (pntMe.GetDist(m_Act_p) > SPHEREMAP_VIEW_SIGHT)
	{
		m_Act_p.StepLinePath(pntMe, SPHEREMAP_VIEW_SIGHT);
	}
	SoundChar(CRESND_GETHIT);

	// a rock or a boulder ?
	ITEMID_TYPE id;
	int iDamage;
	if (!Calc_GetRandVal(3))
	{
		iDamage = m_StatStam / 4 + Calc_GetRandVal(m_StatStam / 4);
		id = (ITEMID_TYPE)(ITEMID_ROCK_B_LO + Calc_GetRandVal(ITEMID_ROCK_B_HI - ITEMID_ROCK_B_LO));
	}
	else
	{
		iDamage = 2 + Calc_GetRandVal(m_StatStam / 4);
		id = (ITEMID_TYPE)(ITEMID_ROCK_2_LO + Calc_GetRandVal(ITEMID_ROCK_2_HI - ITEMID_ROCK_2_LO));
	}

	CItemPtr pRock = CItem::CreateScript(id, this);
	ASSERT(pRock);
	pRock->SetAttr(ATTR_CAN_DECAY);
	pRock->MoveToCheck(m_Act_p, this);
	pRock->Effect(EFFECT_BOLT, id, this);

	// did it hit ?
	if (!Calc_GetRandVal(pChar->GetTopPoint().GetDist(m_Act_p)))
	{
		pChar->OnTakeDamage(iDamage, this, DAMAGE_HIT_BLUNT);
	}

	return(0);
}

int CChar::Skill_Act_Training(CSkillDef::T_TYPE_ stage)
{
	ADDTOCALLSTACK("CChar::Skill_Act_Training");
	// NPCACT_TRAINING
	// finished some traing maneuver.

	if (stage == CSkillDef::T_Start)
	{
		SetTimeout(1 * TICKS_PER_SEC);
		return 0;
	}
	if (stage == CSkillDef::T_Stroke)
	{
		return 0;
	}
	if (stage != CSkillDef::T_Success)
	{
		return(-CSkillDef::T_QTY);
	}

	if (m_Act_TargPrv == m_uidWeapon)
	{
		CItemPtr pItem = g_World.ItemFind(m_Act_Targ);
		if (pItem)
		{
			switch (pItem->GetType())
			{
			case IT_TRAIN_DUMMY:	// Train dummy.
				Use_Train_Dummy(pItem, false);
				break;
			case IT_TRAIN_PICKPOCKET:
				Use_Train_PickPocketDip(pItem, false);
				break;
			case IT_ARCHERY_BUTTE:	// Archery Butte
				Use_Train_ArcheryButte(pItem, false);
				break;
			}
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////
// General skill stuff

ANIM_TYPE CChar::Skill_GetAnim(SKILL_TYPE skill)
{
	switch ( skill )
	{
		/*case SKILL_FISHING:	// softcoded
			return ANIM_ATTACK_2H_BASH;*/
		case SKILL_BLACKSMITHING:
			return ANIM_ATTACK_1H_SLASH;
		case SKILL_MINING:
			return ANIM_ATTACK_1H_BASH;
		case SKILL_LUMBERJACKING:
			return ANIM_ATTACK_2H_SLASH;
		default:
			return static_cast<ANIM_TYPE>(-1);
	}
}

SOUND_TYPE CChar::Skill_GetSound(SKILL_TYPE skill)
{
	switch ( skill )
	{
		/*case SKILL_FISHING:	// softcoded
			return 0x364;*/
		case SKILL_ALCHEMY:
			return 0x242;
		case SKILL_TAILORING:
			return 0x248;
		case SKILL_CARTOGRAPHY:
		case SKILL_INSCRIPTION:
			return 0x249;
		case SKILL_BOWCRAFT:
			return 0x055;
		case SKILL_BLACKSMITHING:
			return 0x02a;
		case SKILL_CARPENTRY:
			return 0x23d;
		case SKILL_MINING:
			return Calc_GetRandVal(2) ? 0x125 : 0x126;
		case SKILL_LUMBERJACKING:
			return 0x13e;
		default:
			return SOUND_NONE;
	}
}

int CChar::Skill_Stroke(bool fResource)
{
	// fResource means decreasing m_atResource.m_Stroke_Count instead of m_atCreate.m_Stroke_Count
	SKILL_TYPE skill = Skill_GetActive();
	SOUND_TYPE sound = SOUND_NONE;
	INT64 delay = Skill_GetTimeout();
	ANIM_TYPE anim = ANIM_WALK_UNARM;
	if ( m_atCreate.m_Stroke_Count > 1 )
	{
		if ( !g_Cfg.IsSkillFlag(skill, SKF_NOSFX) )
			sound = Skill_GetSound(skill);
		if ( !g_Cfg.IsSkillFlag(skill, SKF_NOANIM) )
			anim = Skill_GetAnim(skill);
	}

	if ( IsTrigUsed(TRIGGER_SKILLSTROKE) || IsTrigUsed(TRIGGER_STROKE) )
	{
		CScriptTriggerArgs args;
		args.m_VarsLocal.SetNum("Skill", skill);
		args.m_VarsLocal.SetNum("Sound", sound);
		args.m_VarsLocal.SetNum("Delay", delay);
		args.m_VarsLocal.SetNum("Anim", anim);
		args.m_iN1 = 1;	//UpdateDir() ?
		if ( fResource )
			args.m_VarsLocal.SetNum("Strokes", m_atResource.m_Stroke_Count);
		else
			args.m_VarsLocal.SetNum("Strokes", m_atCreate.m_Stroke_Count);

		if ( OnTrigger(CTRIG_SkillStroke, this, &args) == TRIGRET_RET_TRUE )
			return -SKTRIG_ABORT;
		if ( Skill_OnTrigger(skill, SKTRIG_STROKE, &args) == TRIGRET_RET_TRUE )
			return -SKTRIG_ABORT;

		sound = static_cast<SOUND_TYPE>(args.m_VarsLocal.GetKeyNum("Sound"));
		delay = args.m_VarsLocal.GetKeyNum("Delay");
		anim = static_cast<ANIM_TYPE>(args.m_VarsLocal.GetKeyNum("Anim"));

		if ( args.m_iN1 == 1 )
			UpdateDir(m_Act_p);
		if ( fResource )
			m_atResource.m_Stroke_Count = static_cast<WORD>(args.m_VarsLocal.GetKeyNum("Strokes"));
		else
			m_atCreate.m_Stroke_Count = static_cast<WORD>(args.m_VarsLocal.GetKeyNum("Strokes"));
	}

	if ( sound )
		Sound(sound);
	if ( anim )
		UpdateAnimate(anim);	// keep trying and updating the animation

	if ( fResource )
	{
		if ( m_atResource.m_Stroke_Count )
			--m_atResource.m_Stroke_Count;
		if ( m_atResource.m_Stroke_Count < 1 )
			return SKTRIG_SUCCESS;
	}
	else
	{
		if ( m_atCreate.m_Stroke_Count )
			--m_atCreate.m_Stroke_Count;
		if ( m_atCreate.m_Stroke_Count < 1 )
			return SKTRIG_SUCCESS;
	}

	if ( delay < 10 )
		delay = 10;

	//Skill_SetTimeout();	// old behaviour, removed to keep up dynamic delay coming in with the trigger @SkillStroke
	SetTimeout(delay);
	return -SKTRIG_STROKE;	// keep active.
}

int CChar::Skill_Stage(CSkillDef::T_TYPE_ stage)
{
	// Call Triggers

	SKILL_TYPE skill = Skill_GetActive();
	if (skill == SKILL_NONE)
		return -1;

	CSphereExpArgs execArgs(this, this, skill, 0, 0);

	TRIGRET_TYPE iTrigRet = OnTrigger56((CCharDef::T_TYPE_)(CCharDef::T_SkillAbort + (stage - CSkillDef::T_Abort)), execArgs);
	if (iTrigRet == TRIGRET_RET_VAL)
	{
		return execArgs.m_vValRet;
	}

	CSkillDefPtr pSkillDef = g_Cfg.GetSkillDef(skill);
	if (pSkillDef)
	{
		// RES_Skill
		//CCharActState SaveState = m_Act;
		iTrigRet = pSkillDef->OnTriggerScript(execArgs, stage, CSkillDef::sm_Triggers[stage].m_pszName);
		// m_Act = SaveState;
		if (iTrigRet == TRIGRET_RET_VAL)
		{
			// They handled success, just clean up, don't do skill experience
			return execArgs.m_vValRet;
		}
	}

	switch (skill)
	{
	case SKILL_NONE:	// idling.
		return 0;
	case SKILL_ALCHEMY:
		return Skill_Alchemy(stage);
	case SKILL_ANATOMY:
	case SKILL_ANIMALLORE:
	case SKILL_ITEMID:
	case SKILL_ARMSLORE:
		return Skill_Information(stage);
	case SKILL_PARRYING:
		return 0;
	case SKILL_BEGGING:
		return Skill_Begging(stage);
	case SKILL_BLACKSMITHING:
		return Skill_Blacksmith(stage);
	case SKILL_BOWCRAFT:
		return Skill_Bowcraft(stage);
	case SKILL_PEACEMAKING:
		return Skill_Peacemaking(stage);
	case SKILL_CAMPING:
		return 0;
	case SKILL_CARPENTRY:
		return Skill_Carpentry(stage);
	case SKILL_CARTOGRAPHY:
		return Skill_Cartography(stage);
	case SKILL_COOKING:
		return Skill_Cooking(stage);
	case SKILL_DETECTINGHIDDEN:
		return Skill_DetectHidden(stage);
	case SKILL_ENTICEMENT:
		return Skill_Enticement(stage);
	case SKILL_EVALINT:
		return Skill_Information(stage);
	case SKILL_HEALING:
		return Skill_Healing(stage);
	case SKILL_FISHING:
		return Skill_Fishing(stage);
	case SKILL_FORENSICS:
		return Skill_Information(stage);
	case SKILL_HERDING:
		return Skill_Herding(stage);
	case SKILL_HIDING:
		return Skill_Hiding(stage);
	case SKILL_PROVOCATION:
		return Skill_Provocation(stage);
	case SKILL_INSCRIPTION:
		return Skill_Inscription(stage);
	case SKILL_LOCKPICKING:
		return Skill_Lockpicking(stage);
	case SKILL_MAGERY:
		return Skill_Magery(stage);
	case SKILL_MAGICRESISTANCE:
		return 0;
	case SKILL_TACTICS:
		return 0;
	case SKILL_SNOOPING:
		return Skill_Snooping(stage);
	case SKILL_MUSICIANSHIP:
		return Skill_Musicianship(stage);
	case SKILL_POISONING:	// 30
		return Skill_Poisoning(stage);
	case SKILL_ARCHERY:
		return Skill_Fighting(stage);
	case SKILL_SPIRITSPEAK:
		return Skill_SpiritSpeak(stage);
	case SKILL_STEALING:
		return Skill_Stealing(stage);
	case SKILL_TAILORING:
		return Skill_Tailoring(stage);
	case SKILL_TAMING:
		return Skill_Taming(stage);
	case SKILL_TASTEID:
		return Skill_Information(stage);
	case SKILL_TINKERING:
		return Skill_MakeItem(stage);
	case SKILL_TRACKING:
		return Skill_Tracking(stage);
	case SKILL_VETERINARY:
		return Skill_Healing(stage);
	case SKILL_SWORDSMANSHIP:
	case SKILL_MACEFIGHTING:
	case SKILL_FENCING:
	case SKILL_WRESTLING:
		return Skill_Fighting(stage);
	case SKILL_LUMBERJACKING:
		return Skill_Lumberjack(stage);
	case SKILL_MINING:
		return Skill_Mining(stage);
	case SKILL_MEDITATION:
		return Skill_Meditation(stage);
	case SKILL_Stealth:
		return Skill_Hiding(stage);
	case SKILL_RemoveTrap:
		return Skill_RemoveTrap(stage);
	case SKILL_NECROMANCY:
		return Skill_Magery(stage);

	case NPCACT_BREATH:
		return Skill_Act_Breath(stage);
	case NPCACT_LOOTING:
		return Skill_Act_Looting(stage);
	case NPCACT_THROWING:
		return Skill_Act_Throwing(stage);
	case NPCACT_TRAINING:
		return Skill_Act_Training(stage);
	case NPCACT_Napping:
		return Skill_Act_Napping(stage);

	default:
		if (!IsSkillBase(skill))
		{
			DEBUG_CHECK(IsSkillNPC(skill));
			if (stage == CSkillDef::T_Stroke)
				return(-CSkillDef::T_Stroke); // keep these active. (NPC modes)
			return 0;
		}
	}

	WriteString("Skill not implemented!");
	return -CSkillDef::T_QTY;
}

void CChar::Skill_Fail(bool fCancel)
{
	ADDTOCALLSTACK("CChar::Skill_Fail");
	// This is the normal skill check failure.
	// Other types of failure don't come here.
	//
	// ARGS:
	//	fCancel = no credt.
	//  else We still get some credit for having tried.

	SKILL_TYPE skill = Skill_GetActive();
	if (skill == SKILL_NONE)
		return;

	if (!IsSkillBase(skill))
	{
		DEBUG_CHECK(IsSkillNPC(skill));
		Skill_Cleanup();
		return;
	}

	if (m_Act_Difficulty > 0)
	{
		m_Act_Difficulty = -m_Act_Difficulty;
	}

	if (Skill_Stage(CSkillDef::T_Fail) >= 0)
	{
		// Get some experience for failure ?
		Skill_Experience(skill, m_Act_Difficulty);
	}

	Skill_Cleanup();
}

TRIGRET_TYPE CChar::Skill_OnTrigger(SKILL_TYPE skill, SKTRIG_TYPE stage)
{
	CScriptTriggerArgs pArgs;
	return Skill_OnTrigger(skill, stage, &pArgs);
}

TRIGRET_TYPE CChar::Skill_OnTrigger(SKILL_TYPE skill, SKTRIG_TYPE stage, CScriptTriggerArgs *pArgs)
{
	ADDTOCALLSTACK("CChar::Skill_OnTrigger");
	if ( !IsSkillBase(skill) )
		return TRIGRET_RET_DEFAULT;

	if ( !((stage == SKTRIG_SELECT) || (stage == SKTRIG_GAIN) || (stage == SKTRIG_USEQUICK) || (stage == SKTRIG_WAIT) || (stage == SKTRIG_TARGETCANCEL)) )
		m_Act_SkillCurrent = skill;

	pArgs->m_iN1 = skill;
	if ( g_Cfg.IsSkillFlag(skill, SKF_MAGIC) )
		pArgs->m_VarsLocal.SetNum("spell", m_atMagery.m_Spell, true);

	TRIGRET_TYPE iRet = TRIGRET_RET_DEFAULT;

	CSkillDef *pSkillDef = g_Cfg.GetSkillDef(skill);
	if ( pSkillDef && pSkillDef->HasTrigger(stage) )
	{
		// RES_SKILL
		CResourceLock s;
		if ( pSkillDef->ResourceLock(s) )
			iRet = CScriptObj::OnTriggerScript(s, CSkillDef::sm_szTrigName[stage], this, pArgs);
	}

	return iRet;
}

TRIGRET_TYPE CChar::Skill_OnCharTrigger(SKILL_TYPE skill, CTRIG_TYPE ctrig)
{
	CScriptTriggerArgs pArgs;
	return Skill_OnCharTrigger(skill, ctrig, &pArgs);
}

TRIGRET_TYPE CChar::Skill_OnCharTrigger(SKILL_TYPE skill, CTRIG_TYPE ctrig, CScriptTriggerArgs *pArgs)
{
	ADDTOCALLSTACK("CChar::Skill_OnCharTrigger");
	if ( !IsSkillBase(skill) )
		return TRIGRET_RET_DEFAULT;

	if ( !((ctrig == CTRIG_SkillSelect) || (ctrig == CTRIG_SkillGain) || (ctrig == CTRIG_SkillUseQuick) || (ctrig == CTRIG_SkillWait) || (ctrig == CTRIG_SkillTargetCancel)) )
		m_Act_SkillCurrent = skill;

	pArgs->m_iN1 = skill;
	if ( g_Cfg.IsSkillFlag(skill, SKF_MAGIC) )
		pArgs->m_VarsLocal.SetNum("spell", m_atMagery.m_Spell, true);

	return OnTrigger(ctrig, this, pArgs);
}

int CChar::Skill_Done()
{
	ADDTOCALLSTACK("CChar::Skill_Done");
	// We just finished using a skill. ASYNC timer expired.
	// m_Act_Skill = the skill.
	// Consume resources that have not already been consumed.
	// Confer the benefits of the skill.
	// calc skill gain based on this.
	//
	// RETURN: Did we succeed or fail ?
	//   0 = success
	//	 -CSkillDef::T_Stroke = stay in skill. (stroke)
	//   -CSkillDef::T_Fail = we must print the fail msg. (credit for trying)
	//   -CSkillDef::T_Abort = we must print the fail msg. (But get no credit, canceled )
	//   -CSkillDef::T_QTY = special failure. clean up the skill but say nothing. (no credit)

	SKILL_TYPE skill = Skill_GetActive();
	if (skill == SKILL_NONE)	// we should not be coming here (timer should not have expired)
		return -CSkillDef::T_QTY;

	// multi stroke tried stuff here first.
	// or stuff that never really fails.
	int iRet = Skill_Stage(CSkillDef::T_Stroke);
	if (iRet < 0)
		return(iRet);
	if (m_Act_Difficulty < 0 && !IsGM())
	{
		// Was Bound to fail. But we had to wait for the timer anyhow.
		return -CSkillDef::T_Fail;
	}

	// Success for the skill.
	iRet = Skill_Stage(CSkillDef::T_Success);
	if (iRet < 0)
		return iRet;

	// Success = Advance the skill
	Skill_Experience(skill, m_Act_Difficulty);
	Skill_Cleanup();
	return(-CSkillDef::T_Success);
}

bool CChar::Skill_Wait(SKILL_TYPE skilltry)
{
	ADDTOCALLSTACK("CChar::Skill_Wait");
	// Some sort of push button skill.
	// We want to do some new skill. Can we ?
	// If this is the same skill then tell them to wait.

	SKILL_TYPE skill = Skill_GetActive();
	CScriptTriggerArgs pArgs(skilltry, skill);

	if ( IsTrigUsed(TRIGGER_SKILLWAIT) )
	{
		switch ( Skill_OnCharTrigger(skilltry, CTRIG_SkillWait, &pArgs) )
		{
			case TRIGRET_RET_TRUE:
				return true;
			case TRIGRET_RET_FALSE:
				Skill_Fail(true);
				return false;
			default:
				break;
		}
	}
	if ( IsTrigUsed(TRIGGER_WAIT) )
	{
		switch ( Skill_OnTrigger(skilltry, SKTRIG_WAIT, &pArgs) )
		{
			case TRIGRET_RET_TRUE:
				return true;
			case TRIGRET_RET_FALSE:
				Skill_Fail(true);
				return false;
			default:
				break;
		}
	}

	if ( IsStatFlag(STATF_DEAD|STATF_Freeze|STATF_Stone) )
	{
		SysMessageDefault(DEFMSG_SKILLWAIT_1);
		return true;
	}

	if ( skill == SKILL_NONE )	// not currently doing anything.
	{
		if ( skilltry != SKILL_STEALTH )
			Reveal();
		return false;
	}

	if ( IsStatFlag(STATF_War) )
	{
		SysMessageDefault(DEFMSG_SKILLWAIT_2);
		return true;
	}

	// Cancel passive actions
	if ( skilltry != skill )
	{
		if ( (skill == SKILL_MEDITATION) || (skill == SKILL_HIDING) || (skill == SKILL_STEALTH) )		// SKILL_SPIRITSPEAK ?
		{
			Skill_Fail(true);
			return false;
		}
	}

	SysMessageDefault(DEFMSG_SKILLWAIT_3);
	return true;
}

bool CChar::Skill_Start(SKILL_TYPE skill, int iDifficulty)
{
	ADDTOCALLSTACK("CChar::Skill_Start");
	// We have all the info we need to do the skill. (targeting etc)
	// Set up how long we have to wait before we get the desired results from this skill.
	// Set up any animations/sounds in the mean time.
	// Calc if we will succeed or fail.
	// ARGS:
	//  iDifficulty = 0-100
	// RETURN:
	//  false = failed outright with no wait. "You have no chance of taming this"

	if (g_Serv.IsLoading())
	{
		if (skill != SKILL_NONE &&
			!IsSkillBase(skill) &&
			!IsSkillNPC(skill))
		{
			DEBUG_ERR(("UID:0%x Bad Skill %d for '%s'" LOG_CR, GetUID(), skill, (LPCTSTR)GetName()));
			return(false);
		}
		m_Act_SkillCurrent = skill;
		return(true);
	}

	if (Skill_GetActive() != SKILL_NONE)
	{
		Skill_Fail(true);	// Fail previous skill unfinished. (with NO skill gain!)
	}

	if (skill != SKILL_NONE)
	{
		m_Act_SkillCurrent = skill;	// Start using a skill.
		m_Act_Difficulty = iDifficulty;

		ASSERT(IsSkillBase(skill) || IsSkillNPC(skill));

		// Some skill can start right away. Need no targeting.
		// 0-100 scale of Difficulty
		m_Act_Difficulty = Skill_Stage(CSkillDef::T_Start);
		if (m_Act_Difficulty < 0)
		{
			Skill_Cleanup();
			return(false);
		}

		if (IsSkillBase(skill))
		{
			CSkillDefPtr pSkillDef = g_Cfg.GetSkillDef(skill);
			ASSERT(pSkillDef);
			int iWaitTime = pSkillDef->m_Delay.GetLinear(Skill_GetBase(skill));
			if (iWaitTime)
			{
				// How long before complete skill.
				SetTimeout(iWaitTime);
			}
		}
		if (IsTimerExpired())
		{
			// the skill should have set it's own delay!?
			SetTimeout(1);
		}
		if (m_Act_Difficulty > 0)
		{
			if (!Skill_CheckSuccess(skill, m_Act_Difficulty))
				m_Act_Difficulty = -m_Act_Difficulty; // will result in Failure ?
		}
	}

	return(true);
}