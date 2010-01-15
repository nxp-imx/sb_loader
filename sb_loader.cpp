// sb_loader.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "sb_loader.h"
#include <conio.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// The one and only application object

CWinApp theApp;
CStHidDevice* g_pHidDevice = NULL;
DeviceManager* g_pDeviceManager = NULL;

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

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
//	_CrtSetBreakAlloc (161);

	CString fwFilename = _T("firmware.sb");
	ExtendedFunction function = None;

	int nRetCode = 0;
	CString indent = _T("");
	CFile fwFile;
	UCHAR* fwBytes = NULL;
	ULONGLONG fwSize = 0;

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
		if ( ProcessCommandLine(argc, argv, fwFilename, function) == false )
		{
			nRetCode = 0;
			goto ExitMain;
		}

		//
		// Get data
		//
		if ( fwFile.Open(fwFilename, CFile::modeRead | CFile::shareDenyWrite) == 0 )
		{
			TRACE(__FUNCTION__ " firmware file failed to open.\n");
			_tprintf(_T("%sFirmware file %s failed to open.\n"), indent, fwFilename);
			nRetCode = 2;
			goto ExitMain;
		}

		fwSize = fwFile.GetLength();
		fwBytes = (UCHAR*)malloc((size_t)fwSize);
		fwFile.Read(fwBytes, (UINT)fwSize);
		fwFile.Close();

		
		//
		// Prepare the DeviceManager object for use. 
		// Note: We must call gDeviceManager::Instance().Close() before the app exits.
		g_pDeviceManager = new DeviceManager();
		VERIFY(g_pDeviceManager->Open() == ERROR_SUCCESS);

		//
		// Find device in HID mode
		//
		_tprintf(_T("\n%sLooking for a STMP HID device for 10 seconds...\n"),indent);
		g_pHidDevice = g_pDeviceManager->FindHidDevice(0x066f, 0x3700, 10);
		if ( g_pHidDevice == NULL )
		{
			TRACE(__FUNCTION__ " HID device not found.\n");
			_tprintf(_T("%s  HID device was not found.\n%s Please place the device in recovery mode and restart the program.\n"),indent, indent);
			nRetCode = 3;
			goto ExitMain;

		} else {
			TRACE(__FUNCTION__ " HID device found.\n");
			_tprintf(_T("%s  Found %s.\n"), indent, g_pHidDevice->GetUsbDeviceId());
		}

		//
		// Load firmware.sb
		//
		TRACE(__FUNCTION__ " HID device found.\n");
		_tprintf(_T("%sDownloading %s to device.\n"), indent, fwFilename);
		int error = g_pHidDevice->Download(fwBytes, fwSize, _T("  "));
		if ( error != ERROR_SUCCESS )
		{
			TRACE(__FUNCTION__ " ERROR: During download.\n");
			_tprintf(_T("%s  Error(%d) during download.\n%sQuitting.\n"),indent, error, indent);
			nRetCode = error;
			goto ExitMain;
		}
		
		g_pDeviceManager->WaitForChange(DeviceManager::DEVICE_REMOVAL_EVT, 10);

		delete g_pHidDevice;
		g_pHidDevice = NULL;
		free(fwBytes);
		fwBytes = NULL;

		//
		// Get function
		//
		switch ( function )
		{
			case CaptureTssFunction:
				CaptureTss(_T(""));
				break;
			case None:
			default:
				break;
		}
		
	}

ExitMain:

	if ( g_pHidDevice )
		delete g_pHidDevice;

	if ( g_pDeviceManager )
	{
		g_pDeviceManager->Close();
		delete g_pDeviceManager;
	}

	if ( fwBytes )
		free(fwBytes);

	return nRetCode;
}

bool ProcessCommandLine(int argc, TCHAR* argv[], CString& fwFilename, ExtendedFunction& function)
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
		}else if ( arg.CompareNoCase(_T("/v")) == 0 )
		{
			_tprintf(_T("version 0.2 build %s"), _T(__DATE__));
			ret = false;
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
	g_pHidDevice = g_pDeviceManager->FindHidDevice(0x15a2, 0xff03, 10);
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