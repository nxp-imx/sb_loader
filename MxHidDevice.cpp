/*
 * File:      MxHidDevice.cpp
 *
 * Copyright (c) 2010-2015 Freescale Semiconductor, Inc. All rights reserved.
 * See included license file for license details.
 */


#pragma once
#include "stdafx.h"
#include <assert.h>
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

#include "libfdt\libfdt.h"

#define DCD_WRITE

inline DWORD EndianSwap(DWORD x)
{
	return (x >> 24) |
		((x << 8) & 0x00FF0000) |
		((x >> 8) & 0x0000FF00) |
		(x << 24);
}

MxHidDevice::MxHidDevice()
{
	//Initialize(MX508_USB_VID,MX508_USB_PID);
	_chipFamily = MX508;
	m_IsFitImage = FALSE;
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

	pTmpSDPCmd[0] = (((pSDPCmd->address & 0x00FF0000) << 8)
		| ((pSDPCmd->address & 0xFF000000) >> 8)
		| (pSDPCmd->command & 0x0000FFFF));

	pTmpSDPCmd[1] = ((pSDPCmd->dataCount & 0xFF000000)
		| ((pSDPCmd->format & 0x000000FF) << 16)
		| ((pSDPCmd->address & 0x000000FF) << 8)
		| ((pSDPCmd->address & 0x0000FF00) >> 8));

	pTmpSDPCmd[2] = ((pSDPCmd->data & 0xFF000000)
		| ((pSDPCmd->dataCount & 0x000000FF) << 16)
		| (pSDPCmd->dataCount & 0x0000FF00)
		| ((pSDPCmd->dataCount & 0x00FF0000) >> 16));

	pTmpSDPCmd[3] = (((0x00 & 0x000000FF) << 24)
		| ((pSDPCmd->data & 0x00FF0000) >> 16)
		| (pSDPCmd->data & 0x0000FF00)
		| ((pSDPCmd->data & 0x000000FF) << 16));

}

//Report1 
BOOL MxHidDevice::SendCmd(PSDPCmd pSDPCmd)
{
	//First, pack the command to a report.
	PackSDPCmd(pSDPCmd);

	//Send the report to USB HID device
	if (Write((unsigned char *)m_pWriteReport, m_Capabilities.OutputReportByteLength) != ERROR_SUCCESS)
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
	if (Read((UCHAR *)m_pReadReport, m_Capabilities.InputReportByteLength) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	if ((*(unsigned int *)(m_pReadReport->Payload) != HabEnabled) &&
		(*(unsigned int *)(m_pReadReport->Payload) != HabDisabled))
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
	if (Read((UCHAR *)m_pReadReport, m_Capabilities.InputReportByteLength) != ERROR_SUCCESS)
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
	if (!GetHABType())
		return FALSE;

	if (!GetDevAck(RequiredCmdAck))
		return FALSE;

	return TRUE;
}

BOOL MxHidDevice::WriteReg(PSDPCmd pSDPCmd)
{
	if (!SendCmd(pSDPCmd))
		return FALSE;

	if (!GetCmdAck(ROM_WRITE_ACK))
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

	for (int i = 0; i < RegCount / sizeof(RomFormatDCDData); i++)
	{
		SDPCmd.format = pMemPara[i].format;
		SDPCmd.data = pMemPara[i].data;
		SDPCmd.address = pMemPara[i].addr;
		if (!WriteReg(&SDPCmd))
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
	if (GetDevType() == MX50)
	{
		//i.mx50
		while (RegCount)
		{
			//The data count here is based on register unit.
			SDPCmd.dataCount = (RegCount > MAX_DCD_WRITE_REG_CNT) ? MAX_DCD_WRITE_REG_CNT : RegCount;
			RegCount -= SDPCmd.dataCount;
			UINT ByteCnt = SDPCmd.dataCount * sizeof(RomFormatDCDData);

			if (!SendCmd(&SDPCmd))
				return FALSE;

			if (!SendData(DataBuf, ByteCnt))
				return FALSE;

			if (!GetCmdAck(ROM_WRITE_ACK))
			{
				return FALSE;
			}

			DataBuf += ByteCnt;
		}
	}
	else
	{
		SDPCmd.dataCount = RegCount;
		if (this->m_DevType == MX7ULP)
			SDPCmd.address = 0x2f018000;
		else if (this->m_DevType == K32H422)
			SDPCmd.address = 0x8000;
		else if (this->m_DevType == MX8QM || this->m_DevType == MX8QXP)
			SDPCmd.address = 0x2000e400 ;
		else
			SDPCmd.address = 0x00910000;//IRAM free space

		if (this->m_DevType == MX8QM || this->m_DevType == MX8QXP)
		{
			//write first to avoid ECC error.
			Download(DataBuf, 0x1000, SDPCmd.address);
		}

		if (!SendCmd(&SDPCmd))
			return FALSE;

		UINT MaxHidTransSize = m_Capabilities.OutputReportByteLength - 1;

		while (RegCount > 0)
		{
			UINT ByteCntTransfered = (RegCount > MaxHidTransSize) ? MaxHidTransSize : RegCount;
			RegCount -= ByteCntTransfered;

			if (!SendData(DataBuf, ByteCntTransfered))
				return FALSE;

			DataBuf += ByteCntTransfered;
			SDPCmd.address += ByteCntTransfered;
		}

		if (!GetCmdAck(ROM_WRITE_ACK))
		{
			return FALSE;
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

BOOL MxHidDevice::RunDCD(DWORD* pDCDRegion)
{
	if (pDCDRegion == NULL)
		return FALSE;

	//i.e. DCD_BE  0xD2020840              ;DCD_HEADR Tag=0xd2, len=64*8+4+4, ver= 0x40    
	//i.e. DCD_BE  0xCC020404              ;write dcd cmd headr Tag=0xcc, len=64*8+4, param=4
	//The first 2 32bits data in DCD region is used to give some info about DCD data.
	//Here big endian format is used, so it must be converted.
	if (HAB_TAG_DCD != EndianSwap(*pDCDRegion) >> 24)
	{
		TRACE(CString("DCD header tag doesn't match!\n"));
		return FALSE;
	}

	if (GetDevType() != MX50)
	{
		//The DCD_WRITE command handling was changed from i.MX508.
		//Now the DCD is  performed by HAB and therefore the format of DCD is the same format as in regular image. 
		//The DCD_WRITE parameters require size and address. Size is the size of entire DCD file including header. 
		//Address is the temporary address that USB will use for storing the DCD file before processing.

		DWORD DCDHeader = EndianSwap(*pDCDRegion);
		//Total dcd data bytes:
		INT TotalDCDDataCnt = (DCDHeader & 0x00FFFF00) >> 8;

		int DCDBytesMax = HAB_DCD_BYTES_MAX;
		if (GetDevType() == MX8QM || GetDevType() == MX8QXP)
			DCDBytesMax = 2 * HAB_DCD_BYTES_MAX;

		if (TotalDCDDataCnt > DCDBytesMax)
		{
			_tprintf(_T("DCD data excceeds max limit!!!\r\n"));
			return FALSE;
		}

		if (!DCDWrite((PUCHAR)(pDCDRegion), TotalDCDDataCnt))
		{
			_tprintf(_T("Failed to initialize memory!\r\n"));
			return FALSE;
		}
	}
	else
	{
		DWORD DCDDataCount = ((EndianSwap(*pDCDRegion) & 0x00FFFF00) >> 8) / sizeof(DWORD);

		PRomFormatDCDData pRomFormatDCDData = (PRomFormatDCDData)malloc(DCDDataCount * sizeof(RomFormatDCDData));
		//There are several segments in DCD region, we have to extract DCD data with segment unit.
		//i.e. Below code shows how non-DCD data is inserted to finish a delay operation, we must avoid counting them in.
		/*DCD_BE 0xCF001024   ; Tag = 0xCF, Len = 1*12+4=0x10, parm = 4

		; Wait for divider to update
		DCD_BE 0x53FD408C   ; Address
		DCD_BE 0x00000004   ; Mask
		DCD_BE 0x1FFFFFFF   ; Loop

		DCD_BE 0xCC031C04   ; Tag = 0xCC, Len = 99*8+4=0x031c, parm = 4*/

		DWORD CurDCDDataCount = 1, ValidRegCount = 0;

		//Quit if current DCD data count reaches total DCD data count.
		while (CurDCDDataCount < DCDDataCount)
		{
			DWORD DCDCmdHdr = EndianSwap(*(pDCDRegion + CurDCDDataCount));
			CurDCDDataCount++;
			if ((DCDCmdHdr >> 24) == HAB_CMD_WRT_DAT)
			{
				DWORD DCDDataSegCount = (((DCDCmdHdr & 0x00FFFF00) >> 8) - 4) / sizeof(ImgFormatDCDData);
				PImgFormatDCDData pImgFormatDCDData = (PImgFormatDCDData)(pDCDRegion + CurDCDDataCount);
				//Must convert image dcd data format to ROM dcd format.
				for (DWORD i = 0; i < DCDDataSegCount; i++)
				{
					pRomFormatDCDData[ValidRegCount].addr = pImgFormatDCDData[i].Address;
					pRomFormatDCDData[ValidRegCount].data = pImgFormatDCDData[i].Data;
					pRomFormatDCDData[ValidRegCount].format = EndianSwap(32);
					ValidRegCount++;
					TRACE(CString("{%d,0x%08x,0x%08x},\n"), 32, EndianSwap(pImgFormatDCDData[i].Address), EndianSwap(pImgFormatDCDData[i].Data));
				}
				CurDCDDataCount += DCDDataSegCount * sizeof(ImgFormatDCDData) / sizeof(DWORD);
			}
			else if ((DCDCmdHdr >> 24) == HAB_CMD_CHK_DAT)
			{
				CurDCDDataCount += (((DCDCmdHdr & 0x00FFFF00) >> 8) - 4) / sizeof(DWORD);
			}
		}

		if (!DCDWrite((PUCHAR)(pRomFormatDCDData), ValidRegCount))
		{
			_tprintf(_T("Failed to initialize memory!\r\n"));
			free(pRomFormatDCDData);
			return FALSE;
		}
		free(pRomFormatDCDData);
	}
	return TRUE;
}

BOOL MxHidDevice::InitMemoryDevice(MemoryType MemType)
{
	RomFormatDCDData * pMemPara = Mx50_LPDDR2;
	UINT RegCount = sizeof(Mx50_LPDDR2);

	RegCount = RegCount / sizeof(RomFormatDCDData);

	for (UINT i = 0; i < RegCount; i++)
	{
		pMemPara[i].addr = EndianSwap(pMemPara[i].addr);
		pMemPara[i].data = EndianSwap(pMemPara[i].data);
		pMemPara[i].format = EndianSwap(pMemPara[i].format);
	}

	if (!DCDWrite((PUCHAR)pMemPara, RegCount))
	{
		_tprintf(_T("Failed to initialize memory!\r\n"));
		return FALSE;
	}

	return TRUE;
}
#endif

DWORD MxHidDevice::GetIvtOffset(DWORD *start, ULONGLONG dataCount, DWORD begin)
{
	//Search for a valid IVT Code starting from the given address
	DWORD ImgIVTOffset = begin;

	while (ImgIVTOffset < dataCount &&
		(start[ImgIVTOffset / sizeof(DWORD)] != IVT_BARKER_HEADER &&
		start[ImgIVTOffset / sizeof(DWORD)] != IVT_BARKER2_HEADER &&
		start[ImgIVTOffset / sizeof(DWORD)] != MX8_IVT_BARKER_HEADER &&
		start[ImgIVTOffset / sizeof(DWORD)] != MX8_IVT2_BARKER_HEADER
		))
		ImgIVTOffset += 0x100;

	if (ImgIVTOffset >= dataCount)
		return -1;

	return ImgIVTOffset;
}

int FitGetImageNodeOffset(UCHAR *fit, int image_node, char * type, int index)
{
	int offset;
	int len;
	const char * config;
	int config_node = 0;

	offset = fdt_path_offset(fit, "/configurations");
	if (offset < 0)
	{
		TRACE(_T("Can't found configurations\n"));
		return offset;
	}

	config = (const char *)fdt_getprop(fit, offset, "default", &len);
	for (int node = fdt_first_subnode(fit, offset); node >= 0; node = fdt_next_subnode(fit, node))
	{
		const char *name = fdt_get_name(fit, node, &len);
		if (strcmp(config, name) == 0)
		{
			config_node = node;
			break;
		}
	}

	if (config_node <= 0)
	{
		TRACE(_T("can't find default config"));
		return config_node;
	}

	char * name = (char*)fdt_getprop(fit, config_node, type, &len);
	if (!name)
	{
		TRACE(_T("can't find type\n"));
		return -1;
	}

	char *str = name;
	for (int i=0; i < index; i++)
	{
		str = strchr(str, '\0') + 1;
		if (!str || (str - name) > len)
		{
			TRACE(_T("can't found index %d node"), index);
			return -1;
		}
	}

	return fdt_subnode_offset(fit, image_node, str);
}

int FitGetIntProp(UCHAR *fit, int node, char *prop, int *value)
{
	int len;
	const void * data = fdt_getprop(fit, node, prop, &len);
	if (data == NULL)
	{
		TRACE(_T("failure to get load prop\n"));
		return -1;
	}
	*value = fdt32_to_cpu(*(int*)data);
	return 0;
}


BOOL MxHidDevice::LoadFitImage(UCHAR *fit, ULONGLONG dataCount, PMxFunc pMxFunc)
{
	
	int image_offset = fdt_path_offset(fit, "/images");
	if (image_offset < 0)
	{
		TRACE(_T("Can't find /images\n"));
		return FALSE;
	}

	int depth = 0;
	int count = 0;
	int node_offset = 0;
	int entry = 0;

	int size = fdt_totalsize(fit);
	size = (size + 3) & ~3;
	int base_offset = (size + 3) & ~3;

	int firmware; 
	firmware = FitGetImageNodeOffset(fit, image_offset, "firmware", 0);
	if (firmware < 0)
	{
		TRACE(_T("can't find firmware\n"));
		return FALSE;
	}

	int firmware_load, offset, firmware_len;

	if (FitGetIntProp(fit, firmware, "load", &firmware_load) ||
		FitGetIntProp(fit, firmware, "data-offset", &offset) ||
		FitGetIntProp(fit, firmware, "data-size", &firmware_len))
	{
		TRACE(_T("can't find load data-offset data-len\n"));
		return FALSE;
	}

	if (!Download(fit + base_offset + offset, firmware_len, firmware_load))
		return FALSE;


	int fdt;
	fdt = FitGetImageNodeOffset(fit, image_offset, "fdt", 0);
	if (fdt < 0)
	{
		TRACE(_T("can't find fdt\n"));
		return FALSE;
	}

	int fdt_load, fdt_size;

	if (FitGetIntProp(fit, fdt, "data-offset", &offset) ||
		FitGetIntProp(fit, fdt, "data-size", &fdt_size))
	{
		TRACE(_T("can't find load data-offset data-len\n"));
		return FALSE;
	}

	fdt_load = firmware_load + firmware_len;

	if (!Download(fit + base_offset + offset, fdt_size, fdt_load))
		return FALSE;

	int load_node;
	int index=0;
	do
	{
		load_node = FitGetImageNodeOffset(fit, image_offset, "loadables", index);
		if (load_node >= 0)
		{
			int load, offset, len, entry;
			if (FitGetIntProp(fit, load_node, "load", &load) ||
				FitGetIntProp(fit, load_node, "data-offset", &offset) ||
				FitGetIntProp(fit, load_node, "data-size", &len))
			{
				TRACE(_T("can't find load data-offset data-len\n"));
				return FALSE;
			}
			if (!Download(fit + base_offset + offset, len, load))
				return FALSE;

			if (FitGetIntProp(fit, load_node, "entry", &entry) == 0)
			{
				pMxFunc->ImageParameter.ExecutingAddr = entry;
				pMxFunc->pIVT = NULL;
			}	
		}
		index++;
	} while (load_node >= 0);

	return TRUE;
}

BOOL MxHidDevice::RunPlugIn(UCHAR* pBuffer, ULONGLONG dataCount, PMxFunc pMxFunc)
{
	DWORD * pPlugIn = NULL;
	DWORD ImgIVTOffset = 0;
	DWORD PlugInAddr = 0;
	PIvtHeader pIVT = NULL, pIVT2 = NULL;
	PBootData pPluginDataBuf;

	while (1)
	{
		ImgIVTOffset = GetIvtOffset((DWORD*)(pBuffer), dataCount, ImgIVTOffset);
		if (ImgIVTOffset < 0)
		{
			TRACE(_T("Can't find IVT header\n"));
			return FALSE;
		}

		//Search for IVT
		pPlugIn = (DWORD *)pBuffer;

		//Now we find IVT
		pPlugIn += ImgIVTOffset / sizeof(DWORD);

		pIVT = (PIvtHeader)pPlugIn;

		//Set image parameter as if we don't use a plug-in, gets overwritten in case we use a plug-in by IVT2.
		pMxFunc->ImageParameter.PhyRAMAddr4KRL = pIVT->SelfAddr - ImgIVTOffset;
		pMxFunc->ImageParameter.CodeOffset = pIVT->ImageStartAddr - pMxFunc->ImageParameter.PhyRAMAddr4KRL;
		pMxFunc->ImageParameter.ExecutingAddr = pIVT->ImageStartAddr;
		pMxFunc->pIVT = pIVT;

		//Now we have to judge DCD way or plugin way used in the image
		//The method is to check plugin flag in boot data region
		// IVT boot data format
		//   0x00    IMAGE START ADDR
		//   0x04    IMAGE SIZE
		//   0x08    PLUGIN FLAG
		pPluginDataBuf = (PBootData)(pPlugIn + (pIVT->BootData - pIVT->SelfAddr) / sizeof(DWORD));

		//skip HDMI image
		if (pPluginDataBuf->PluginFlag & 0xFFFFFFFE)
			ImgIVTOffset += 0x100;
		else
			break;
	}

	if (pPluginDataBuf->PluginFlag || pIVT->Reserved)
	{
		//Plugin mode

		//---------------------------------------------------------
		//Run plugin in IRAM
		//Download plugin data into IRAM.
		PlugInAddr = pIVT->ImageStartAddr;
		DWORD PlugInDataOffset = pIVT->ImageStartAddr - pIVT->SelfAddr;
		if (!TransData(pIVT->SelfAddr, pPluginDataBuf->ImageSize, (PUCHAR)((DWORD)pIVT)))
		{
			TRACE(_T("Download(): TransData(0x%X, 0x%X,0x%X) failed.\n"), \
				PlugInAddr, pPluginDataBuf->ImageSize, (PUCHAR)((DWORD)pIVT + PlugInDataOffset));
			return FALSE;
		}

		if (!Jump(pIVT->SelfAddr))
		{
			TRACE(_T("Download(): Failed to jump to RAM address: 0x%x.\n"), m_jumpAddr);
			return FALSE;
		}

		UCHAR * pI = ((UCHAR*)pIVT) + pIVT->Reserved + IVT_OFFSET_SD;
		if (pIVT->Reserved)
		{
			Sleep(200);
			//this->FindKnownHidDevices();
			image_header_t *pImage = (image_header_t*)(pI);
			if (EndianSwap(pImage->ih_magic) == IH_MAGIC)
			{
				pMxFunc->ImageParameter.PhyRAMAddr4KRL = EndianSwap(pImage->ih_load);
				pMxFunc->ImageParameter.CodeOffset = pIVT->Reserved + sizeof(image_header_t) + IVT_OFFSET_SD;
				pMxFunc->ImageParameter.ExecutingAddr = EndianSwap(pImage->ih_ep);

				pMxFunc->pIVT = NULL;
			} 
			else if (fdt_check_header(pI) == 0)
			{
				m_IsFitImage = TRUE;
				pMxFunc->ImageParameter.CodeOffset = (pI - pBuffer);
			}
			else
			{
				TRACE(_T("Unknow Image format\n"));
				return FALSE;
			}
		}
		else 
		{
			//---------------------------------------------------------
			//Download eboot to ram		
			//Search IVT2.
			//ImgIVTOffset indicates the IVT's offset from the beginning of the image.
			DWORD IVT2Offset = sizeof(IvtHeader);

			while ((IVT2Offset + ImgIVTOffset) < dataCount &&
				(pPlugIn[IVT2Offset / sizeof(DWORD)] != IVT_BARKER_HEADER &&
					pPlugIn[IVT2Offset / sizeof(DWORD)] != IVT_BARKER2_HEADER))
				IVT2Offset += sizeof(DWORD);

			if ((IVT2Offset + ImgIVTOffset) >= dataCount)
				return FALSE;
			pIVT2 = (PIvtHeader)(pPlugIn + IVT2Offset / sizeof(DWORD));

			//IVTOffset indicates the offset used by ROM, entirely different with ImgIVTOffset.
			DWORD IVTOffset = pIVT->SelfAddr - pPluginDataBuf->ImageStartAddr;

			PBootData pBootDataBuf = (PBootData)(pPlugIn + (pIVT2->BootData - pIVT2->SelfAddr + IVT2Offset) / sizeof(DWORD));

			pMxFunc->ImageParameter.PhyRAMAddr4KRL = pBootDataBuf->ImageStartAddr + IVTOffset - ImgIVTOffset;
			pMxFunc->ImageParameter.CodeOffset = pIVT2->ImageStartAddr - pMxFunc->ImageParameter.PhyRAMAddr4KRL;
			pMxFunc->ImageParameter.ExecutingAddr = pIVT2->ImageStartAddr;

			pMxFunc->pIVT = pIVT2;
		}
	}
	else if (pIVT->DCDAddress)
	{
		//DCD mode
		DWORD * pDCDRegion = pPlugIn + (pIVT->DCDAddress - pIVT->SelfAddr) / sizeof(DWORD);
		return RunDCD(pDCDRegion);
	}
	return TRUE;
}

BOOL MxHidDevice::RunMxMultiImg(UCHAR* pBuffer, ULONGLONG dataCount)
{
	DWORD *pImg = (DWORD*)pBuffer;
	DWORD * pDCDRegion;
	DWORD ImageOffset = 0;
	DWORD ImgIVTOffset = GetIvtOffset(pImg, MX8_INITIAL_IMAGE_SIZE);
	PIvtHeaderV2 pIVT = NULL, pIVT2 = NULL;
	PBootDataV2 pBootData1 = NULL, pBootData2 = NULL;
	unsigned int i;

	//if we did not find a valid IVT within INITIAL_IMAGE_SIZE we have a non zero ImageOffset
	if (ImgIVTOffset < 0)
	{
		TRACE(_T("Not a valid image.\n"));
		return FALSE;
	}

	pImg += ImageOffset / sizeof(DWORD);
	if (pImg[ImgIVTOffset / sizeof(DWORD)] != MX8_IVT_BARKER_HEADER && pImg[ImgIVTOffset / sizeof(DWORD)] != MX8_IVT2_BARKER_HEADER)
	{
		TRACE(_T("Not a valid image.\n"));
		return FALSE;
	}

	pIVT = (PIvtHeaderV2)(pImg + ImgIVTOffset / sizeof(DWORD));

	if (pImg[ImgIVTOffset / sizeof(DWORD)] == MX8_IVT2_BARKER_HEADER)
	{
		pIVT2 = (PIvtHeaderV2)(pImg + ImgIVTOffset + pIVT->Next / sizeof(DWORD));
	}
	else
	{
		pIVT2 = pIVT + 1; // The IVT for the second container is immediatly after IVT1
	}

	pBootData1 = (PBootDataV2)((DWORD*)pIVT + (pIVT->BootData - pIVT->SelfAddr) / sizeof(DWORD));
	pBootData2 = (PBootDataV2)((DWORD*)pIVT2 + (pIVT2->BootData - pIVT2->SelfAddr) / sizeof(DWORD));


	if (pIVT->DCDAddress)
	{
		pDCDRegion = (DWORD*)pIVT + (pIVT->DCDAddress - pIVT->SelfAddr) / sizeof(DWORD);
		if (!RunDCD(pDCDRegion)){
			_tprintf(_T("Failed to download DCDs!\r\n"));
			return FALSE;
		}
	}

	// Load Initial Image
	assert((pIVT->SelfAddr - ImgIVTOffset) < (1ULL << 32));
	if (!Download((UCHAR*)pImg, MX8_INITIAL_IMAGE_SIZE, (UINT)(SCUViewAddr(pIVT->SelfAddr) - ImgIVTOffset))){
		_tprintf(_T("Failed to download Initial image headers!\r\n"));
		return FALSE;
	}

	//Load all the images in the first container to their respective Address
	for (i = 0; i < (pBootData1->NrImages); ++i) {
		assert(pBootData1->Images[i].ImageAddr < (1ULL << 32));
		if (!Download((UCHAR*)pImg + pBootData1->Images[i].Offset - IVT_OFFSET_SD,
			pBootData1->Images[i].ImageSize,
			(UINT)SCUViewAddr(pBootData1->Images[i].ImageAddr))) {
			_tprintf(_T("Failed to download first container images!\r\n"));
			return FALSE;
		}
	}

	//Load all the images in the second container to their respective Address
	for (i = 0; i < (pBootData2->NrImages); ++i) {
		assert(pBootData2->Images[i].ImageAddr < (1ULL << 32));
		if (!Download((UCHAR*)pImg + pBootData2->Images[i].Offset - IVT_OFFSET_SD,
			pBootData2->Images[i].ImageSize,
			(UINT)SCUViewAddr(pBootData2->Images[i].ImageAddr))) {
			_tprintf(_T("Failed to download second container images!\r\n"));
			return FALSE;
		}
	}

	if (!SkipDCD()) {
		_tprintf(_T("Failed to Skip DCDs!\r\n"));
		return FALSE;
	}

	assert(pIVT->SelfAddr < (1ULL << 32));
	return Jump((UINT)pIVT->SelfAddr);
}

BOOL MxHidDevice::Download(UCHAR* pBuffer, ULONGLONG dataCount, UINT RAMAddress)
{
	//if(pMxFunc->Task == TRANS)
	DWORD byteIndex, numBytesToWrite = 0;

	for (byteIndex = 0; byteIndex < dataCount; byteIndex += numBytesToWrite)
	{
		// Get some data
		numBytesToWrite = min(MAX_SIZE_PER_DOWNLOAD_COMMAND, (DWORD)dataCount - byteIndex);

		if (!TransData(RAMAddress + byteIndex, numBytesToWrite, pBuffer + byteIndex))
		{
			TRACE(_T("Download(): TransData(0x%X, 0x%X,0x%X) failed.\n"), \
				RAMAddress + byteIndex, numBytesToWrite, pBuffer + byteIndex);
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
	if (!ReadData(FlashHdrAddr, ROM_TRANSFER_SIZE, FlashHdr))
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
	if (!TransData(FlashHdrAddr, ROM_TRANSFER_SIZE, FlashHdr))
	{
		TRACE(_T("DownloadImage(): TransData(0x%X, 0x%X, 0x%X) failed.\n"), \
			FlashHdrAddr, ROM_TRANSFER_SIZE, FlashHdr);
		return FALSE;
	}

	//Verify the data
	unsigned char Tempbuf[ROM_TRANSFER_SIZE] = { 0 };
	if (!ReadData(FlashHdrAddr, ROM_TRANSFER_SIZE, Tempbuf))
	{
		TRACE(_T("DownloadImage(): TransData(0x%X, 0x%X, 0x%X) failed.\n"), \
			FlashHdrAddr, ROM_TRANSFER_SIZE, FlashHdr);
		return FALSE;
	}

	if (memcmp(FlashHdr, Tempbuf, ROM_TRANSFER_SIZE) != 0)
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
	if (!AddIvtHdr(ImageStartAddr))
	{
		TRACE(_T("RunPlugIn(): Failed to addhdr to RAM address: 0x%x.\n"), ImageStartAddr);
		return FALSE;
	}

	if (!Jump(m_jumpAddr))
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

	if (!SendCmd(&SDPCmd))
		return FALSE;

	if (!GetHABType())
		return FALSE;

	UINT MaxHidTransSize = m_Capabilities.InputReportByteLength - 1;

	while (byteCount > 0)
	{
		UINT TransSize = (byteCount > MaxHidTransSize) ? MaxHidTransSize : byteCount;

		memset((UCHAR *)m_pReadReport, 0, m_Capabilities.InputReportByteLength);

		if (Read((UCHAR *)m_pReadReport, m_Capabilities.InputReportByteLength) != ERROR_SUCCESS)
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

	UINT MaxHidTransSize = m_Capabilities.OutputReportByteLength - 1;
	UINT TransSize;

	if (this->m_DevType >= MX8QM)
		byteCount = ((byteCount + MaxHidTransSize -1) / MaxHidTransSize) * MaxHidTransSize;

	SDPCmd.command = ROM_KERNEL_CMD_WR_FILE;
	SDPCmd.dataCount = byteCount;
	SDPCmd.format = 0;
	SDPCmd.data = 0;
	SDPCmd.address = address;

	//Mscale need extra delay. Reason unknown.
	Sleep(10);

	if (!SendCmd(&SDPCmd))
		return FALSE;

	Sleep(10);


	while (byteCount > 0)
	{
		TransSize = (byteCount > MaxHidTransSize) ? MaxHidTransSize : byteCount;

		if (!SendData(pBuf, TransSize))
			return FALSE;

		byteCount -= TransSize;
		pBuf += TransSize;
		//TRACE("Transfer Size: %d\n", MaxHidTransSize);
	}

	//below function should be invoked for mx50
	if (!GetCmdAck(ROM_STATUS_ACK))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL MxHidDevice::SkipDCD()
{
	SDPCmd SDPCmd;

	SDPCmd.command = ROM_KERNEL_CMD_SKIP_DCD_HEADER;
	SDPCmd.dataCount = 0;
	SDPCmd.format = 0;
	SDPCmd.data = 0;
	SDPCmd.address = 0;

	if (!SendCmd(&SDPCmd))
		return FALSE;

	if (!GetCmdAck(ROM_OK_ACK))
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

	if (!SendCmd(&SDPCmd))
		return FALSE;


	if (!GetHABType())
		return FALSE;

	Sleep(300); // Wait for plug in complete.

	GetDevAck(ROM_OK_ACK);  /*omit rom return error code*/

	//TRACE("*********Jump to Ramkernel successfully!**********\r\n");
	return TRUE;
}

BOOL MxHidDevice::RegRead(UINT address, UINT *value)
{
	return ReadData(address, 4, (unsigned char *)value);
}

BOOL MxHidDevice::RegWrite(UINT address, UINT value)
{
	return TransData(address, 4, (unsigned char *)&value);
}