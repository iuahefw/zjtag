/*
 * created by hugebird @ chinadsl.net 17/10/2010 Brjtag rev1.9m
 *
 * Brjtag application for HID-BRJTAG v2.xx MCU firmware.
 * fw v2.0 on STM32 platform
 *
 * Copyright (C) 2010 Hugebird
 *
 * This code is covered by the GPL v2.
 */

#if defined(_MSC_VER)
#define WINDOWS_VERSION
#endif

#define STMHID

#include "zjtag.h"
#include "stmhid.h"
#include "libusb.h"

static char cmdbuf[4100];
static char st[10];
static int gIndex = 0;
static int skip = 0;
static DWORD tmpbuf[2100];

///////////////////////////////extern global////////////////////////////////////////////////////
extern int instruction_length;
extern int endian;
extern DWORD FLASH_MEMORY_START;
extern DWORD cmd_type;
extern int safemode;
extern int showgpio;
extern int silent_mode;
extern int bypass;
extern int flsPrgTimeout;
extern int ejtag_speed;

extern DWORD LL1;
extern BYTE  LL2;
extern DWORD LL3;
extern DWORD LL4;
extern DWORD LL5;
extern DWORD LL6;
extern DWORD LL7;
extern DWORD LL8;
extern DWORD LL9;

extern DWORD USBID;

static void u_getconfig(void)
{
	// performance fine tune parameters
	//L1: clk
	//L2: FT2232 USB read latency>>> 2 ~ 255ms, default 2ms
	//L3: DMA xfer Polling timeout>>> 0 ~ 48 us, defualt 1 us
	//L4: FLASH Write Polling timeout>>> 1 ~ 127 us, defualt 16 us
	//L5: USB Buffer size
	//L6: Allowed DMA Bulk xfer count>>>
	//L7: Block Size on FLASH DWORD Read in x32Bits Mode>>>
	//L8: Block Size on FLASH DWORD Write in x16Bits Mode>>>
	//L9: default profile>>>

	if (LL4 == 0xFFFF)
		LL4 = FLASH_POLLING;
	if (LL1 > 20)
		LL1 = 20;  //limit to 20 cycles delay max
	if (!LL7 || LL7 > 1024)
		LL7 = 1024;
	if (!LL8)
		LL8 = 128;
	if (LL8 > 200)
		LL8 = 200;
	if (LL9 == 1) { //safe mode
		LL1 = 10;
		LL7 = 64;
		LL8 = 32;
		LL4 = 128;
	} else if (LL9 == 2) { //risk read mode
		LL1 = 0;
	}
	if (!USBID)
		USBID = 0x04835750;
}

static void u_set_speed(DWORD cy)
{
	int cnt = 0;

	cmdbuf[0] = 0;
	cmdbuf[1] = 0;
	cmdbuf[2] = CMD_TAP_SETSPD;
	*(DWORD *)(cmdbuf + 3) = cy;

	cnt = libusb_bulk_write(cmdbuf, 7);
	if (cnt != 7)
		printf("STM HID-Brjtag set speed wr error (%d)\n", cnt);

	while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
	       st[0] != ST_MCU_RDY) {}
	cnt = libusb_bulk_read(cmdbuf, 200);
	if (cnt != 1 || cmdbuf[0])
		printf("MCU set speed error!\n");
}

static void u_get_mcuversion(void)
{
	int cnt = 0;
	int i = 1;

	while (i--) {
		cmdbuf[0] = 0;
		cmdbuf[1] = 0;
		cmdbuf[2] = CMD_TAP_GETHWVER;
		cnt = libusb_bulk_write(cmdbuf, 3);
		if (cnt != 3)
			printf("STM HID-Brjtag get HW version wr error (%d)\n", cnt);

		cnt = libusb_bulk_read(cmdbuf, 200);
		if (cnt == 4 && cmdbuf[0] == 'B' && cmdbuf[1] == 'r')
			printf("HID-Brjtag MCU ROM version: %d.%02d on STM32!\n",
			       cmdbuf[3], cmdbuf[2]);
		else
			printf("HID-Brjtag MCU ROM version not fetched!\n");
	}
}

static void fill_cable_prop(void *p)
{
	cable_prop_type *pcbl = (cable_prop_type *)p;

	pcbl->feature = 0;
	pcbl->close = stclose;
	pcbl->test_reset = sttest_reset;
	pcbl->det_instr = stdet_instr;
	pcbl->set_instr = stset_instr;
	pcbl->ReadWriteData = stReadWriteData;
	pcbl->ReadData = stReadData;
	pcbl->WriteData = stWriteData;
	pcbl->feature |= CBL_DMA_RD | CBL_DMA_WR;
	pcbl->ejtag_dma_read_x = stejtag_dma_read_x;
	pcbl->ejtag_dma_write_x = stejtag_dma_write_x;
	pcbl->ejtag_pracc_read_x = 0;
	pcbl->ejtag_pracc_write_x = 0;
	pcbl->sflash_blkread = st_sflash_blkread;
	pcbl->sflash_blkwrite = st_sflash_blkwrite;
}

////////////////////// iNIT  USB device /////////////////////////////////////////////////////
int stinit(void *p)
{
	char vname[] = "Brjtag";

	fill_cable_prop(p);
	u_getconfig();
	libusb_open(USBID, JL_EP1, JL_EP2, 0, vname, NULL, NULL);
	if (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 && st[0] != ST_MCU_IDLE)
		libusb_msg_write(CUSTOM_RQ_RESET, 0, 0, st, 0);
	u_get_mcuversion();
	u_set_speed(LL1);
	return 1;
}

void stclose(void)
{
	libusb_close();
}

void sttest_reset(void)
{
	int cnt = 0;

	cmdbuf[0] = 0;
	cmdbuf[1] = 0;
	cmdbuf[2] = CMD_TAP_RESET;
	cnt = libusb_bulk_write(cmdbuf, 3);
	if (cnt != 3)
		printf("STM HID-Brjtag tap reset wr error (%d)\n", cnt);
	mssleep(1);
	cnt = libusb_bulk_read(cmdbuf, 200);
	if (cnt != 1 || cmdbuf[0])
		printf("MCU reset error!\n");
}

DWORD stdet_instr(void)
{
	int cnt = 0;
	DWORD *dd;

	cmdbuf[0] = 0;
	cmdbuf[1] = 0;
	cmdbuf[2] = CMD_TAP_DETIR;
	cmdbuf[3] = 32; //32
	dd = (DWORD *)(cmdbuf + 4);
	*dd = 0xFFFFFFFF;

	while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
	       st[0] != ST_MCU_IDLE) {}
	cnt = libusb_bulk_write(cmdbuf, 8);
	if (cnt != 8) {
		printf(" cmd [detir] write to usb error! len = %d\n", cnt);
		return 0;
	}
	// ussleep(100);
	while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
	       st[0] != ST_MCU_RDY) {}
	cnt = libusb_bulk_read(cmdbuf, 20);
	if (cnt != 4) {
		printf(" cmd [detir] read from usb error! len = %d\n", cnt);
		return 0;
	}
	dd = (DWORD *)cmdbuf;
	return *dd;
}

DWORD stset_instr(DWORD instr)
{
	int cnt = 0;
	DWORD *dd;
	// int dlen;

	cmdbuf[0] = 0;
	cmdbuf[1] = 0;
	cmdbuf[2] = CMD_TAP_SETIR;
	cmdbuf[3] = instruction_length;
	dd = (DWORD *)(cmdbuf + 4);
	*dd = instr;
	// dlen = (cmdbuf[1] + 7) >> 3;

	while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
	       st[0] != ST_MCU_IDLE) {}
	cnt = libusb_bulk_write(cmdbuf, 8);
	if (cnt != 8)
		printf(" cmd [setir] write to usb error! len = %d\n", cnt);
	ussleep(100);
	dd = (DWORD *)cmdbuf;
	return *dd;
}

//////////////////////////////////////////////////////////////////////////////////////
// RW 32bits
DWORD stReadWriteData(DWORD data)
{
	int cnt = 0;
	DWORD *dd;

	cmdbuf[0] = 0;
	cmdbuf[1] = 0;
	cmdbuf[2] = CMD_TAP_DR32; //det ir
	dd = (DWORD *)(cmdbuf + 3);
	*dd = data;

	while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
	       st[0] != ST_MCU_IDLE) {}
	cnt = libusb_bulk_write(cmdbuf, 7);
	if (cnt != 7) {
		printf(" cmd [rwdata] write to usb error! len = %d\n", cnt);
		return 0;
	}

	while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
	       st[0] != ST_MCU_RDY) {}
	cnt = libusb_bulk_read(cmdbuf, 200);
	if (cnt != 4) {
		printf(" cmd [rwdata] read from usb error! len = %d\n", cnt);
		return 0;
	}
	dd = (DWORD *)cmdbuf;
	return *dd;
}

// RO 32bits
DWORD stReadData(void)
{
	return stReadWriteData(0);
}

// WO 32bits
void stWriteData(DWORD data)
{
	stReadWriteData(data);
}

///////////////////////////////////////////////////////////////////////////
DWORD stejtag_dma_read_x(DWORD addr, int mode)
{
	int k, cnt = 0;
	DWORD data;
	DWORD *dd;

	cmdbuf[0] = 0;
	cmdbuf[1] = 0;
	cmdbuf[2] = CMD_TAP_DMAREAD;
	cmdbuf[3] = (mode & 0x03);        //| (instruction_length <<4);
	dd = (DWORD *)(cmdbuf + 4);
	*dd = addr;

	while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
	       st[0] != ST_MCU_IDLE) {}
	cnt = libusb_bulk_write(cmdbuf, 8);

	if (cnt != 8) {
		printf(" cmd [dmard] write to usb error! len = %d\n", cnt);
		return 0xFFFFFFFF;
	}

	while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
	       st[0] != ST_MCU_RDY) {
		ussleep(100);
	}
	cnt = libusb_bulk_read(cmdbuf, 200);
	if (cnt != 4) {
		printf(" cmd [dmard] read from usb error! len = %d\n", cnt);
		return 0xFFFFFFFF;
	}
	data = *(DWORD *)(cmdbuf);

	switch (mode) {
	case MIPS_WORD:
		break;
	case MIPS_HALFWORD:
		k = addr & 0x2;
		if (BigEndian)
			data = (data >> (8 * (2 - k))) & 0xffff;  //low 16 at high
		else	//little
			data = (data >> (8 * k)) & 0xffff;      //low 16 at low
		break;
	case MIPS_BYTE:
		k = addr & 0x3;
		if (BigEndian)
			data = (data >> (8 * (3 - k))) & 0xff;    //low 8 at high
		else	//little
			data = (data >> (8 * k)) & 0xff;        //low 8 at low
		break;
	default:      //not supported mode
		data = 0xFFFFFFFF;
		break;
	}
	return(data);
}

void stejtag_dma_write_x(DWORD addr, DWORD data, int mode)
{
	int cnt = 0;
	DWORD *dd;

	cmdbuf[0] = 0;
	cmdbuf[1] = 0;
	cmdbuf[2] = CMD_TAP_DMAWRITE;
	cmdbuf[3] = (mode & 0x03);         // | (instruction_length <<4);
	dd = (DWORD *)(cmdbuf + 4);
	*dd = addr;
	dd = (DWORD *)(cmdbuf + 8);
	*dd = data;

	while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
	       st[0] != ST_MCU_IDLE) {}
	cnt = libusb_bulk_write(cmdbuf, 12);
	if (cnt != 12) {
		printf(" cmd [dmawr] write to usb error! len = %d\n", cnt);
		printf(" cmd [dmawr] addr,data %08X,%08X\n", addr, data);
		return;
	}
	ussleep(100);
}

////////////////////////////////////////////////////////////////////
int st_sflash_blkread(DWORD Inaddr, DWORD *pbuff, int len)
{
	int i, cnt = 0;
	DWORD data;
	DWORD *dd;

	len = (len > 512) ? 512 : len;

	cmdbuf[0] = 0;
	cmdbuf[1] = 0;
	cmdbuf[2] = CMD_TAP_DMABLKRD32;
	cmdbuf[3] = MIPS_WORD;              // | (instruction_length <<4);
	dd = (DWORD *)(cmdbuf + 4);
	*dd = Inaddr;
	*(WORD *)(cmdbuf + 8) = len;

	while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
	       st[0] != ST_MCU_IDLE) {}
	cnt = libusb_bulk_write(cmdbuf, 10);
	if (cnt != 10) {
		printf(" cmd [dmablkrd] write to usb error! len = %d\n", cnt);
		return 0;
	}

	while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
	       st[0] != ST_MCU_RDY) {
		ussleep(50);
	}
	cnt = libusb_bulk_read(cmdbuf, 2100);

	if (cnt < 4 * len) {
		printf(" cmd [dmablkrd] read from usb error! len = %d\n", cnt);
		return 0;
	}

	for (i = 0; i < len; i++) {
		data = *(DWORD *)(cmdbuf + 4 * i);
		if (BigEndian)
			data = rev_endian(data);
		pbuff[i] = data;
	}
	return len;
}

int st_sflash_blkwrite(DWORD Inaddr, DWORD *pbuff, int len, int flpg_x8)
{
	int i, cnt, ilen, xfer_op;
	WORD *dl;
	DWORD data_poll, data, addr;
	DWORD *dd;

	ilen = LL8;
	if (ilen > 200)
		ilen = 200; // supported max x16 mode DWORD number, guarentee buffer not overflow.
	if (len == 0)
		return ilen; // answer back the caller query result for max supported block size
	if (len > ilen)
		len = ilen;

	gIndex = 0;   //buffer clean
	xfer_op = flpg_x8 ? MIPS_BYTE : MIPS_HALFWORD;
	ilen = flpg_x8 ? 4 : 2;

	// file cmd head
	cmdbuf[0] = 0;
	cmdbuf[1] = 0;
	cmdbuf[2] = CMD_TAP_FLSHBLKWR;
	cmdbuf[3] = xfer_op;                       //| (instruction_length <<4);
	dd = (DWORD *)(cmdbuf + 4);
	*dd = FLASH_MEMORY_START;
	dl = (WORD *)(cmdbuf + 8);
	*dl = len * ilen;
	dd = (DWORD *)(cmdbuf + 10);
	*dd = Inaddr;
	gIndex += 14;

	for (i = 0; i < len; i++) {
		data = pbuff[i];
		addr = Inaddr + 4 * i;
		if (flpg_x8) {
			u_buf_write_x8((addr & (~3)), data);
			u_buf_delayus(LL4);
			u_buf_write_x8((addr & (~3)) + 1, data);
			u_buf_delayus(LL4);
			u_buf_write_x8((addr & (~3)) + 2, data);
			u_buf_delayus(LL4);
			u_buf_write_x8((addr & (~3)) + 3, data);
			u_buf_delayus(LL4);
		} else {
			u_buf_write_x16((addr & (~3)), data);
			u_buf_delayus(LL4);
			u_buf_write_x16((addr & (~3)) + 2, data);
			u_buf_delayus(LL4);
		}
	}
	if ((cmd_type != CMD_TYPE_AMD) || !bypass) {
		// fetch polling data
		cmdbuf[gIndex] = CMD_TAP_DMABLKRD32;
		cmdbuf[gIndex + 1] = MIPS_WORD;                   // | (instruction_length <<4);
		dd = (DWORD *)(cmdbuf + gIndex + 2);
		*dd = Inaddr;
		dd = (DWORD *)(cmdbuf + gIndex + 6);
		*dd = (DWORD)len & 0xFF;
		gIndex += 7;
		if (gIndex & 1) gIndex++;
	}

	//     if(gIndex%64 == 0)      //mcu rx len can't equal to N*64
	*(WORD *)cmdbuf = gIndex;
	//     	   cmdbuf[gIndex++] = CMD_TAP_NOOP;
	while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
	       st[0] != ST_MCU_IDLE) {}
	cnt = libusb_bulk_write(cmdbuf, gIndex);

	if (cnt != gIndex) {
		printf(" cmd [flshwrite] write to usb error! len = %d\n", cnt);
		return 0;
	}

	if ((cmd_type != CMD_TYPE_AMD) || !bypass) {
		while (libusb_msg_read(CUSTOM_RQ_GET_STATUS, 0, 0, st, 1) == 1 &&
		       st[0] != ST_MCU_RDY) {
			ussleep(50);
		}
		cnt = libusb_bulk_read((char *)tmpbuf, 2100);
		if (cnt < 4 * len) {
			printf(" cmd [flshwrite] read from usb error! len = %d\n", cnt);
			return 0;
		}
		// check polling data

		for (i = 0; i < len; i++) {
			data = *(pbuff + i);
			data_poll = *(tmpbuf + i);
			if (BigEndian)
				data_poll = rev_endian(data_poll);
			if (data_poll != data)
				printf("\nDMA write error!\n [%d] %08X - %08X\n", i, data, data_poll);
		}
	}
	return len;
}

static void u_buf_write_x16(DWORD addr, DWORD data)
{
	int k;
	DWORD ldata, odata;

	k = (addr & 0x2) >> 1;
	if (BigEndian) {
		odata = rev_endian(data) & BEMASK16(k);
		ldata = odata >> (16 * (1 - k));
	} else {
		odata = data & LEMASK16(k);
		ldata = odata >> (16 * k);
	}

	if (ldata == 0xffff) { // no need to program
		skip = 1;
		return;
	}
	switch (cmd_type) {
	case CMD_TYPE_AMD:
		if (!bypass) {
			u_buf_write_I(AMD16_I1);
			u_buf_write_I(AMD16_I2);
		}
		u_buf_write_I(AMD16_I3);
		break;
	case CMD_TYPE_SST:
		u_buf_write_I(SST16_I1);
		u_buf_write_I(SST16_I2);
		u_buf_write_I(SST16_I3);
		break;
	case CMD_TYPE_BCS:
	case CMD_TYPE_SCS:
	default:
		u_buf_write_I(INTL16_I1);
		u_buf_write_I(INTL16_I2);
		break;
	}

	u_buf_write(odata);
}

static void u_buf_write_x8(DWORD addr, DWORD data)
{
	int k;
	DWORD ldata, odata;

	k = addr & 0x3;
	ldata = (data >> (8 * k)) & 0xFF;
	odata = ldata | (ldata << 8);
	odata |= (odata << 16);

	// printf(" I am in x8, addr, data = %08x, %08x, %08x\n", addr, data,ldata);
	if (ldata == 0xff) { // no need to program
		skip = 1;
		return;
	}

	switch (cmd_type) {
	case CMD_TYPE_AMD:
		if (!bypass) {
			u_buf_write_I(AMD8_I1);
			u_buf_write_I(AMD8_I2);
		}
		u_buf_write_I(AMD8_I3);
		break;
	case CMD_TYPE_SST:
		return; // SST 39 doesn't support x8
	case CMD_TYPE_BCS:
	case CMD_TYPE_SCS:
	default:
		u_buf_write_I(INTL8_I1);
		u_buf_write_I(INTL8_I2);
		break;
	}
	u_buf_write(odata);
}

static void u_buf_write(DWORD data)
{
	DWORD *dd;

	cmdbuf[gIndex + 0] = 0;
	dd = (DWORD *)(cmdbuf + gIndex + 1);
	*dd = data;
	gIndex += 5;
}

static void u_buf_write_I(DWORD id)
{
	cmdbuf[gIndex + 0] = (BYTE)(0x80 | id);
	gIndex++;
}

static void u_buf_delayus(DWORD us)
{
	cmdbuf[gIndex + 0] = 0xFF;
	if (skip)
		cmdbuf[gIndex + 1] = 0;
	else
		cmdbuf[gIndex + 1] = (BYTE)(us);
	gIndex += 2;
	skip = 0;
}
