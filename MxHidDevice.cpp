#pragma once
#include "stdafx.h"
#include "Common/StdString.h"
#include "Common/StdInt.h"

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
#include "MxHidDevice.h"


MxHidDevice::MxHidDevice()
{
    //Initialize(MX508_USB_VID,MX508_USB_PID);
	_chipFamily = MX508;

    //InitMemoryDevice();
	//TRACE("************The new i.mx device is initialized**********\n");
	//TRACE("\n");
	return;
Exit:
	TRACE("Failed to initialize the new i.MX device!!!\n");
}

MxHidDevice::~MxHidDevice()
{
}

/// <summary>
//-------------------------------------------------------------------------------------		
// Function to 16 byte SDP command format, these 16 bytes will be sent by host to 
// device in SDP command field of report 1 data structure
//
// @return
//		a report packet to be sent.
//-------------------------------------------------------------------------------------
//
VOID MxHidDevice::PackSDPCmd(PSDPCmd pSDPCmd)
{
    memset((UCHAR *)m_pWriteReport, 0, m_Capabilities.OutputReportByteLength);
    m_pWriteReport->ReportId = (unsigned char)REPORT_ID_SDP_CMD;
    PLONG pTmpSDPCmd = (PLONG)(m_pWriteReport->Payload);

	pTmpSDPCmd[0] = (  ((pSDPCmd->address  & 0x00FF0000) << 8) 
		          | ((pSDPCmd->address  & 0xFF000000) >> 8) 
		          |  (pSDPCmd->command   & 0x0000FFFF) );

	pTmpSDPCmd[1] = (   (pSDPCmd->dataCount & 0xFF000000)
		          | ((pSDPCmd->format   & 0x000000FF) << 16)
		          | ((pSDPCmd->address  & 0x000000FF) <<  8)
		          | ((pSDPCmd->address  & 0x0000FF00) >>  8 ));

	pTmpSDPCmd[2] = (   (pSDPCmd->data     & 0xFF000000)
		          | ((pSDPCmd->dataCount & 0x000000FF) << 16)
		          |  (pSDPCmd->dataCount & 0x0000FF00)
		          | ((pSDPCmd->dataCount & 0x00FF0000) >> 16));

	pTmpSDPCmd[3] = (  ((0x00  & 0x000000FF) << 24)
		          | ((pSDPCmd->data     & 0x00FF0000) >> 16) 
		          |  (pSDPCmd->data     & 0x0000FF00)
		          | ((pSDPCmd->data     & 0x000000FF) << 16));   

}

BOOL MxHidDevice::GetCmdAck(UINT RequiredCmdAck)
{
    memset((UCHAR *)m_pReadReport, 0, m_Capabilities.InputReportByteLength);

    //Get Report3, Device to Host:
    //4 bytes HAB mode indicating Production/Development part
	if ( Read( (UCHAR *)m_pReadReport, m_Capabilities.InputReportByteLength )  != ERROR_SUCCESS)
	{
		return FALSE;
	}
	if ( (*(unsigned int *)(m_pReadReport->Payload) != HabEnabled)  && 
		 (*(unsigned int *)(m_pReadReport->Payload) != HabDisabled) ) 
	{
		return FALSE;	
	}

    memset((UCHAR *)m_pReadReport, 0, m_Capabilities.InputReportByteLength);

    //Get Report4, Device to Host:
	if ( Read( (UCHAR *)m_pReadReport, m_Capabilities.InputReportByteLength ) != ERROR_SUCCESS)
	{
		return FALSE;
	}

	if (*(unsigned int *)(m_pReadReport->Payload) != RequiredCmdAck)
	{
		TRACE("WriteReg(): Invalid write ack: 0x%x\n", ((PULONG)m_pReadReport)[0]);
		return FALSE; 
	}

    return TRUE;
}

BOOL MxHidDevice::WriteReg(PSDPCmd pSDPCmd)
{
    //First, pack the command to a report.
    PackSDPCmd(pSDPCmd);

	//Send the report to USB HID device
	if ( Write((unsigned char *)m_pWriteReport, m_Capabilities.OutputReportByteLength) != ERROR_SUCCESS)
	{
		return FALSE;
	}

	if ( !GetCmdAck(ROM_WRITE_ACK) )
	{
		return FALSE;
	}
    
    return TRUE;
}

//Actually, this is the setting for i.mx51 since we simulate mx508 HID ROM on mx51.
typedef struct 
{
    UINT addr;
    UINT data;
    UINT format;
} stMemoryInit;

/*static stMemoryInit Mx508MDDR[] = 
{
    {0x73fa88a0, 0x00000020, 32},
    {0x73fa850c, 0x000020c5, 32},
    {0x73fa8510, 0x000020c5, 32},
    {0x73fa883c, 0x00000002, 32},
    {0x73fa8848, 0x00000002, 32},
    {0x73fa84b8, 0x000000e7, 32},
    {0x73fa84bc, 0x00000045, 32},
    {0x73fa84c0, 0x00000045, 32},
    {0x73fa84c4, 0x00000045, 32},
    {0x73fa84c8, 0x00000045, 32},
    {0x73fa8820, 0x00000000, 32},
    {0x73fa84a4, 0x00000003, 32},
    {0x73fa84a8, 0x00000003, 32},
    {0x73fa84ac, 0x000000e3, 32},
    {0x73fa84b0, 0x000000e3, 32},
    {0x73fa84b4, 0x000000e3, 32},
    {0x73fa84cc, 0x000000e3, 32},
    {0x73fa84d0, 0x000000e2, 32},
    {0x83fd9000, 0x82a20000, 32},
    {0x83fd9008, 0x82a20000, 32},
    {0x83fd9010, 0x000ad0d0, 32},
    {0x83fd9004, 0x333574aa, 32},
    {0x83fd900c, 0x333574aa, 32},
    {0x83fd9014, 0x04008008, 32},
    {0x83fd9014, 0x0000801a, 32},
    {0x83fd9014, 0x0000801b, 32},
    {0x83fd9014, 0x00448019, 32},
    {0x83fd9014, 0x07328018, 32},
    {0x83fd9014, 0x04008008, 32},
    {0x83fd9014, 0x00008010, 32},
    {0x83fd9014, 0x00008010, 32},
    {0x83fd9014, 0x06328018, 32},
    {0x83fd9014, 0x03808019, 32},
    {0x83fd9014, 0x00408019, 32},
    {0x83fd9014, 0x00008000, 32},
    {0x83fd9014, 0x0400800c, 32},
    {0x83fd9014, 0x0000801e, 32},
    {0x83fd9014, 0x0000801f, 32},
    {0x83fd9014, 0x0000801d, 32},
    {0x83fd9014, 0x0732801c, 32},
    {0x83fd9014, 0x0400800c, 32},
    {0x83fd9014, 0x00008014, 32},
    {0x83fd9014, 0x00008014, 32},
    {0x83fd9014, 0x0632801c, 32},
    {0x83fd9014, 0x0380801d, 32},
    {0x83fd9014, 0x0040801d, 32},
    {0x83fd9014, 0x00008004, 32},
    {0x83fd9000, 0xb2a20000, 32},
    {0x83fd9008, 0xb2a20000, 32},
    {0x83fd9010, 0x000ad6d0, 32},
    {0x83fd9034, 0x90000000, 32},
    {0x83fd9014, 0x00000000, 32},
};*/
static stMemoryInit Mx508MDDR[] = 
{
    {0x53fd4068, 0xffffffff, 32},
    {0x53fd406c, 0xffffffff, 32},
    {0x53fd4070, 0xffffffff, 32},
    {0x53fd4074, 0xffffffff, 32},
    {0x53fd4078, 0xffffffff, 32},
    {0x53fd407c, 0xffffffff, 32},
    {0x53fd4080, 0xffffffff, 32},
    {0x53fd4084, 0xffffffff, 32},
    {0x53FD4098, 0x80000004, 32},
    {0x53fa86ac, 0x02000000, 32},
    {0x53fa866c, 0x00000000, 32},
    {0x53fa868c, 0x00000000, 32},
    {0x53fa86a4, 0x00200000, 32},
    {0x53fa8668, 0x00200000, 32},
    {0x53fa8698, 0x00200000, 32},
    {0x53fa86a0, 0x00200000, 32},
    {0x53fa86a8, 0x00200000, 32},
    {0x53fa86b4, 0x00200000, 32},
    {0x53fa8498, 0x00200000, 32},
    {0x53fa849c, 0x00200000, 32},
    {0x53fa84f0, 0x00200000, 32},
    {0x53fa8500, 0x00200000, 32},
    {0x53fa84c8, 0x00200000, 32},
    {0x53fa8528, 0x00200000, 32},
    {0x53fa84f4, 0x00200000, 32},
    {0x53fa84fc, 0x00200000, 32},
    {0x53fa84cc, 0x00200000, 32},
    {0x53fa8524, 0x00200000, 32},
    {0x53fa8670, 0x00000000, 32},
    {0x14000000, 0x00000100, 32},
    {0x14000008, 0x00009c40, 32},
    {0x14000014, 0x02000000, 32},
    {0x14000018, 0x01010706, 32},
    {0x1400001c, 0x080b0201, 32},
    {0x14000020, 0x02000303, 32},
    {0x14000024, 0x0136b002, 32},
    {0x14000028, 0x01000101, 32},
    {0x1400002c, 0x06030301, 32},
    {0x14000030, 0x00000000, 32},
    {0x14000034, 0x00000a02, 32},
    {0x14000038, 0x00000003, 32},
    {0x1400003c, 0x00001401, 32},
    {0x14000040, 0x0005030f, 32},
    {0x14000044, 0x00000200, 32},
    {0x14000048, 0x00180018, 32},
    {0x1400004c, 0x00010000, 32},  
    {0x1400005c, 0x01000000, 32},
    {0x14000060, 0x00000001, 32},
    {0x14000064, 0x00000000, 32},
    {0x14000068, 0x00320000, 32},
    {0x1400006c, 0x00000000, 32},
    {0x14000070, 0x00000000, 32},
    {0x14000074, 0x00320000, 32},
    {0x14000080, 0x02000000, 32},
    {0x14000084, 0x00000100, 32},
    {0x14000088, 0x02400040, 32},
    {0x1400008c, 0x01000000, 32},
    {0x14000090, 0x0a000100, 32},
    {0x14000094, 0x01011f1f, 32},
    {0x14000098, 0x01010101, 32},
    {0x1400009c, 0x00030101, 32},
    {0x140000a4, 0x00010000, 32},
    {0x140000ac, 0x0000ffff, 32},
    {0x140000c8, 0x02020101, 32},
    {0x140000cc, 0x00000000, 32},
    {0x140000d0, 0x01000202, 32},
    {0x140000d4, 0x02030302, 32},
    {0x140000d8, 0x00000001, 32},
    {0x140000dc, 0x0000ffff, 32},
    {0x140000e0, 0x0000ffff, 32},
    {0x140000e4, 0x02020000, 32},
    {0x140000e8, 0x02020202, 32},
    {0x140000ec, 0x00000202, 32},
    {0x140000f0, 0x01010064, 32},
    {0x140000f4, 0x01010101, 32},
    {0x140000f8, 0x00010101, 32},
    {0x140000fc, 0x00000064, 32},
    {0x14000104, 0x02000602, 32},
    {0x14000108, 0x06120000, 32},
    {0x1400010c, 0x06120612, 32},
    {0x14000110, 0x06120612, 32},
    {0x14000114, 0x01030612, 32},
    {0x14000118, 0x01010002, 32},
    {0x14000200, 0x00000000, 32},
    {0x14000204, 0x00000000, 32},
    {0x14000208, 0xf5002725, 32},
    {0x14000210, 0xf5002725, 32},
    {0x14000218, 0xf5002725, 32},
    {0x14000220, 0xf5002725, 32},
    {0x14000228, 0xf5002725, 32},
    {0x1400020c, 0x070002d0, 32},
    {0x14000214, 0x074002d0, 32},
    {0x1400021c, 0x074002d0, 32},
    {0x14000224, 0x074002d0, 32},
    {0x1400022c, 0x074002d0, 32},
    {0x14000230, 0x00000000, 32},
    {0x14000234, 0x00800006, 32},
    {0x14000238, 0x200e1014, 32},
    {0x14000240, 0x200e1014, 32},
    {0x14000248, 0x200e1014, 32},
    {0x14000250, 0x200e1014, 32},
    {0x14000258, 0x200e1014, 32},
    {0x1400023c, 0x000d9f01, 32},
    {0x14000244, 0x000d9f01, 32},
    {0x1400024c, 0x000d9f01, 32},
    {0x14000254, 0x000d9f01, 32},
    {0x1400025c, 0x000d9f01, 32},
    {0x14000000, 0x00000101, 32},
};

static stMemoryInit Mx508LPDDR2[] = 
{
    {0x53fd4068, 0xffffffff, 32},
    {0x53fd406c, 0xffffffff, 32},
    {0x53fd4070, 0xffffffff, 32},
    {0x53fd4074, 0xffffffff, 32},
    {0x53fd4078, 0xffffffff, 32},
    {0x53fd407c, 0xffffffff, 32},
    {0x53fd4080, 0xffffffff, 32},
    {0x53fd4084, 0xffffffff, 32},
    {0x53FD4098, 0x80000003, 32},
    {0x53fa86ac, 0x04000000, 32},
    {0x53fa86a4, 0x00380000, 32},
    {0x53fa8668, 0x00380000, 32},
    {0x53fa8698, 0x00380000, 32},
    {0x53fa86a0, 0x00380000, 32},
    {0x53fa86a8, 0x00380000, 32},
    {0x53fa86b4, 0x00380000, 32},
    {0x53fa8498, 0x00380000, 32},
    {0x53fa849c, 0x00380000, 32},
    {0x53fa84f0, 0x00380000, 32},
    {0x53fa8500, 0x00380000, 32},
    {0x53fa84c8, 0x00380000, 32},
    {0x53fa8528, 0x00380000, 32},
    {0x53fa84f4, 0x00380000, 32},
    {0x53fa84fc, 0x00380000, 32},
    {0x53fa84cc, 0x00380000, 32},
    {0x53fa8524, 0x00380000, 32},
    {0x14000000, 0x00000500, 32},
    {0x14000004, 0x00000000, 32},
    {0x14000008, 0x0000001b, 32},
    {0x1400000c, 0x0000d056, 32},
    {0x14000010, 0x0000010b, 32},
    {0x14000014, 0x00000a6b, 32},
    {0x14000018, 0x02020d0c, 32},
    {0x1400001c, 0x0c110302, 32},
    {0x14000020, 0x05020503, 32},
    {0x14000024, 0x00000105, 32},
    {0x14000028, 0x01000403, 32},
    {0x1400002c, 0x09040501, 32},
    {0x14000030, 0x02000000, 32},
    {0x14000034, 0x00000e02, 32},
    {0x14000038, 0x00000006, 32},
    {0x1400003c, 0x00002301, 32},
    {0x14000040, 0x00050408, 32},
    {0x14000044, 0x00000300, 32},
    {0x14000048, 0x00260026, 32},
    {0x1400004c, 0x00010000, 32},
    {0x1400005c, 0x02000000, 32},
    {0x14000060, 0x00000002, 32},
    {0x14000064, 0x00000000, 32},
    {0x14000068, 0x00000000, 32},
    {0x1400006c, 0x00040042, 32},
    {0x14000070, 0x00000001, 32},
    {0x14000074, 0x00000000, 32},
    {0x14000078, 0x00040042, 32},
    {0x1400007c, 0x00000001, 32},
    {0x14000080, 0x010b0000, 32},
    {0x14000084, 0x00000060, 32},
    {0x14000088, 0x02400018, 32},
    {0x1400008c, 0x01000e00, 32},
    {0x14000090, 0x0a010101, 32},
    {0x14000094, 0x01011f1f, 32},
    {0x14000098, 0x01010101, 32},
    {0x1400009c, 0x00030101, 32},
    {0x140000a0, 0x00010000, 32},
    {0x140000a4, 0x00010000, 32},
    {0x140000a8, 0x00000000, 32},
    {0x140000ac, 0x0000ffff, 32},
    {0x140000c8, 0x02020101, 32},
    {0x140000cc, 0x01000000, 32},
    {0x140000d0, 0x01000201, 32},
    {0x140000d4, 0x00000200, 32},
    {0x140000d8, 0x00000102, 32},
    {0x140000dc, 0x0000ffff, 32},
    {0x140000e0, 0x0000ffff, 32},
    {0x140000e4, 0x02020000, 32},
    {0x140000e8, 0x02020202, 32},
    {0x140000ec, 0x00000202, 32},
    {0x140000f0, 0x01010064, 32},
    {0x140000f4, 0x01010101, 32},
    {0x140000f8, 0x00010101, 32},
    {0x140000fc, 0x00000064, 32},
    {0x14000100, 0x00000000, 32},
    {0x14000104, 0x02000802, 32},
    {0x14000108, 0x04080000, 32},
    {0x1400010c, 0x04080408, 32},
    {0x14000110, 0x04080408, 32},
    {0x14000114, 0x03060408, 32},
    {0x14000118, 0x01010002, 32},
    {0x1400011c, 0x00000000, 32},
    {0x14000200, 0x00000000, 32},
    {0x14000204, 0x00000000, 32},
    {0x14000208, 0xf5003a27, 32},
    {0x1400020c, 0x074002e1, 32},
    {0x14000210, 0xf5003a27, 32},
    {0x14000214, 0x074002e1, 32},
    {0x14000218, 0xf5003a27, 32},
    {0x1400021c, 0x074002e1, 32},
    {0x14000220, 0xf5003a27, 32},
    {0x14000224, 0x074002e1, 32},
    {0x14000228, 0xf5003a27, 32},
    {0x1400022c, 0x074002e1, 32},
    {0x14000230, 0x00000000, 32},
    {0x14000234, 0x00810006, 32},
    {0x14000238, 0x20099414, 32},
    {0x1400023c, 0x000a1401, 32},
    {0x14000240, 0x20099414, 32},
    {0x14000244, 0x000a1401, 32},
    {0x14000248, 0x20099414, 32},
    {0x1400024c, 0x000a1401, 32},
    {0x14000250, 0x20099414, 32},
    {0x14000254, 0x000a1401, 32},
    {0x14000258, 0x20099414, 32},
    {0x1400025c, 0x000a1401, 32},
    {0x14000000, 0x00000501, 32},
};

BOOL MxHidDevice::InitMemoryDevice(MemoryType MemType)
{
    SDPCmd SDPCmd;

    SDPCmd.command = ROM_KERNEL_CMD_WR_MEM;
    SDPCmd.dataCount = 4;

    /*for(int i=0; i<sizeof(Mx508MDDR)/sizeof(stMemoryInit); i++)
    {
        SDPCmd.format = Mx508MDDR[i].format;
        SDPCmd.data = Mx508MDDR[i].data;
        SDPCmd.address = Mx508MDDR[i].addr;
        if ( !WriteReg(&SDPCmd) )
        {
            TRACE("In InitMemoryDevice(): write memory failed\n");
            return FALSE;
        }
    }*/
    stMemoryInit * pMemPara = (MemType == LPDDR2) ? Mx508LPDDR2 : Mx508MDDR;
    UINT NumMemPara = (MemType == LPDDR2) ? sizeof(Mx508LPDDR2) : sizeof(Mx508MDDR);

    for(int i=0; i<NumMemPara/sizeof(stMemoryInit); i++)
    {
        SDPCmd.format = pMemPara[i].format;
        SDPCmd.data = pMemPara[i].data;
        SDPCmd.address = pMemPara[i].addr;
        if ( !WriteReg(&SDPCmd) )
        {
            TRACE("In InitMemoryDevice(): write memory failed\n");
            return FALSE;
        }
        _tprintf(_T("Reg #%d is initialized.\n"), i);
    }

	return TRUE;
}

// Write the register of i.MX

#define FLASH_HEADER_SIZE	0x20
#define ROM_TRANSFER_SIZE	0x400
#define DCD_BARKER_CODE     0xB17219E9
BOOL MxHidDevice::Download(UCHAR* pBuffer, ULONGLONG dataCount, PMxFunc pMxFunc)
{
    //return FALSE;
    MemorySection loadSection = MemSectionOTH;
    MemorySection setSection = MemSectionAPP;

    //if(pMxFunc->Task == TRANS)

	    DWORD byteIndex, numBytesToWrite = 0;
	    for ( byteIndex = 0; byteIndex < dataCount; byteIndex += numBytesToWrite )
	    {
		    // Get some data
		    numBytesToWrite = min(MAX_SIZE_PER_DOWNLOAD_COMMAND, dataCount - byteIndex);

		    if (!TransData(pMxFunc->MxTrans.PhyRAMAddr4KRL + byteIndex, numBytesToWrite, pBuffer + byteIndex))
		    {
			    TRACE(_T("DownloadImage(): TransData(0x%X, 0x%X,0x%X) failed.\n"), \
                    pMxFunc->MxTrans.PhyRAMAddr4KRL + byteIndex, numBytesToWrite, pBuffer + byteIndex);
			    return FALSE;
		    }
	    }
        return TRUE;
}

BOOL MxHidDevice::Execute(UINT32 ImageStartAddr)
{
	int FlashHdrAddr;

	//transfer length of ROM_TRANSFER_SIZE is a must to ROM code.
	unsigned char FlashHdr[ROM_TRANSFER_SIZE] = { 0 };
	unsigned char Tempbuf[ROM_TRANSFER_SIZE] = { 0 };	

	PIvtHeader pIvtHeader = (PIvtHeader)FlashHdr;

	FlashHdrAddr = ImageStartAddr - sizeof(IvtHeader);
    //FlashHdrAddr = 0xF8006000;//IRAM

    //Read the data first
	if ( !ReadData(FlashHdrAddr, ROM_TRANSFER_SIZE, FlashHdr) )
	{
		TRACE(_T("DownloadImage(): TransData(0x%X, 0x%X, 0x%X) failed.\n"), \
            FlashHdrAddr, ROM_TRANSFER_SIZE, FlashHdr);
		return FALSE;
	}
	//Add IVT header to the image.
    //Clean the IVT header region
    ZeroMemory(FlashHdr, sizeof(IvtHeader));
    
    //Fill IVT header parameter
	pIvtHeader->IvtBarker = IVT_BARKER_HEADER;
	pIvtHeader->ImageStartAddr = ImageStartAddr;
	pIvtHeader->SelfAddr = FlashHdrAddr;
    
    //Send the IVT header to destiny address
	if ( !TransData(FlashHdrAddr, ROM_TRANSFER_SIZE, FlashHdr) )
	{
		TRACE(_T("DownloadImage(): TransData(0x%X, 0x%X, 0x%X) failed.\n"), \
            FlashHdrAddr, ROM_TRANSFER_SIZE, FlashHdr);
		return FALSE;
	}
    
    //Verify the data
	if ( !ReadData(FlashHdrAddr, ROM_TRANSFER_SIZE, Tempbuf) )
	{
		TRACE(_T("DownloadImage(): TransData(0x%X, 0x%X, 0x%X) failed.\n"), \
            FlashHdrAddr, ROM_TRANSFER_SIZE, FlashHdr);
		return FALSE;
	}

    if(memcmp(FlashHdr, Tempbuf, ROM_TRANSFER_SIZE)!= 0 )
	{
		TRACE(_T("DownloadImage(): TransData(0x%X, 0x%X, 0x%X) failed.\n"), \
            FlashHdrAddr, ROM_TRANSFER_SIZE, FlashHdr);
		return FALSE;
	}

    if( !Jump(FlashHdrAddr))
	{
        TRACE(_T("DownloadImage(): Failed to jump to RAM address: 0x%x.\n"), FlashHdrAddr);
		return FALSE;
	}

	return TRUE;
}

BOOL MxHidDevice::ReadData(UINT address, UINT byteCount, unsigned char * pBuf)
{
    SDPCmd SDPCmd;

    SDPCmd.command = ROM_KERNEL_CMD_RD_MEM;
    SDPCmd.dataCount = byteCount;
    SDPCmd.format = 32;
    SDPCmd.data = 0;
    SDPCmd.address = address;

    //First, pack the command to a report.
    PackSDPCmd(&SDPCmd);

	//Send the report to USB HID device
	if ( Write((unsigned char *)m_pWriteReport, m_Capabilities.OutputReportByteLength)  != ERROR_SUCCESS)
	{
		return FALSE;
	}
    
    //It should be the fault of elvis_mcurom.elf which only returns report3
    memset((UCHAR *)m_pReadReport, 0, m_Capabilities.InputReportByteLength);
    //Get Report3, Device to Host:
    //4 bytes HAB mode indicating Production/Development part
	if ( Read( (UCHAR *)m_pReadReport, m_Capabilities.InputReportByteLength )  != ERROR_SUCCESS)
	{
		return FALSE;
	}
	if ( (*(unsigned int *)(m_pReadReport->Payload) != HabEnabled)  && 
		 (*(unsigned int *)(m_pReadReport->Payload) != HabDisabled) ) 
	{
		return FALSE;	
	}


    UINT MaxHidTransSize = m_Capabilities.InputReportByteLength -1;
    
    while(byteCount > 0)
    {
        UINT TransSize = (byteCount > MaxHidTransSize) ? MaxHidTransSize : byteCount;

        memset((UCHAR *)m_pReadReport, 0, m_Capabilities.InputReportByteLength);

        if ( Read( (UCHAR *)m_pReadReport, m_Capabilities.InputReportByteLength )  != ERROR_SUCCESS)
        {
            return FALSE;
        }

        memcpy(pBuf, m_pReadReport->Payload, TransSize);
        pBuf += TransSize;

        byteCount -= TransSize;
        //TRACE("Transfer Size: %d\n", TransSize);
    }

	return TRUE;
}


BOOL MxHidDevice::TransData(UINT address, UINT byteCount, const unsigned char * pBuf)
{
    SDPCmd SDPCmd;

    SDPCmd.command = ROM_KERNEL_CMD_WR_FILE;
    SDPCmd.dataCount = byteCount;
    SDPCmd.format = 0;
    SDPCmd.data = 0;
    SDPCmd.address = address;

    //First, pack the command to a report.
    PackSDPCmd(&SDPCmd);

	//Send the report to USB HID device
	if ( Write((unsigned char *)m_pWriteReport, m_Capabilities.OutputReportByteLength)  != ERROR_SUCCESS)
	{
		return FALSE;
	}
    
    Sleep(10);
    m_pWriteReport->ReportId = REPORT_ID_DATA;
    UINT MaxHidTransSize = m_Capabilities.OutputReportByteLength -1;
    UINT TransSize;
    
    while(byteCount > 0)
    {
        TransSize = (byteCount > MaxHidTransSize) ? MaxHidTransSize : byteCount;

        memcpy(m_pWriteReport->Payload, pBuf, TransSize);

        if (Write((unsigned char *)m_pWriteReport, m_Capabilities.OutputReportByteLength) != ERROR_SUCCESS)
            return FALSE;
        byteCount -= TransSize;
        pBuf += TransSize;
        //TRACE("Transfer Size: %d\n", MaxHidTransSize);
    }

    /*//It should be the fault of elvis_mcurom.elf which only returns report3
    memset((UCHAR *)m_pReadReport, 0, m_Capabilities.InputReportByteLength);
    //Get Report3, Device to Host:
    //4 bytes HAB mode indicating Production/Development part
	if ( Read( (UCHAR *)m_pReadReport, m_Capabilities.InputReportByteLength )  != ERROR_SUCCESS)
	{
		return FALSE;
	}
	if ( (*(unsigned int *)(m_pReadReport->Payload) != HabEnabled)  && 
		 (*(unsigned int *)(m_pReadReport->Payload) != HabDisabled) ) 
	{
		return FALSE;	
	}*/
    
    //below function should be invoked for mx50
	if ( !GetCmdAck(ROM_STATUS_ACK) )
	{
		return FALSE;
	}

	return TRUE;
}

BOOL MxHidDevice::Jump(UINT RAMAddress)
{
    SDPCmd SDPCmd;

    SDPCmd.command = ROM_KERNEL_CMD_JUMP_ADDR;
    SDPCmd.dataCount = 0;
    SDPCmd.format = 0;
    SDPCmd.data = 0;
    SDPCmd.address = RAMAddress;

	//Send write Command to USB
    //First, pack the command to a report.
    PackSDPCmd(&SDPCmd);

	//Send the report to USB HID device
	if ( Write((unsigned char *)m_pWriteReport, m_Capabilities.OutputReportByteLength) != ERROR_SUCCESS )
	{
		return FALSE;
	}

    memset((UCHAR *)m_pReadReport, 0, m_Capabilities.InputReportByteLength);
    //Get Report3, Device to Host:
    //4 bytes HAB mode indicating Production/Development part
	if ( Read( (UCHAR *)m_pReadReport, m_Capabilities.InputReportByteLength )  != ERROR_SUCCESS)
	{
		return FALSE;
	}
	if ( (*(unsigned int *)(m_pReadReport->Payload) != HabEnabled)  && 
		 (*(unsigned int *)(m_pReadReport->Payload) != HabDisabled) ) 
	{
		return FALSE;	
	}

	/*if ( !GetCmdAck(ROM_STATUS_ACK) )
	{
		return FALSE;
	}*/
	TRACE("*********Jump to Ramkernel successfully!**********\r\n");
	return TRUE;
}