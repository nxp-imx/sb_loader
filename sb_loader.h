#pragma once

#include "resource.h"
#include "StHidDevice.h"
#include "DeviceManager.h"

#define SUCCESS	0

extern CStHidDevice* g_pHidDevice;
extern DeviceManager* g_pDeviceManager;

enum ExtendedFunction { None, CaptureTssFunction };

// Prototypes
int CaptureTss(CString indent);
bool ProcessCommandLine(int argc, TCHAR* argv[], CString& fwFilename, ExtendedFunction& function);
void PrintUsage();

