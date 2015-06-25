#include "stdafx.h"

static HANDLE hMirOTRMenuObject;
static HGENMENU hStatusInfoItem, hHTMLConvMenuItem;
HWND hDummyPaintWin;

//contactmenu exec param(ownerdata)
//also used in checkservice
typedef struct
{
	char *szServiceName;
}
MirOTRMenuExecParam,*lpMirOTRMenuExecParam;

////////////////////////////////////////////
// MirOTR MENU
///////////////////////////////////////////

static HGENMENU AddMirOTRMenuItem(TMO_MenuItem *pmi, const char *pszService)
{
	// add owner data
	lpMirOTRMenuExecParam cmep = ( lpMirOTRMenuExecParam )mir_calloc(sizeof(MirOTRMenuExecParam));
	cmep->szServiceName = mir_strdup(pszService);
	pmi->ownerdata = cmep;
	return Menu_AddItem(hMirOTRMenuObject, pmi);
}

//called with:
//wparam - ownerdata
//lparam - lparam from winproc
INT_PTR MirOTRMenuExecService(WPARAM wParam,LPARAM lParam)
{
	if (wParam!=0) {
		lpMirOTRMenuExecParam cmep=(lpMirOTRMenuExecParam)wParam;
		//call with wParam=(MCONTACT)hContact
		CallService(cmep->szServiceName,lParam,0);
	}
	return 0;
}

// true - ok,false ignore
INT_PTR MirOTRMenuCheckService(WPARAM wParam, LPARAM)
{
	TCheckProcParam *pcpp = (TCheckProcParam*)wParam;
	if (pcpp == NULL)
		return FALSE;

	lpMirOTRMenuExecParam cmep = (lpMirOTRMenuExecParam)pcpp->MenuItemOwnerData;
	if (cmep == NULL) //this is rootsection...build it
		return TRUE;

	MCONTACT hContact = (MCONTACT)pcpp->wParam, hSub;
	if ((hSub = db_mc_getMostOnline(hContact)) != 0)
		hContact = hSub;

	ConnContext *context = otrl_context_find_miranda(otr_user_state, hContact);
	TrustLevel level = (TrustLevel)otr_context_get_trust(context);

	TMO_MenuItem mi;
	if (CallService(MO_GETMENUITEM, (WPARAM)pcpp->MenuItemHandle, (LPARAM)&mi) == 0) {
		if (mi.flags & CMIF_HIDDEN) return FALSE;
		if (mi.flags & CMIF_NOTPRIVATE  && level == TRUST_PRIVATE) return FALSE;
		if (mi.flags & CMIF_NOTFINISHED && level == TRUST_FINISHED) return FALSE;
		if (mi.flags & CMIF_NOTUNVERIFIED  && level == TRUST_UNVERIFIED) return FALSE;
		if (mi.flags & CMIF_NOTNOTPRIVATE && level == TRUST_NOT_PRIVATE) return FALSE;

		if (pcpp->MenuItemHandle == hStatusInfoItem) {
			TCHAR text[128];

			switch (level) {
			case TRUST_PRIVATE:
				mir_sntprintf(text, _T("%s [v%i]"), TranslateT(LANG_STATUS_PRIVATE), context->protocol_version);
				Menu_ModifyItem(hStatusInfoItem, text, IcoLib_GetIconHandle(ICON_PRIVATE));
				break;

			case TRUST_UNVERIFIED:
				mir_sntprintf(text, _T("%s [v%i]"), TranslateT(LANG_STATUS_UNVERIFIED), context->protocol_version);
				Menu_ModifyItem(hStatusInfoItem, text, IcoLib_GetIconHandle(ICON_UNVERIFIED));
				break;

			case TRUST_FINISHED:
				Menu_ModifyItem(hStatusInfoItem, TranslateT(LANG_STATUS_FINISHED), IcoLib_GetIconHandle(ICON_UNVERIFIED));
				break;

			default:
				Menu_ModifyItem(hStatusInfoItem, TranslateT(LANG_STATUS_DISABLED), IcoLib_GetIconHandle(ICON_NOT_PRIVATE));
			}
		}
		else if (pcpp->MenuItemHandle == hHTMLConvMenuItem) {
			int flags = db_get_b(hContact, MODULENAME, "HTMLConv", 0) ? CMIF_CHECKED : 0;
			Menu_ModifyItem(hHTMLConvMenuItem, NULL, INVALID_HANDLE_VALUE, flags);
		}
	}
	return TRUE;
}

INT_PTR FreeOwnerDataMirOTRMenu(WPARAM, LPARAM lParam)
{
	lpMirOTRMenuExecParam cmep = (lpMirOTRMenuExecParam)lParam;
	if (cmep != NULL) {
		if (cmep->szServiceName) mir_free(cmep->szServiceName);
		mir_free(cmep);
	}
	return 0;
}

INT_PTR OnAddMenuItemMirOTRMenu(WPARAM wParam, LPARAM lParam)
{
	MENUITEMINFO *mii = (MENUITEMINFO*)wParam;
	if (!mii || mii->cbSize != sizeof(MENUITEMINFO))
		return 0;

	TMO_MenuItem mi;
	if (CallService(MO_GETMENUITEM, (WPARAM)lParam, (LPARAM)&mi) == 0) {
		if (mi.flags & CMIF_DISABLED) {
			mii->fMask |= MIIM_STATE;
			mii->fState |= MF_DISABLED;
		}
	}
	return 1;
}

LRESULT CALLBACK PopupMenuWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_MEASUREITEM:
		if (CallService(MS_CLIST_MENUMEASUREITEM, wParam, lParam)) return TRUE;
		break;
	case WM_DRAWITEM:
		if (CallService(MS_CLIST_MENUDRAWITEM, wParam, lParam)) return TRUE;
		break;
	case WM_COMMAND:
		if (Menu_ProcessCommandById(wParam, GetWindowLongPtr(hwnd, GWLP_USERDATA)))
			return TRUE;
		break;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

void ShowOTRMenu(MCONTACT hContact, POINT pt)
{
	HMENU hMenu = CreatePopupMenu();
	Menu_Build(hMenu, hMirOTRMenuObject, hContact);

	SetWindowLongPtr(hDummyPaintWin, GWLP_USERDATA, (LONG_PTR)hContact);

	TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, hDummyPaintWin, 0);
	DestroyMenu(hMenu);
}

void InitMirOTRMenu(void)
{
	WNDCLASS wc = { 0 };
	wc.hInstance = hInst;
	wc.lpfnWndProc = PopupMenuWndProc;
	wc.lpszClassName = _T("MirOTRPopupMenuProcessor");
	RegisterClass(&wc);
	hDummyPaintWin = CreateWindowEx(0, _T("MirOTRPopupMenuProcessor"), NULL, 0, 0, 0, 1, 1, 0, 0, hInst, 0);

	CreateServiceFunction("MirOTRMenuExecService", MirOTRMenuExecService);
	CreateServiceFunction("MirOTRMenuCheckService", MirOTRMenuCheckService);

	// menu object
	CreateServiceFunction("MIROTRMENUS/FreeOwnerDataMirOTRMenu", FreeOwnerDataMirOTRMenu);
	CreateServiceFunction("MIROTRMENUS/OnAddMenuItemMirOTRMenu", OnAddMenuItemMirOTRMenu);

	hMirOTRMenuObject = Menu_AddObject("MirOTRMenu", LPGEN("MirOTR menu"), "MirOTRMenuCheckService", "MirOTRMenuExecService");
	Menu_ConfigureObject(hMirOTRMenuObject, MCO_OPT_FREE_SERVICE, "MIROTRMENUS/FreeOwnerDataMirOTRMenu");
	Menu_ConfigureObject(hMirOTRMenuObject, MCO_OPT_ONADD_SERVICE, "MIROTRMENUS/OnAddMenuItemMirOTRMenu");

	// menu items
	TMO_MenuItem tmi = { 0 };
	tmi.flags = CMIF_DISABLED | CMIF_TCHAR;
	tmi.name.t = LPGENT("OTR Status");
	tmi.position = 0;
	hStatusInfoItem = AddMirOTRMenuItem(&tmi, NULL);

	tmi.flags = CMIF_TCHAR | CMIF_NOTPRIVATE | CMIF_NOTUNVERIFIED;
	tmi.name.t = LANG_MENU_START;
	tmi.position = 100001;
	tmi.hIcolibItem = IcoLib_GetIconHandle(ICON_UNVERIFIED);
	AddMirOTRMenuItem(&tmi, MS_OTR_MENUSTART);

	tmi.flags = CMIF_TCHAR | CMIF_NOTNOTPRIVATE | CMIF_NOTFINISHED;
	tmi.name.t = LANG_MENU_REFRESH;
	tmi.position = 100002;
	tmi.hIcolibItem = IcoLib_GetIconHandle(ICON_FINISHED);
	AddMirOTRMenuItem(&tmi, MS_OTR_MENUREFRESH);

	tmi.flags = CMIF_TCHAR | CMIF_NOTNOTPRIVATE;
	tmi.name.t = LANG_MENU_STOP;
	tmi.position = 100003;
	tmi.hIcolibItem = IcoLib_GetIconHandle(ICON_NOT_PRIVATE);
	AddMirOTRMenuItem(&tmi, MS_OTR_MENUSTOP);

	tmi.flags = CMIF_TCHAR | CMIF_NOTNOTPRIVATE | CMIF_NOTFINISHED;
	tmi.name.t = LANG_MENU_VERIFY;
	tmi.position = 200001;
	tmi.hIcolibItem = IcoLib_GetIconHandle(ICON_PRIVATE);
	AddMirOTRMenuItem(&tmi, MS_OTR_MENUVERIFY);

	tmi.flags = CMIF_TCHAR | CMIF_CHECKED;
	tmi.name.t = LANG_MENU_TOGGLEHTML;
	tmi.position = 300001;
	hHTMLConvMenuItem = AddMirOTRMenuItem(&tmi, MS_OTR_MENUTOGGLEHTML);
}

void UninitMirOTRMenu(void)
{
	DestroyWindow(hDummyPaintWin);
	hDummyPaintWin = 0;

	UnregisterClass(_T("MirOTRPopupMenuProcessor"), hInst);
	
	Menu_RemoveObject(hMirOTRMenuObject);
	hMirOTRMenuObject = 0;
}
