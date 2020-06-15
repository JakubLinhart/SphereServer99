#include "graysvr.h"	// predef header.
#include "../network/send.h"
#include "../network/receive.h"

// endgroup, master, hue ????

enum GUMPCTL_TYPE	// controls we can put in a gump.
{
	GUMPCTL_BUTTON	= 0,		// 7 = X,Y,Down gump,Up gump,pressable(1/0),page,id
	GUMPCTL_BUTTONTILEART,		// NEW: 11 = X,Y,Down gump,Up gump,pressable(1/0),page,id,tileart,hue,X,Y
	GUMPCTL_CHECKBOX,			// 6 = x,y,gumpcheck,gumpuncheck,starting state,checkid

	GUMPCTL_CHECKERTRANS,		// NEW: x,y,w,h
	GUMPCTL_CROPPEDTEXT,		// 6 = x,y,sx,sy,color?,startindex

	GUMPCTL_DCROPPEDTEXT,
	GUMPCTL_DHTMLGUMP,
	GUMPCTL_DORIGIN,
	GUMPCTL_DTEXT,
	GUMPCTL_DTEXTENTRY,
	GUMPCTL_DTEXTENTRYLIMITED,

	GUMPCTL_GROUP,

	GUMPCTL_GUMPPIC,		// 3 = x,y,gumpID	 hue=color// put gumps in the dlg.
	GUMPCTL_GUMPPICTILED,		// x, y, gumpID, w, h, hue=color
	GUMPCTL_HTMLGUMP,		// 7 = x,y,sx,sy, 0 0 0
	GUMPCTL_HTMLGUMPA,

	// Not really controls but more attributes.
	GUMPCTL_NOCLOSE,		// 0 = The gump cannot be closed by right clicking.
	GUMPCTL_NODISPOSE,		// 0 = The gump cannot be closed by gump-closing macro.
	GUMPCTL_NOMOVE,			// 0 = The gump cannot be moved around.

	GUMPCTL_PAGE,			// 1 = set current page number	// for multi tab dialogs.

	GUMPCTL_RADIO,			// 6 = x,y,gump1,gump2,starting state,id
	GUMPCTL_RESIZEPIC,		// 5 = x,y,gumpback,sx,sy	// can come first if multi page. put up some background gump
	GUMPCTL_SETLOCATION,
	GUMPCTL_SETTEXT,
	GUMPCTL_TEXT,			// 4 = x,y,color?,startstringindex	// put some text here.
	GUMPCTL_TEXTA,
	GUMPCTL_TEXTENTRY,
	GUMPCTL_TEXTENTRYA,
	GUMPCTL_TEXTENTRYLIMITED,
	GUMPCTL_TILEPIC,		// 3 = x,y,item	// put item tiles in the dlg.
	GUMPCTL_TILEPICHUE,		// NEW: x,y,item,color

	GUMPCTL_TOOLTIP,		// From SE client. tooltip cliloc(1003000)

	GUMPCTL_XMFHTMLGUMP,		// 7 = x,y,sx,sy, cliloc(1003000) hasBack canScroll
	GUMPCTL_XMFHTMLGUMPCOLOR,	// NEW: x,y,w,h ???
	GUMPCTL_XMFHTMLTOK,			// 9 = x y width height has_background has_scrollbar color cliloc_id @args

	GUMPCTL_QTY
};

bool CDialogDefSetup::r_Verb(CScript& s, CTextConsole* pSrc, CScriptTriggerArgs* pArgs)
{
	ADDTOCALLSTACK("CDialogDefSetup::r_Verb");

	return m_pDef->r_Verb(s, pSrc, pArgs, this);
}

bool CDialogDefSetup::r_WriteValBase(LPCTSTR pszKey, CGString& sVal, CTextConsole* pSrc, CScriptTriggerArgs* pArgs)
{
	return CScriptObj::r_WriteVal(pszKey, sVal, pSrc, pArgs);
}

bool CDialogDefSetup::r_WriteVal(LPCTSTR pszKey, CGString& sVal, CTextConsole* pSrc, CScriptTriggerArgs* pArgs)
{
	ADDTOCALLSTACK("CDialogDefSetup::r_WriteVal");

	return m_pDef->r_WriteVal(pszKey, sVal, pSrc, pArgs, this);
}

bool CDialogDefSetup::r_LoadVal(CScript& s, CScriptTriggerArgs* pArgs, CTextConsole* pSrc)
{
	ADDTOCALLSTACK("CDialogDefSetup::r_LoadVal");

	return m_pDef->r_LoadVal(s, pArgs, pSrc);
}

//*******************************************
// -CDialogDef
LPCTSTR const CDialogDef::sm_szLoadKeys[GUMPCTL_QTY+1] =
{
	"button",
	"buttontileart",
	"checkbox",

	"checkertrans",
	"croppedtext",

	"dcroppedtext",
	"dhtmlgump",
	"dorigin",
	"dtext",
	"dtextentry",
	"dtextentrylimited",

	"group",

	"gumppic",
	"gumppictiled",
	"htmlgump",
	"htmlgumpa",

	"noclose",
	"nodispose",
	"nomove",

	"page",

	"radio",
	"resizepic",
	"setlocation",
	"settext",
	"text",
	"texta",
	"textentry",
	"textentrya",
	"textentrylimited",
	"tilepic",
	"tilepichue",

	"tooltip",

	"xmfhtmlgump",
	"xmfhtmlgumpcolor",
	"xmfhtmltok",

	NULL
};


size_t CDialogDef::GumpAddText( LPCTSTR pszText )
{
	ADDTOCALLSTACK("CDialogDef::GumpAddText");
	m_sText[m_iTexts] = pszText;
	m_iTexts++;
	return (m_iTexts - 1);
}

#define SKIP_ALL( args )		SKIP_SEPARATORS( args ); GETNONWHITESPACE( args );
#define GET_ABSOLUTE( c )		SKIP_ALL( pszArgs );	int c = Exp_GetSingle( pszArgs );

#define GET_EVAL( c )		SKIP_ALL( pszArgs );	int c = Exp_GetVal( pszArgs );

#define GET_RELATIVE( c, base )								\
	SKIP_ALL( pszArgs ); int c;								\
	if ( *pszArgs == '-' && IsSpace(pszArgs[1]))				\
		c	= base, ++pszArgs;								\
	else if ( *pszArgs == '+' )								\
		c = base + Exp_GetSingle( ++pszArgs );					\
	else if ( *pszArgs == '-' )								\
		c = base - Exp_GetSingle( ++pszArgs );					\
	else if ( *pszArgs == '*' )								\
		base = c	= base + Exp_GetSingle( ++pszArgs );		\
	else													\
		c = Exp_GetSingle( pszArgs );			
	
bool CDialogDef::r_Verb(CScript& s, CTextConsole* pSrc, CScriptTriggerArgs* pArgs)	// some command on this object as a target
{
	return r_Verb(s, pSrc, pArgs, false);
}

bool CDialogDef::r_Verb( CScript & s, CTextConsole * pSrc, CScriptTriggerArgs* pArgs, CDialogDefSetup* pDlgSetup)	// some command on this object as a target
{
	ADDTOCALLSTACK("CDialogDef::r_Verb");
	EXC_TRY("Verb");
	// The first part of the key is GUMPCTL_TYPE
	LPCTSTR pszKey = s.GetKey();

	int index = FindTableSorted( pszKey, sm_szLoadKeys, COUNTOF(sm_szLoadKeys)-1 );
	if ( index < 0 )
	{
		CGString sVal;

		if (pDlgSetup)
		{
			if (m_tagHolder.r_Verb(s, pSrc, pArgs, pDlgSetup))
				return true;
		}
		CScriptTriggerArgs Args(s.GetArgRaw());
		if (pDlgSetup)
		{
			if (pDlgSetup->r_Call(s.GetKey(), pSrc, &Args, &sVal))
				return true;
		}
		else
		{
			if (r_Call(s.GetKey(), pSrc, &Args, &sVal))
				return true;
		}
		if (!m_pObj)
			return CResourceLink::r_Verb(s, pSrc, pArgs);
		if (m_pObj->r_Verb(s, pSrc, pArgs))
			return true;
		if (!pDlgSetup)
		{
			if (m_tagHolder.r_Verb(s, pSrc, pArgs, this))
				return true;
		}
	}
	
	LPCTSTR pszArgs	= s.GetArgStr();
	CScriptObj* pExpressionContext = pDlgSetup != NULL ? dynamic_cast<CScriptObj*>(pDlgSetup) : dynamic_cast<CScriptObj*>(this);
	CExpression expr(pArgs, pSrc, pExpressionContext);

	switch( index )
	{
		case GUMPCTL_PAGE:
		{
			if ( m_iControls >= (COUNTOF(m_sControls) - 1) )
				return false;

			GET_ABSOLUTE( page );

			if ( page <= 0 )		return true;

			int	iNewPage;
			if ( m_iPage == 0 || page > m_iPage || page == 0 )
				iNewPage	= page;
			else if ( page == m_iPage  )
				iNewPage	= 1;
			else
				iNewPage	= page + 1;	

			m_sControls[m_iControls].Format( "page %d", iNewPage );
			m_iControls++;
			return true;
		}
		case GUMPCTL_BUTTON:			// 7 = X,Y,Down gump,Up gump,pressable(1/0),page,id
		{
			TCHAR* ppArgs[7];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[2], &(ppArgs[3]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[3], &(ppArgs[4]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[4], &(ppArgs[5]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[5], &(ppArgs[6]))) return false;

			int x = expr.GetVal(ppArgs[0]);
			int y = expr.GetVal(ppArgs[1]);
			int downGump = expr.GetVal(ppArgs[2]);
			int upGump = expr.GetVal(ppArgs[3]);
			int pressable = expr.GetVal(ppArgs[4]);
			int page = expr.GetVal(ppArgs[5]);
			int id = expr.GetVal(ppArgs[6]);

			m_sControls[m_iControls].Format("Button %d %d %d %d %d %d %d", x, y, downGump, upGump, pressable, page, id);
			m_iControls++;
			return true;
		}
		case GUMPCTL_BUTTONTILEART:		// 11 = X,Y,Down gump,Up gump,pressable(1/0),page,id,tileart,hue,X,Y
		{
			if ( m_iControls >= (COUNTOF(m_sControls) - 1) )
				return false;

			GET_RELATIVE( x, m_iOriginX );
			GET_RELATIVE( y, m_iOriginY );
			GET_ABSOLUTE( down );
			GET_ABSOLUTE( up );
			GET_ABSOLUTE( press );
			GET_ABSOLUTE( page );
			GET_ABSOLUTE( id );

			int	iNewPage;
			if ( m_iPage == 0 || page > m_iPage || page == 0 )
				iNewPage	= page;
			else if ( page == m_iPage  )
				iNewPage	= 1;
			else
				iNewPage	= page + 1;	

			if (index == GUMPCTL_BUTTON)
				m_sControls[m_iControls].Format( "button %d %d %d %d %d %d %d", x, y, down, up, press, iNewPage, id );
			else
			{
				GET_ABSOLUTE( tileId );
				GET_ABSOLUTE( tileHue );
				GET_ABSOLUTE( tileX );
				GET_ABSOLUTE( tileY );				
				
				m_sControls[m_iControls].Format( "buttontileart %d %d %d %d %d %d %d %d %d %d %d", x, y, down, up, press, iNewPage, id, tileId, tileHue, tileX, tileY );
			}

			m_iControls++;
			return true;
		}
		case GUMPCTL_GUMPPIC:
		{
			TCHAR* ppArgs[3];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]))) return false;

			int x = expr.GetVal(ppArgs[0]);
			int y = expr.GetVal(ppArgs[1]);
			int id = expr.GetVal(ppArgs[2]);

			m_sControls[m_iControls].Format( "GumpPic %d %d %d", x, y, id );
			m_iControls++;
			return true;
		}
		case GUMPCTL_GUMPPICTILED:
		{
			TCHAR* ppArgs[5];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[2], &(ppArgs[3]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[3], &(ppArgs[4]))) return false;

			int x = expr.GetVal(ppArgs[0]);
			int y = expr.GetVal(ppArgs[1]);
			int sx = expr.GetVal(ppArgs[2]);
			int sy = expr.GetVal(ppArgs[3]);
			int id = expr.GetVal(ppArgs[4]);

			m_sControls[m_iControls].Format( "GumpPicTiled %d %d %d %d %d", x, y, sx, sy, id );
			m_iControls++;
			return true;
		}
		case GUMPCTL_RESIZEPIC:
		{
			TCHAR* ppArgs[5];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[2], &(ppArgs[3]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[3], &(ppArgs[4]))) return false;

			int x = expr.GetVal(ppArgs[0]);
			int y = expr.GetVal(ppArgs[1]);
			int id = expr.GetVal(ppArgs[2]);
			int sX = expr.GetVal(ppArgs[3]);
			int sY = expr.GetVal(ppArgs[4]);

			m_sControls[m_iControls].Format( "ResizePic %d %d %d %d %d", x, y, id, sX, sY );
			m_iControls++;
			return true;
		}
		case GUMPCTL_TILEPIC:
		{
			TCHAR* ppArgs[3];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]))) return false;

			int x = expr.GetVal(ppArgs[0]);
			int y = expr.GetVal(ppArgs[1]);
			int id = expr.GetVal(ppArgs[2]);

			m_sControls[m_iControls].Format("TilePic %d %d %d", x, y, id);
			m_iControls++;
			return true;
		}
		case GUMPCTL_TILEPICHUE:
		{
			TCHAR* ppArgs[4];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[2], &(ppArgs[3]))) ppArgs[3] = "0";

			int x = expr.GetVal(ppArgs[0]);
			int y = expr.GetVal(ppArgs[1]);
			int id = expr.GetVal(ppArgs[2]);
			int color = expr.GetVal(ppArgs[3]);

			m_sControls[m_iControls].Format( "TilePicHue %d %d %d %d", x, y, id, color);
			m_iControls++;
			return true;
		}
		case GUMPCTL_DTEXT:
		{
			if ( m_iControls >= (COUNTOF(m_sControls) - 1) )
				return false;
			if ( m_iTexts >= (COUNTOF(m_sText) - 1) )
				return false;

			GET_RELATIVE( x, m_iOriginX );
			GET_RELATIVE( y, m_iOriginY );
			GET_ABSOLUTE( hue );
			SKIP_ALL( pszArgs )
			if ( *pszArgs == '.' )			pszArgs++;

			size_t iText = GumpAddText( *pszArgs ? pszArgs : "" );
			m_sControls[m_iControls].Format( "text %d %d %d %" FMTSIZE_T, x, y, hue, iText );
			m_iControls++;
			return true;
		}
		case GUMPCTL_DCROPPEDTEXT:
		{
			if ( m_iControls >= (COUNTOF(m_sControls) - 1) )
				return false;
			if ( m_iTexts >= (COUNTOF(m_sText) - 1) )
				return false;
			
			GET_RELATIVE( x, m_iOriginX );
			GET_RELATIVE( y, m_iOriginY );
			GET_ABSOLUTE( w );
			GET_ABSOLUTE( h );
			GET_ABSOLUTE( hue );
			SKIP_ALL( pszArgs )
			if ( *pszArgs == '.' )			pszArgs++;

			size_t iText = GumpAddText( *pszArgs ? pszArgs : "" );
			m_sControls[m_iControls].Format( "croppedtext %d %d %d %d %d %" FMTSIZE_T, x, y, w, h, hue, iText );
			m_iControls++;
			return true;
		}
		case GUMPCTL_HTMLGUMPA:
		{
			if (m_iControls >= (COUNTOF(m_sControls) - 1))
				return false;
			if (m_iTexts >= (COUNTOF(m_sText) - 1))
				return false;

			TCHAR* ppArgs[7];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[2], &(ppArgs[3]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[3], &(ppArgs[4]))) return false;
			if (!Str_Parse(ppArgs[4], &(ppArgs[5]), ",")) return false;
			if (!Str_ParseExpressionArgument(ppArgs[5], &(ppArgs[6]))) return false;

			int x = expr.GetVal(ppArgs[0]);
			int y = expr.GetVal(ppArgs[1]);
			int w = expr.GetVal(ppArgs[2]);
			int h = expr.GetVal(ppArgs[3]);

			int idx = GumpAddText(Str_TrimDoublequotes(ppArgs[4]));

			int unk1 = expr.GetVal(ppArgs[5]);
			int unk2 = expr.GetVal(ppArgs[6]);

			m_sControls[m_iControls].Format("HTMLGump %d %d %d %d %d %d %d", x, y, w, h, idx, unk1, unk2);
			m_iControls++;
			return true;
		}
		case GUMPCTL_HTMLGUMP:
		case GUMPCTL_DHTMLGUMP:
		{
			if ( m_iControls >= (COUNTOF(m_sControls) - 1) )
				return false;
			if ( m_iTexts >= (COUNTOF(m_sText) - 1) )
				return false;

			TCHAR* ppArgs[7];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[2], &(ppArgs[3]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[3], &(ppArgs[4]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[4], &(ppArgs[5]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[5], &(ppArgs[6]))) return false;

			int x = expr.GetVal(ppArgs[0]);
			int y = expr.GetVal(ppArgs[1]);
			int w = expr.GetVal(ppArgs[2]);
			int h = expr.GetVal(ppArgs[3]);
			int bck = expr.GetVal(ppArgs[4]);
			int options = expr.GetVal(ppArgs[5]);
			int unk1 = expr.GetVal(ppArgs[6]);

			m_sControls[m_iControls].Format( "HTMLGump %d %d %d %d %d %d %d", x, y, w, h, bck, options, unk1 );
			m_iControls++;
			return true;
		}
		case GUMPCTL_DTEXTENTRY:
		{
			if ( m_iControls >= (COUNTOF(m_sControls) - 1) )
				return false;
			if ( m_iTexts >= (COUNTOF(m_sText) - 1) )
				return false;

			GET_RELATIVE( x, m_iOriginX );
			GET_RELATIVE( y, m_iOriginY );
			GET_ABSOLUTE( w );
			GET_ABSOLUTE( h );
			GET_ABSOLUTE( hue );
			GET_ABSOLUTE( id );
			SKIP_ALL( pszArgs )

			size_t iText = GumpAddText( *pszArgs ? pszArgs : "" );
			m_sControls[m_iControls].Format( "textentry %d %d %d %d %d %d %" FMTSIZE_T, x, y, w, h, hue, id, iText );
			m_iControls++;
			return true;
		}
		case GUMPCTL_DTEXTENTRYLIMITED:
		{
			if ( m_iControls >= (COUNTOF(m_sControls) - 1) )
				return false;
			if ( m_iTexts >= (COUNTOF(m_sText) - 1) )
				return false;

			GET_RELATIVE( x, m_iOriginX );
			GET_RELATIVE( y, m_iOriginY );
			GET_ABSOLUTE( w );
			GET_ABSOLUTE( h );
			GET_ABSOLUTE( hue );
			GET_ABSOLUTE( id );
			GET_ABSOLUTE( charLimit );
			SKIP_ALL( pszArgs )

			size_t iText = GumpAddText( *pszArgs ? pszArgs : "" );
			m_sControls[m_iControls].Format( "textentrylimited %d %d %d %d %d %d %" FMTSIZE_T " %d", x, y, w, h, hue, id, iText, charLimit );
			m_iControls++;
			return true;
		}
		case GUMPCTL_CHECKBOX:
		{
			if ( m_iControls >= (COUNTOF(m_sControls) - 1) )
				return false;

			TCHAR* ppArgs[6];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[2], &(ppArgs[3]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[3], &(ppArgs[4]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[4], &(ppArgs[5]))) return false;

			int x = expr.GetVal(ppArgs[0]);
			int y = expr.GetVal(ppArgs[1]);
			int checkGump = expr.GetVal(ppArgs[2]);
			int uncheckGump = expr.GetVal(ppArgs[3]);
			int unk1 = expr.GetVal(ppArgs[4]);
			int id = expr.GetVal(ppArgs[5]);

			m_sControls[m_iControls].Format("CheckBox %d %d %d %d %d %d", x, y, checkGump, uncheckGump, unk1, id);
			m_iControls++;
			return true;
		}
		case GUMPCTL_RADIO:
		{
			if ( m_iControls >= (COUNTOF(m_sControls) - 1) )
				return false;

			GET_RELATIVE( x, m_iOriginX );
			GET_RELATIVE( y, m_iOriginY );
			GET_ABSOLUTE( down );
			GET_ABSOLUTE( up );
			GET_ABSOLUTE( state );
			GET_ABSOLUTE( id );

			m_sControls[m_iControls].Format( "radio %d %d %d %d %d %d", x, y, down, up, state, id );

			m_iControls++;
			return true;
		}
		case GUMPCTL_CHECKERTRANS:
		{
			if ( m_iControls >= (COUNTOF(m_sControls) - 1) )
				return false;

			GET_RELATIVE( x, m_iOriginX );
			GET_RELATIVE( y, m_iOriginY );
			GET_ABSOLUTE( width );
			GET_ABSOLUTE( height );

			m_sControls[m_iControls].Format( "checkertrans %d %d %d %d", x, y, width, height );
			m_iControls++;
			return true;
		}
		case GUMPCTL_DORIGIN:
		{
			// GET_RELATIVE( x, m_iOriginX );
			// GET_RELATIVE( y, m_iOriginY );
			// m_iOriginX	= x;
			// m_iOriginY	= y;
			
			SKIP_ALL( pszArgs );
			if ( *pszArgs == '-' && (IsSpace( pszArgs[1] ) || !pszArgs[1]) )		pszArgs++;
			else  if ( *pszArgs == '*' )	m_iOriginX	+= Exp_GetSingle( ++pszArgs );
			else							m_iOriginX	 = Exp_GetSingle( pszArgs );

			SKIP_ALL( pszArgs );
			if ( *pszArgs == '-' && (IsSpace( pszArgs[1] ) || !pszArgs[1]) )		pszArgs++;
			else  if ( *pszArgs == '*' )	m_iOriginY	+= Exp_GetSingle( ++pszArgs );
			else							m_iOriginY	= Exp_GetSingle( pszArgs );
			
			return true;
		}
		case GUMPCTL_NODISPOSE:
			m_bNoDispose = true;
			break;
		case GUMPCTL_SETLOCATION:
		{
			TCHAR* ppArgs[2];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;

			m_x = expr.GetVal(ppArgs[0]);
			m_y = expr.GetVal(ppArgs[1]);

			return true;
		}
		case GUMPCTL_SETTEXT:
		{
			TCHAR* ppArgs[2];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;

			int textIndex = expr.GetVal(ppArgs[0]);
			if (textIndex > COUNTOF(m_sText))
				return false;
			m_sText[textIndex] = Str_TrimDoublequotes(ppArgs[1]);
			return true;
		}
		case GUMPCTL_TEXTENTRYA:
		{
			TCHAR* ppArgs[7];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[2], &(ppArgs[3]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[3], &(ppArgs[4]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[4], &(ppArgs[5]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[5], &(ppArgs[6]))) return false;

			int x = expr.GetVal(ppArgs[0]);
			int y = expr.GetVal(ppArgs[1]);
			int width = expr.GetVal(ppArgs[2]);
			int maxLength = expr.GetVal(ppArgs[3]);
			int unk1 = expr.GetVal(ppArgs[4]);
			int id = expr.GetVal(ppArgs[5]);
			int textIndex = expr.GetVal(ppArgs[6]);

			m_sControls[m_iControls].Format("TextEntry %d %d %d %d %d %d %d", x, y, width, maxLength, unk1, id, textIndex);
			m_iControls++;

			return 1;
		}
		case GUMPCTL_CROPPEDTEXT:
		case GUMPCTL_TEXTENTRY:
		case GUMPCTL_TEXTENTRYLIMITED:
			break;
		case GUMPCTL_TEXT:
		{
			TCHAR* ppArgs[4];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[2], &(ppArgs[3]))) return false;

			int x = expr.GetVal(ppArgs[0]);
			int y = expr.GetVal(ppArgs[1]);
			int unk1 = expr.GetVal(ppArgs[2]);
			int index = expr.GetVal(ppArgs[3]);

			m_sControls[m_iControls].Format("Text %d %d %d %d", x, y, unk1, index);
			m_iControls++;

			return true;
		}
		case GUMPCTL_TEXTA:
		{
			TCHAR* ppArgs[4];
			ppArgs[0] = const_cast<TCHAR*>(pszArgs);
			if (!Str_ParseExpressionArgument(ppArgs[0], &(ppArgs[1]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[1], &(ppArgs[2]))) return false;
			if (!Str_ParseExpressionArgument(ppArgs[2], &(ppArgs[3]))) return false;

			int x = expr.GetVal(ppArgs[0]);
			int y = expr.GetVal(ppArgs[1]);
			int unk1 = expr.GetVal(ppArgs[2]);

			int index = GumpAddText(Str_TrimDoublequotes(ppArgs[3]));
			
			m_sControls[m_iControls].Format("Text %d %d %d %d", x, y, unk1, index);
			m_iControls++;

			return true;
		}

		case GUMPCTL_XMFHTMLGUMP:		// 7 = x,y,sx,sy, cliloc(1003000) hasBack canScroll
		case GUMPCTL_XMFHTMLGUMPCOLOR: // 7 + color. 
		{
			GET_RELATIVE( x, m_iOriginX );
			GET_RELATIVE( y, m_iOriginY );
			GET_ABSOLUTE( sX );
			GET_ABSOLUTE( sY );
			GET_ABSOLUTE( cliloc );
			GET_ABSOLUTE( hasBack );
			GET_ABSOLUTE( canScroll );
			//SKIP_ALL( pszArgs )

			if ( index == GUMPCTL_XMFHTMLGUMP ) // xmfhtmlgump doesn't use color
				m_sControls[m_iControls].Format( "xmfhtmlgump %d %d %d %d %d %d %d" , x, y, sX, sY, cliloc, hasBack, canScroll );
			else
				m_sControls[m_iControls].Format( "xmfhtmlgumpcolor %d %d %d %d %d %d %d%s%s", x, y, sX, sY, cliloc, hasBack, canScroll, *pszArgs ? " " : "", *pszArgs ? pszArgs : "" );

			m_iControls++;
			return true;
		}
		case GUMPCTL_XMFHTMLTOK: // 9 = x y width height has_background has_scrollbar color cliloc_id @args
		{
			GET_RELATIVE(x, m_iOriginX);
			GET_RELATIVE(y, m_iOriginY);
			GET_ABSOLUTE(sX);
			GET_ABSOLUTE(sY);
			GET_ABSOLUTE(hasBack);
			GET_ABSOLUTE(canScroll);
			GET_ABSOLUTE(color);
			GET_ABSOLUTE(cliloc);
			SKIP_ALL(pszArgs);

			m_sControls[m_iControls].Format("xmfhtmltok %d %d %d %d %d %d %d %d %s", x, y, sX, sY, hasBack, canScroll, color, cliloc, *pszArgs ? pszArgs : "");

			m_iControls++;
			return true;
		}
		default:
			break;
	}

	if ( m_iControls >= (COUNTOF(m_sControls) - 1) )
		return false;

	m_sControls[m_iControls].Format("%s %s", pszKey, pszArgs);
	m_iControls++;
	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_SCRIPTSRC;
	EXC_DEBUG_END;
	return false;
}


CDialogDef::CDialogDef( RESOURCE_ID rid ) :
	CResourceLink( rid )
{
	m_iControls = 0;
	m_iTexts = 0;
	m_pObj = NULL;
	m_x = 0;
	m_y = 0;
	m_iOriginX = 0;
	m_iOriginY = 0;
	m_iPage = 0;
	m_bNoDispose = false;
}

bool CDialogDef::r_WriteVal(LPCTSTR pszKey, CGString& sVal, CTextConsole* pSrc, CScriptTriggerArgs* pArgs)
{
	return r_WriteVal(pszKey, sVal, pSrc, pArgs, false);
}

bool CDialogDef::r_WriteVal(LPCTSTR pszKey, CGString& sVal, CTextConsole* pSrc, CScriptTriggerArgs* pArgs, CDialogDefSetup *pDlgSetup)
{
	ADDTOCALLSTACK("CDialogDef::r_WriteVal");
	if ( !m_pObj )
		return false;
	
	if (pDlgSetup)
	{
		if (m_tagHolder.r_WriteVal(pszKey, sVal, pSrc, pArgs, pDlgSetup))
			return true;
		if (pDlgSetup->r_CallRaw(pszKey, pSrc, pArgs, &sVal))
			return true;
	}
	else
	{
		if (r_CallRaw(pszKey, pSrc, pArgs, &sVal))
			return true;
	}

	if (pDlgSetup)
	{
		if (pDlgSetup->r_WriteValBase(pszKey, sVal, pSrc, pArgs))
			return true;
	}

	if (m_pObj->r_WriteVal(pszKey, sVal, pSrc, pArgs))
		return true;

	if (!pDlgSetup)
	{
		if (m_tagHolder.r_WriteVal(pszKey, sVal, pSrc, pArgs, this))
			return true;
	}

	return false;
}

bool CDialogDef::r_LoadVal( CScript & s, CScriptTriggerArgs* pArgs, CTextConsole* pSrc)
{
	ADDTOCALLSTACK("CDialogDef::r_LoadVal");
	if ( !m_pObj )
		return false;
	return m_pObj->r_LoadVal( s, pArgs, pSrc);
}

bool CDialogDef::GumpSetup( int iPage, CClient * pClient, CObjBase * pObjSrc, LPCTSTR Arguments )
{
	ADDTOCALLSTACK("CDialogDef::GumpSetup");
	CResourceLock	s;

	m_iControls		= 0;
	m_iTexts		= 0;
	m_pObj			= pObjSrc;
	m_iOriginX		= 0;
	m_iOriginY		= 0;
	m_iPage			= static_cast<WORD>(iPage);
	m_bNoDispose	= false;

	CScriptTriggerArgs	Args(iPage, 0, pObjSrc);
	//DEBUG_ERR(("Args.m_s1_raw %s  Args.m_s1 %s  Arguments 0x%x\n",Args.m_s1_raw, Args.m_s1, Arguments));
	Args.m_s1_raw = Args.m_s1 = Arguments;

	// read text first
	if ( g_Cfg.ResourceLock( s, RESOURCE_ID( RES_DIALOG, GetResourceID().GetResIndex(), RES_DIALOG_TEXT ) ) )
	{
		while ( s.ReadKey())
		{
			if ( m_iTexts >= (COUNTOF(m_sText) - 1) )
				break;
			m_pObj->ParseText( s.GetKeyBuffer(), pClient->GetChar() );
			m_sText[m_iTexts] = s.GetKey();
			m_iTexts++;
		}
	}
	else
	{
		// no gump text?
	}

	// read the main dialog
	if ( !ResourceLock( s ) )
		return false;
	if ( !s.ReadKey() )		// read the size.
		return( false );

	// starting x,y location.
	INT64 iSizes[2];
	TCHAR * pszBuf = s.GetKeyBuffer();
	Args.m_pO1 = this;

	TCHAR* ppArgs[2];
	bool hasPosition = false;
	if (Str_ParseCmds(pszBuf, ppArgs, COUNTOF(iSizes)) == 2)
	{
		if (IsSimpleNumberString(ppArgs[0]) && IsSimpleNumberString(ppArgs[1]))
		{
			m_x = Exp_GetLLVal(ppArgs[0]);
			m_y = Exp_GetLLVal(ppArgs[1]);
			hasPosition = true;
		}
	}

	if (!hasPosition)
	{
		if (!ResourceLock(s))
			return false;
	}

	m_iTexts = 101;
	CDialogDefSetup dlgSetup;
	dlgSetup.m_pDef = this;
	Args.m_pO1 = &dlgSetup;
	if ( OnTriggerRunVal( s, TRIGRUN_SECTION_TRUE, pClient->GetChar(), &Args ) == TRIGRET_RET_TRUE )
		return false;
	return true;
}


bool CClient::Dialog_Setup( CLIMODE_TYPE mode, RESOURCE_ID_BASE rid, int iPage, CObjBase * pObj, LPCTSTR Arguments )
{
	ADDTOCALLSTACK("CClient::Dialog_Setup");
	if ( !pObj )
		return false;

	CResourceDef *pRes = g_Cfg.ResourceGetDef(rid);
	CDialogDef *pDlg = dynamic_cast<CDialogDef *>(pRes);
	if ( !pDlg )
	{
		DEBUG_ERR(("Invalid RES_DIALOG\n"));
		return false;
	}

	if ( !pDlg->GumpSetup(iPage, this, pObj, Arguments) )
		return false;

	// Now pack it up to send,
	// m_tmGumpDialog.m_ResourceID = rid;
	DWORD context = static_cast<DWORD>(rid);
	if ( m_NetState->isClientKR() )
	{
		// KR enhanced clients can use only client internal dialogs, so check if the dialog can be translated into KR equivalent DialogID.
		// SA+ enhanced clients doesn't need this translation anymore because OSI had added back support for server dynamic dialogs again.
		context = g_Cfg.GetKRDialog(context);
		if ( context == 0 )
			g_Log.Event(LOGL_WARN, "A Kingdom Reborn equivalent of dialog '%s' has not been defined\n", pDlg->GetResourceName());
	}

	addGumpDialog(mode, pDlg->m_sControls, pDlg->m_iControls, pDlg->m_sText, pDlg->m_iTexts, pDlg->m_x, pDlg->m_y, pObj, context);
	return true;
}


void CClient::addGumpInpVal(bool fCancel, INPVAL_TYPE type, DWORD dwMaxLength, LPCTSTR pszText1, LPCTSTR pszText2, CObjBase *pObj)
{
	ADDTOCALLSTACK("CClient::addGumpInpVal");
	// CLIMODE_INPVAL
	// Should result in PacketGumpValueInputResponse::onReceive
	// just input an objects attribute.

	// ARGS:
	// 	m_Targ_UID = pObj->GetUID();
	//  m_Targ_Text = verb

	if ( !pObj )
		return;

	new PacketGumpValueInput(this, fCancel, type, dwMaxLength, pszText1, pszText2, pObj);

	//m_Targ_Text = verb
	m_Targ_UID = pObj->GetUID();
	SetTargMode(CLIMODE_INPVAL);
}


void CClient::addGumpDialog(CLIMODE_TYPE mode, const CGString *psControls, size_t iControls, const CGString *psText, size_t iTexts, DWORD x, DWORD y, CObjBase *pObj, DWORD rid)
{
	ADDTOCALLSTACK("CClient::addGumpDialog");
	// Open generic dialog
	// Should return a PacketGumpDialogRet::onReceive
	// NOTE: These packets can get rather LARGE.
	// x,y = where on the screen ?
	if ( !pObj )
		pObj = m_pChar;

	DWORD context = mode;
	if ( (mode == CLIMODE_DIALOG) && (rid != 0) )
		context = (rid & 0xFFFFFF);

	PacketGumpDialog *cmd = new PacketGumpDialog(x, y, pObj, context);
	cmd->writeControls(this, psControls, iControls, psText, iTexts);
	cmd->push(this);
	
	if ( m_pChar )
		++m_mapOpenedGumps[context];
}

bool CClient::addGumpDialogProps(CObjBase *pObj)
{
	ADDTOCALLSTACK("CClient::addGumpDialogProps");
	// Open object properties dialog
	if ( !pObj )
		return false;

	RESOURCE_ID rid = g_Cfg.ResourceGetIDType(RES_DIALOG, pObj->IsItem() ? "d_ITEMPROP1" : "d_CHARPROP1");
	if ( !rid.IsValidUID() )
		return false;

	m_Prop_UID = m_Targ_UID = pObj->GetUID();
	Dialog_Setup(CLIMODE_DIALOG, rid, 0, pObj);
	return true;
}

TRIGRET_TYPE CClient::Dialog_OnButton(RESOURCE_ID_BASE rid, DWORD dwButtonID, CObjBase *pObj, CDialogResponseArgs *pArgs)
{
	ADDTOCALLSTACK("CClient::Dialog_OnButton");
	// Dialog button was pressed
	if ( !pObj )
		return TRIGRET_ENDIF;

	CResourceLock s;
	if ( !g_Cfg.ResourceLock(s, RESOURCE_ID(RES_DIALOG, rid.GetResIndex(), RES_DIALOG_BUTTON)) )
		return TRIGRET_ENDIF;

	while ( s.ReadKeyParse() )
	{
		if ( !s.IsKeyHead("ON", 2) )
			continue;

		TCHAR* pKey = s.GetArgStr();
		if (stricmp(pKey, "@anybutton"))
		{
			INT64 iCmd = Exp_GetLLSingle(pKey);
			if ( dwButtonID != static_cast<DWORD>(iCmd) )
				continue;
		}

		pArgs->m_iN1 = dwButtonID;
		return pObj->OnTriggerRunVal(s, TRIGRUN_SECTION_TRUE, m_pChar, pArgs);
	}
	return TRIGRET_ENDIF;
}

bool CClient::Dialog_Close(CObjBase *pObj, DWORD rid, DWORD dwButtonID)
{
	ADDTOCALLSTACK("CClient::Dialog_Close");
	// Dialog was closed

	DWORD context = (rid & 0xFFFFFF);
	new PacketGumpChange(this, context, dwButtonID);

	if ( m_NetState->isClientVersion(MINCLIVER_CLOSEDIALOG) )
	{
		CChar *pSrc = dynamic_cast<CChar *>(pObj);
		if ( pSrc )
		{
			OpenedGumpsMap_t::iterator itGumpFound = m_mapOpenedGumps.find(context);
			if ( (itGumpFound != m_mapOpenedGumps.end()) && ((*itGumpFound).second > 0) )
			{
				PacketGumpDialogRet packet;
				packet.writeByte(PACKET_GumpDialogRet);
				packet.writeInt16(27);
				packet.writeInt32(pObj->GetUID());
				packet.writeInt32(context);
				packet.writeInt32(dwButtonID);
				packet.writeInt32(0);
				packet.writeInt32(0);
				packet.writeInt32(0);

				packet.seek(1);
				packet.onReceive(m_NetState);
			}
		}
	}
	return true;
}




TRIGRET_TYPE CClient::Menu_OnSelect( RESOURCE_ID_BASE rid, int iSelect, CObjBase * pObj ) // Menus for general purpose
{
	ADDTOCALLSTACK("CClient::Menu_OnSelect");
	// A select was made. so run the script.
	// iSelect = 0 = cancel.

	CResourceLock s;
	if ( ! g_Cfg.ResourceLock( s, rid ))
	{
		// Can't find the resource ?
		return( TRIGRET_ENDIF );
	}

	if ( pObj == NULL )
		pObj = m_pChar;

	// execute the menu script.
	if ( iSelect == 0 )	// Cancel button
	{

		while ( s.ReadKeyParse() )
		{
			if ( !s.IsKey( "ON" ) || ( *s.GetArgStr() != '@' ) )
				continue;

			if ( strcmpi( s.GetArgStr(), "@cancel" ) )
				continue;

			return pObj->OnTriggerRunVal( s, TRIGRUN_SECTION_TRUE, m_pChar, NULL );
		}
	}
	else
	{
		int i=0;	// 1 based selection.
		while ( s.ReadKeyParse())
		{
			if ( !s.IsKey( "ON" ) || ( *s.GetArgStr() == '@' ) )
				continue;

			i++;
			if ( i < iSelect )
				continue;
			if ( i > iSelect )
				break;

			return pObj->OnTriggerRunVal( s, TRIGRUN_SECTION_TRUE, m_pChar, NULL );
		}
	}

	// No selection ?
	return( TRIGRET_ENDIF );
}

bool CMenuItem::ParseLine( TCHAR * pszArgs, CScriptObj * pObjBase, CTextConsole * pSrc )
{
	ADDTOCALLSTACK("CMenuItem::ParseLine");

	if ( *pszArgs == '@' )
	{
		// This allows for triggers in menus
		return false;
	}

	TCHAR * pszArgStart = pszArgs;
	while ( _ISCSYM( *pszArgs ))
		pszArgs++;

	if ( *pszArgs )
	{
		*pszArgs = '\0';
		pszArgs++;
		GETNONWHITESPACE( pszArgs );
	}

	// The item id (if we want to have an item type menu) or 0
	if ( strcmp( pszArgStart, "0" ) != 0 )
	{
		CItemBase * pItemBase = CItemBase::FindItemBase(static_cast<ITEMID_TYPE>(g_Cfg.ResourceGetIndexType( RES_ITEMDEF, pszArgStart )));
		if ( pItemBase != NULL )
		{
			m_id = static_cast<WORD>(pItemBase->GetDispID());
			pObjBase = pItemBase;
		}
		else
		{
			DEBUG_ERR(( "Bad MENU item id '%s'\n", pszArgStart ));
			return( false );	// skip this.
		}
	}
	else
	{
		m_id = 0;
	}

	if ( pObjBase != NULL )
	{
		pObjBase->ParseText( pszArgs, pSrc );
	}
	else
	{
		g_Serv.ParseText( pszArgs, pSrc );
	}

	// Parsing @color
	if ( *pszArgs == '@' )
	{
		pszArgs++;
		HUE_TYPE wHue = static_cast<HUE_TYPE>(Exp_GetVal( pszArgs ));
		if ( wHue != 0 )
			wHue = (wHue == 1? 0x7FF: wHue-1);

		m_color = wHue;
		SKIP_ARGSEP( pszArgs );
	}
	else
		m_color = 0;

	m_sText = pszArgs;

	if ( m_sText.IsEmpty())
	{
		if ( pObjBase )	// use the objects name by default.
		{
			m_sText = pObjBase->GetName();	
			if ( ! m_sText.IsEmpty())
				return( true );
		}
		DEBUG_ERR(( "Bad MENU item text '%s'\n", pszArgStart ));
	}

	return( !m_sText.IsEmpty() );
}

void CClient::Menu_Setup( RESOURCE_ID_BASE rid, CObjBase * pObj )
{
	ADDTOCALLSTACK("CClient::Menu_Setup");
	// Menus for general purpose
	// Expect PacketMenuChoice::onReceive() back.

	CResourceLock s;
	if ( ! g_Cfg.ResourceLock( s, rid ))
	{
		return;
	}

	if ( pObj == NULL )
	{
		pObj = m_pChar;
	}

	static_cast<void>(s.ReadKey());		// get menu title
	pObj->ParseText( s.GetKeyBuffer(), m_pChar );

	CMenuItem item[MAX_MENU_ITEMS];
	item[0].m_sText = s.GetKey();
	// item[0].m_id = rid.m_internalrid;	// general context id

	size_t i = 0;
	while (s.ReadKeyParse())
	{
		if ( ! s.IsKey( "ON" ))
			continue;

		i++;
		if ( ! item[i].ParseLine( s.GetArgRaw(), pObj, m_pChar ))
			i--;

		if ( i >= (COUNTOF( item ) - 1))
			break;
	}

	m_tmMenu.m_ResourceID = rid;

	ASSERT(i < COUNTOF(item));
	addItemMenu( CLIMODE_MENU, item, i, pObj );
}

