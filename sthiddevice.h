// StRecoveryDev.h: interface for the CStHidDevice class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STHIDDEVICE_H__E57DC981_5DB4_4194_8912_CB96EB83F056__INCLUDED_)
#define AFX_STHIDDEVICE_H__E57DC981_5DB4_4194_8912_CB96EB83F056__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define DEVICE_READ_TIMEOUT   10

//#include "stdevice.h"

#include <basetyps.h>
#include <setupapi.h>
#include <initguid.h>

#pragma warning( push )
#pragma warning( disable : 4201 )

extern "C" {
    //#include "usb200.h"
    #include "hidsdi.h"
}
#pragma warning( pop )

#pragma pack(1)
//------------------------------------------------------------------------------
// HID Download Firmware CDB
//------------------------------------------------------------------------------
typedef struct _CDBHIDDOWNLOAD {
    UCHAR	Command;
    UINT	Length;
    UCHAR	Reserved[11];
} CDBHIDDOWNLOAD, *PCDBHIDDOWNLOAD;


//------------------------------------------------------------------------------
// HID Command Block Wrapper (CBW)
//------------------------------------------------------------------------------
struct _ST_HID_CBW
{
    UINT Signature;        // Signature: 0x43544C42, o "BLTC" (little endian) for the BLTC CBW
    UINT Tag;              // Tag: to be returned in the csw
    UINT XferLength;       // XferLength: number of bytes to transfer
    UCHAR Flags;            // Flags:
    //   Bit 7: direction - device shall ignore this bit if the
    //     XferLength field is zero, otherwise:
    //     0 = data-out from the host to the device,
    //     1 = data-in from the device to the host.
    //   Bits 6..0: reserved - shall be zero.
    UCHAR Reserved[2];       // Reserved - shall be zero.
    CDBHIDDOWNLOAD Cdb;        // cdb: the command descriptor block
};
// Signature value for _ST_HID_CBW
static const UINT CBW_BLTC_SIGNATURE = 0x43544C42; // "BLTC" (little endian)
static const UINT CBW_PITC_SIGNATURE = 0x43544950; // "PITC" (little endian)
// Flags values for _ST_HID_CBW
static const UCHAR CBW_DEVICE_TO_HOST_DIR = 0x80; // "Data Out"
static const UCHAR CBW_HOST_TO_DEVICE_DIR = 0x00; // "Data In"

//------------------------------------------------------------------------------
// HID Command Status Wrapper (CSW)
//------------------------------------------------------------------------------
struct _ST_HID_CSW
{
    UINT Signature;        // Signature: 0x53544C42, or "BLTS" (little endian) for the BLTS CSW
    UINT Tag;              // Tag: matches the value from the CBW
    UINT Residue;          // Residue: number of bytes not transferred
    UCHAR  Status;           // Status:
    //  00h command passed ("good status")
    //  01h command failed
    //  02h phase error
    //  03h to FFh reserved
};
// Signature value for _ST_HID_CSW
static const UINT CSW_BLTS_SIGNATURE = 0x53544C42; // "BLTS" (little endian)
static const UINT CSW_PITS_SIGNATURE = 0x53544950; // "PITS" (little endian)
// Status values for _ST_HID_CSW
static const UCHAR CSW_CMD_PASSED = 0x00;
static const UCHAR CSW_CMD_FAILED = 0x01;
static const UCHAR CSW_CMD_PHASE_ERROR = 0x02;

struct _TSS_PACKET
{
    UCHAR ReportId;
    UCHAR Payload[32];
};

struct _ST_HID_COMMMAND_REPORT
{
    UCHAR ReportId;
    _ST_HID_CBW Cbw;
};

struct _ST_HID_DATA_REPORT
{
    UCHAR ReportId;
    UCHAR Payload[32];
};

struct _ST_HID_STATUS_REPORT
{
    UCHAR ReportId;
    _ST_HID_CSW Csw;
};
#pragma pack()

#define BLTC_DOWNLOAD_FW					2
#define HID_BLTC_REPORT_TYPE_DATA_OUT		2
#define HID_BLTC_REPORT_TYPE_COMMAND_OUT	1

class CStFwComponent;

class CStHidDevice
{
public:
    CStHidDevice();
    virtual ~CStHidDevice();

    UINT const Swap4(const UCHAR *const p) const
    {
        return (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3];
    }

    int Initialize(USHORT vid, USHORT pid);
    int Open(LPCTSTR _p_device_name);
    int Close();
    int Read(void* buf, UINT size);
    int Write(UCHAR* buf, ULONG size);
	int Download(UCHAR* data, ULONGLONG size, CString indent = _T(""));

    CString GetUsbDeviceId() { return m_usb_device_id; }

    static VOID CALLBACK WriteCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,  
        LPOVERLAPPED lpOverlapped);       
    static VOID CALLBACK ReadCompletionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,  
        LPOVERLAPPED lpOverlapped);

    HANDLE	        m_hid_drive_handle;

private:
    int Trash();
    int AllocateIoBuffers();
    void FreeIoBuffers();

    OVERLAPPED	    m_overlapped;
    CString			m_device_name;
    CString			m_usb_device_id;
	USHORT          m_vid;
	USHORT          m_pid;

    static HANDLE	m_sync_event_tx;	
    static HANDLE	m_sync_event_rx;	

#pragma pack(1)

    struct _ST_HID_DATA_REPORT
    {
        UCHAR ReportId;
        UCHAR Payload[32];
    };
#pragma pack()

    HIDP_CAPS				m_Capabilities;
    _ST_HID_DATA_REPORT		*m_pReadReport;
    _ST_HID_DATA_REPORT		*m_pWriteReport;

    HANDLE OpenOneDevice (IN HDEVINFO HardwareDeviceInfo,
        IN PSP_INTERFACE_DEVICE_DATA DeviceInfoData,
        OUT CString& devName, OUT DWORD& devInst, rsize_t bufsize);
    HANDLE OpenUsbDevice( LPGUID  pGuid, CString& outNameBuf, DWORD& outDevInst, rsize_t bufsize);
    BOOL GetUsbDeviceFileName( LPGUID  pGuid, CString& outNameBuf, DWORD& outDevInst, rsize_t bufsize);
};

#endif // !defined(AFX_STHIDDEVICE_H__E57DC981_5DB4_4194_8912_CB96EB83F056__INCLUDED_)