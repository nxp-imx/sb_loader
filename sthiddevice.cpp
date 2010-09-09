// Sthiddevice.cpp: implementation of the CStHidDevice class.
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
    #include "hidsdi.h"
}
#pragma warning( pop )
#include "HidDevice.h"
#include "StHidDevice.h"

// (name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)
DEFINE_GUID( GUID_DEVINTERFACE_HID, 0x4D1E55B2L, 0xF16F, 0x11CF, \
            0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CStHidDevice::CStHidDevice()
{
}

CStHidDevice::~CStHidDevice()
{
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
		_tprintf(_T("%s  Error(%d) during Write cbw.\n%sQuitting.\n"),_T("CStHidDevice::Download()"), err, _T(""));
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
			_tprintf(_T("%s  Error(%d) during Write data %d.\n%sQuitting.\n"),_T("CStHidDevice::Download()"), err,iteration, _T(""));
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
		ulWriteSize = (ULONG)(size - current_pos_in_arr);
	
	if( ulWriteSize >= m_Capabilities.OutputReportByteLength)
		ulWriteSize = m_Capabilities.OutputReportByteLength -1;

	memset( m_pWriteReport, 0xDB, ulWriteSize);

//	m_p_fw_component->GetData(current_pos_in_arr, ulWriteSize, buf);
	memcpy(buf, data + current_pos_in_arr, ulWriteSize);
    m_pWriteReport->ReportId = HID_BLTC_REPORT_TYPE_DATA_OUT;
	memcpy(&m_pWriteReport->Payload[0], buf, ulWriteSize);

	err = Write( (UCHAR *)m_pWriteReport, m_Capabilities.OutputReportByteLength );

	free (buf);

	// Read status:CSW_REPORT
	Sleep(10);
	if(err = Read( (UCHAR *)m_pReadReport, m_Capabilities.InputReportByteLength ))
	{
		_tprintf(_T("%s  Error(%d) during read.\n%sQuitting.\n"),_T(" CStHidDevice::Download()"), err, _T(""));
		CancelIo( m_hid_drive_handle );
		return err;
	}

	_tprintf(_T("% Done.\n"));
	return err;
}