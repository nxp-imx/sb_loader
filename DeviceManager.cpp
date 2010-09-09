#include "stdafx.h"
#include <Dbt.h>

#include <basetyps.h>
#include <setupapi.h>
#include <initguid.h>
#pragma warning( push )
#pragma warning( disable : 4201 )

extern "C" {
    #include "hidsdi.h"
}
#pragma warning( pop )
#include "hiddevice.h"
#include "StHidDevice.h"
#include "MxHidDevice.h"
#include "DeviceManager.h"
#include "sb_loader.h"
#include <initguid.h>
#include "usbiodef.h"

//////////////////////////////////////////////////////////////////////
//
// DeviceManager class implementation
//
//////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(DeviceManager, CWinThread)

DeviceManager::DeviceManager()
: _hUsbDev(NULL)
, _hUsbHub(NULL)
, _hChangeEvent(NULL)
, _bStopped(true)
, _hStartEvent(NULL)
, _lastEventType(UNKNOWN_EVT)
{
	ATLTRACE(_T("  +DeviceManager::DeviceManager()\n"));
	// SingletonHolder likes it better if this thing is around
	// when it calls delete.
	m_bAutoDelete = FALSE;
}

DeviceManager::~DeviceManager()
{
	ATLTRACE(_T("  +DeviceManager::~DeviceManager()\n"));

	// Client has to call gDeviceManager::Instance().Close() before it exits;
	ASSERT ( _bStopped == true );

	ATLTRACE(_T("  -DeviceManager::~DeviceManager()\n"));
}

// Runs in the context of the Client thread.
uint32_t DeviceManager::Open()
{
	ATLTRACE(_T("+DeviceManager::Open()\n"));
	// Client can not call gDeviceManager::Instance().Open() more than once.
	ASSERT ( _bStopped == true );
	
	// Create an event that we can wait on so we will know when
    // the DeviceManager thread has completed InitInstance()
    _hStartEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	
    // Create the user-interface thread supporting messaging
	uint32_t error;
    if ( CreateThread() != 0 ) // success
    {
	    // Hold the client thread until the DeviceManager thread is running
		VERIFY(::WaitForSingleObject(_hStartEvent, INFINITE) == WAIT_OBJECT_0);

        // Set flag to running. Flag set to stopped in constructor and in Close()
	    _bStopped = false;

        error = ERROR_SUCCESS;
        ATLTRACE(" Created thread(%#x, %d).\n", m_nThreadID, m_nThreadID);
    }
	else
    {
        error = GetLastError();
        ATLTRACE(" *** FAILED TO CREATE THREAD.\n");
    }

	// clean up
    ::CloseHandle(_hStartEvent);
    _hStartEvent = NULL;

	ATLTRACE(_T("-DeviceManager::Open()\n"));
	return error;
}

// Runs in the context of the Client thread
void DeviceManager::Close()
{
	ATLTRACE(_T("+DeviceManager::Close()\n"));
	// Client has to call gDeviceManager::Instance().Open() before it 
    // can call gDeviceManager::Instance().Close();
	ASSERT ( _bStopped == false );

    // Post a KILL event to the DeviceManager thread
	PostThreadMessage(WM_MSG_DEV_EVENT, EVENT_KILL, 0);
	// Wait for the DeviceManager thread to die before returning
	WaitForSingleObject(m_hThread, INFINITE);

	// Bad things happen in ~DeviceManager() if ExitInstance() has not been called,
	// so the destuctor asserts if the client did not call DeviceManager::Close().
	_bStopped = true;
	m_hThread = NULL;
    ATLTRACE(_T("-DeviceManager::Close()\n"));
}

// Runs in the context of the DeviceManager thread
BOOL DeviceManager::InitInstance()
{
	ATLTRACE(_T("  +DeviceManager::InitInstance()\n"));
    // Create a hidden window and register it to receive WM_DEVICECHANGE messages
	if( _DevChangeWnd.CreateEx(WS_EX_TOPMOST, _T("STATIC"), _T("DeviceChangeWnd"), 0, CRect(0,0,5,5), NULL, 0) )
    {
		DEV_BROADCAST_DEVICEINTERFACE broadcastInterface;
		
		broadcastInterface.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		broadcastInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	    
		memcpy(&(broadcastInterface.dbcc_classguid),&(GUID_DEVINTERFACE_USB_DEVICE),sizeof(struct _GUID));
		
		// register for usb devices
		_hUsbDev=RegisterDeviceNotification(_DevChangeWnd.GetSafeHwnd(),&broadcastInterface,
											DEVICE_NOTIFY_WINDOW_HANDLE);
	    
		memcpy(&(broadcastInterface.dbcc_classguid),&(GUID_DEVINTERFACE_USB_HUB),sizeof(struct _GUID));
		
		// register for usb hubs
		_hUsbHub=RegisterDeviceNotification(_DevChangeWnd.GetSafeHwnd(),&broadcastInterface,
											DEVICE_NOTIFY_WINDOW_HANDLE);
    }
    else
    {
        ATLTRACE(_T(" *** FAILED TO CREATE WINDOW FOR WM_DEVCHANGE NOTIFICATIONS.\n"));
    }
    
	// Let the client thread that called Open() resume.
	VERIFY(::SetEvent(_hStartEvent));

    ATLTRACE(_T("  -DeviceManager::InitInstance()\n"));
	return (TRUE);
}

// Runs in the context of the DeviceManager thread
int DeviceManager::ExitInstance()
{
	ATLTRACE(_T(" +DeviceManager::ExitInstance()\n"));

	// Messaging support
    if ( _hUsbDev != INVALID_HANDLE_VALUE )
        UnregisterDeviceNotification(_hUsbDev);
	if ( _hUsbHub != INVALID_HANDLE_VALUE )
        UnregisterDeviceNotification(_hUsbHub);
    
    // Windows must be Destroyed in the thread in which they were created.
    // _DevChangeWnd was created in the DeviceManager thread in InitInstance().
    BOOL ret = _DevChangeWnd.DestroyWindow();

	// Clean up the AutoPlay object
	SetCancelAutoPlay(false);

	ATLTRACE(_T(" -DeviceManager::ExitInstance()\n"));
    return CWinThread::ExitInstance();;
}

// Runs in the context of the Client thread.
bool DeviceManager::FindHidDevice(CHidDevice* pHidDevice,USHORT vid, USHORT pid, int timeout)
{
    ATLTRACE(_T("+DeviceManager::FindHidDevice()\n"));
	bool bRet = TRUE;
	
	if ( pHidDevice->FindKnownHidDevices(vid, pid) == ERROR_SUCCESS )
		return TRUE;

    //
    // We didn't find a device on our first pass
    //
    // Just return right away if there was no wait period specified
    if ( timeout == 0 )
    {
        ATLTRACE(_T("-DeviceManager::FindHidDevice() : No device, not waiting.\n"));
        return FALSE;
    }

	_tprintf(_T("  Waiting for HID device for %d seconds...\n"), timeout);
    //
    // Since a waitPeriod was specified, we need to hang out and see if a device arrives before the 
    // waitPeriod expires.
    //
 	// Create an event that we can wait on so we will know if
    // the DeviceManager got a device arrival
    _hChangeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

    HANDLE hTimer = CreateWaitableTimer(NULL, true, _T("FindDeviceTimer"));
    LARGE_INTEGER waitTime;
    waitTime.QuadPart = timeout * (-10000000);
    SetWaitableTimer(hTimer, &waitTime, 0, NULL, NULL, false);

    HANDLE waitHandles[2] = { _hChangeEvent, hTimer };
    DWORD waitResult;
    while( pHidDevice->FindKnownHidDevices(vid, pid) != ERROR_SUCCESS )
    {

        waitResult = MsgWaitForMultipleObjects(2, &waitHandles[0], false, INFINITE, 0);
        if ( waitResult == WAIT_OBJECT_0 )
        {
            // Device Change Event
           	VERIFY(::ResetEvent(_hChangeEvent));
			_tprintf(_T("    %s event.\n"), GetEventString(_lastEventType));
			Sleep(50);
            continue;
        }
        else if ( waitResult == WAIT_OBJECT_0 + 1 )
        {
            // Timeout
			//delete pHidDevice;
			//pHidDevice = NULL;
			bRet = FALSE;
            break;
        }
        else
        {
            // unreachable, but catch it just in case.
            ASSERT(0);
        }
    }
    // clean up
    CloseHandle(_hChangeEvent);
    _hChangeEvent = NULL;
    
	ATLTRACE(_T("-DeviceManager::FindHidDevice() : %s.\n"), pHidDevice ? _T("Found device") : _T("No device"));
	return bRet;
}

bool DeviceManager::WaitForChange(DevChangeEvent eventType, int timeout)
{
    
	if ( _lastEventType == eventType )
	{
//		_tprintf(_T("%s already happened.\n"), GetEventString(eventType));
		return true;
	}

	if ( timeout == 0 )
	{
		return false;
	}
	else
	{
		_tprintf(_T("  Waiting for %s for %d seconds...\n"), GetEventString(eventType), timeout);
	}
	//
 	// Create an event that we can wait on so we will know if
    // the DeviceManager got a device arrival
    _hChangeEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);

    HANDLE hTimer = CreateWaitableTimer(NULL, true, _T("FindDeviceTimer"));
    LARGE_INTEGER waitTime;
    waitTime.QuadPart = timeout * (-10000000);
    SetWaitableTimer(hTimer, &waitTime, 0, NULL, NULL, false);

    HANDLE waitHandles[2] = { _hChangeEvent, hTimer };
    DWORD waitResult;
	bool found = true;
    while( 1 )
    {

        waitResult = MsgWaitForMultipleObjects(2, &waitHandles[0], false, INFINITE, 0);
        if ( waitResult == WAIT_OBJECT_0 )
        {
            // Device Change Event
           	VERIFY(::ResetEvent(_hChangeEvent));
			
			_tprintf(_T("    %s event.\n"), GetEventString(_lastEventType));
			if ( _lastEventType == eventType)
				break;
			else
				continue;
        }
        else if ( waitResult == WAIT_OBJECT_0 + 1 )
        {
            // Timeout
			found = false;
            break;
        }
        else
        {
            // unreachable, but catch it just in case.
            ASSERT(0);
        }
    }
    // clean up
    CloseHandle(_hChangeEvent);
    _hChangeEvent = NULL;
    
	return found;

}

BEGIN_MESSAGE_MAP(DeviceManager::DevChangeWnd, CWnd)
    ON_WM_DEVICECHANGE()
END_MESSAGE_MAP()

// Runs in the context of the DeviceManager thread
BOOL DeviceManager::DevChangeWnd::OnDeviceChange(UINT nEventType,DWORD_PTR dwData)
{
	CString MsgStr;
	uint32_t event = UNKNOWN_EVT;
    PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR)dwData;

	if (lpdb) 
	{
		switch(nEventType) 
		{
			case DBT_DEVICEARRIVAL: 
				if (lpdb->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) 
				{
					if(((PDEV_BROADCAST_DEVICEINTERFACE)lpdb)->dbcc_classguid == GUID_DEVINTERFACE_USB_DEVICE) 
					{
						MsgStr = (LPCTSTR)((PDEV_BROADCAST_DEVICEINTERFACE)lpdb)->dbcc_name;
						event = DEVICE_ARRIVAL_EVT;
					}
					else if(((PDEV_BROADCAST_DEVICEINTERFACE)lpdb)->dbcc_classguid == GUID_DEVINTERFACE_USB_HUB) 
					{
						MsgStr = (LPCTSTR)((PDEV_BROADCAST_DEVICEINTERFACE)lpdb)->dbcc_name;
						event = HUB_ARRIVAL_EVT;
					}
				}
				else if(lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME) 
				{
					MsgStr = DrivesFromMask( ((PDEV_BROADCAST_VOLUME)lpdb)->dbcv_unitmask );
					event = VOLUME_ARRIVAL_EVT;
					ATLTRACE(_T("DeviceManager::DevChangeWnd::OnDeviceChange() - VOLUME_ARRIVAL_EVT(%s)\n"), MsgStr);
				}
				break;
			case DBT_DEVICEREMOVECOMPLETE:
				if(lpdb->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) 
				{
					if(((PDEV_BROADCAST_DEVICEINTERFACE)lpdb)->dbcc_classguid == GUID_DEVINTERFACE_USB_DEVICE) 
					{
						MsgStr = (LPCTSTR)((PDEV_BROADCAST_DEVICEINTERFACE)lpdb)->dbcc_name;
						event = DEVICE_REMOVAL_EVT;
					}
					else if(((PDEV_BROADCAST_DEVICEINTERFACE)lpdb)->dbcc_classguid == GUID_DEVINTERFACE_USB_HUB) 
					{
						MsgStr = (LPCTSTR)((PDEV_BROADCAST_DEVICEINTERFACE)lpdb)->dbcc_name;
						event = HUB_REMOVAL_EVT;
					}
				}
				else if(lpdb->dbch_devicetype == DBT_DEVTYP_VOLUME) 
				{
					MsgStr = DrivesFromMask( ((PDEV_BROADCAST_VOLUME)lpdb)->dbcv_unitmask );
					event = VOLUME_REMOVAL_EVT;
					ATLTRACE(_T("DeviceManager::DevChangeWnd::OnDeviceChange() - VOLUME_REMOVAL_EVT(%s)\n"), MsgStr);
				}
				break;
			default:
				ASSERT(0);
		} // end switch (nEventType)

		// let's figure out what to do with the WM_DEVICECHANGE message
		// after we get out of this loop so we don't miss any messages.
		BSTR bstr_msg = MsgStr.AllocSysString();
		g_pDeviceManager->PostThreadMessage(WM_MSG_DEV_EVENT, (WPARAM)event, (LPARAM)bstr_msg);

	} // end if (lpdb)

	return TRUE;
}

// worker function for OnDeviceChange() to get drive letters from the bitmask
// Runs in the context of the DeviceManager thread
CString DeviceManager::DevChangeWnd::DrivesFromMask(ULONG UnitMask)
{
	CString Drive;
	TCHAR Char;

	for (Char = 0; Char < 26; ++Char) 
	{
		if (UnitMask & 0x1)
			Drive.AppendFormat(_T("%c"), Char + _T('A'));
		UnitMask = UnitMask >> 1;
	}
	return Drive;
}

BEGIN_MESSAGE_MAP(DeviceManager, CWinThread)
	ON_THREAD_MESSAGE(DeviceManager::WM_MSG_DEV_EVENT, OnMsgDeviceEvent)
END_MESSAGE_MAP()

// Runs in the context of the DeviceManager thread.
void DeviceManager::OnMsgDeviceEvent(WPARAM eventType, LPARAM desc)
{
	CString msg = (LPCTSTR)desc;
	SysFreeString((BSTR)desc);
	if ( eventType == EVENT_KILL ) 
	{
		PostQuitMessage(0);
		return;
    }

	_lastEventType = (DevChangeEvent)eventType;

	// Signal anyone waiting on a device change to go see what changed. See FindKnownHidDevices() for example.
    if (_hChangeEvent)
	{
		VERIFY(::SetEvent(_hChangeEvent));
	}
}

CString DeviceManager::GetEventString(DevChangeEvent eventType)
{
	CString retString = _T("");

	switch ( eventType )
	{
		case DEVICE_ARRIVAL_EVT:
			retString = _T("Device Arrival");
			break;

		case DEVICE_REMOVAL_EVT:
			retString = _T("Device Removal");
			break;

		case VOLUME_ARRIVAL_EVT:
			retString = _T("Volume Arrival");
			break;

		case VOLUME_REMOVAL_EVT:
			retString = _T("Volume Removal");
			break;

		case HUB_ARRIVAL_EVT:
			retString = _T("Hub Arrival");
			break;

		case HUB_REMOVAL_EVT:
			retString = _T("Hub Removal");
			break;

		default:
		{
			ASSERT(0);
		}
	} // end switch (eventType)

	return retString;
}


STDMETHODIMP DeviceManager::CQueryCancelAutoplay::AllowAutoPlay(LPCTSTR pszPath,
    DWORD dwContentType, LPCWSTR pszLabel, DWORD dwSerialNumber)
{
    HRESULT hr = S_OK;
	CString drv_path = pszPath;

	// Is it the drive we want to cancel Autoplay for?
	if(drv_path.FindOneOf(_driveList) != -1 ) {
		ATLTRACE("*** CAutoPlayReject: Rejected AutoPlay Drive: %s ***\n", pszPath);
        hr = S_FALSE;
    }

    return hr;
}

HRESULT DeviceManager::CQueryCancelAutoplay::SetCancelAutoPlay(bool rejectAutoPlay, LPCTSTR driveList)
{
	if ( rejectAutoPlay == false )
	{
		return Unregister();
	}

	if(driveList)
	{
		CString RejectDriveLetters = driveList;
		if ( !RejectDriveLetters.IsEmpty() )
			_driveList = RejectDriveLetters;
	}

	HKEY hKey;
	// add a registry key so our IQueryCancelAutoPlay will work
	if(RegCreateKey(HKEY_LOCAL_MACHINE,
		_T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers\\CancelAutoplay\\CLSID"),
		&hKey) == ERROR_SUCCESS) 
	{
		LPTSTR Str; 
		CString  ClsId;

		StringFromCLSID(CLSID_CANCEL_AUTOPLAY, &Str);
		ClsId = Str;
		CoTaskMemFree(Str);
		ClsId.Remove(_T('{')); ClsId.Remove(_T('}'));

		if(RegSetValueEx( hKey, ClsId, 0, REG_SZ, 0, 0) != ERROR_SUCCESS ) 
			ATLTRACE(_T("*** DeviceManager::CQueryCancelAutoplay: [ERROR] Failed to create IQueryCancelAutoPlay CLSID. ***\n"));
			RegCloseKey(hKey);
	}
	else 
	{
		ATLTRACE(_T("*** DeviceManager::CQueryCancelAutoplay: [ERROR] Failed to create IQueryCancelAutoPlay CLSID.\n ***"));
	}

    // Create the moniker that we'll put in the ROT
    IMoniker* pmoniker;
    HRESULT hr = CreateClassMoniker(CLSID_CANCEL_AUTOPLAY, &pmoniker);
    
	if(SUCCEEDED(hr)) 
	{
        IRunningObjectTable* prot;
        hr = GetRunningObjectTable(0, &prot);
        if(SUCCEEDED(hr)) 
		{
			CQueryCancelAutoplay* pQCA = new CQueryCancelAutoplay();
            if(pQCA) 
			{
	            IUnknown* punk;
                hr = pQCA->QueryInterface(IID_IUnknown, (void**)&punk);
                if (SUCCEEDED(hr)) 
				{
                    // Register...
                    hr = prot->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, punk, pmoniker, &_ROT);
                    if(SUCCEEDED(hr)) 
					{
						_isRegistered = true;
						ATLTRACE(_T("*** DeviceManager::CQueryCancelAutoplay: Registered drives %s***\n"), _driveList);
                    }
					else 
						ATLTRACE(_T("*** DeviceManager::CQueryCancelAutoplay: [ERROR] Registration failed ***\n"));
                    punk->Release();
                }
				else 
					ATLTRACE(_T("*** DeviceManager::CQueryCancelAutoplay: [ERROR] Registration failed ***\n"));
                pQCA->Release();
            }
            else 
			{
                hr = E_OUTOFMEMORY;
				ATLTRACE(_T("*** DeviceManager::CQueryCancelAutoplay: [ERROR] Registration failed ***\n"));
            }
            prot->Release();
        }
		else 
			ATLTRACE(_T("*** DeviceManager::CQueryCancelAutoplay: [ERROR] Registration failed ***\n"));    
		pmoniker->Release();
    }
    return hr;
}

HRESULT DeviceManager::CQueryCancelAutoplay::Unregister()
{
    IRunningObjectTable *prot;

	if(_isRegistered)
	{
		if (SUCCEEDED(GetRunningObjectTable(0, &prot))) 
		{
			// Remove our instance from the ROT
			prot->Revoke(_ROT);
			prot->Release();
			_isRegistered = false;
			ATLTRACE(_T("*** DeviceManager::CQueryCancelAutoplay: Unregistered. ***\n"));
		}
		else
			return S_FALSE;
	}

    return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// COM boiler plate code...
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// CQueryCancelAutoplay
//
STDMETHODIMP DeviceManager::CQueryCancelAutoplay::QueryInterface(REFIID riid, void** ppv)
{
    IUnknown* punk = NULL;
    HRESULT hr = S_OK;

    if (IID_IUnknown == riid)
    {
        punk = static_cast<IUnknown*>(this);
        punk->AddRef();
    }
    else
    {
        if (IID_IQueryCancelAutoPlay == riid)
        {
            punk = static_cast<IQueryCancelAutoPlay*>(this);
            punk->AddRef();
        }
        else
        {
            hr = E_NOINTERFACE;
        }
    }

    *ppv = punk;

    return hr;
}

STDMETHODIMP_(ULONG) DeviceManager::CQueryCancelAutoplay::AddRef()
{
    return ::InterlockedIncrement((LONG*)&_cRef);
}

STDMETHODIMP_(ULONG) DeviceManager::CQueryCancelAutoplay::Release()
{
    ULONG cRef = ::InterlockedDecrement((LONG*)&_cRef);

    if(!cRef)
    {
        delete this;
    }

    return cRef;
}
