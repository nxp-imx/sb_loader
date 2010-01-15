// StRecoveryDev.cpp: implementation of the CStHidDevice class.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include <Assert.h>
#include <cfgmgr32.h>
#include "StHidDevice.h"

// (name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)
DEFINE_GUID( GUID_DEVINTERFACE_HID, 0x4D1E55B2L, 0xF16F, 0x11CF, \
            0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);

#define DEVICE_TIMEOUT			INFINITE // 5000 ms

HANDLE CStHidDevice::m_sync_event_tx = NULL;
HANDLE CStHidDevice::m_sync_event_rx = NULL;


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CStHidDevice::CStHidDevice()
{
    m_hid_drive_handle = INVALID_HANDLE_VALUE;
    m_sync_event_tx = NULL;
    m_sync_event_rx = NULL;
    m_device_name = L"";
    m_pReadReport = NULL;
    m_pWriteReport = NULL;
}

CStHidDevice::~CStHidDevice()
{
    Close();
    Trash();
}

int CStHidDevice::Trash()
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

int CStHidDevice::Close()
{
    if( m_hid_drive_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hid_drive_handle);
        m_hid_drive_handle = INVALID_HANDLE_VALUE;
    }
    FreeIoBuffers();

    return ERROR_SUCCESS;
}


void CStHidDevice::FreeIoBuffers()
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

int CStHidDevice::Open(LPCTSTR _p_device_name)
{
    m_hid_drive_handle = ::CreateFile(_p_device_name,
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
    return ERROR_SUCCESS;
}

int CStHidDevice::Initialize(USHORT vid, USHORT pid)
{
    m_vid = vid;
	m_pid = pid;

    CString sLinkName;
    DWORD dwDevInst = 0;
    int err = ERROR_SUCCESS;
    //Trash(); //make a clean start

    //
    // Find and open a handle to the hid driver object.
    //
    if( GetUsbDeviceFileName((LPGUID) &GUID_DEVINTERFACE_HID, sLinkName, dwDevInst, MAX_PATH) == false )
    {
        // Failed to find our device.  This can happen on initial startup.
        return -70      /*STERR_FAILED_TO_FIND_HID_DEVICE*/;
    }

    err = Open( sLinkName );
    if( err != ERROR_SUCCESS )
    {
        assert(false);
        return err;
    }
    m_device_name = sLinkName;

    // Get USB devnode from HID devnode.
    DWORD usbDevInst;
    err = CM_Get_Parent(&usbDevInst, dwDevInst, 0);
    if( err != ERROR_SUCCESS )
    {
        assert(false);
        return err;
    }

    // Get the DeviceID string from the USB device devnode.
    TCHAR buf[4096];
    err = CM_Get_Device_ID(usbDevInst, buf, sizeof(buf), 0);
    if( err != ERROR_SUCCESS )
    {
        assert(false);
        return err;
    }

    // Fixup name
    m_usb_device_id = buf;
    m_usb_device_id.Replace(_T('\\'), _T('#'));

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
    if ( err != ERROR_SUCCESS ) {
        assert(false);
        return err;
    }

    return ERROR_SUCCESS;
}

int CStHidDevice::Download(UCHAR* data, ULONGLONG size, CString indent)
{
	ULONG			usb_iterations;
	ULONG			usb_data_fraction;
	size_t			current_pos_in_arr = 0;
	UCHAR			*buf;
	int 			err = ERROR_SUCCESS;
	ULONG			ulWriteSize = m_Capabilities.OutputReportByteLength - 1;
	_ST_HID_CBW		cbw = {0};
	ULONGLONG		length = size;
	UINT			tag = 0;
//	BOOL			device_rebooted = FALSE;

	buf = (UCHAR *)malloc ( m_Capabilities.OutputReportByteLength );
	if ( !buf )
		return ERROR_OUTOFMEMORY;

	memset( &cbw, 0, sizeof( _ST_HID_CBW ) );
	cbw.Cdb.Command = BLTC_DOWNLOAD_FW;
	cbw.Cdb.Length = Swap4((UCHAR *)&length);

    memset( m_pWriteReport, 0xDB, m_Capabilities.OutputReportByteLength) ;
    m_pWriteReport->ReportId = HID_BLTC_REPORT_TYPE_COMMAND_OUT;
	cbw.Tag = ++tag;
	cbw.Signature = CBW_BLTC_SIGNATURE;
	cbw.XferLength = (UINT)length;
	cbw.Flags = CBW_HOST_TO_DEVICE_DIR;

	memcpy( &m_pWriteReport->Payload[0], &cbw, sizeof( _ST_HID_CBW ) );

	if( (err = Write( (UCHAR *)m_pWriteReport, m_Capabilities.OutputReportByteLength )) != ERROR_SUCCESS )
	{
		CancelIo( m_hid_drive_handle );
		free (buf);
		return err;
	}

	//
	// Write to the device, splitting in the right number of blocks
	//
	usb_iterations = (ULONG)(size / ulWriteSize);
	if( size % ulWriteSize )
	{
		usb_iterations ++;
	}

	usb_data_fraction = (ULONG)(size % ulWriteSize);
	if( !usb_data_fraction ) 
	{
		usb_data_fraction = ulWriteSize;
	}

//	_p_progress->SetCurrentTask( TASK_TYPE_NONE, (ULONG)usb_iterations );

	_tprintf(_T("%s"), indent);
	for( ULONG iteration=0; iteration<(usb_iterations-1); iteration++ )
	{
//		m_p_fw_component->GetData(current_pos_in_arr, ulWriteSize, buf);
		memcpy(buf, data + current_pos_in_arr, ulWriteSize);

		memset( m_pWriteReport, 0xDB, ulWriteSize);
	    m_pWriteReport->ReportId = HID_BLTC_REPORT_TYPE_DATA_OUT;
		memcpy(&m_pWriteReport->Payload[0], buf, ulWriteSize);

		if( (err = Write( (UCHAR *)m_pWriteReport, m_Capabilities.OutputReportByteLength )) != ERROR_SUCCESS )
		{
			CancelIo( m_hid_drive_handle );
			free (buf);
			return err;
		}
//		GetUpdater()->GetProgress()->UpdateProgress();
		current_pos_in_arr += ulWriteSize;
		if ( iteration % 30 == 0 )
			_tprintf(_T("."));
	}

	if( current_pos_in_arr + ulWriteSize > size)
		ulWriteSize = size - current_pos_in_arr;
	
	if( ulWriteSize >= m_Capabilities.OutputReportByteLength)
		ulWriteSize = m_Capabilities.OutputReportByteLength -1;

	memset( m_pWriteReport, 0xDB, ulWriteSize);

//	m_p_fw_component->GetData(current_pos_in_arr, ulWriteSize, buf);
	memcpy(buf, data + current_pos_in_arr, ulWriteSize);
    m_pWriteReport->ReportId = HID_BLTC_REPORT_TYPE_DATA_OUT;
	memcpy(&m_pWriteReport->Payload[0], buf, ulWriteSize);

	err = Write( (UCHAR *)m_pWriteReport, m_Capabilities.OutputReportByteLength );

	free (buf);

	_tprintf(_T("% Done.\n"));
	return err;
}

// Write to HID device
int CStHidDevice::Write(UCHAR* _buf, ULONG _size)
{
    DWORD status;

    // Preparation
    m_overlapped.Offset				= 0;
    m_overlapped.OffsetHigh			= 0;
    m_overlapped.hEvent				= (PVOID)(ULONGLONG)_size;

    ::ResetEvent( CStHidDevice::m_sync_event_tx );

    // Write to the device
    if( !::WriteFileEx( m_hid_drive_handle, _buf, _size, &m_overlapped, 
        CStHidDevice::WriteCompletionRoutine ) )
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

VOID CStHidDevice::WriteCompletionRoutine( 
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

    ::SetEvent( CStHidDevice::m_sync_event_tx );
}


// Read from HID device
int CStHidDevice::Read(void* _buf, UINT _size)
{
    DWORD status;

    // Preparation
    m_overlapped.Offset				= 0;
    m_overlapped.OffsetHigh			= 0;
    m_overlapped.hEvent				= (PVOID)(ULONGLONG)_size;

    ::ResetEvent( CStHidDevice::m_sync_event_rx );

    if( m_hid_drive_handle == INVALID_HANDLE_VALUE ) {
        return WAIT_TIMEOUT;
    }

    //  The read command does not sleep very well right now.
    Sleep(1); 

    // Read from device
    if( !::ReadFileEx( m_hid_drive_handle, _buf, _size, &m_overlapped, 
        CStHidDevice::ReadCompletionRoutine ) )
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

VOID CStHidDevice::ReadCompletionRoutine( 
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

HANDLE CStHidDevice::OpenOneDevice (
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

    // Is this our vid+pid?  If not, ignore it.
    if ( (devPath.MakeUpper().Find(_T("HID#VID_066F")) != -1) ||
		 (devPath.MakeUpper().Find(_T("HID#VID_15A2")) != -1) )
    {
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
        TRACE(__FUNCTION__ " VID-PID does not match\n");
    }

    free( functionClassDeviceData );
    return hOut;
}


HANDLE CStHidDevice::OpenUsbDevice( LPGUID  pGuid, CString& outNameBuf, DWORD& outDevInst, rsize_t bufsize)
/*++
Routine Description:

Do the required PnP things in order to find the next available proper device in the system at this time.

Arguments:
pGuid:      ptr to GUID registered by the driver itself
outNameBuf: the generated name for this device

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
                hOut = OpenOneDevice (hDevInfo, &deviceInterfaceData, outNameBuf, outDevInst, bufsize);
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




BOOL CStHidDevice::GetUsbDeviceFileName( LPGUID  pGuid, CString& outNameBuf, DWORD& outDevInst, rsize_t bufsize)
/*++
Routine Description:

Given a ptr to a driver-registered GUID, give us a string with the device name
that can be used in a CreateFile() call.
Actually briefly opens and closes the device and sets outBuf if successfull;
returns FALSE if not

Arguments:

pGuid:      ptr to GUID registered by the driver itself
outNameBuf: the generated zero-terminated name for this device

Return Value:

TRUE on success else FALSE

--*/
{
    HANDLE hDev = OpenUsbDevice( pGuid, outNameBuf, outDevInst, bufsize );
    if ( hDev == INVALID_HANDLE_VALUE )
    {
        return FALSE;
    }
    CloseHandle( hDev );
    return TRUE;
}

typedef UINT (CALLBACK* LPFNDLLFUNC1)(HANDLE, PVOID);
typedef UINT (CALLBACK* LPFNDLLFUNC2)(PVOID);

int CStHidDevice::AllocateIoBuffers()
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
        m_pReadReport = (_ST_HID_DATA_REPORT*)malloc( m_Capabilities.InputReportByteLength );
        if ( m_pReadReport == NULL )
            return ERROR_OUTOFMEMORY;
    }

    if ( m_Capabilities.OutputReportByteLength )
    {
        m_pWriteReport = (_ST_HID_DATA_REPORT*)malloc( m_Capabilities.OutputReportByteLength );
        if ( m_pWriteReport == NULL )
            return ERROR_OUTOFMEMORY;
    }

    return ERROR_SUCCESS;
}
