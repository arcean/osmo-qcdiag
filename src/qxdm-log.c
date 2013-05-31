#include "framing.h"
#include "serial.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

char *DumpBYTEs(unsigned char *p, long n, int nBytesPerRow /* = 16 */, char *szLineSep /* = "\n" */, int bRaw /* = FALSE */, const char *szIndent /* = "" */)
{
  long i;
  char szBuf[20];
  static char szRes[4 * 1024];

  szRes[0] = 0;

  if(n == 0)
    return szRes;

  memset(szBuf, 0, sizeof(szBuf));
  for(i = 0; i < n; i++)
  {
    if(i % nBytesPerRow == 0)
    {
        strcat(szRes, szIndent);
        if(!bRaw)
            sprintf(szRes + strlen(szRes), "%04d : ", i);
    }

    sprintf(szRes + strlen(szRes), "%02X ", p[i]);
    szBuf[i % nBytesPerRow] = (char)((p[i] < 128 && p[i] >= ' ') ?  p[i] : '.'); 

    if((i + 1) % nBytesPerRow == 0)
    {
      if(bRaw)
        sprintf(szRes + strlen(szRes), "%s", szLineSep);
      else
        sprintf(szRes + strlen(szRes), "  %s%s", szBuf, szLineSep);
      memset(szBuf, 0, sizeof(szBuf));
    } 
  }

  if(i % nBytesPerRow != 0)
  {
    if(bRaw)
      sprintf(szRes + strlen(szRes), "%s", szLineSep);
    else
    {
      n = nBytesPerRow - i % nBytesPerRow;
      for(i = 0; i < n ; i++) 
        sprintf(szRes + strlen(szRes), "   ");
      sprintf(szRes + strlen(szRes), "  %s%s", szBuf, szLineSep);
    }
  }

  return szRes;
}

static int transmit_packet(int fd, const uint8_t *data, size_t data_len)
{
	int out_len, rc;
	uint8_t packet[MAX_PACKET * 2];	

	out_len = frame_pack(data, data_len, packet, sizeof(packet));
	if (out_len < 0) {
		printf("Failed to pack packet\n");
		return -1;
	}

	rc = write(fd, packet, out_len);
	if (rc != out_len) {
		printf("Short write on packet.\n");
		return -1;
	}

	return 0;
}

static int do_read(int fd, uint8_t *data)
{
	uint8_t buf[MAX_PACKET];
	int rc;

	rc = read(fd, buf, sizeof(buf));
	if (rc <= 0 ) {
		printf("Short read!\n");
		exit(EXIT_FAILURE);
	}

	rc = frame_unpack(buf, rc, data);	
	if (rc > 0) {
		printf("Got %d data of payload\n", rc); 
		printf("%s\n", DumpBYTEs(data, rc, 16, "\n", 0, ""));
	}

	return rc;
}

static void do_configure(int fd)
{
	static uint8_t timestamp[] = { 0x1D };
	static uint8_t enable_logging[] = {
		0x73, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
		0x05, 0x00, 0x00, 0x00, 0x28, 0x04, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x02, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t enable_evt_report[] = {
		0x60, 0x01
	};
	static const uint8_t disable_evt_report[] = {
		0x60, 0x00
	};
	static const uint8_t extended_report_cfg[] = {
		0x7D, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x00, 0x00, 0x00,
	};

	uint8_t data[MAX_PACKET];

	/* TODO: introduce a wait for response kind of method */
	transmit_packet(fd, timestamp, sizeof(timestamp));
	do_read(fd, data);

	/* enable the event report */
//	transmit_packet(fd, enable_evt_report, sizeof(enable_evt_report));
//	do_read(fd, data);
	transmit_packet(fd, disable_evt_report, sizeof(disable_evt_report));
	do_read(fd, data);

	transmit_packet(fd, extended_report_cfg, sizeof(extended_report_cfg));
	do_read(fd, data);

	/* enable the logging */
	transmit_packet(fd, enable_logging, sizeof(enable_logging));
	do_read(fd, data);
}

int main(int argc, char **argv)
{
	uint8_t data[MAX_PACKET];
	int flags;

	int fd, rc;
	if (argc < 2) {
		printf("Invoke with %s PATH_TO_SERIAL\n",
			argv[0]);
		return EXIT_FAILURE;
	}

	/* Use nonblock as the device might block otherwise */
	fd = open(argv[1], O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
	if (fd < 0) {
		printf("Opening the serial failed: %d/%s\n",
			errno, strerror(errno));
		return EXIT_FAILURE;
	}

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		printf("Failed to get the flags.\n");
		return EXIT_FAILURE;
	}

	flags &= ~O_NONBLOCK;
	rc = fcntl(fd, F_SETFL, flags);
	if (rc != 0) {
		printf("Failed to set the flags.\n");
		return EXIT_FAILURE;
	}

	rc = serial_configure(fd);
	if (rc != 0)
		return EXIT_FAILURE;

	do_configure(fd);

	while (1)
		do_read(fd, data);
}
