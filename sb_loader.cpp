// sb_loader.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "resource.h"
#include "Common/StdString.h"
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
#include <conio.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//#define PRE_DEF
//#define INIT_MODE
//#define TRAN_MODE
//#define EXEC_MODE
//MX508
#define MX508_USB_VID 0x15A2
#define MX508_USB_PID 0x0052
//MX23
#define MX23_USB_VID 0x066f
#define MX23_USB_PID 0x3780
//MX28
#define MX28_USB_VID 0x15A2
#define MX28_USB_PID 0x004f
// The one and only application object

CWinApp theApp;
DeviceManager* g_pDeviceManager = NULL;
CStHidDevice* g_pHidDevice = NULL;
MxHidDevice* g_pMxHidDevice = NULL;
// Global Options
//CString	g_IniFilename = _T("sb_loader.ini");
//CString	g_LogFilename = _T("sb_loader.log");
//FILE *	g_pLogHndl = NULL;

// These values are shared between UI and Packet Driver
//unsigned int g_uDiscardedBytes      =0; // bytes tossed
//unsigned int g_uPacketsFromTarget   =0; // packets read from target
//unsigned int g_uPacketsToTarget     =0; // packets sent to target
//unsigned int g_uRouteDiscard        =0; // bool (do we route discarded)
//unsigned int g_DiscardPort          =0; // port associated with stream 0

using namespace std;

/*int ReadFile(CString fwFilename, UCHAR* DataBuf)
{
	//
		// Get data
		//
		if ( fwFile.Open(fwFilename, CFile::modeRead | CFile::shareDenyWrite) == 0 )
		{
			TRACE(__FUNCTION__ " firmware file failed to open.\n");
			_tprintf(_T("%sFirmware file %s failed to open.errcode is %d\n"), indent, fwFilename,GetLastError());
			return FALSE;
		}

		UINT fwSize = fwFile.GetLength();
		DataBuf = (UCHAR*)malloc((size_t)fwSize);
		fwFile.Read(DataBuf, (UINT)fwSize);
		fwFile.Close();

        return fwSize;
}

bool closefile(UCHAR* DataBuf)
{
    free(DataBuf);
}*/

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
//	_CrtSetBreakAlloc (161);

	CString fwFilename = _T("");
	ExtendedFunction function = None;
	CString indent = _T("");
	int nRetCode = 0;
	CFile fwFile;
	UCHAR* DataBuf = NULL;
	ULONGLONG fwSize = 0;
	DeviceType DevType = NoDev;
    MxHidDevice::MxFunc MxFunc;

    ZeroMemory(&MxFunc, sizeof(MxFunc));

	int vid=0,pid=0;
#ifdef PRE_DEF
	argv[0] = _T("sb_loader");
	argv[1] = _T("-imx508");
#endif

#ifdef INIT_MODE
	argc = 4;
    argv[2] = _T("-init");
    argv[3] = _T("MDDR");
#endif
#ifdef TRAN_MODE    
    argc = 6;
    argv[2] = _T("-trans");
    argv[3] = _T("0x70040000");
    argv[4] = _T("/f");
    argv[5] = _T("eboot.nb0");
#endif
#ifdef EXEC_MODE
    argc = 4;
    argv[2] = _T("-exec");
    argv[3] = _T("0x70041800");
#endif
    //argv[2] = _T("u-boot.bin");
    //argv[6] = _T("0xF8006000");//IRAM address
    //argv[6] = _T("0x77800000");
    //argv[7] = _T("0x00");
    //argv[8] = _T("LPDDR2");

	/*argv[2] = _T("eboot.nb0");
    argv[6] = _T("0x70040000");
    argv[7] = _T("0x00");*/

    /*argv[2] = _T("xldr.nb0");
    argv[6] = _T("0xF8008000");
    argv[7] = _T("0x400");*/


	// initialize MFC and print and error on failure
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		// TODO: change error code to suit your needs
		_tprintf(_T("Fatal Error: MFC initialization failed\n"));
		nRetCode = 1;
	}
	else
	{
		//
		// Process command line arguements
		//
		if ( ProcessCommandLine(argc, argv, fwFilename, function, &DevType, &MxFunc) == false )
		{
			nRetCode = 0;
			goto ExitMain;
		}
		
		//
		// Get data
		//
        if(fwFilename.CompareNoCase(_T("")) != 0)
        {
		    if ( fwFile.Open(fwFilename, CFile::modeRead | CFile::shareDenyWrite) == 0 )
		    {
			    TRACE(__FUNCTION__ " firmware file failed to open.\n");
			    _tprintf(_T("%sFirmware file %s failed to open.errcode is %d\n"), indent, fwFilename,GetLastError());
			    nRetCode = 2;
			    goto ExitMain;
		    }

		    fwSize = fwFile.GetLength();
		    DataBuf = (UCHAR*)malloc((size_t)fwSize);
		    fwFile.Read(DataBuf, (UINT)fwSize);
		    fwFile.Close();
        }

		
		//
		// Prepare the DeviceManager object for use. 
		// Note: We must call gDeviceManager::Instance().Close() before the app exits.
		g_pDeviceManager = new DeviceManager();
		VERIFY(g_pDeviceManager->Open() == ERROR_SUCCESS);

		switch(DevType)
		{
		case MX23:
			{
				vid = MX23_USB_VID;
				pid = MX23_USB_PID;
				nRetCode = StDownload(vid,pid,fwFilename,DataBuf,fwSize);
			}
			break;
		case MX28:
			{
				vid = MX28_USB_VID;
				pid = MX28_USB_PID;
				nRetCode = StDownload(vid,pid,fwFilename,DataBuf,fwSize);
			}
			break;
		case MX508:
			{
				vid = MX508_USB_VID;
				pid = MX508_USB_PID;
                switch(MxFunc.Task)
                {
                    case MxHidDevice::INIT:
                        nRetCode = MxInit(vid,pid,&MxFunc);
                        break;
                    case MxHidDevice::TRANS:
                        nRetCode = MxDownload(vid,pid,fwFilename,DataBuf,fwSize, &MxFunc);
                        break;
                    case MxHidDevice::EXEC:
                        nRetCode = MxExecute(vid,pid,&MxFunc);
                        break;
                    default:
                        break;
                }
			}
			break;
		}
		
	}

ExitMain:

	if ( g_pHidDevice )
		delete g_pHidDevice;

	if ( g_pMxHidDevice )
		delete g_pMxHidDevice;

	if ( g_pDeviceManager )
	{
		g_pDeviceManager->Close();
		delete g_pDeviceManager;
	}

	if ( DataBuf )
		free(DataBuf);

	return nRetCode;
}

int MxExecute(int vid, int pid, MxHidDevice::PMxFunc pMxFunc)
{
	CString indent = _T("");
	int nRetCode = 0;
	BOOL bRet = FALSE;	
	//
	// Find device in HID mode
	//
	g_pMxHidDevice = new MxHidDevice();
	_tprintf(_T("\n%sLooking for a MX HID device within 2 seconds...\n"),indent);
	bRet = g_pDeviceManager->FindHidDevice(g_pMxHidDevice ,vid,pid, 2);
	if ( !bRet )
	{
		TRACE(__FUNCTION__ " HID device not found.\n");
		_tprintf(_T("%s  HID device was not found.\n%s Please place the device in recovery mode and restart the program.\n"),indent, indent);
		nRetCode = 3;
		return nRetCode;

	} else {
		TRACE(__FUNCTION__ " HID device found.\n");
		_tprintf(_T("%s  Found %s.\n"), indent, g_pMxHidDevice->GetUsbDeviceId());
	}

    if(!g_pMxHidDevice->Execute(pMxFunc->MxTrans.PhyRAMAddr4KRL))
        return 4;

    TRACE(__FUNCTION__ " Jump to RAM successfully.\n");
    _tprintf(_T("%s\n********Jump to RAM successfully.**************\n"),indent);
	return nRetCode;	    
}

int MxInit(int vid, int pid, MxHidDevice::PMxFunc pMxFunc)
{
	CString indent = _T("");
	int nRetCode = 0;
	BOOL bRet = FALSE;	
	//
	// Find device in HID mode
	//
	g_pMxHidDevice = new MxHidDevice();
	_tprintf(_T("\n%sLooking for a MX HID device within 2 seconds...\n"),indent);
	bRet = g_pDeviceManager->FindHidDevice(g_pMxHidDevice ,vid,pid, 2);
	if ( !bRet )
	{
		TRACE(__FUNCTION__ " HID device not found.\n");
		_tprintf(_T("%s  HID device was not found.\n%s Please place the device in recovery mode and restart the program.\n"),indent, indent);
		nRetCode = 3;
		return nRetCode;

	} else {
		TRACE(__FUNCTION__ " HID device found.\n");
		_tprintf(_T("%s  Found %s.\n"), indent, g_pMxHidDevice->GetUsbDeviceId());
	}

    if(!g_pMxHidDevice->InitMemoryDevice(pMxFunc->MemType))
        return 4;

    TRACE(__FUNCTION__ " MX device initialized.\n");
    _tprintf(_T("%s\n********MX device initialized.************\n"),indent);
	return nRetCode;	
}

int MxDownload(int vid,int pid,CString fwFilename,UCHAR* DataBuf,ULONGLONG fwSize, MxHidDevice::PMxFunc pMxFunc)
{
	CString indent = _T("");
	int nRetCode = 0;
	BOOL bRet = FALSE;	
	//
	// Find device in HID mode
	//
	g_pMxHidDevice = new MxHidDevice();
	_tprintf(_T("\n%sLooking for a MX HID device within 2 seconds...\n"),indent);
	bRet = g_pDeviceManager->FindHidDevice(g_pMxHidDevice ,vid,pid, 2);
	if ( !bRet )
	{
		TRACE(__FUNCTION__ " HID device not found.\n");
		_tprintf(_T("%s  HID device was not found.\n%s Please place the device in recovery mode and restart the program.\n"),indent, indent);
		nRetCode = 3;
		return nRetCode;

	} else {
		TRACE(__FUNCTION__ " HID device found.\n");
		_tprintf(_T("%s  Found %s.\n"), indent, g_pMxHidDevice->GetUsbDeviceId());
	}

	//
	// Load firmware
	//
    if(pMxFunc->Task == MxHidDevice::TRANS)
	    _tprintf(_T("%sDownloading %s to device.\n"), indent, fwFilename);
	//bRet = g_pMxHidDevice->Download(DataBuf, fwSize, TRUE, 0x90041000, 0x400);
    //bRet = g_pMxHidDevice->Download(DataBuf, fwSize, TRUE, 0x90040000, 0x00);
    bRet = g_pMxHidDevice->Download(DataBuf, fwSize, pMxFunc);
	if ( !bRet)
	{
		TRACE(__FUNCTION__ " ERROR: During download.\n");
		_tprintf(_T("%s  Failed to download %s to the device.\n"),indent, fwFilename);
		nRetCode = 3;	
		return nRetCode;
	}
    else
    {
        _tprintf(_T("%s\n********Succeed to download %s to the device.********\n"),indent, fwFilename);
    }
	
	//g_pDeviceManager->WaitForChange(DeviceManager::DEVICE_REMOVAL_EVT, 10);

	return nRetCode;	
}

int StDownload(int vid,int pid,CString fwFilename,UCHAR* DataBuf,ULONGLONG fwSize)
{
	CString indent = _T("");
	int nRetCode = 0;
	BOOL bRet = FALSE;
	//
	// Find device in HID mode
	//
	g_pHidDevice = new CStHidDevice();
	_tprintf(_T("\n%sLooking for a STMP HID device within 2 seconds...\n"),indent);
	bRet = g_pDeviceManager->FindHidDevice(g_pHidDevice ,vid,pid, 10);
	if ( !bRet )
	{
		TRACE(__FUNCTION__ " HID device not found.\n");
		_tprintf(_T("%s  HID device was not found.\n%s Please place the device in recovery mode and restart the program.\n"),indent, indent);
		nRetCode = 3;
		return nRetCode;

	} else {
		TRACE(__FUNCTION__ " HID device found.\n");
		_tprintf(_T("%s  Found %s.\n"), indent, g_pHidDevice->GetUsbDeviceId());
	}

	//
	// Load firmware
	//
	TRACE(__FUNCTION__ " HID device found.\n");
	_tprintf(_T("%sDownloading %s to device.\n"), indent, fwFilename);
	int error = g_pHidDevice->Download(DataBuf, fwSize, _T("  "));
	if ( error != ERROR_SUCCESS )
	{
		TRACE(__FUNCTION__ " ERROR: During download.\n");
		_tprintf(_T("%s  Error(%d) during download.\n%sQuitting.\n"),indent, error, indent);
		nRetCode = error;	
		return nRetCode;
	}
	
	g_pDeviceManager->WaitForChange(DeviceManager::DEVICE_REMOVAL_EVT, 10);

	return nRetCode;	
}

UINT String2Uint(CString attr)
{
    UINT retVal;
    if(attr.Left(2) == _T("0x"))
    {
        TCHAR *p;
        retVal = _tcstoul(attr.Mid(2),&p,16);
    }
    else
    {
        retVal = _tstoi64(attr);
    }
    return retVal;
}

BOOL String2Bool(CString attr)
{
	if ( attr.CompareNoCase(_T("TRUE")) == 0 )
		return TRUE;
			
	return FALSE;
}

bool ProcessCommandLine(int argc, TCHAR* argv[], CString& fwFilename, ExtendedFunction& function, DeviceType* DevType, MxHidDevice::PMxFunc pMxFunc)
{
	bool ret = true;
	CString arg;
	
	for ( int i=0; i<argc; ++i )
	{
		arg = argv[i];

		if ( arg.CompareNoCase(_T("/tss")) == 0 )
		{
			function = CaptureTssFunction;
		}
		else if ( arg.CompareNoCase(_T("/f")) == 0 && i <= argc-1 )
		{
			fwFilename = argv[++i];
		}
		else if ( arg.Compare(_T("/?")) == 0 || arg.CompareNoCase(_T("/h")) == 0 )
		{
			PrintUsage();
			ret = false;
		}
		else if ( arg.CompareNoCase(_T("/v")) == 0 )
		{
			_tprintf(_T("version 0.2 build %s"), _T(__DATE__));
			ret = false;
		}
		else if(arg.CompareNoCase(_T("-imx23")) == 0)
		{
			*DevType = MX23;			
		}
		else if(arg.CompareNoCase(_T("-imx28")) == 0)
		{
			*DevType = MX28;
		}
		else if(arg.CompareNoCase(_T("-imx508")) == 0)
		{
			*DevType = MX508;
            arg = argv[++i];
		    if ( arg.CompareNoCase(_T("-init")) == 0 && i <= argc-1 )
		    {
                pMxFunc->Task = MxHidDevice::INIT;
                arg = argv[++i];
		        if(arg.CompareNoCase(_T("LPDDR2")) == 0)
		        {
                    pMxFunc->MemType = MxHidDevice::LPDDR2;
		        }
                else
		        {
                    pMxFunc->MemType = MxHidDevice::MDDR;
		        }
		    }
		    else if ( arg.CompareNoCase(_T("-trans")) == 0 && i <= argc-1 )
		    {
                pMxFunc->Task = MxHidDevice::TRANS;
                pMxFunc->MxTrans.PhyRAMAddr4KRL = String2Uint(argv[++i]);
            }
		    else if ( arg.CompareNoCase(_T("-exec")) == 0 && i <= argc-1 )
		    {
                pMxFunc->Task = MxHidDevice::EXEC;
                pMxFunc->MxTrans.PhyRAMAddr4KRL = String2Uint(argv[++i]);
            }
		}
	}

	return ret;
}

void PrintUsage()
{
	CString usage =
		_T("\nUsage: sb_loader.exe [/f <filename>] [/tss] [/? | /h]\n\n") \
		_T("\t/f <filename> - where <filename> is the file to download. Default: \"firmware.sb\"\n") \
		_T("\t/tss - captures and prints the TSS output after the file is downloaded.\n") \
		_T("\t/? or /h - displays this screen.\n\n");

	_tprintf(usage);
}

#define SIZE_OF_BUFFER 1024

int CaptureTss(CString indent)
{
	USES_CONVERSION_EX;

	int error = ERROR_SUCCESS;

	TRACE(__FUNCTION__ " Capturing TSS output.\n");
	_tprintf(_T("%sCapturing TSS output. Hit 'q' to exit.\n"), indent);
	indent += _T("  ");

	//
	// Find device in HID mode
	//
	g_pDeviceManager->FindHidDevice(g_pHidDevice,0x15a2, 0xff03, 10);
	if ( g_pHidDevice == NULL )
	{
		error = ERROR_BAD_UNIT;
		TRACE(__FUNCTION__ " Never found HID device.\n");
		_tprintf(_T("%sNever found HID device.\n%sQuitting\n\n"), indent, indent);
		return error;
	}
	TRACE(__FUNCTION__ " HID device found.\n");
	_tprintf(_T("%sFound %s.\n\n"), indent, g_pHidDevice->GetUsbDeviceId());

	//
	// Read the data.
	//
    UCHAR u8UsbBuffer[SIZE_OF_BUFFER+1];  // I increased the size from 32 to 1024 to see is I could avoid heap corruption
	UCHAR tempBuff[25];
	CString str;
	USHORT len = 0;
    
	while(1)
    {
        // USB Interface
        // Read USB Stream, we read one packet at a time
		error = g_pHidDevice->Read(u8UsbBuffer,sizeof(_TSS_PACKET));
        if( error == ERROR_SUCCESS ) {
			
			if(*(USHORT*)&u8UsbBuffer[3] & (1<<15))
			{
				len = ((*(USHORT*)&u8UsbBuffer[3] & 0x3fff)%24)+1;
			}
			else
			{
				len = 24;
			}
			memset(tempBuff, 0, 25);
			memcpy(tempBuff, &u8UsbBuffer[7], len);

			str = A2W_EX((LPCSTR)tempBuff, len+1);

			_tprintf(_T("%s"), str);

			if ( str.Find(_T("RPC Ready:")) != -1 )
				break;
        }

		if ( g_pDeviceManager->WaitForChange(DeviceManager::DEVICE_REMOVAL_EVT, 0) )
		{
			error = ERROR_INVALID_HANDLE;
			TRACE(__FUNCTION__ " Device disconnected while getting info.\n");
			_tprintf(_T("\n%sDevice disconnected while getting info.\n%sQuitting\n\n"), indent, indent);
			break;
		}

		// See if user pressed 'q'
		if ( _kbhit() )
		{
			TCHAR ch = _gettch();
			if ( ch == _T('q') || ch == _T('Q') )
			{
				_tprintf(_T("%s\n\nUser hit %c to exit.\n%sQuitting\n\n"), indent, ch, indent);
				break;
			}
		}

	}

	return error;
}