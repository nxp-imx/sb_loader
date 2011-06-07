/*
 * File:	HidDevice.h
 *
 * Copyright (c) 2010 Freescale Semiconductor, Inc. All rights reserved.
 * See included license file for license details.
*/

// Definition of the CHidDevice class.
//
//////////////////////////////////////////////////////////////////////
#pragma pack(1)

#define FSL_VID 0x15A2
#define OBS_VID 0x066f

//MX6Q
#define MX6Q_USB_PID 0x0054
//MX50
#define MX50_USB_PID 0x0052
//MX23
#define MX23_USB_PID 0x3780
//MX28
#define MX28_USB_PID 0x004f

enum DeviceType {MX23=0,MX28,MX50,MX6Q,NoDev};

struct _HID_DATA_REPORT
{
    UCHAR ReportId;
    UCHAR Payload[1];
};
#pragma pack()

class CHidDevice
{
public:
    CHidDevice();
    virtual ~CHidDevice();

    UINT const Swap4(const UCHAR *const p) const
    {
        return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3];
    }

    int FindKnownHidDevices();
    int Read(void* buf, UINT size);
    int Write(UCHAR* buf, ULONG size);
    CString GetUsbDeviceId() { return m_usb_device_id; }

	HANDLE OpenSpecifiedDevice (IN HDEVINFO HardwareDeviceInfo,
        IN PSP_INTERFACE_DEVICE_DATA DeviceInfoData,
        OUT CString& devName, OUT DWORD& devInst, rsize_t bufsize);
    HANDLE OpenUsbDevice( LPGUID  pGuid, CString& devpath, DWORD& outDevInst, rsize_t bufsize);

	int Download(UCHAR* data, ULONGLONG size, CString indent);

    HANDLE	        m_hid_drive_handle;

    OVERLAPPED	    m_overlapped;
    CString			m_device_name;
    CString			m_usb_device_id;
	USHORT          m_vid;
	USHORT          m_pid;
	DeviceType		m_DevType;
    static HANDLE	m_sync_event_tx;	
    static HANDLE	m_sync_event_rx;
    HIDP_CAPS		m_Capabilities;
    _HID_DATA_REPORT		*m_pReadReport;
    _HID_DATA_REPORT		*m_pWriteReport;

	DeviceType	GetDevType();

private:
    int Close();
	int Trash();
    int AllocateIoBuffers();
    void FreeIoBuffers();
    int SetUsbDeviceId(int dwDevInst);
	static VOID CALLBACK WriteCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,  
        LPOVERLAPPED lpOverlapped);       
    static VOID CALLBACK ReadCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,  
        LPOVERLAPPED lpOverlapped);
};

