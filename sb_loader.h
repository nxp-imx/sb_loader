/*
 * File:	sb_loader.h
 *
 * Copyright (c) 2010 Freescale Semiconductor, Inc. All rights reserved.
 * See included license file for license details.
*/

#define SUCCESS	0

extern DeviceManager* g_pDeviceManager;

enum ExtendedFunction { None, CaptureTssFunction };

// Prototypes
int CaptureTss(CString indent);
bool ProcessCommandLine(int argc, TCHAR* argv[], CString& fwFilename, ExtendedFunction& function, DeviceType* DevType, MxHidDevice::PMxFunc pMxFunc);
void PrintUsage();
BOOL SearchDevice();
int StDownload(CString fwFilename,UCHAR* fwBytes,ULONGLONG fwSize);
int MxRun(CString fwFilename,UCHAR* DataBuf,ULONGLONG fwSize, MxHidDevice::PMxFunc pMxFunc);
