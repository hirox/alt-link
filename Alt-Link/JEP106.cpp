
#include "stdafx.h"
#include "JEP106.h"

const char* getJEP106DesignerName(uint32_t continuation, uint32_t id)
{
	if (continuation == 0x04)
	{
		switch (id)
		{
		case 0x26:
			return "MediaTek";
		case 0x3B:
			return "ARM";
		default:
			return "UNKNOWN(4)";
		}
	}
	else if (continuation == 0x03)
	{
		switch (id)
		{
		case 0xE9:
			return "Marvell Semiconductors";
		default:
			return "UNKNOWN(3)";
		}
	}
	else if (continuation == 0x00)
	{
		switch (id)
		{
		case 0x01:
			return "AMD";
		case 0x0E:
			return "Freescale(Motorola)";
		case 0x15:
			return "NXP(Philips)";
		case 0x17:
			return "Texas Instruments";
		case 0x1F:
			return "Atmel";
		case 0x20:
			return "STMicroelectronics";
		case 0x21:
			return "Lattice Semi.";
		case 0x29:
			return "Microchip Technology";
		case 0x34:
			return "Cypress";
		case 0x48:
			return "Apple Computer";
		case 0x49:
			return "Xilinx";
		case 0x70:
			return "Qualcomm";
		case 0x98:
			return "Toshiba";
		default:
			return "UNKNOWN(0)";
		}
	}
	return "UNKNOWN";
}