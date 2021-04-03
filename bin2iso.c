/*
 * bin2iso (C) 2000 by DeXT
 *
 * This is a very simple utility to convert a BIN image
 * (either RAW/2352 or Mode2/2336 format) to standard ISO format (2048 b/s).
 *
 * Structure of images are as follows:
 * Mode 1 (2352): Sync (12), Address (3), Mode (1), Data (2048), ECC (288)
 * Mode 2 (2352): Sync (12), Address (3), Mode (1), Subheader (8), Data (2048), ECC (280)
 * Mode 2 (2336): Subheader (8), Data (2048), ECC (280)
 * Mode 2 / 2336 is the same as Mode 2 / 2352 but without header (sync+addr+mode)
 *
 * Sector size is detected by the presence of Sync data.
 * Mode is detected from Mode field.
 *
 * Changelog:
 * 2000-11-16 - Added mode detection for RAW data images (adds Mode2/2352 support)
 * 2007-03-13 - Added input validation checks (Magnus-swe).
 * 2021-04-03 - Added error handling and image consistency checks
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODE_OFFSET 15
static const char SYNC_HEADER[12] =
	{ 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0 };

static void warn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fputs("warning: ", stderr);
	vfprintf(stderr, fmt, ap);
	fputs("\n", stderr);
	va_end(ap);
}

static void die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputs("\n", stderr);
	va_end(ap);
	exit(EXIT_FAILURE);
}

static void *xmalloc(size_t sz)
{
	void *buf = malloc(sz);
	if (!buf)
		die("Out of memory");
}

static void xfread(char *buf, size_t sz, FILE *stream)
{
	if (fread(buf, sz, 1, stream) != 1)
		die("Read error: %s", strerror(errno));
}

static void xfwrite(char *buf, size_t sz, FILE *stream)
{
	if (fwrite(buf, sz, 1, stream) != 1)
		die("Write error: %s", strerror(errno));
}

static void xfseek(FILE *stream, long offset, int whence)
{
	if (fseek(stream, offset, whence) != 0)
		die("Seek error: %s", strerror(errno));
}

int main(int argc, char **argv)
{
	FILE  *dst, *src;
	char  *dstname;
	char   buf[2352];
	char   mode;
	long   img_sz;
	long   nr_sectors;
	size_t hdr_sz, sector_sz, tail_sz;

	if (argc != 2 && argc != 3)
		die("usage: bin2iso image.bin [image.iso]");

	if (argc == 3) {
		dstname = argv[2];
	} else {
		size_t namelen = strlen(argv[1]);
		if (namelen < 5 || strcmp(argv[1] + namelen - 4, ".bin"))
			namelen += 4;
		dstname = xmalloc(namelen);
		strcpy(dstname, argv[1]);
		strcpy(dstname + namelen - 4, ".iso");
	}

	if ((src = fopen(argv[1], "rb")) == NULL)
		die("Source file does not exist");

	if ((dst = fopen(dstname, "wb")) == NULL)
		die("Cannot write to destination file");

	xfread(buf, 16, src);

	if (memcmp(SYNC_HEADER, buf, sizeof SYNC_HEADER)) {
		/* Mode 2 / 2336 */
		mode      = 0;
		hdr_sz    = 8;
		sector_sz = 2336;
	} else {
		mode = buf[MODE_OFFSET];
		switch (mode) {
			case 2: /* Mode 2 / 2352 */
				hdr_sz    = 24;
				sector_sz = 2352;
				break;
			case 1: /* Mode 1 / 2352 */
				hdr_sz    = 16;
				sector_sz = 2352;
				break;
			default:
				die("Unsupported track mode %u", mode);
		}
	}

	xfseek(src, 0L, SEEK_END);
	img_sz = ftell(src);
	if (img_sz == -1)
		die("Cannot determine source file size");
	tail_sz = img_sz % sector_sz;
	if (tail_sz) {
		warn("Image size is not a factor of sector size %zu, "
		     "last %zu bytes will be dropped", sector_sz, tail_sz);
	}
	nr_sectors = img_sz / sector_sz;
	rewind(src);

	for (long i = 0; i < nr_sectors; i++) {
		xfread(buf, sector_sz, src);
		if (mode && buf[MODE_OFFSET] != mode) {
			warn("Sector %lu has different mode (%u instead of %u)",
			     i, buf[MODE_OFFSET], mode);
		}
		xfwrite(&buf[hdr_sz], 2048, dst);
	}

	fclose(dst);
	fclose(src);
	if (dstname != argv[2])
		free(dstname);

	exit(EXIT_SUCCESS);
}
