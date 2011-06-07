// sb_loader.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "resource.h"
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
#include "gitversion.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// The one and only application object

CWinApp theApp;
DeviceManager* g_pDeviceManager = NULL;
CStHidDevice* g_pHidDevice = NULL;
MxHidDevice* g_pMxHidDevice = NULL;

using namespace std;

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

		if(!SearchDevice())
			goto ExitMain;

		switch(g_pHidDevice->GetDevType())
		{
		case MX23:
		case MX28:
			{
				nRetCode = StDownload(fwFilename,DataBuf,fwSize);
			}
			break;
		case MX50:
		case MX6Q:
			{
                switch(MxFunc.Task)
                {
                    case MxHidDevice::INIT:
						if(!g_pMxHidDevice->InitMemoryDevice(MxFunc.MemType))
							nRetCode = 5;

						TRACE(__FUNCTION__ " MX device initialized.\n");
						_tprintf(_T("%sMX device initialized.\n"),indent);
                        break;
                    case MxHidDevice::TRANS:
						if ( !g_pMxHidDevice->Download(DataBuf, fwSize, &MxFunc))
						{
							TRACE(__FUNCTION__ " ERROR: During download.\n");
							_tprintf(_T("%s  Failed to download %s to the device.\n"),indent, fwFilename);
							nRetCode = 3;	
							return nRetCode;
						}
						else
						{
							_tprintf(_T("%sSucceeded to download %s to the device.\n"),indent, fwFilename);
						}
                        break;
                    case MxHidDevice::EXEC:
                        if(!g_pMxHidDevice->Execute(MxFunc.ImageParameter.PhyRAMAddr4KRL))
							nRetCode = 4;
	
						TRACE(__FUNCTION__ " Jump to RAM successfully.\n");
						_tprintf(_T("%sJump to RAM successfully.\n"),indent);
                        break;
                    case MxHidDevice::RUN_PLUGIN:
                    case MxHidDevice::RUN:
                        nRetCode = MxRun(fwFilename,DataBuf,fwSize, &MxFunc);
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

BOOL SearchDevice()
{
	BOOL bRet = FALSE;	
	//_tprintf(_T("\nLooking for a Freescale HID device within 2 seconds...\n"));

	g_pHidDevice = new CStHidDevice();
	bRet = g_pDeviceManager->FindHidDevice(g_pHidDevice ,2);
	
	if ( !bRet )
	{
		TRACE(__FUNCTION__ " HID device not found.\n");
		_tprintf(_T("HID device was not found.\n Please place the device in recovery mode and restart the program.\n"));
		return FALSE;
	}

	if((g_pHidDevice->GetDevType() == MX50) || 
	   (g_pHidDevice->GetDevType() == MX6Q))
	{
		g_pMxHidDevice = new MxHidDevice();
		g_pDeviceManager->FindHidDevice(g_pMxHidDevice, 2);
	}

	TRACE(__FUNCTION__ "Freescale HID device found.\n");
	//_tprintf(_T("Found %s.\n"), g_pMxHidDevice->GetUsbDeviceId());

	return TRUE;
}

int MxRun(CString fwFilename,UCHAR* DataBuf,ULONGLONG fwSize, MxHidDevice::PMxFunc pMxFunc)
{
	CString indent = _T("");
	int nRetCode = 0;
	BOOL bRet = FALSE;	

	//
	// Execute plugin
	//
    bRet = g_pMxHidDevice->RunPlugIn(DataBuf, fwSize, pMxFunc);
	if ( !bRet)
	{
		TRACE(__FUNCTION__ " ERROR: During RunPlugIn.\n");
		_tprintf(_T("%s  Failed to run plugin %s to the device.\n"),indent, fwFilename);
		nRetCode = 3;	
		return nRetCode;
	}
    else
    {
		TRACE(__FUNCTION__ " Executed plugin successfully.\n");
		_tprintf(_T("%s\nExecuted plugin successfully.\n"),indent);
    }
	//
	// Load firmware
	//
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
        _tprintf(_T("%sSucceed to download %s to the device.\n"),indent, fwFilename);
    }
	
	if(pMxFunc->Task != MxHidDevice::RUN_PLUGIN)
	{
		if(!g_pMxHidDevice->Execute(pMxFunc->ImageParameter.ExecutingAddr))
		{
			TRACE(__FUNCTION__ " ERROR: During RunPlugIn.\n");
			_tprintf(_T("%s  Failed to run plugin %s to the device.\n"),indent, fwFilename);
			nRetCode = 3;	
			return nRetCode;
		}

		TRACE(__FUNCTION__ " Jump to RAM successfully.\n");
		_tprintf(_T("%sRun into the image successfully.\n"),indent);
	}

	return nRetCode;	
}


int StDownload(CString fwFilename,UCHAR* DataBuf,ULONGLONG fwSize)
{
	CString indent = _T("");
	int nRetCode = 0;
	BOOL bRet = FALSE;

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
        retVal = _tstoi(attr);
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

		if ( arg.CompareNoCase(_T("/tss")) == 0 || arg.CompareNoCase(_T("-tss")) == 0 )
		{
			function = CaptureTssFunction;
		}
		else if ( (arg.CompareNoCase(_T("/f")) == 0 || arg.CompareNoCase(_T("-f")) == 0) && i <= argc-1 )
		{
			fwFilename = argv[++i];
		}
		else if ( arg.Compare(_T("/?")) == 0 || arg.CompareNoCase(_T("/h")) == 0 ||arg.CompareNoCase(_T("-h")) == 0 )
		{
			PrintUsage();
			ret = false;
		}
		else if ( arg.CompareNoCase(_T("/v")) == 0 || arg.CompareNoCase(_T("-v")) == 0)
		{
			_tprintf(_T("Version: %s--%s\nBuild time: %s"),VERSION, _T(GIT_VERSION), _T(__DATE__));
			ret = false;
		}
	    else if ( arg.CompareNoCase(_T("-init")) == 0 && i <= argc-1 )
	    {
            pMxFunc->Task = MxHidDevice::INIT;
            arg = argv[++i];
	        if(arg.CompareNoCase(_T("LPDDR2_V3")) == 0)
	        {
                pMxFunc->MemType = MxHidDevice::LPDDR2_V3;
	        }
	    }
	    else if ( arg.CompareNoCase(_T("-trans")) == 0 && i <= argc-1 )
	    {
            pMxFunc->Task = MxHidDevice::TRANS;
            pMxFunc->ImageParameter.PhyRAMAddr4KRL = String2Uint(argv[++i]);
        }
	    else if ( arg.CompareNoCase(_T("-exec")) == 0 && i <= argc-1 )
	    {
            pMxFunc->Task = MxHidDevice::EXEC;
            pMxFunc->ImageParameter.PhyRAMAddr4KRL = String2Uint(argv[++i]);
        }
		else if ( arg.CompareNoCase(_T("-nojump")) == 0 && i <= argc-1 ) 
	    {
            pMxFunc->Task = MxHidDevice::RUN_PLUGIN;
        }
		else if(1 < argc)
	    {
            pMxFunc->Task = MxHidDevice::RUN;
        }
		else
		{
			PrintUsage();
			ret = false;
		}
	}

	return ret;
}

void PrintUsage()
{
	CString usage =
		_T("\nUsage: sb_loader.exe [/f <filename>] [/tss] [/? | /h]\n\n\n") \
		_T("\t-f or /f <filename> - where <filename> is the file to download. Default: \"firmware.sb\"\n\n") \
		_T("\t-tss or /tss - captures and prints the TSS output after the file is downloaded.\n\n") \
		_T("\t-h or /h - displays this screen.\n\n") \
		_T("\t-nojump - Load but don't execute the image in which plugin is contained to RAM. Only available for mx50.\n\n") \
		_T("\t-trans - Load the image to RAM, the target address must be followed. Only available for mx50.\n\n") \
		_T("\t-exec - Execute the image, the execution address must be followed. Only available for mx50.\n\n") \
		_T("\t-init - initialize RAM by using the settings defined in memoryinit.h. Only available for mx50.\n\n") \
		_T("\tbelow instance download and run an image:\n") \
		_T("\tsb_loader -f uboot.bin\n\n");

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
	g_pDeviceManager->FindHidDevice(g_pHidDevice,10);
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