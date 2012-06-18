#include "StdAfx.h"
#include "QuoteInfoDlg.h"
#include "EconomicRateInfo.h"
#include "resource.h"
#include "ModuleInfo.h"
#include "QuotesProviders.h"
#include "IconLib.h"
#include "DBUtils.h"
#include "IQuotesProvider.h"
#include "Locale.h"
#include "SettingsDlg.h"

// extern HANDLE g_hWindowListEditSettings;
extern HGENMENU g_hMenuEditSettings;
extern HGENMENU g_hMenuOpenLogFile;
#ifdef CHART_IMPLEMENT
extern HGENMENU g_hMenuChart;
#endif
extern HGENMENU g_hMenuRefresh;


#define WINDOW_PREFIX_INFO "Quote Info"


namespace 
{
	HANDLE g_hContact;

	inline bool IsMyContact(HANDLE hContact)
	{
		CQuotesProviders::TQuotesProviderPtr pProvider = CModuleInfo::GetQuoteProvidersPtr()->GetContactProviderPtr(hContact);
		return (NULL != pProvider);
	}

	inline HANDLE get_contact(HWND hWnd)
	{
		return reinterpret_cast<HANDLE>(GetWindowLongPtr(hWnd,GWLP_USERDATA));
	}


	bool get_fetch_time(time_t& rTime,HANDLE hContact)
	{
		DBVARIANT dbv;
		DBCONTACTGETSETTING cgs;

		cgs.szModule=QUOTES_PROTOCOL_NAME;
		cgs.szSetting=DB_STR_QUOTE_FETCH_TIME;
		cgs.pValue=&dbv;
		if(CallService(MS_DB_CONTACT_GETSETTING,reinterpret_cast<WPARAM>(hContact),reinterpret_cast<LPARAM>(&cgs))
			|| (DBVT_DWORD != dbv.type))
		{
			return false;
		}

		rTime = dbv.dVal;
		return true;
	}

	INT_PTR CALLBACK QuoteInfoDlgProcImpl(HANDLE hContact,HWND hdlg,UINT msg,WPARAM wParam,LPARAM lParam)
	{
		switch(msg)
		{
		case WM_INITDIALOG:
			{
				assert(hContact);

				TranslateDialogDefault(hdlg);

				tstring sDescription = GetContactName(hContact);
				::SetDlgItemText(hdlg,IDC_STATIC_QUOTE_NAME,sDescription.c_str());

				double dRate = 0.0;
				if(true == Quotes_DBReadDouble(hContact,QUOTES_PROTOCOL_NAME,DB_STR_QUOTE_PREV_VALUE,dRate))
				{
					tostringstream o;
					o.imbue(GetSystemLocale());
					o << dRate;

					::SetDlgItemText(hdlg,IDC_EDIT_PREVIOUS_RATE,o.str().c_str());
				}

				dRate = 0.0;
				if(true == Quotes_DBReadDouble(hContact,QUOTES_PROTOCOL_NAME,DB_STR_QUOTE_CURR_VALUE,dRate))
				{
					tostringstream o;
					o.imbue(GetSystemLocale());
					o << dRate;

					::SetDlgItemText(hdlg,IDC_EDIT_RATE,o.str().c_str());
				}

				time_t nFetchTime;
				if(true == get_fetch_time(nFetchTime,hContact))
				{
					TCHAR szTime[50];
					if(0 == _tctime_s(szTime,50,&nFetchTime))
					{
						szTime[::_tcslen(szTime)-1] = _T('\0');
						::SetDlgItemText(hdlg,IDC_EDIT_RATE_FETCH_TIME,szTime);
					}
				}

				CQuotesProviders::TQuotesProviderPtr pProvider = CModuleInfo::GetQuoteProvidersPtr()->GetContactProviderPtr(hContact);

				const IQuotesProvider::CProviderInfo& pi = pProvider->GetInfo();
				tostringstream o;
				o << TranslateT("Info provided by") << _T(" <a href=\"") << pi.m_sURL << _T("\">") << pi.m_sName << _T("</a>");

				::SetDlgItemText(hdlg,IDC_SYSLINK_PROVIDER,o.str().c_str());
			}
			return TRUE;
		case WM_NOTIFY:
			{
				LPNMHDR pNMHDR = reinterpret_cast<LPNMHDR>(lParam);
				switch(pNMHDR->code)
				{
				case NM_CLICK:
					if(IDC_SYSLINK_PROVIDER == wParam)
					{
						PNMLINK pNMLink = reinterpret_cast<PNMLINK>(pNMHDR);
						::ShellExecute(hdlg,_T("open"),pNMLink->item.szUrl,NULL,NULL,SW_SHOWNORMAL);					
					}
					break;
				}
			}
			break;
		}
		return FALSE;
	}

	INT_PTR CALLBACK QuoteInfoDlgProc(HWND hdlg,UINT msg,WPARAM wParam,LPARAM lParam)
	{
		return QuoteInfoDlgProcImpl(g_hContact,hdlg,msg,wParam,lParam);
	}
}

int QuotesEventFunc_OnUserInfoInit(WPARAM wp,LPARAM lp)
{
	HANDLE hContact = reinterpret_cast<HANDLE>(lp);
	if(NULL == hContact)
	{
		return 0;
	}


	if(false == IsMyContact(hContact))
	{
		return 0;
	}

	g_hContact = hContact;

	OPTIONSDIALOGPAGE odp = {0};
	odp.cbSize = sizeof( odp );
	odp.hInstance = CModuleInfo::GetModuleHandle();
	//odp.dwInitParam = ( LPARAM )this;

	odp.hIcon = Quotes_LoadIconEx(ICON_STR_MAIN);
	odp.flags = ODPF_TCHAR;
	odp.pfnDlgProc = QuoteInfoDlgProc;
	odp.position = -2000000000;
	odp.pszTemplate = MAKEINTRESOURCEA(IDD_DIALOG_QUOTE_INFO);
	odp.ptszTitle = TranslateT("Quote");
	UserInfo_AddPage(wp, &odp);
	return 0;
}


INT_PTR QuotesMenu_EditSettings(WPARAM wp,LPARAM lp)
{
	HANDLE hContact = reinterpret_cast<HANDLE>(wp);
	if(NULL == hContact)
	{
		return 0;
	}

	ShowSettingsDlg(hContact);

	return 0;
}

namespace
{
	bool get_log_file(HANDLE hContact,tstring& rsLogfile)
	{
		rsLogfile = GetContactLogFileName(hContact);
		return ((rsLogfile.empty()) ? false : true);
	}
}

INT_PTR QuotesMenu_OpenLogFile(WPARAM wp,LPARAM lp)
{
	HANDLE hContact = reinterpret_cast<HANDLE>(wp);
	if(NULL == hContact)
	{
		return 0;
	}

	tstring sLogFileName;
	if ((true == get_log_file(hContact,sLogFileName)) && (false == sLogFileName.empty()))
	{
		::ShellExecute(NULL,_T("open"),sLogFileName.c_str(),NULL,NULL,SW_SHOWNORMAL);
	}

	return 0;
}

INT_PTR QuotesMenu_RefreshContact(WPARAM wp,LPARAM lp)
{
	HANDLE hContact = reinterpret_cast<HANDLE>(wp);
	if(NULL == hContact)
	{
		return 0;
	}

	CQuotesProviders::TQuotesProviderPtr pProvider = CModuleInfo::GetQuoteProvidersPtr()->GetContactProviderPtr(hContact);
	if (!pProvider)
	{
		return 0;
	}

	pProvider->RefreshContact(hContact);

	return 0;
}

namespace
{
	INT_PTR CALLBACK QuoteInfoDlgProc1(HWND hdlg,UINT msg,WPARAM wParam,LPARAM lParam)
	{
		HANDLE hContact = NULL;
		switch(msg)
		{
		case WM_INITDIALOG:
			{
				hContact = reinterpret_cast<HANDLE>(lParam);
				HANDLE hWL = CModuleInfo::GetInstance().GetWindowList(WINDOW_PREFIX_INFO,false);
				assert(hWL);
				WindowList_Add(hWL,hdlg,hContact);

				::SetWindowLongPtr(hdlg,GWLP_USERDATA,reinterpret_cast<LONG>(hContact));
				Utils_RestoreWindowPositionNoSize(hdlg,hContact,QUOTES_MODULE_NAME,WINDOW_PREFIX_INFO);
				::ShowWindow(hdlg,SW_SHOW);
			}
			break;
		case WM_CLOSE:
			DestroyWindow(hdlg);
			return FALSE;
		case WM_DESTROY:
			{
				HANDLE hContact = get_contact(hdlg);
				if(hContact)
				{
					SetWindowLongPtr(hdlg,GWLP_USERDATA,0);

					HANDLE hWL = CModuleInfo::GetInstance().GetWindowList(WINDOW_PREFIX_INFO,false);
					assert(hWL);
					WindowList_Remove(hWL,hdlg);
					Utils_SaveWindowPosition(hdlg,hContact,QUOTES_MODULE_NAME,WINDOW_PREFIX_INFO);
				}
			}
			return FALSE;
		case WM_COMMAND:
			if(LOWORD(wParam) == IDOK) 
			{
				::DestroyWindow(hdlg);
				return FALSE;
			}

		default:
			hContact = get_contact(hdlg);
			break;
		}

		return QuoteInfoDlgProcImpl(hContact,hdlg,msg,wParam,lParam);
	}
}

int Quotes_OnContactDoubleClick(WPARAM wp,LPARAM/* lp*/)
{
	HANDLE hContact = reinterpret_cast<HANDLE>(wp);
	if(CModuleInfo::GetQuoteProvidersPtr()->GetContactProviderPtr(hContact))
	{
		HANDLE hWL = CModuleInfo::GetInstance().GetWindowList(WINDOW_PREFIX_INFO,true);
		assert(hWL);
		HWND hWnd = WindowList_Find(hWL,hContact);
		if(NULL != hWnd) 
		{
			SetForegroundWindow(hWnd);
			SetFocus(hWnd);
		}
		else if(true == IsMyContact(hContact))
		{
			CreateDialogParam(CModuleInfo::GetModuleHandle(),MAKEINTRESOURCE(IDD_DIALOG_QUOTE_INFO_1),NULL,QuoteInfoDlgProc1,reinterpret_cast<LPARAM>(hContact));
		}

		return 1;
	}
	else
	{
		return 0;
	}
}

namespace
{
	void enable_menu(HANDLE hMenu,bool bEnable)
	{
		CLISTMENUITEM clmi = {0};
		clmi.cbSize = sizeof( CLISTMENUITEM );
		clmi.flags = CMIM_FLAGS;
		if(false == bEnable)
		{
			clmi.flags |= /*CMIF_HIDDEN*/CMIF_GRAYED;
		}

		CallService(MS_CLIST_MODIFYMENUITEM,reinterpret_cast<WPARAM>(hMenu),reinterpret_cast<LPARAM>(&clmi));
	}
}

int Quotes_PrebuildContactMenu(WPARAM wp,LPARAM lp)
{
	enable_menu(g_hMenuEditSettings,false);
	enable_menu(g_hMenuOpenLogFile,false);
#ifdef CHART_IMPLEMENT
	enable_menu(g_hMenuChart,false);
#endif
	enable_menu(g_hMenuRefresh,false);
	
	HANDLE hContact = reinterpret_cast<HANDLE>(wp);
	if(NULL == hContact)
	{
		return 0;
	}

	enable_menu(g_hMenuEditSettings,true);

	enable_menu(g_hMenuRefresh,true);

	tstring sLogFileName;
	bool bThereIsLogFile = (true == get_log_file(hContact,sLogFileName))
		&& (false == sLogFileName.empty()) && (0 == _taccess(sLogFileName.c_str(),04));
	if(true == bThereIsLogFile)
	{
#ifdef CHART_IMPLEMENT
		enable_menu(g_hMenuChart,true);
#endif
		enable_menu(g_hMenuOpenLogFile,true);
	}

	return 0;
}
