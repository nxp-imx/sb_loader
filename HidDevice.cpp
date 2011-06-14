/*
 * File:	HidDevice.cpp
 *
 * Copyright (c) 2010 Freescale Semiconductor, Inc. All rights reserved.
 * See included license file for license details.
*/

// Implementation of the CHidDevice class.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include <Assert.h>
#include <cfgmgr32.h>
#include <basetyps.h>
#include <setupapi.h>
#include <initguid.h>
#pragma warning( push )
#pragma warning( disable : 4201 )

extern "C" {
    #include <hidsdi.h>
}
#pragma warning( pop )
#include "HidDevice.h"

// (name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)
DEFINE_GUID( GUID_DEVINTERFACE_HID, 0x4D1E55B2L, 0xF16F, 0x11CF, \
            0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);

#define DEVICE_TIMEOUT			INFINITE // 5000 ms
#define DEVICE_READ_TIMEOUT   10

HANDLE CHidDevice::m_sync_event_tx = NULL;
HANDLE CHidDevice::m_sync_event_rx = NULL;
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CHidDevice::CHidDevice()
{
    m_hid_drive_handle = INVALID_HANDLE_VALUE;
    m_sync_event_tx = NULL;
    m_sync_event_rx = NULL;
    m_device_name = L"";
    m_pReadReport = NULL;
    m_pWriteReport = NULL;
}

CHidDevice::~CHidDevice()
{
	Close();
	Trash();
}

DeviceType	CHidDevice::GetDevType()
{
	return m_DevType;
}

int CHidDevice::Trash()
{
    if( m_sync_event_tx != NULL )
    {
        CloseHandle(m_sync_event_tx);
        m_sync_event_tx = NULL;
    }
    if( m_sync_event_rx != NULL )
    {
        CloseHandle(m_sync_event_rx);
        m_sync_event_rx = NULL;
    }
    return ERROR_SUCCESS;
}

int CHidDevice::Close()
{
    if( m_hid_drive_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hid_drive_handle);
        m_hid_drive_handle = INVALID_HANDLE_VALUE;
    }
    FreeIoBuffers();

    return ERROR_SUCCESS;
}

void CHidDevice::FreeIoBuffers()
{
    if( m_pReadReport )
    {
        free( m_pReadReport );
        m_pReadReport = NULL;
    }

    if( m_pWriteReport )
    {
        free( m_pWriteReport );
        m_pWriteReport = NULL;
    }

}

HANDLE CHidDevice::OpenSpecifiedDevice (
        IN       HDEVINFO                    HardwareDeviceInfo,
        IN       PSP_INTERFACE_DEVICE_DATA   DeviceInterfaceData,
        OUT		 CString&					 devName,
        OUT      DWORD&						 devInst,
        rsize_t	 bufsize
        )
        /*++
        Routine Description:

        Given the HardwareDeviceInfo, representing a handle to the plug and
        play information, and deviceInfoData, representing a specific usb device,
        open that device and fill in all the relevant information in the given
        USB_DEVICE_DESCRIPTOR structure.

        Arguments:

        HardwareDeviceInfo:  handle to info obtained from Pnp mgr via SetupDiGetClassDevs()
        DeviceInfoData:      ptr to info obtained via SetupDiEnumInterfaceDevice()

        Return Value:

        return HANDLE if the open and initialization was successfull,
        else INVLAID_HANDLE_VALUE.

        --*/
{
    PSP_INTERFACE_DEVICE_DETAIL_DATA     functionClassDeviceData = NULL;
    ULONG                                predictedLength = 0;
    ULONG                                requiredLength = 0;
    HANDLE								 hOut = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA						 devInfoData;

    //
    // allocate a function class device data structure to receive the
    // goods about this particular device.
    //
    SetupDiGetInterfaceDeviceDetail (
        HardwareDeviceInfo,
        DeviceInterfaceData,
        NULL, // probing so no output buffer yet
        0, // probing so output buffer length of zero
        &requiredLength,
        NULL); // not interested in the specific dev-node


    predictedLength = requiredLength;
    // sizeof (SP_FNCLASS_DEVICE_DATA) + 512;

    functionClassDeviceData = (PSP_INTERFACE_DEVICE_DETAIL_DATA) malloc (predictedLength);
    functionClassDeviceData->cbSize = sizeof (SP_INTERFACE_DEVICE_DETAIL_DATA);
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    //
    // Retrieve the information from Plug and Play.
    //
    if (! SetupDiGetInterfaceDeviceDetail (
        HardwareDeviceInfo,
        DeviceInterfaceData,
        functionClassDeviceData,
        predictedLength,
        &requiredLength,
        &devInfoData)) 
    {
            free( functionClassDeviceData );
            return INVALID_HANDLE_VALUE;
    }

    //	wcscpy_s( devName, bufsize, functionClassDeviceData->DevicePath) ;
    CString devPath = functionClassDeviceData->DevicePath;

	CString filter;
	BOOL DeviceFound = FALSE;
	DWORD DevType;
	for(DevType = MX23; DevType < NoDev;  DevType += 1)
	{
		switch (DevType)
		{
			case MX23:
				filter.Format(_T("%s#vid_%04x&pid_%04x"), _T("HID"),OBS_VID, MX23_USB_PID);
				break;
			case MX28:
				filter.Format(_T("%s#vid_%04x&pid_%04x"), _T("HID"),FSL_VID, MX28_USB_PID);
				break;
			case MX50:
				filter.Format(_T("%s#vid_%04x&pid_%04x"), _T("HID"),FSL_VID, MX50_USB_PID);
				break;
			case MX6Q:
				filter.Format(_T("%s#vid_%04x&pid_%04x"), _T("HID"),FSL_VID, MX6Q_USB_PID);
				break;
			default:
				filter.Format(_T("%s#vid_%04x&pid_%04x"), _T("HID"),0xFFFF, 0xFFFF);
				break;
		}

		if(devPath.MakeUpper().Find(filter.MakeUpper()) != -1 )
		{
			DeviceFound = TRUE;
			break;
		}
	}

    // Is this our vid?  If not, ignore it.
	if ( DeviceFound)
    {
		m_DevType = (DeviceType)DevType;
        devName = devPath;
        devInst = devInfoData.DevInst;

        hOut = CreateFile (
            functionClassDeviceData->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, // no SECURITY_ATTRIBUTES structure
            OPEN_EXISTING, // No special create flags
            0, // No special attributes
            NULL); // No template file
    } else {
        TRACE(__FUNCTION__ " VID does not match\n");
    }

    free( functionClassDeviceData );
    return hOut;
}


HANDLE CHidDevice::OpenUsbDevice( LPGUID  pGuid, CString& devpath, DWORD& outDevInst, rsize_t bufsize)
/*++
Routine Description:

Do the required PnP things in order to find the next available proper device in the system at this time.

Arguments:
pGuid:      ptr to GUID registered by the driver itself
devpath: the generated name for this device

Return Value:

return HANDLE if the open and initialization was successful,
else INVALID_HANDLE_VALUE.
--*/
{
    ULONG                    NumberDevices;
    HANDLE                   hOut = INVALID_HANDLE_VALUE;
    HDEVINFO                 hDevInfo;
    SP_INTERFACE_DEVICE_DATA deviceInterfaceData;
    ULONG                    i;
    BOOLEAN                  done;

    NumberDevices = 0;

    //
    // Open a handle to the plug and play dev node.
    // SetupDiGetClassDevs() returns a device information set that contains info on all
    // installed devices of a specified class.
    //
    hDevInfo = SetupDiGetClassDevs (
        pGuid,
        NULL, // Define no enumerator (global)
        NULL, // Define no
        (DIGCF_PRESENT | // Only Devices present
        DIGCF_INTERFACEDEVICE)); // Function class devices.
    if (hDevInfo == INVALID_HANDLE_VALUE)
    {
        // Insert error handling here.
        return INVALID_HANDLE_VALUE;
    }
    //
    // Take a wild guess at the number of devices we have;
    // Be prepared to realloc and retry if there are more than we guessed
    //
    NumberDevices = 4;
    done = FALSE;
    deviceInterfaceData.cbSize = sizeof (SP_INTERFACE_DEVICE_DATA);

    i=0;
    while (!done) {
        NumberDevices *= 2;  // keep increasing the number of devices until we reach the limit
        for (; i < NumberDevices; i++) {

            // SetupDiEnumDeviceInterfaces() returns information about device interfaces
            // exposed by one or more devices. Each call returns information about one interface;
            // the routine can be called repeatedly to get information about several interfaces
            // exposed by one or more devices.
            if ( SetupDiEnumDeviceInterfaces (
                hDevInfo,   // pointer to a device information set
                NULL,       // pointer to an SP_DEVINFO_DATA, We don't care about specific PDOs
                pGuid,      // pointer to a GUID
                i,          //zero-based index into the list of interfaces in the device information set
                &deviceInterfaceData)) // pointer to a caller-allocated buffer that contains a completed SP_DEVICE_INTERFACE_DATA structure
            {
                // open the device
                hOut = OpenSpecifiedDevice (hDevInfo, &deviceInterfaceData, devpath, outDevInst, bufsize);
                if ( hOut != INVALID_HANDLE_VALUE )
                {
                    done = TRUE;
                    TRACE(__FUNCTION__ " SetupDiEnumDeviceInterfaces PASS. index=%d\n",i);
                    break;
                } else {
                    TRACE(__FUNCTION__ " SetupDiEnumDeviceInterfaces FAILED to OPEN. index=%d\n",i);
                }
            }
            else
            {
                // EnumDeviceInterfaces error
                if (ERROR_NO_MORE_ITEMS == ::GetLastError())
                {
                    done = TRUE;
                    TRACE(__FUNCTION__ " SetupDiEnumDeviceInterfaces FAILED to find interface. index=%d\n",i);
                    break;
                } else {
                    TRACE(__FUNCTION__ " SetupDiEnumDeviceInterfaces FAILED for reason other then MAX_ITEMS. index=%d\n",i);
                }
            }
        }  // end-for
    }  // end-while

    // SetupDiDestroyDeviceInfoList() destroys a device information set and frees all associated memory.
    SetupDiDestroyDeviceInfoList (hDevInfo);
    return hOut;
}

int CHidDevice::SetUsbDeviceId(int dwDevInst)
{ 
    // Get USB devnode from HID devnode.
    DWORD usbDevInst;
    int err = CM_Get_Parent(&usbDevInst, dwDevInst, 0);
    if( err != ERROR_SUCCESS )
    {
        assert(false);
        return err;
    }

    // Get the DeviceID string from the USB device devnode.
    TCHAR buf[MAX_PATH];
    err = CM_Get_Device_ID(usbDevInst, buf, sizeof(buf), 0);
    if( err != ERROR_SUCCESS )
    {
        assert(false);
        return err;
    }

    // Fixup name
    m_usb_device_id = buf;
    m_usb_device_id.Replace(_T('\\'), _T('#'));
    return err;
}

typedef UINT (CALLBACK* LPFNDLLFUNC1)(HANDLE, PVOID);
typedef UINT (CALLBACK* LPFNDLLFUNC2)(PVOID);

int CHidDevice::AllocateIoBuffers()
{
    int error = ERROR_SUCCESS;
    HINSTANCE h_HidDll;
    LPFNDLLFUNC1 lpfnDllFunc1; 
    LPFNDLLFUNC2 lpfnDllFunc2; 

    h_HidDll = LoadLibrary(_T("hid.dll"));
    if (h_HidDll == NULL)
    {
        return ::GetLastError();
    }
    // Get the Capabilities including the max size of the report buffers
    PHIDP_PREPARSED_DATA  PreparsedData = NULL;

    lpfnDllFunc1 = (LPFNDLLFUNC1)GetProcAddress(h_HidDll, "HidD_GetPreparsedData");
    if (!lpfnDllFunc1)
    {
        // handle the error
        FreeLibrary(h_HidDll);       
        return ::GetLastError();
    }

    if ( !lpfnDllFunc1(m_hid_drive_handle, &PreparsedData) )
    {
        error = ::GetLastError();
        FreeLibrary(h_HidDll);       
        return error;
    }

    lpfnDllFunc1 = (LPFNDLLFUNC1)GetProcAddress(h_HidDll, "HidP_GetCaps");
    if (!lpfnDllFunc1)
    {
        // handle the error
        error = ::GetLastError();
    }
    else 
    {
        if( lpfnDllFunc1(PreparsedData, &m_Capabilities) != HIDP_STATUS_SUCCESS )
        {
            error = ::GetLastError();
        }
    }

    lpfnDllFunc2 = (LPFNDLLFUNC2)GetProcAddress(h_HidDll, "HidD_FreePreparsedData");
    if (!lpfnDllFunc2)
    {
        // handle the error
        FreeLibrary(h_HidDll);       
        return ::GetLastError();
    }

    lpfnDllFunc2(PreparsedData);

    FreeLibrary(h_HidDll);       

    // Allocate a Read and Write Report buffers
    FreeIoBuffers();

    if ( m_Capabilities.InputReportByteLength )
    {
        m_pReadReport = (_HID_DATA_REPORT*)malloc( m_Capabilities.InputReportByteLength );
        if ( m_pReadReport == NULL )
            return ERROR_OUTOFMEMORY;
    }

    if ( m_Capabilities.OutputReportByteLength )
    {
        m_pWriteReport = (_HID_DATA_REPORT*)malloc( m_Capabilities.OutputReportByteLength );
        if ( m_pWriteReport == NULL )
            return ERROR_OUTOFMEMORY;
    }

    return ERROR_SUCCESS;
}

int CHidDevice::FindKnownHidDevices()
{
    CString devpath;
    DWORD dwDevInst = 0;
    int err = ERROR_SUCCESS;

    //
    // Find and open a handle to the hid driver object.
    //
    if ( OpenUsbDevice((LPGUID) &GUID_DEVINTERFACE_HID, devpath, dwDevInst, MAX_PATH) == INVALID_HANDLE_VALUE )
    {
        // Failed to find our device.  This can happen on initial startup.
        return -70;
    }

    m_hid_drive_handle = ::CreateFile(devpath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL);

    if( m_hid_drive_handle == INVALID_HANDLE_VALUE )
    {
        return ::GetLastError();
    }

    m_device_name = devpath;

    SetUsbDeviceId(dwDevInst);

    // create TX and RX events
    m_sync_event_tx = ::CreateEvent( NULL, TRUE, FALSE, NULL );
    if( !m_sync_event_tx )
    {
        assert(false);
        return ::GetLastError();
    }
    m_sync_event_rx = ::CreateEvent( NULL, TRUE, FALSE, NULL );
    if( !m_sync_event_rx )
    {
        assert(false);
        return ::GetLastError();
    }

    memset(&m_Capabilities, 0, sizeof(m_Capabilities));

    err = AllocateIoBuffers();
	if ( err != ERROR_SUCCESS )
	{
		TRACE(__FUNCTION__ " ERROR: AllocateIoBuffers failed.\n");
		return err;
	}

    return ERROR_SUCCESS;
}


// Write to HID device
int CHidDevice::Write(UCHAR* _buf, ULONG _size)
{
    DWORD status;

    // Preparation
    m_overlapped.Offset				= 0;
    m_overlapped.OffsetHigh			= 0;
    m_overlapped.hEvent				= (PVOID)(ULONGLONG)_size;

    ::ResetEvent( CHidDevice::m_sync_event_tx );

    // Write to the device
    if( !::WriteFileEx( m_hid_drive_handle, _buf, _size, &m_overlapped, 
        CHidDevice::WriteCompletionRoutine ) )
    {
        return ::GetLastError();
    }

    // wait for completion
    if( (status = ::WaitForSingleObjectEx( m_sync_event_tx, INFINITE, TRUE )) == WAIT_TIMEOUT )
    {
        ::CancelIo( m_hid_drive_handle );
        return WAIT_TIMEOUT;
    }

    if( m_overlapped.Offset == 0 )
        return -13 /*STERR_FAILED_TO_WRITE_FILE_DATA*/;
    else
        return ERROR_SUCCESS;
}

VOID CHidDevice::WriteCompletionRoutine( 
    DWORD _err_code, 
    DWORD _bytes_transferred, 
    LPOVERLAPPED _lp_overlapped
    )
{
    if( ((ULONG)(ULONGLONG)_lp_overlapped->hEvent != _bytes_transferred) || _err_code )
    {
        *(BOOL *)&_lp_overlapped->Offset = 0;
    }
    else
    {
        *(BOOL *)&_lp_overlapped->Offset = _bytes_transferred;
    }

    ::SetEvent( CHidDevice::m_sync_event_tx );
}


// Read from HID device
int CHidDevice::Read(void* _buf, UINT _size)
{
    DWORD status;

    // Preparation
    m_overlapped.Offset				= 0;
    m_overlapped.OffsetHigh			= 0;
    m_overlapped.hEvent				= (PVOID)(ULONGLONG)_size;

    ::ResetEvent( CHidDevice::m_sync_event_rx );

    if( m_hid_drive_handle == INVALID_HANDLE_VALUE ) {
        return WAIT_TIMEOUT;
    }

    //  The read command does not sleep very well right now.
    Sleep(35); 

    // Read from device
    if( !::ReadFileEx( m_hid_drive_handle, _buf, _size, &m_overlapped, 
        CHidDevice::ReadCompletionRoutine ) )
    {
        return ::GetLastError();
    }

    // wait for completion
    if( (status = ::WaitForSingleObjectEx( m_sync_event_rx, DEVICE_READ_TIMEOUT, TRUE )) == WAIT_TIMEOUT )
    {
        ::CancelIo( m_hid_drive_handle );
        return WAIT_TIMEOUT;
    }

    if( m_overlapped.Offset == 0 )
        return -13 /*STERR_FAILED_TO_WRITE_FILE_DATA*/;
    else
        return ERROR_SUCCESS;
}

VOID CHidDevice::ReadCompletionRoutine( 
        DWORD _err_code, 
        DWORD _bytes_transferred, 
        LPOVERLAPPED _lp_overlapped
        )
{
    if( ((ULONG)(ULONGLONG)_lp_overlapped->hEvent != _bytes_transferred) || _err_code )
    {
        *(BOOL *)&_lp_overlapped->Offset = 0;
    }
    else
    {
        *(BOOL *)&_lp_overlapped->Offset = _bytes_transferred;
    }

    if( m_sync_event_rx != NULL) {
        ::SetEvent( m_sync_event_rx );
    }
}

int CHidDevice::Download(UCHAR* data, ULONGLONG size, CString indent)
{
	return ERROR_SUCCESS; 
}