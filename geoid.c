#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "geoid.h"

/*
 * This file is mostly derived from GeographicLib/src/Geoid.cpp
 */
/**
* \file Geoid.cpp
* \brief Implementation for GeographicLib::Geoid class
*
* Copyright (c) Charles Karney (2009) <charles@karney.com>
* and licensed under the LGPL.  For more information, see
* http://geographiclib.sourceforge.net/
**********************************************************************/

struct geoid_ctx {
	/* raw should be (uint16_t *) but it might not be aligned */
	unsigned char *raw;
	double offset;
	double scale;
	double lonres;
	double latres;
	unsigned int width;
	unsigned int height;
	void *base;
	size_t total_len;
};

struct geoid_ctx *
geoid_init(const char *geoid_pgm_filename, ...)
{
int fd;
struct stat sbuf;
struct geoid_ctx *c;
char *p, *q, *r;
size_t remain;
int have_line;
int line_number;
int expect_depth = 0;
char line[150];
size_t linelen;
va_list ap;

	va_start(ap, geoid_pgm_filename);
	for (;;) {
		if ((fd = open(geoid_pgm_filename, O_RDONLY)) < 0) {
			if (errno == ENOENT) {
				geoid_pgm_filename = va_arg(ap, const char *);
				if (geoid_pgm_filename) continue;
			}
			fprintf(stderr, "geoid: cannot open \"%s\": %s\n", geoid_pgm_filename, strerror(errno));
			va_end(ap);
			return NULL;
		}
		break;
	}
	va_end(ap);
	if (fstat(fd, &sbuf) < 0) {
		fprintf(stderr, "geoid: stat \"%s\" failed: %s\n", geoid_pgm_filename, strerror(errno));
		close(fd);
		return NULL;
	}
	if (sbuf.st_size < 30) {
		fprintf(stderr, "geoid: \"%s\" file wrong format (too small)\n", geoid_pgm_filename);
		close(fd);
		return NULL;
	}
	if (!(c = malloc(sizeof(*c)))) {
		fprintf(stderr, "geoid: malloc failed: %s\n", strerror(errno));
		close(fd);
		return NULL;
	}
	c->total_len = sbuf.st_size;
	c->offset = 0.0;
	c->scale = 1.0;
	if ((c->base = mmap(NULL, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		fprintf(stderr, "geoid: mmap \"%s\" failed: %s\n", geoid_pgm_filename, strerror(errno));
		close(fd);
		free(c);
		return NULL;
	}
	close(fd);
	p = c->base;
	remain = sbuf.st_size;
	if ((p[0] != 'P') || (p[1] != '5')) {
nopgmheader:
		fprintf(stderr, "geoid: \"%s\": No PGM header\n", geoid_pgm_filename);
error:
		munmap(c->base, sbuf.st_size);
		free(c);
		return NULL;
	}
	if ((p[2] == 13) && (p[3] == 10)) {
		p += 4;
		remain -= 4;
	} else if (p[2] == 10) {
		p += 3;
		remain -= 3;
	} else {
		goto nopgmheader;
	}
	line_number = 1;
	for (;;) {
		linelen = 0;
		have_line = 0;
		line_number++;
		while (remain) {
			if (linelen == (sizeof(line)-1)) {
				fprintf(stderr, "geoid: \"%s\" line %d: Line too long in header\n", geoid_pgm_filename, line_number);
				goto error;
			}
			if ((remain > 1) && (p[0] == 13) && (p[1] == 10)) {
				p += 2;
				remain -= 2;
				have_line = 1;
				break;
			} else if (p[0] == 10) {
				p++;
				remain--;
				have_line = 1;
				break;
			}
			line[linelen++] = *(p++);
			remain--;
		}
		if (!have_line) {
			fprintf(stderr, "geoid: \"%s\" line %d: Reached EOF before end of line in header\n", geoid_pgm_filename, line_number);
			goto error;
		}
		line[linelen] = 0;

		if (expect_depth) {
			have_line = strtoul(line, &q, 10);
			if ((q == (&(line[0]))) || (*q)) {
				fprintf(stderr, "geoid: \"%s\" line %d: expected depth (single unsigned int)\n", geoid_pgm_filename, line_number);
				goto error;
			}
			if (have_line != 65535) {
				fprintf(stderr, "geoid: \"%s\": only PGM files with depth 65535 supported\n", geoid_pgm_filename);
				goto error;
			}
			break;
		}
		if (strncmp(line, "# Offset ", 9) == 0) {
			c->offset = strtod(line + 9, &q);
			if ((q == (&(line[9]))) || (*q)) {
				fprintf(stderr, "geoid: \"%s\" line %d: expected offset (float)\n", geoid_pgm_filename, line_number);
				goto error;
			}
			continue;
		} else if (strncmp(line, "# Scale ", 8) == 0) {
			c->scale = strtod(line + 8, &q);
			if ((q == (&(line[8]))) || (*q)) {
				fprintf(stderr, "geoid: \"%s\" line %d: expected scale (float)\n", geoid_pgm_filename, line_number);
				goto error;
			}
			continue;
		} else if (line[0] == '#') {
			continue;
		}
		c->width = strtoul(line, &q, 10);
		if ((q == (&(line[0]))) || (*q != ' ')) {
whproblem:
			fprintf(stderr, "geoid: \"%s\" line %d: expected \"width height\"\n", geoid_pgm_filename, line_number);
			goto error;
		}
		r = q+1;
		c->height = strtoul(r, &q, 10);
		if ((q == r) || (*q)) goto whproblem;
		expect_depth = 1;
	}
	if ((c->width * c->height * 2) != remain) {
		fprintf(stderr, "geoid: \"%s\": expected %d bytes after header, have %lu\n",
			geoid_pgm_filename,
			c->width * c->height * 2, (unsigned long)remain
		);
		goto error;
	}
	c->raw = (unsigned char *)p;
	c->lonres = c->width / 360.0;
	c->latres = (c->height - 1) / 180.0;
	return c;
}

void
geoid_free(struct geoid_ctx *c)
{
	munmap(c->base, c->total_len);
	free(c);
}

static unsigned int
_rawval(struct geoid_ctx *c, int x, int y)
{
unsigned char *p;

	p = c->raw + ((y * c->width + x) << 1);
	return (p[0] << 8) | p[1];
}

double
geoid_height_linear(struct geoid_ctx *c, double lat, double lon)
{
double fx, fy, ixf, iyf;
int v00, v01, v10, v11;
int ix, iy;

	if (lon < 0.0) lon += 360.0;
	fy = modf((90 - lat) * c->latres, &iyf);
	fx = modf(lon * c->lonres, &ixf);
	ix = ixf; iy = iyf;
	if (iy == (c->height - 1)) iy -= 1;

	v00 = _rawval(c, ix  , iy  );
	v01 = _rawval(c, ix+1, iy  );
	v10 = _rawval(c, ix  , iy+1);
	v11 = _rawval(c, ix+1, iy+1);

	return c->offset + c->scale * (
		(1 - fy) * ((1 - fx) * v00 + fx * v01) +
		fy * ((1 - fx) * v10 + fx * v11)
	);
}

#ifdef GEOID_HEIGHT_SUPPORT_CUBIC_INTERPOLATION
double
geoid_height_cubic(struct geoid_ctx *c, double lat, double lon)
{
static const double c0 = 240.0;
static const int c3[] = {
	  9, -18, -88,    0,  96,   90,   0,   0, -60, -20,
	 -9,  18,   8,    0, -96,   30,   0,   0,  60, -20,
	  9, -88, -18,   90,  96,    0, -20, -60,   0,   0,
	186, -42, -42, -150, -96, -150,  60,  60,  60,  60,
	 54, 162, -78,   30, -24,  -90, -60,  60, -60,  60,
	 -9, -32,  18,   30,  24,    0,  20, -60,   0,   0,
	 -9,   8,  18,   30, -96,    0, -20,  60,   0,   0,
	 54, -78, 162,  -90, -24,   30,  60, -60,  60, -60,
	-54,  78,  78,   90, 144,   90, -60, -60, -60, -60,
	  9,  -8, -18,  -30, -24,    0,  20,  60,   0,   0,
	 -9,  18, -32,    0,  24,   30,   0,   0, -60,  20,
	  9, -18,  -8,    0, -24,  -30,   0,   0,  60,  20,
};
static const double c0n = 372.0;
static const int c3n[] = {
	  0, 0, -131, 0,  138,  144, 0,   0, -102, -31,
	  0, 0,    7, 0, -138,   42, 0,   0,  102, -31,
	 62, 0,  -31, 0,    0,  -62, 0,   0,    0,  31,
	124, 0,  -62, 0,    0, -124, 0,   0,    0,  62,
	124, 0,  -62, 0,    0, -124, 0,   0,    0,  62,
	 62, 0,  -31, 0,    0,  -62, 0,   0,    0,  31,
	  0, 0,   45, 0, -183,   -9, 0,  93,   18,   0,
	  0, 0,  216, 0,   33,   87, 0, -93,   12, -93,
	  0, 0,  156, 0,  153,   99, 0, -93,  -12, -93,
	  0, 0,  -45, 0,   -3,    9, 0,  93,  -18,   0,
	  0, 0,  -55, 0,   48,   42, 0,   0,  -84,  31,
	  0, 0,   -7, 0,  -48,  -42, 0,   0,   84,  31,
};
static const double c0s = 327.0;
static const int c3s[] = {
	 18,  -36, -122,   0,  120,  135, 0,   0,  -84, -31,
	-18,   36,   -2,   0, -120,   51, 0,   0,   84, -31,
	 36, -165,  -27,  93,  147,   -9, 0, -93,   18,   0,
	210,   45, -111, -93,  -57, -192, 0,  93,   12,  93,
	162,  141,  -75, -93, -129, -180, 0,  93,  -12,  93,
	-36,  -21,   27,  93,   39,    9, 0, -93,  -18,   0,
	  0,    0,   62,   0,    0,   31, 0,   0,    0, -31,
	  0,    0,  124,   0,    0,   62, 0,   0,    0, -62,
	  0,    0,  124,   0,    0,   62, 0,   0,    0, -62,
	  0,    0,   62,   0,    0,   31, 0,   0,    0, -31,
	-18,   36,  -64,   0,   66,   51, 0,   0, -102,  31,
	 18,  -36,    2,   0,  -66,  -51, 0,   0,  102,  31,
};
double fx, fy, ixf, iyf;
int v[12];
double t[10];
int ix, iy, i, j;
int acc;
double c0x;
const int *c3x;

	if (lon < 0.0) lon += 360.0;
	fy = modf((90 - lat) * c->latres, &iyf);
	fx = modf(lon * c->lonres, &ixf);
	ix = ixf; iy = iyf;
	if (iy == (c->height - 1)) iy -= 1;

	v[ 0] = _rawval(c, ix  , iy-1);
	v[ 1] = _rawval(c, ix+1, iy-1);
	v[ 2] = _rawval(c, ix-1, iy  );
	v[ 3] = _rawval(c, ix  , iy  );
	v[ 4] = _rawval(c, ix+1, iy  );
	v[ 5] = _rawval(c, ix+2, iy  );
	v[ 6] = _rawval(c, ix-1, iy+1);
	v[ 7] = _rawval(c, ix  , iy+1);
	v[ 8] = _rawval(c, ix+1, iy+1);
	v[ 9] = _rawval(c, ix+2, iy+1);
	v[10] = _rawval(c, ix  , iy+2);
	v[11] = _rawval(c, ix+1, iy+2);

	if (iy == 0) {
		c3x = c3n;
		c0x = c0n;
	} else if (iy == (c->height - 2)) {
		c3x = c3s;
		c0x = c0s;
	} else {
		c3x = c3;
		c0x = c0;
	}

	for (i = 0; i < 10; i++) {
		acc = 0;
		for (j = 0; j < 12; j++) acc += v[j] * c3x[j*10+i];
		t[i] = ((double)acc) / c0x;
	}

	return c->offset + c->scale * (
		t[0] +
		fx * (t[1] + fx * (t[3] + fx * t[6])) +
		fy * (
			t[2] + fx * (t[4] + fx * t[7]) +
			fy * (t[5] + fx * t[8] + fy * t[9])
		)
	);
}
#endif
