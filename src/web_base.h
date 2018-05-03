////By Fanxiushu 2014-01-02
#pragma once


#include <atlbase.h>
#include <ExDispid.h>

#import  "c:\windows\system32\ieframe.dll"
//#import  "c:\windows\system32\mshtml.tlb"
#include <atlwin.h>
//#pragma comment(lib,"atl")

#include <process.h>
#include <set>
#include <process.h>
#include <ShlObj.h>

#define IDC_IEXPLORE    0

#pragma comment(lib,"urlmon.lib")

////////////////////////////////////

////////////////////////////////////
class ATL_NO_VTABLE MyAxHostWindowEx: 
	public CAxHostWindow,
//	public IDocHostShowUI,
	public IOleCommandTarget
{
public:
//	MyAxHostWindowEx(){MessageBox("**&&&");}
//	~MyAxHostWindowEx(){MessageBox("~MyAxHostWindowEx");}

	DECLARE_PROTECT_FINAL_CONSTRUCT()
	
	DECLARE_NO_REGISTRY()
	DECLARE_POLY_AGGREGATABLE(MyAxHostWindowEx)
	DECLARE_GET_CONTROLLING_UNKNOWN()
//	DECLARE_NOT_AGGREGATABLE(MyAxHostWindowEx)
	DECLARE_WND_SUPERCLASS(_T("MyAxHostWindowEx"), CAxHostWindow::GetWndClassName())

	BEGIN_COM_MAP(MyAxHostWindowEx)
		COM_INTERFACE_ENTRY(IDocHostUIHandler)
//		COM_INTERFACE_ENTRY(IDocHostShowUI)
		COM_INTERFACE_ENTRY(IOleCommandTarget)
		COM_INTERFACE_ENTRY_CHAIN(CAxHostWindow)
	END_COM_MAP() 

	virtual void OnFinalMessage(HWND hWnd)
	{
		FinalRelease();
		GetControllingUnknown()->Release();
	}

	void FinalRelease()
	{
		CAxHostWindow::FinalRelease();
	} 

	STDMETHOD(GetHostInfo)(DOCHOSTUIINFO FAR *pInfo)  
    {  
  //      if (m_spDefaultDocHostUIHandler)  
  //          return m_spDefaultDocHostUIHandler->GetHostInfo(pInfo);  
		pInfo->cbSize = sizeof(DOCHOSTUIINFO);
		pInfo->dwFlags = pInfo->dwFlags | DOCHOSTUIFLAG_NO3DBORDER | DOCHOSTUIFLAG_ENABLE_REDIRECT_NOTIFICATION;// | DOCHOSTUIFLAG_FLAT_SCROLLBAR; //Æ½Ì¹¹ö¶¯Ìõ

		return S_OK;  
    } 

	//IOleCommandTarget
	STDMETHOD(QueryStatus)(const GUID *pguidCmdGroup, ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT *pCmdText)
	{
		return pguidCmdGroup ? OLECMDERR_E_UNKNOWNGROUP : OLECMDERR_E_NOTSUPPORTED;
	}
	STDMETHOD(Exec)(
		/* [unique][in] */ const GUID *pguidCmdGroup,
		/* [in] */ DWORD nCmdID,
		/* [in] */ DWORD nCmdexecopt,
		/* [unique][in] */ VARIANT *pvaIn,
		/* [unique][out][in] */ VARIANT *pvaOut)
	{
		HRESULT hr = pguidCmdGroup ? OLECMDERR_E_UNKNOWNGROUP : OLECMDERR_E_NOTSUPPORTED;
		if(pguidCmdGroup && IsEqualGUID(*pguidCmdGroup, CGID_DocHostCommandHandler)) {
			if(nCmdID == OLECMDID_SHOWSCRIPTERROR) {
				hr = S_OK;
				// http://support.microsoft.com/default.aspx?scid=kb;en-us;261003
				(*pvaOut).vt = VT_BOOL;
				// Continue running scripts on the page.
				(*pvaOut).boolVal = VARIANT_TRUE;
			}
		}
		return hr;
	}

	///IDocHostShowUI
/*	STDMETHOD(ShowMessage)(HWND hwnd,
		LPOLESTR lpstrText,
		LPOLESTR lpstrCaption,
		DWORD dwType,
		LPOLESTR lpstrHelpFile,
		DWORD dwHelpContext,
		LRESULT *plResult)
	{
		//ÆÁ±Îµ¯³ö¿ò
		return S_OK;
	}
	STDMETHOD(ShowHelp)(
		HWND hwnd,
		LPOLESTR pszHelpFile,
		UINT uCommand,
		DWORD dwData,
		POINT ptMouse,
		IDispatch *pDispatchObjectHit )
	{
		///ÆÁ±Îµ¯³ö¿ò
		return S_OK;
	}*/

	//////
	static HRESULT CreateIeControl( LPCSTR lpszName, HWND hWnd, IStream* pStream, 
		IUnknown** ppUnkContainer, IUnknown** ppUnkControl, REFIID iidSink, IUnknown* punkSink, BSTR bstrLic )
	{
		AtlAxWinInit();
		///
		HRESULT hr;
		CComPtr<IUnknown> spUnkContainer;
		CComPtr<IUnknown> spUnkControl;

		hr = MyAxHostWindowEx::_CreatorClass::CreateInstance(NULL, __uuidof(IUnknown), (void**)&spUnkContainer);
		if (SUCCEEDED(hr)) {
			CComPtr<IAxWinHostWindowLic> pAxWindow;
			spUnkContainer->QueryInterface(__uuidof(IAxWinHostWindow), (void**)&pAxWindow);
			CComBSTR bstrName(lpszName);
			hr = pAxWindow->CreateControlLicEx(bstrName, hWnd, pStream, &spUnkControl, iidSink, punkSink, bstrLic);
		}
		if (ppUnkContainer != NULL)	{
			if (SUCCEEDED(hr)) {
				*ppUnkContainer = spUnkContainer.p;
				spUnkContainer.p = NULL;
			}
			else
				*ppUnkContainer = NULL;
		}
		if (ppUnkControl != NULL) {
			if (SUCCEEDED(hr)) {
				*ppUnkControl = SUCCEEDED(hr) ? spUnkControl.p : NULL;
				spUnkControl.p = NULL;
			}
			else
				*ppUnkControl = NULL;
		}
		return hr;
		///////////////////
	}
	/////////
};

class CWebBrowser : 
        public CWindowImpl<CWebBrowser, CAxWindow> ,
		public IDispEventImpl< IDC_IEXPLORE, CWebBrowser, &DIID_DWebBrowserEvents2 ,&LIBID_SHDocVw, 1, 0>
{
public:
    CComPtr<IWebBrowser2> m_pWebBrowser;
	/////

protected:
	///
public:
	DECLARE_WND_SUPERCLASS(_T("CWebBrowser"), CAxWindow::GetWndClassName())

	BEGIN_SINK_MAP(CWebBrowser)
		SINK_ENTRY_EX( IDC_IEXPLORE, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE, OnDocumentComplete )
		SINK_ENTRY_EX( IDC_IEXPLORE, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2, OnBeforeNavigate2)
		SINK_ENTRY_EX( IDC_IEXPLORE, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2, OnNavigateComplete2)
		SINK_ENTRY_EX( IDC_IEXPLORE, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE, OnNavigateComplete)
		SINK_ENTRY_EX( IDC_IEXPLORE, DIID_DWebBrowserEvents2, DISPID_STATUSTEXTCHANGE, OnStatusTextChange)
		SINK_ENTRY_EX( IDC_IEXPLORE, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW2, OnNewWindow2)
		SINK_ENTRY_EX( IDC_IEXPLORE, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW3, OnNewWindow3)
		SINK_ENTRY_EX( IDC_IEXPLORE, DIID_DWebBrowserEvents2, DISPID_COMMANDSTATECHANGE, OnCommandStateChange )
	END_SINK_MAP()
	///
	void __stdcall OnDocumentComplete(IDispatch* pDisp, VARIANT* URL)  
    {  
        try{
			on_document_complete( pDisp, URL ); 
		}
		catch(...){
			printf("OnDocumentComplete exception\n");
		}
    }  
	////
	void __stdcall OnBeforeNavigate2 (
		IDispatch* pDisp, VARIANT* URL, VARIANT* Flags, VARIANT* TargetFrameName,
		VARIANT* PostData, VARIANT* Headers, VARIANT_BOOL* Cancel )
	{
		try{
			on_navigate_before2(pDisp, URL, Flags, TargetFrameName, PostData, Headers, Cancel);
		}
		catch (...){}
		
	}
	void __stdcall OnStatusTextChange( BSTR url_text )
	{
		///
	}
	void __stdcall OnNewWindow2(LPDISPATCH * ppDisp, VARIANT_BOOL* Cancel)
	{
		///
	}
	void __stdcall OnNewWindow3(LPDISPATCH* ppDisp, VARIANT_BOOL * Cancel, DWORD dwFlags, BSTR bstrUrlContext, BSTR bstrUrl)
	{
		///
		try{
			on_new_window3( ppDisp, Cancel, dwFlags, bstrUrlContext, bstrUrl ); 
		}catch(...){
			////
		}
	}
	void __stdcall OnCommandStateChange(long Command,VARIANT_BOOL Enable )
	{
		on_command_state_change( Command, Enable ); 
	}
	void __stdcall OnNavigateComplete( LPDISPATCH pDisp, VARIANT* URL)
	{
		try{
			on_navigate_complete( pDisp, URL ); 
		}catch(...){
		}
	}
	void __stdcall OnNavigateComplete2( LPDISPATCH pDisp, VARIANT* URL)
	{
		try{
			on_navigate_complete2( pDisp, URL ); 
		}catch(...){
			printf("OnNavigateComplete2 exception\n");
		}
	}
    
	/////virtual function
	virtual void on_navigate_before2(IDispatch* pDisp, VARIANT* URL, 
		VARIANT* Flags, VARIANT* TargetFrameName,
		VARIANT* PostData, VARIANT* Headers, VARIANT_BOOL* Cancel){}
	virtual void on_document_complete(IDispatch* pDisp, VARIANT* URL){}
	virtual void on_new_window3(LPDISPATCH* ppDisp, VARIANT_BOOL * Cancel, DWORD dwFlags, BSTR bstrUrlContext, BSTR bstrUrl){}
	virtual void on_command_state_change( long cmd, VARIANT_BOOL Enable ){}
	virtual void on_navigate_complete( IDispatch* pDisp, VARIANT* URL ){}
	virtual void on_navigate_complete2( IDispatch* pDisp, VARIANT* URL ){}
	///////////////

public:
    ////  
	BEGIN_MSG_MAP(CWebBrowser)  
        MESSAGE_HANDLER(WM_CREATE,  OnCreate)  
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy) 
		MESSAGE_HANDLER(WM_NCDESTROY, OnNcDestroy)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
    END_MSG_MAP()  
	////////
	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled )  
    {  
  
//		m_pWebBrowser->put_Silent( VARIANT_TRUE ); // ÉèÖÃ°²¾²£¬×èÖ¹´íÎóµ¯´°
        
		::OleInitialize(NULL);

		CREATESTRUCT* lpCreate = (CREATESTRUCT*)lParam;
		int nCreateSize = 0;
		if (lpCreate && lpCreate->lpCreateParams)
			nCreateSize = *((WORD*)lpCreate->lpCreateParams);
		HGLOBAL h = GlobalAlloc(GHND, nCreateSize);
		CComPtr<IStream> spStream;
		if (h && nCreateSize) {
			BYTE* pBytes = (BYTE*) GlobalLock(h);
			BYTE* pSource = ((BYTE*)(lpCreate->lpCreateParams)) + sizeof(WORD); 
			//Align to DWORD
			//pSource += (((~((DWORD)pSource)) + 1) & 3);
			memcpy(pBytes, pSource, nCreateSize);
			GlobalUnlock(h);
			CreateStreamOnHGlobal(h, TRUE, &spStream);
		}

		IAxWinHostWindow* pAxWindow = NULL;
		CComPtr<IUnknown> spUnk;
	/*	BSTR                pClsId;  
		_bstr_t             bStrClsid;  
		CLSID               Clsid = CLSID_WebBrowser;
		StringFromCLSID(CLSID_WebBrowser, &pClsId); 
		bStrClsid = pClsId; 
		CoTaskMemFree(pClsId);  */
		LPCSTR lpstrName = "Shell.Explorer.2";// CLSID_WebBrowser¶ÔÓ¦µÄProgID
		HRESULT hRet = MyAxHostWindowEx::CreateIeControl( /*bStrClsid*/ lpstrName, m_hWnd, spStream, &spUnk, NULL, IID_NULL, NULL, NULL );

		if(FAILED(hRet))
			return -1;	// abort window creation
		hRet = spUnk->QueryInterface(__uuidof(IAxWinHostWindow), (void**)&pAxWindow);
		if(FAILED(hRet))
			return -1;	// abort window creation

		SetWindowLongPtr(GWLP_USERDATA, (DWORD_PTR)pAxWindow);
		// continue with DefWindowProc
		LRESULT ret = ::DefWindowProc(m_hWnd, uMsg, wParam, lParam);
		bHandled = TRUE;
		//////
		
		m_pWebBrowser = NULL; 
		///
		hRet = QueryControl(IID_IWebBrowser2, (void**)&m_pWebBrowser);  
        if(SUCCEEDED(hRet))  
        {  
            if(FAILED(DispEventAdvise(m_pWebBrowser, &DIID_DWebBrowserEvents2)))  {
                ATLASSERT(FALSE);  
				return -1L;
			}
        }
		else{
			return -1L; ////
		}
		///////////
		CoInternetSetFeatureEnabled( FEATURE_DISABLE_NAVIGATION_SOUNDS, SET_FEATURE_ON_PROCESS, TRUE ); ///

		///
		return on_create( (LPCREATESTRUCT)lParam ); 
		/////
		
	}
	//////
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		if( m_pWebBrowser ){
			DispEventUnadvise( m_pWebBrowser , &DIID_DWebBrowserEvents2);  
		}
		////
		m_pWebBrowser = NULL; 

		bHandled = FALSE; 
		
		////
		on_destroy(); 
		/////
		return 1L;  /////
	}
	LRESULT OnNcDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		bHandled = FALSE; 
		///
		on_ncdestroy();
		////
		return 1L;
	}
	LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		bHandled = FALSE;
		///
		on_timer(wParam);
		/////
		return 1L; 
	}
	/////////
	virtual LRESULT  on_create(LPCREATESTRUCT lpcs){ return 0;  }
	virtual void on_destroy(){}
	virtual void on_ncdestroy(){}
	virtual void on_timer(UINT nIDEvent ){}

public:
	//////////
};

