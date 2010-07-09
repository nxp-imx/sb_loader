#define SUCCESS	0

extern DeviceManager* g_pDeviceManager;

enum ExtendedFunction { None, CaptureTssFunction };
enum DeviceType {MX23,MX28,MX508,NoDev};

// Prototypes
int CaptureTss(CString indent);
bool ProcessCommandLine(int argc, TCHAR* argv[], CString& fwFilename, ExtendedFunction& function, DeviceType* DevType, MxHidDevice::PMxFunc pMxFunc);
void PrintUsage();
int StDownload(int vid,int pid,CString fwFilename,UCHAR* fwBytes,ULONGLONG fwSize);
int MxInit(int vid, int pid, MxHidDevice::PMxFunc pMxFunc);
int MxDownload(int vid,int pid,CString fwFilename,UCHAR* fwBytes,ULONGLONG fwSize, MxHidDevice::PMxFunc pMxFunc);