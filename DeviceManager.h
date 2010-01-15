////////////////////////////////////////////////////////////////////////////////
//! \addtogroup gDeviceManager
//! @{
//
// Copyright (c) 2006 SigmaTel, Inc.
//
//! \file DeviceManager.h
//! \brief Global class instance for device enumeration.
//!
//! Detailed description here.
//!
//! \see Loki::SingletonHolder
////////////////////////////////////////////////////////////////////////////////

/*============================================================================
  ____________________________________________________________________________
                                ______________________________________________
   SSSS  GGGGG  TTTTT  L                                        SigmaTel, Inc.
  S      G        T    L                              1601 S. Mopac Expressway
   SSS   G  GG    T    L                                             Suite 100
      S  G   G    T    L                              Austin, Texas 78746-7010
  SSSS   GGGGG    T    LLLLL    ______________________________________________
  ____________________________________________________________________________

  Copyright(C) 2006, SigmaTel, Inc.
  All Rights Reserved.

  This program code listing is proprietary to SigmaTel and may not be copied,
  distributed, or used without a license to do so.  Such license may have
  Limited or Restricted Rights. Please refer to the license for further
  clarification.
  ____________________________________________________________________________

  Notice: The program contained in this listing is a proprietary trade
  secret of SigmaTel, Austin, Texas, and is copyrighted
  under the United States Copyright Act of 1976 as an unpublished work,
  pursuant to Section 104 and Section 408 of Title XVII of the United
  States code. Unauthorized copying, adaption, distribution, use, or
  display is prohibited by this law.
  ____________________________________________________________________________
  ____________________________________________________________________________

  Revision History
  Date      Who  Comment
  ________  ___  _____________________________________________________________
  07/21/06  clw  initial version
============================================================================*/
#pragma once
#include "stdafx.h"
#include "StHidDevice.h"

#include <Dbt.h>

DEFINE_GUID(CLSID_CANCEL_AUTOPLAY, 0x66a32fe6, 0x229d, 0x427b, 0xa6, 0x8, 0xd2, 0x73, 0xf4, 0xc, 0x3, 0x4c);

//////////////////////////////////////////////////////////////////////
//
// DeviceManager
//
//////////////////////////////////////////////////////////////////////
class DeviceManager : public CWinThread
{
	DECLARE_DYNCREATE(DeviceManager)

public:
    //! \brief Device event types determined in DeviceManager::OnMsgDeviceEvent(WPARAM eventType, LPARAM msg).
	typedef enum DevChangeEvent 
						{UNKNOWN_EVT = 100, EVENT_KILL,
                         HUB_ARRIVAL_EVT, HUB_REMOVAL_EVT,
						 DEVICE_ARRIVAL_EVT, DEVICE_REMOVAL_EVT, 
						 VOLUME_ARRIVAL_EVT, VOLUME_REMOVAL_EVT,
                         WMDM_DEVICE_ARRIVAL_EVT, WMDM_DEVICE_REMOVAL_EVT,
                         WMDM_MEDIA_ARRIVAL_EVT, WMDM_MEDIA_REMOVAL_EVT};

    DeviceManager();
    ~DeviceManager();
	//! \brief Start the message thread.
	//! \return Status.
    //! \retval ERROR_SUCCESS               No error occurred.
    //! \retval GetLastError() result       Error has occurred.
    //! \pre Can not have been called previously.
    //! \post Must call Close() to release resources.
    //! \note Runs in the context of the Client thread.
    //! \note Function holds Client thread until the DeviceManager thread has started.
    unsigned int Open();
	//! \brief Gracefully stop the thread.
    //! \note Runs in the context of the Client thread.
	void Close();
	//! \brief Calls CQueryCancelAutoplay::SetCancelAutoPlay(bool rejectAutoPlay, LPCTSTR driveList)
    //! \note Runs in the context of the Client thread.
	HRESULT SetCancelAutoPlay(bool rejectAutoPlay, LPCTSTR driveList = NULL) 
	{ 
		return _ICancelAutoPlayCallbackObject.SetCancelAutoPlay(rejectAutoPlay, driveList);
	};

	CStHidDevice* FindHidDevice(USHORT vid, USHORT pid, int timeout);
	bool WaitForChange(DevChangeEvent eventType, int timeout);
	CString GetEventString(DevChangeEvent eventType);

private:
    //! \brief The purpose of this hidden window is just to pass WM_DEVICECHANGE messages to the DeviceManager thread.
    //! \see DeviceManager::OnMsgDeviceEvent(WPARAM eventType, LPARAM msg)
	class DevChangeWnd : public CWnd
    {
    public:
        afx_msg BOOL OnDeviceChange(UINT nEventType,DWORD_PTR dwData);
		CString DrivesFromMask(ULONG UnitMask);
		DECLARE_MESSAGE_MAP()
    } _DevChangeWnd;
    //! \brief Use this message in the DevChangeWnd::OnDeviceChange() to post messages to the DeviceManager thread.
    //! \see DeviceManager::Close()
    //! \see DeviceManager::OnMsgDeviceEvent(WPARAM eventType, LPARAM msg)
    const static unsigned int WM_MSG_DEV_EVENT = WM_USER+36;
    //! \brief Processes WM_MSG_DEV_EVENT messages for the DeviceManager thread.
    //! \see DeviceManager::Close()
    //! \see DeviceManager::DevChangeWnd::OnDeviceChange(UINT nEventType,DWORD_PTR dwData)
	virtual afx_msg void OnMsgDeviceEvent(WPARAM eventType, LPARAM desc);
	//! \brief Event handle used to notify Client thread about a change in the DeviceManager thread.
    //! \see DeviceManager::OnMsgDeviceEvent()
    //! \see DeviceManager::FindDevice()
    HANDLE _hChangeEvent;

	// All this is private since Singleton holder is the only one who can create, copy or assign
    // a DeviceManager class.
	DeviceManager(const DeviceManager&);
	DeviceManager& operator=(const DeviceManager&);
    
    virtual BOOL InitInstance();
    virtual int ExitInstance();

	//! \brief Used by ~DeviceManager() to tell if DeviceManager::ExitInstance() was called.
    //! \see DeviceManager::Open()
    //! \see DeviceManager::Close()
	bool _bStopped;
	//! \brief Used by Open() to syncronize DeviceManager thread start.
    //! \see DeviceManager::Open()
    HANDLE _hStartEvent;
	DevChangeEvent _lastEventType;
	CString _eventString;

	// Message Support
	HDEVNOTIFY _hUsbDev;
	HDEVNOTIFY _hUsbHub;

	// AutoPlay support
	class CQueryCancelAutoplay : public IQueryCancelAutoPlay
	{
	public:
		// IUnknown interface
		STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
		STDMETHODIMP_(ULONG) AddRef();
		STDMETHODIMP_(ULONG) Release();
		// IQueryCancelAutoPlay interface
		STDMETHODIMP AllowAutoPlay(LPCWSTR pszPath, DWORD dwContentType,
			LPCWSTR pszLabel, DWORD dwSerialNumber);
		CQueryCancelAutoplay() 
			: _cRef(1)
			, _defaultDriveList(_T("DEFGHIJKLMNOPQRSTUVWXYZ"))
			, _driveList(_T("DEFGHIJKLMNOPQRSTUVWXYZ"))
			, _ROT(0)
			, _isRegistered(0)
		{};
		~CQueryCancelAutoplay() { };
		HRESULT SetCancelAutoPlay(bool rejectAutoPlay, LPCTSTR driveList);
	private:
		HRESULT Unregister();
		ULONG _cRef;
		CString _driveList;
		DWORD _ROT;
		bool _isRegistered;
		const CString _defaultDriveList;
	} _ICancelAutoPlayCallbackObject;

	DECLARE_MESSAGE_MAP()
};
