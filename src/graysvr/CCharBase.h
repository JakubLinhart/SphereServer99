//
// CCharBase.h
//

#ifndef _INC_CCHARBASE_H
#define _INC_CCHARBASE_H
#pragma once

#define CCharDefPtr CCharBase*
#define CCharDef CCharBase

enum CTRIG_TYPE
{
	CTRIG_AAAUNUSED,
	CTRIG_AfterClick,
	CTRIG_Attack,				// Char attacked someone (SRC)
	CTRIG_CallGuards,
	CTRIG_charAttack,
	CTRIG_charClick,
	CTRIG_charClientTooltip,
	CTRIG_charContextMenuRequest,
	CTRIG_charContextMenuSelect,
	CTRIG_charDClick,
	CTRIG_charTradeAccepted,
	CTRIG_ClientTooltip,		// Char tooltip got requested by someone
	CTRIG_CombatAdd,			// Char added someone on attacker list
	CTRIG_CombatDelete,			// Char deleted someone from attacker list
	CTRIG_CombatEnd,			// Char had end an combat
	CTRIG_CombatStart,			// Char had started an combat
	CTRIG_ContextMenuRequest,
	CTRIG_ContextMenuSelect,
	CTRIG_Create,				// Char got created (not placed in the world yet)
	CTRIG_Criminal,				// Char got flagged criminal for someone
	CTRIG_Death,				// Char got killed
	CTRIG_DeathCorpse,			// Char dead corpse is being created
	CTRIG_Destroy,				// Char got destroyed (removed)
	CTRIG_Dismount,
	CTRIG_Eat,
	CTRIG_EffectAdd,
	CTRIG_EnvironChange,		// Char environment has changed (region, light, weather, season)
	CTRIG_ExpChange,			// Char EXP got changed
	CTRIG_ExpLevelChange,		// Char LEVEL got changed
	CTRIG_FameChange,			// Char FAME got changed
	CTRIG_FollowersUpdate,		// Char CURFOLLOWER got changed
	CTRIG_GetHit,				// Char got hit by someone
	CTRIG_Hit,					// Char hit someone
	CTRIG_HitCheck,
	CTRIG_HitIgnore,
	CTRIG_HitMiss,
	CTRIG_HitTry,				// Char is trying to hit someone
	CTRIG_HouseDesignCommit,	// Char committed a new house design
	CTRIG_HouseDesignExit,		// Char exited house design mode
	CTRIG_itemAfterClick,
	CTRIG_itemBuy,
	CTRIG_itemClick,			// Char single clicked an item
	CTRIG_itemClientTooltip,
	CTRIG_itemContextMenuRequest,
	CTRIG_itemContextMenuSelect,
	CTRIG_itemCreate,
	CTRIG_itemDamage,
	CTRIG_itemDCLICK,			// Char double clicked an item
	CTRIG_itemDestroy,			// Char destroyed an item
	CTRIG_itemDROPON_CHAR,		// Char dropped an item over an char
	CTRIG_itemDROPON_GROUND,	// Char dropped an item on ground
	CTRIG_itemDROPON_ITEM,		// Char dropped an item over an item
	CTRIG_itemDROPON_SELF,		// Char dropped an item inside an container itself
	CTRIG_itemDROPON_TRADE,		// Char dropped an item on trade window
	CTRIG_itemEQUIP,			// Char equipped an item
	CTRIG_itemEQUIPTEST,
	CTRIG_itemMemoryEquip,
	CTRIG_itemPICKUP_GROUND,	// Char picked up an item on the ground
	CTRIG_itemPICKUP_PACK,		// Char picked up an item inside an container
	CTRIG_itemPICKUP_SELF,		// Char picked up an item inside an container itself
	CTRIG_itemPICKUP_STACK,		// Char picked up an item from an stack
	CTRIG_itemSell,
	CTRIG_itemSPELL,			// Char casted an spell on an item
	CTRIG_itemSTEP,				// Char stepped on an item
	CTRIG_itemTARGON_CANCEL,
	CTRIG_itemTARGON_CHAR,
	CTRIG_itemTARGON_GROUND,
	CTRIG_itemTARGON_ITEM,
	CTRIG_itemTimer,
	CTRIG_itemToolTip,			// Char requested an item tooltip
	CTRIG_itemUNEQUIP,			// Char unequipped an item
	CTRIG_Jailed,				// Char got jailed/forgived
	CTRIG_KarmaChange,			// Char KARMA got changed
	CTRIG_Kill,					// Char killed someone
	CTRIG_LogIn,				// Char client is logging in
	CTRIG_LogOut,				// Char client is logging out
	CTRIG_Mount,
	CTRIG_MurderDecay,			// Char got an murder count (KILL) decayed
	CTRIG_MurderMark,			// Char got flagged as murderer
	CTRIG_NotoSend,				// Char notoriety got requested by someone
	CTRIG_NPCAcceptItem,		// NPC accepted an item given by someone (according to DESIRES)
	CTRIG_NPCActFight,
	CTRIG_NPCActFollow,			// NPC decided to follow someone
	CTRIG_NPCAction,
	CTRIG_NPCHearGreeting,		// NPC heared someone for the first time
	CTRIG_NPCHearUnknown,		// NPC heared something that it doesn't understand
	CTRIG_NPCLookAtChar,
	CTRIG_NPCLookAtItem,
	CTRIG_NPCLostTeleport,		// NPC got teleported back to spawn point after walk too far
	CTRIG_NPCRefuseItem,		// NPC refused an item given by someone
	CTRIG_NPCRestock,
	CTRIG_NPCSeeNewPlayer,		// NPC saw an new player for the first time
	CTRIG_NPCSeeWantItem,		// NPC saw an wanted item when looting corpses
	CTRIG_NPCSpecialAction,
	CTRIG_PartyDisband,			// Char disbanded his party
	CTRIG_PartyInvite,			// Char invited someone to join his party
	CTRIG_PartyLeave,			// Char had left an party
	CTRIG_PartyRemove,			// Char got removed from an party
	CTRIG_PersonalSpace,		// Char got stepped by someone
	CTRIG_PetDesert,			// NPC deserted his master after get attacked by him or get starving for too long
	CTRIG_Profile,				// Char profile got requested by someone
	CTRIG_ReceiveItem,			// NPC is receiving an item from someone
	CTRIG_RegenStat,			// Char stats got regenerated (hits, mana, stam, food)
	CTRIG_RegionEnter,
	CTRIG_RegionLeave,
	CTRIG_RegionResourceFound,	// Char found an resource using an gathering skill
	CTRIG_RegionResourceGather,	// Char gathered an resource using an gathering skill
	CTRIG_Rename,				// Char got renamed
	CTRIG_Resurrect,			// Char got resurrected
	CTRIG_SeeCrime,				// Char saw someone nearby committing a crime
	CTRIG_SeeHidden,			// Char saw someone hidden
	CTRIG_SeeSnoop,				// Char saw someone nearby snooping an item
	CTRIG_SkillAbort,			// SKTRIG_ABORT
	CTRIG_SkillChange,
	CTRIG_SkillFail,			// SKTRIG_FAIL
	CTRIG_SkillGain,			// SKTRIG_GAIN
	CTRIG_SkillMakeItem,
	CTRIG_SkillMenu,
	CTRIG_SkillPreStart,		// SKTRIG_PRESTART
	CTRIG_SkillSelect,			// SKTRIG_SELECT
	CTRIG_SkillStart,			// SKTRIG_START
	CTRIG_SkillStroke,			// SKTRIG_STROKE
	CTRIG_SkillSuccess,			// SKTRIG_SUCCESS
	CTRIG_SkillTargetCancel,	// SKTRIG_TARGETCANCEL
	CTRIG_SkillUseQuick,		// SKTRIG_USEQUICK
	CTRIG_SkillWait,			// SKTRIG_WAIT
	CTRIG_SpellBook,			// Char opened a spellbook
	CTRIG_SpellCast,			// Char casted a spell
	CTRIG_SpellEffect,			// Char got hit by a spell
	CTRIG_SpellFail,			// Char failed an spell cast
	CTRIG_SpellSelect,			// Char selected a spell to cast
	CTRIG_SpellSuccess,			// Char succeeded an spell cast
	CTRIG_SpellTargetCancel,	// Char cancelled spell target
	CTRIG_StatChange,			// Char stats got changed (STR/hits, INT/mana, DEX/stam, food)
	CTRIG_Step,
	CTRIG_StepStealth,			// Char is walking/running while hidden
	CTRIG_Targon_Cancel,		// Char cancelled current TARGETF
	CTRIG_ToggleFlying,			// Char toggled flying mode (gargoyle only)
	CTRIG_ToolTip,				// Char tooltip got requested by someone
	CTRIG_TradeAccepted,		// Char accepted a trade window
	CTRIG_TradeClose,			// Char closed a trade window
	CTRIG_TradeCreate,			// Char created a trade window
	CTRIG_UserBugReport,
	CTRIG_UserChatButton,
	CTRIG_UserClick,			// Char got single clicked by someone
	CTRIG_UserDClick,			// Char got double clicked by someone
	CTRIG_UserExtCmd,
	CTRIG_UserExWalkLimit,
	CTRIG_UserGuildButton,
	CTRIG_UserKRToolbar,
	CTRIG_UserMailBag,
	CTRIG_UserQuestArrowClick,
	CTRIG_UserQuestButton,
	CTRIG_UserSkills,
	CTRIG_UserSpecialMove,
	CTRIG_UserStats,
	CTRIG_UserUltimaStoreButton,
	CTRIG_UserVirtue,
	CTRIG_UserVirtueInvoke,
	CTRIG_UserWarmode,
	CTRIG_QTY
};

class CCharBase : public CBaseBaseDef	// define basic info about each "TYPE" of monster/creature
{
	// RES_CHARDEF
public:
	enum P_TYPE_
	{
		T_Create,
		T_Death,
		T_DeathCorpse,
		T_Destroy,
		T_EnvironChange,
		T_GetHit,
		T_Hit,
		T_HitMiss,
		T_HitTry,
		T_LogIn, 
		T_LogOut,
		T_NPCAcceptItem,
		T_NPCHearGreeting,
		T_NPCHearNeed,
		T_NPCHearUnknown,
		T_NPCRefuseItem,
		T_NPCRestock,
		T_NPCSeeNewPlayer,
		T_NPCSeeWantItem,
		T_PersonalSpace,
		T_ReceiveItem,
		T_SpellCast,
		T_SpellEffect,
		T_Step,
		T_UserButton,
		T_UserClick,
		T_UserDClick,
		T_UserToolTip
	};

	static const char *m_sClassName;
	static LPCTSTR const sm_szLoadKeys[];

	explicit CCharBase(CREID_TYPE id);
	~CCharBase() { }

public:
	ITEMID_TYPE m_trackID;			// ITEMID_TYPE what look like on tracking
	SOUND_TYPE m_soundBase;			// sounds (typically 5 sounds per creature, humans and birds have more)
	SOUND_TYPE m_soundIdle;
	SOUND_TYPE m_soundNotice;
	SOUND_TYPE m_soundHit;
	SOUND_TYPE m_soundGetHit;
	SOUND_TYPE m_soundDie;

	CResourceQtyArray m_FoodType;	// FOODTYPE=MEAT 15 (3)
	int m_MaxFood;					// Derived from foodtype, this is the max amount of food we can eat

	WORD m_defense;			// base defense (basic to body type)
	WORD m_attack;
	DWORD m_Anims;			// bitmask of animations available for monsters (ANIM_TYPE)
	ITEMID_TYPE m_MountID;	// Can creature be mounted?
	HUE_TYPE m_wBloodHue;	// when damaged, what color is the blood (-1 = no blood)
	HUE_TYPE m_wColor;

	int m_Str;
	int m_Dex;
	int m_Int;
	short m_iMoveRate;
	short m_FollowerSlots;
	short m_FollowerMax;
	int m_Tithing;

	// NPC info
	CResourceQtyArray m_Aversions;	// traps, civilization
	CResourceQtyArray m_Desires;	// desires that are typical for the char class (see also m_sNeed)
	CResourceRefArray m_Speech;		// speech fragment list (other stuff we know)
	unsigned int m_iHireDayWage;	// gold required to hire an player vendor

public:
	static CCharBase *FindCharBase(CREID_TYPE id);
	bool SetDispID(CREID_TYPE id);
	LPCTSTR GetTradeName() const;

	bool r_WriteVal(LPCTSTR pszKey, CGString &sVal, CTextConsole *pSrc, CScriptTriggerArgs* pArgs);
	bool r_LoadVal(CScript &s, CScriptTriggerArgs* pArgs, CTextConsole* pSrc);
	bool r_Load(CScript &s);

private:
	void CopyBasic(const CCharBase *pCharDef);
	void SetFoodType(LPCTSTR pszFood);

public:
	virtual void UnLink()
	{
		// We are being reloaded
		m_FoodType.RemoveAll();
		m_Desires.RemoveAll();
		m_Speech.RemoveAll();
		CBaseBaseDef::UnLink();
	}

	CREID_TYPE GetID() const
	{
		return static_cast<CREID_TYPE>(GetResourceID().GetResIndex());
	}

	CREID_TYPE GetDispID() const
	{
		return static_cast<CREID_TYPE>(m_dwDispIndex);
	}

	static bool IsValidDispID(CREID_TYPE id)
	{
		return ((id > CREID_INVALID) && (id < CREID_QTY));
	}

	static bool IsPlayableID(CREID_TYPE id, bool bCheckGhost = false)
	{
		return (IsHumanID(id, bCheckGhost) || IsElfID(id, bCheckGhost) || IsGargoyleID(id, bCheckGhost));
	}

	static bool IsHumanID(CREID_TYPE id, bool bCheckGhost = false)
	{
		if ( bCheckGhost )
			return ((id == CREID_MAN) || (id == CREID_WOMAN) || (id == CREID_EQUIP_GM_ROBE) || (id == CREID_GHOSTMAN) || (id == CREID_GHOSTWOMAN));
		else
			return ((id == CREID_MAN) || (id == CREID_WOMAN) || (id == CREID_EQUIP_GM_ROBE));
	}

	static bool IsElfID(CREID_TYPE id, bool bCheckGhost = false)
	{
		if ( bCheckGhost )
			return ((id == CREID_ELFMAN) || (id == CREID_ELFWOMAN) || (id == CREID_ELFGHOSTMAN) || (id == CREID_ELFGHOSTWOMAN));
		else
			return ((id == CREID_ELFMAN) || (id == CREID_ELFWOMAN));
	}

	static bool IsGargoyleID(CREID_TYPE id, bool bCheckGhost = false)
	{
		if ( bCheckGhost )
			return ((id == CREID_GARGMAN) || (id == CREID_GARGWOMAN) || (id == CREID_GARGGHOSTMAN) || (id == CREID_GARGGHOSTWOMAN));
		else
			return ((id == CREID_GARGMAN) || (id == CREID_GARGWOMAN));
	}

	bool IsFemale() const
	{
		return (m_Can & CAN_C_FEMALE);
	}

	bool IsValidNewObj() const
	{
		return true;
	}

private:
	CCharBase(const CCharBase &copy);
	CCharBase &operator=(const CCharBase &other);
};

#endif	// _INC_CCHARBASE_H
