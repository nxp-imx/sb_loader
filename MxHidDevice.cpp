#pragma once
#include "stdafx.h"
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
#include "MemoryInit.h"

#define DCD_WRITE

inline DWORD EndianSwap(DWORD x)
{
    return (x>>24) | 
        ((x<<8) & 0x00FF0000) |
        ((x>>8) & 0x0000FF00) |
        (x<<24);
}

MxHidDevice::MxHidDevice()
{
    //Initialize(MX508_USB_VID,MX508_USB_PID);
	_chipFamily = MX508;

    //InitMemoryDevice();
	//TRACE("************The new i.mx device is initialized**********\n");
	//TRACE("\n");
	return;
//Exit:
//	TRACE("Failed to initialize the new i.MX device!!!\n");
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

//Report1 
BOOL MxHidDevice::SendCmd(PSDPCmd pSDPCmd)
{
	//First, pack the command to a report.
	PackSDPCmd(pSDPCmd);

	//Send the report to USB HID device
	if ( Write((unsigned char *)m_pWriteReport, m_Capabilities.OutputReportByteLength) != ERROR_SUCCESS)
	{
		return FALSE;
	}

	return TRUE;
}

//Report 2
BOOL MxHidDevice::SendData(const unsigned char * DataBuf, UINT ByteCnt)
{
	memcpy(m_pWriteReport->Payload, DataBuf, ByteCnt);

	m_pWriteReport->ReportId = REPORT_ID_DATA;
	if (Write((unsigned char *)m_pWriteReport, m_Capabilities.OutputReportByteLength) != ERROR_SUCCESS)
		return FALSE;	

	return TRUE;
}

//Report3, Device to Host
BOOL MxHidDevice::GetHABType()
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

	return TRUE;
}

//Report4, Device to Host
BOOL MxHidDevice::GetDevAck(UINT RequiredCmdAck)
{
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

BOOL MxHidDevice::GetCmdAck(UINT RequiredCmdAck)
{
	if(!GetHABType())
		return FALSE;

	if(!GetDevAck(RequiredCmdAck))
		return FALSE;

    return TRUE;
}

BOOL MxHidDevice::WriteReg(PSDPCmd pSDPCmd)
{
	if(!SendCmd(pSDPCmd))
		return FALSE;

	if ( !GetCmdAck(ROM_WRITE_ACK) )
	{
		return FALSE;
	}
    
    return TRUE;
}


#ifndef DCD_WRITE
BOOL MxHidDevice::InitMemoryDevice(MemoryType MemType)
{
    SDPCmd SDPCmd;

    SDPCmd.command = ROM_KERNEL_CMD_WR_MEM;
    SDPCmd.dataCount = 4;

    RomFormatDCDData * pMemPara = (MemType == LPDDR2) ? Mx508LPDDR2 : Mx508MDDR;
    UINT RegCount = (MemType == LPDDR2) ? sizeof(Mx508LPDDR2) : sizeof(Mx508MDDR);

    for(int i=0; i<RegCount/sizeof(RomFormatDCDData); i++)
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

#else

BOOL MxHidDevice::DCDWrite(PUCHAR DataBuf, UINT RegCount)
{
	SDPCmd SDPCmd;
    SDPCmd.command = ROM_KERNEL_CMD_DCD_WRITE;
    SDPCmd.format = 0;
    SDPCmd.data = 0;
    SDPCmd.address = 0;
	if(GetDevType() != MX6Q)
	{
		while(RegCount)
		{
			SDPCmd.dataCount = (RegCount > MAX_DCD_WRITE_REG_CNT) ? MAX_DCD_WRITE_REG_CNT : RegCount;
			RegCount -= SDPCmd.dataCount;
			UINT ByteCnt = SDPCmd.dataCount*sizeof(RomFormatDCDData);

			if(!SendCmd(&SDPCmd))
				return FALSE;

			if(!SendData(DataBuf, ByteCnt))
				return FALSE;

			if (!GetCmdAck(ROM_WRITE_ACK) )
			{
				return FALSE;
			}

			DataBuf += ByteCnt;
		}
	}
	else
	{
		UINT RegNumTransfered = 0;
		while(RegCount)
		{
			RegNumTransfered = (RegCount > MAX_DCD_WRITE_REG_CNT) ? MAX_DCD_WRITE_REG_CNT : RegCount;
			RegCount -= RegNumTransfered;
			SDPCmd.dataCount = RegNumTransfered*sizeof(RomFormatDCDData);
			SDPCmd.address = 0x00910000;

			if(!SendCmd(&SDPCmd))
				return FALSE;

			if(!SendData(DataBuf, SDPCmd.dataCount))
				return FALSE;

			if (!GetCmdAck(ROM_WRITE_ACK) )
			{
				return FALSE;
			}

			DataBuf += SDPCmd.dataCount;
		}
		/*SDPCmd.command = ROM_KERNEL_CMD_WR_MEM;
		SDPCmd.dataCount = 4;
		PRomFormatDCDData pMemPara = (PRomFormatDCDData)DataBuf;

		for(int i=0; i<RegCount; i++)
		{
			SDPCmd.format = EndianSwap(pMemPara[i].format);
			SDPCmd.data = EndianSwap(pMemPara[i].data);
			SDPCmd.address = EndianSwap(pMemPara[i].addr);
			if ( !WriteReg(&SDPCmd) )
			{
				TRACE("Failed to write register: No. 0x%x, format = 0x%x, data = 0x%x, address = 0x%x.\n", i, pMemPara[i].format, pMemPara[i].data, pMemPara[i].addr);
				return FALSE;
			}
			_tprintf(_T("Reg #%d is initialized.\n"), i);
		}*/
	}

	return TRUE;
}

BOOL MxHidDevice::InitMemoryDevice(MemoryType MemType)
{
	RomFormatDCDData * pMemPara = Mx50_LPDDR2;
	UINT RegCount = sizeof(Mx50_LPDDR2);

	RegCount = RegCount/sizeof(RomFormatDCDData);

	for(UINT i=0; i<RegCount; i++)
	{
		pMemPara[i].addr = EndianSwap(pMemPara[i].addr);
		pMemPara[i].data = EndianSwap(pMemPara[i].data);
		pMemPara[i].format = EndianSwap(pMemPara[i].format);
	}

    if ( !DCDWrite((PUCHAR)pMemPara,RegCount) )
    {
        _tprintf(_T("Failed to initialize memory!\r\n"));
        return FALSE;
    }

	return TRUE;
}
#endif


BOOL MxHidDevice::RunPlugIn(UCHAR* pBuffer, ULONGLONG dataCount, PMxFunc pMxFunc)
{
	DWORD * pPlugIn = NULL;
	DWORD ImgIVTOffset= 0;
    DWORD PlugInAddr = 0;
	PIvtHeader pIVT = NULL,pIVT2 = NULL;

	//Search for IVT
    pPlugIn = (DWORD *)pBuffer;

	//ImgIVTOffset indicates the IVT's offset from the beginning of the image.
    while(ImgIVTOffset < dataCount && pPlugIn[ImgIVTOffset/sizeof(DWORD)] != IVT_BARKER_HEADER)
		ImgIVTOffset+= 0x100;
	
	if(ImgIVTOffset >= dataCount)
		return FALSE;

	//Now we find IVT
	pPlugIn += ImgIVTOffset/sizeof(DWORD);

	pIVT = (PIvtHeader) pPlugIn;
	//Now we have to judge DCD way or plugin way used in the image
	//The method is to check plugin flag in boot data region
    // IVT boot data format
    //   0x00    IMAGE START ADDR
    //   0x04    IMAGE SIZE
    //   0x08    PLUGIN FLAG
	PBootData pPluginDataBuf = (PBootData)(pPlugIn + (pIVT->BootData - pIVT->SelfAddr)/sizeof(DWORD));
	if(pPluginDataBuf->PluginFlag)
	{
		//Plugin mode
	  
		//---------------------------------------------------------
		//Run plugin in IRAM
		//Download plugin data into IRAM.
		PlugInAddr = pIVT->ImageStartAddr;
		DWORD PlugInDataOffset = pIVT->ImageStartAddr - pIVT->SelfAddr;
		if (!TransData(PlugInAddr, pPluginDataBuf->ImageSize, (PUCHAR)((DWORD)pIVT + PlugInDataOffset)))
		{
			TRACE(_T("Download(): TransData(0x%X, 0x%X,0x%X) failed.\n"), \
				PlugInAddr, pPluginDataBuf->ImageSize, (PUCHAR)((DWORD)pIVT + PlugInDataOffset));
			return FALSE;
		}

		if(!Execute(PlugInAddr))
			return FALSE;

		//---------------------------------------------------------
		//Download eboot to ram		
		//IVT2 location follows IVT.
		DWORD IVT2Offset = sizeof(IvtHeader);

		while((IVT2Offset + ImgIVTOffset) < dataCount && pPlugIn[IVT2Offset/sizeof(DWORD)] != IVT_BARKER_HEADER )
			IVT2Offset+= sizeof(DWORD);
		
		if((IVT2Offset + ImgIVTOffset) >= dataCount)
			return FALSE;
		pIVT2 = (PIvtHeader)(pPlugIn + IVT2Offset/sizeof(DWORD));

		//IVTOffset indicates the offset used by ROM, entirely different with ImgIVTOffset.
		DWORD IVTOffset = pIVT->SelfAddr - pPluginDataBuf->ImageStartAddr;

		PBootData pBootDataBuf = (PBootData)(pPlugIn + (pIVT2->BootData - pIVT->SelfAddr)/sizeof(DWORD));

		pMxFunc->ImageParameter.PhyRAMAddr4KRL = pBootDataBuf->ImageStartAddr + IVTOffset - ImgIVTOffset;
		pMxFunc->ImageParameter.CodeOffset = pIVT2->ImageStartAddr - pMxFunc->ImageParameter.PhyRAMAddr4KRL;
		pMxFunc->ImageParameter.ExecutingAddr = pIVT2->ImageStartAddr;
	}
	else
	{
		//DCD mode
		DWORD * pDCDRegion = pPlugIn + (pIVT->DCDAddress - pIVT->SelfAddr)/sizeof(DWORD);
		//i.e. DCD_BE  0xD2020840              ;DCD_HEADR Tag=0xd2, len=64*8+4+4, ver= 0x40    
		//i.e. DCD_BE  0xCC020404              ;write dcd cmd headr Tag=0xcc, len=64*8+4, param=4
		//The first 2 32bits data in DCD region is used to give some info about DCD data.
		//Here big endian format is used, so it must be converted.
		if(HAB_TAG_DCD != EndianSwap(*pDCDRegion)>>24)
		{
			TRACE(CString("DCD header tag doesn't match!\n"));
			return FALSE;
		}
		
		DWORD DCDDataCount = ((EndianSwap(*pDCDRegion) & 0x00FFFF00)>>8)/sizeof(DWORD);

		PRomFormatDCDData pRomFormatDCDData = (PRomFormatDCDData)malloc(DCDDataCount*sizeof(RomFormatDCDData));
		//There are several segments in DCD region, we have to extract DCD data with segment unit.
		//i.e. Below code shows how non-DCD data is inserted to finish a delay operation, we must avoid counting them in.
		/*DCD_BE 0xCF001024   ; Tag = 0xCF, Len = 1*12+4=0x10, parm = 4

		; Wait for divider to update
		DCD_BE 0x53FD408C   ; Address
		DCD_BE 0x00000004   ; Mask
		DCD_BE 0x1FFFFFFF   ; Loop

		DCD_BE 0xCC031C04   ; Tag = 0xCC, Len = 99*8+4=0x031c, parm = 4*/

		DWORD CurDCDDataCount = 1, ValidRegCount=0;
		
		//Quit if current DCD data count reaches total DCD data count.
		while(CurDCDDataCount < DCDDataCount)
		{
			DWORD DCDCmdHdr = EndianSwap(*(pDCDRegion+CurDCDDataCount));
			CurDCDDataCount++;
			if((DCDCmdHdr >> 24) == HAB_CMD_WRT_DAT)
			{
				DWORD DCDDataSegCount = (((DCDCmdHdr & 0x00FFFF00) >>8) -4)/sizeof(ImgFormatDCDData); 
				PImgFormatDCDData pImgFormatDCDData = (PImgFormatDCDData)(pDCDRegion + CurDCDDataCount);
				//Must convert image dcd data format to ROM dcd format.
				for(DWORD i=0; i<DCDDataSegCount; i++)
				{
					pRomFormatDCDData[ValidRegCount].addr = pImgFormatDCDData[i].Address;
					pRomFormatDCDData[ValidRegCount].data = pImgFormatDCDData[i].Data;
					pRomFormatDCDData[ValidRegCount].format = EndianSwap(32);
					ValidRegCount++;
					TRACE(CString("{%d,0x%08x,0x%08x},\n"),32, EndianSwap(pImgFormatDCDData[i].Address),EndianSwap(pImgFormatDCDData[i].Data));
				}
				CurDCDDataCount+=DCDDataSegCount*sizeof(ImgFormatDCDData)/sizeof(DWORD);
			}
			else if((DCDCmdHdr >> 24) == HAB_CMD_CHK_DAT)
			{
				CurDCDDataCount += (((DCDCmdHdr & 0x00FFFF00) >>8) -4)/sizeof(DWORD); 
			}
		}

		if ( !DCDWrite((PUCHAR)(pRomFormatDCDData),ValidRegCount) )
		{
			_tprintf(_T("Failed to initialize memory!\r\n"));
			free(pRomFormatDCDData);
			return FALSE;
		}
		free(pRomFormatDCDData);

		//---------------------------------------------------------
		//Download eboot to ram
		pMxFunc->ImageParameter.PhyRAMAddr4KRL = pIVT->SelfAddr - ImgIVTOffset;
		pMxFunc->ImageParameter.CodeOffset = pIVT->ImageStartAddr - pMxFunc->ImageParameter.PhyRAMAddr4KRL;
		pMxFunc->ImageParameter.ExecutingAddr = pIVT->ImageStartAddr;
	}	
	return TRUE;
}

BOOL MxHidDevice::Download(UCHAR* pBuffer, ULONGLONG dataCount, PMxFunc pMxFunc)
{
    //if(pMxFunc->Task == TRANS)
	DWORD byteIndex, numBytesToWrite = 0;
	for ( byteIndex = 0; byteIndex < dataCount; byteIndex += numBytesToWrite )
	{
		// Get some data
		numBytesToWrite = min(MAX_SIZE_PER_DOWNLOAD_COMMAND, (DWORD)dataCount - byteIndex);

		if (!TransData(pMxFunc->ImageParameter.PhyRAMAddr4KRL + byteIndex, numBytesToWrite, pBuffer + byteIndex))
		{
			TRACE(_T("Download(): TransData(0x%X, 0x%X,0x%X) failed.\n"), \
				pMxFunc->ImageParameter.PhyRAMAddr4KRL + byteIndex, numBytesToWrite, pBuffer + byteIndex);
			return FALSE;
		}
	}
    return TRUE;
}

BOOL MxHidDevice::AddIvtHdr(UINT32 ImageStartAddr)
{
	UINT FlashHdrAddr;

	//transfer length of ROM_TRANSFER_SIZE is a must to ROM code.
	unsigned char FlashHdr[ROM_TRANSFER_SIZE] = { 0 };	

	// Otherwise, create a header and append the data

    	PIvtHeader pIvtHeader = (PIvtHeader)FlashHdr;

    	FlashHdrAddr = ImageStartAddr - sizeof(IvtHeader);

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
   	unsigned char Tempbuf[ROM_TRANSFER_SIZE] = { 0 };
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

    m_jumpAddr = FlashHdrAddr;
    
	return TRUE;
}

BOOL MxHidDevice::Execute(UINT32 ImageStartAddr)
{
	if(!AddIvtHdr(ImageStartAddr))
	{
		TRACE(_T("RunPlugIn(): Failed to addhdr to RAM address: 0x%x.\n"), ImageStartAddr);
		return FALSE;
	}

    if( !Jump(m_jumpAddr))
	{
        TRACE(_T("Download(): Failed to jump to RAM address: 0x%x.\n"), m_jumpAddr);
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

	if(!SendCmd(&SDPCmd))
		return FALSE;

	if(!GetHABType())
		return FALSE;

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

	if(!SendCmd(&SDPCmd))
		return FALSE;
    
    Sleep(10);

    UINT MaxHidTransSize = m_Capabilities.OutputReportByteLength -1;
    UINT TransSize;
    
    while(byteCount > 0)
    {
        TransSize = (byteCount > MaxHidTransSize) ? MaxHidTransSize : byteCount;

		if(!SendData(pBuf, TransSize))
			return FALSE;

        byteCount -= TransSize;
        pBuf += TransSize;
        //TRACE("Transfer Size: %d\n", MaxHidTransSize);
    }

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

	if(!SendCmd(&SDPCmd))
		return FALSE;


	if(!GetHABType())
		return FALSE;


	//TRACE("*********Jump to Ramkernel successfully!**********\r\n");
	return TRUE;
}