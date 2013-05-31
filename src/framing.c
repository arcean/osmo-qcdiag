/*
 * Author: Dieter Spaar  
 * October 2012
 *
 * Modified by Holger Freyther
 */

#include "framing.h"

#include <stdio.h>
#include <string.h>


#define PACKET_START_STOP   0x7E
#define PACKET_ESCAPE       0x7D

#define CRC_SEED        0xFFFF

/*
 * Comment is from: utils.c of libqcdm
 * QCDM protocol frames are pseudo Async HDLC frames which end with a 3-byte
 * trailer.  This trailer consists of the 16-bit CRC of the frame plus an ending
 * "async control character" whose value is 0x7E.  The frame *and* the CRC are
 * escaped before adding the trailing control character so that the control
 * character (0x7E) and the escape marker (0x7D) are never seen in the frame.
 */


/* 16 bit CRC, generator polynomial 0x8408 */
static const uint16_t crc_ccit16[256] = 
{
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

static uint16_t crc16(const uint8_t *data, size_t len)
{
	uint16_t crc = CRC_SEED;

	for(; len != 0; len--, data++) 
		crc = crc_ccit16[(crc ^ *data) & 0x00ff] ^ (crc >> 8);

    	/* return the 1's complement */
    	return ~crc; 
}

#define DO_ESCAPE(x, idx, out, out_len) \
    if(x == PACKET_START_STOP || x == PACKET_ESCAPE) \
    { \
        if(idx + 2 > out_len) \
        { \
            printf("output buffer too small need %d have %d\n", idx + 2, out_len); \
            return -1; \
        } \
        out[idx++] = PACKET_ESCAPE; \
        out[idx++] = (x ^ 0x20); \
    } \
    else \
    { \
        if(idx + 1 > out_len) \
        { \
            printf("output buffer too small need %d have %d\n", idx + 2, out_len); \
            return -1; \
        } \
        out[idx++] = x; \
    } \

int frame_pack(const uint8_t *in, const size_t in_len, uint8_t *out, const size_t out_len)
{
	uint8_t c;
	size_t i, idx = 0;

	/* packet data */
	for (i = 0; i < in_len; i++) {
		c = in[i];
		DO_ESCAPE(c, idx, out, out_len);
	}
    
    	/* two bytes CRC */
	uint16_t crc = crc16(in, in_len);

	c = (uint8_t) (crc & 0xFF);
	DO_ESCAPE(c, idx, out, out_len);
    
	c = (uint8_t) (crc >> 8);
    	DO_ESCAPE(c, idx, out, out_len);

	/* end of packet */
	out[idx++] = PACKET_START_STOP;
	return idx;
}

#undef DO_ESCAPE

/* returns number of bytes in pPacket if a packet is complete */
int frame_unpack(uint8_t *pDataIn, int nDataIn, uint8_t *pPacket)
{
	static uint8_t ubBuf[MAX_PACKET * 2];
	static int nDataBuf = 0;
	int i;

	if(nDataIn > sizeof(ubBuf)) {
		printf("Too many data\n");
		return -1;
	}

	/* is there enough free space in the buffer ? */

	if(nDataIn + nDataBuf >= sizeof(ubBuf)) {
		/* no, empty buffer */
		nDataBuf = 0;
	}

	/* append data to buffer */
	memcpy(ubBuf + nDataBuf, pDataIn, nDataIn);
	nDataBuf += nDataIn;

	/* search for packet end */
	int idxStop = -1;
	for (i = 0; i < nDataBuf; i++) {
		if (ubBuf[i] == PACKET_START_STOP) {
			idxStop = i;
			break;
		}
	}

	if (idxStop < 0)
		return 0;

	/* get packet */

	int nDataPacket = 0;
	int bEscape = 0;
	for (i = 0; i < idxStop; i++)
	{
		if (ubBuf[i] == PACKET_ESCAPE) {
			bEscape = 1;
		} else {
			if (bEscape)
				pPacket[nDataPacket++] = (ubBuf[i] ^ 0x20);
			else
				pPacket[nDataPacket++] = ubBuf[i];
			bEscape = 0;
		}
		if (nDataPacket >= MAX_PACKET) {
			printf("Too many packet data\n");
			return -1;
		}
	}

	/* remove packet from buffer */
	memmove(ubBuf, ubBuf + idxStop + 1, nDataBuf - (idxStop + 1));
	nDataBuf -= (idxStop + 1);

	/* check CRC */

	if (nDataPacket <= 2) {
		printf("Not enough packet data\n");
		return -1;
	}

	uint16_t crc = crc16(pPacket, nDataPacket - 2);
	if((crc & 0xFF) != pPacket[nDataPacket - 2] || 
			(crc >> 8) != pPacket[nDataPacket - 1]) {
		printf("Invalid CRC (0x%04X != 0x%02X%02X)\n",
			crc, pPacket[nDataPacket - 1], pPacket[nDataPacket - 2]);
		return -1;
	}

	return nDataPacket - 2;
}
