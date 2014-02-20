#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif


typedef enum {
	FF_FLO,
	FF_FLOW,
	FF_LAST
} FLOformat;


// header identification strings
static const char *flo_formats[FF_LAST] = {"PIEH", "PIEI"};


typedef struct {
	int w, h;
	FLOformat fmt;
	int nmemb;
	float *data;
} FLOfile;


FLOfile*
read_flow(char *f)
{
	FILE *fd = fopen(f, "r");
	if (!fd) {
		fprintf(stderr, "Error: Could not open file '%s'.\n", f);
		return NULL;
	}

	char *tag = calloc(4, sizeof(char));
	if (fread(tag, sizeof(char), 4, fd) < 4) {
		fprintf(stderr, "Error: Could not read file header of '%s'\n", f);
		fclose(fd);
		return NULL;
	}

	FLOformat fmt = FF_LAST;
	if (!strncmp(tag, flo_formats[FF_FLO], 4)) {
		fmt = FF_FLO;
	}
	else if (!strncmp(tag, flo_formats[FF_FLOW], 4)) {
		fmt = FF_FLOW;
	}
	else {
		fprintf(stderr, "Error: Invalid file type in file '%s'\n", f);
		fclose(fd);
		return NULL;
	}
	free(tag);

	int w;
	if (fread(&w, sizeof(int), 1, fd) < 1) {
		fprintf(stderr, "Error: Could not read width from file '%s'\n", f);
		fclose(fd);
		return NULL;
	}

	int h;
	if (fread(&h, sizeof(int), 1, fd) < 1) {
		fprintf(stderr, "Error: Could not read height from file '%s'\n", f);
		fclose(fd);
		return NULL;
	}

	if (w <= 0 || h <= 0) {
		fprintf(stderr, "Error: Invalid width or height in file '%s'\n", f);
		fclose(fd);
		return NULL;
	}

	int sz = w * h;
	switch (fmt) {
	case FF_FLO:
		sz *= 2;
		break;
	case FF_FLOW:
		sz *= 3;
		break;
	default:
		// should not happen
		break;
	}
	FLOfile *flow = malloc(sizeof(FLOfile));
	flow->nmemb = sz;
	flow->fmt = fmt;
	flow->w = w;
	flow->h = h;
	flow->data = malloc(flow->nmemb * sizeof(float));
	if (fread(flow->data, 1, flow->nmemb * sizeof(float), fd) < flow->nmemb * sizeof(float)) {
		fprintf(stderr, "Error: Incomplete data in file '%s'\n", f);
		fclose(fd);
		free(flow->data);
		free(flow);
		return NULL;
	}

	fclose(fd);
	return flow;
}


void
free_flow(FLOfile *f)
{
	if (f) free(f->data);
	free(f);
}

#define _U_IDX(flow,x,y, npb) ((y) * ((flow)->w * (npb)) + (x) * (npb) + 0)
#define _V_IDX(flow,x,y, npb) ((y) * ((flow)->w * (npb)) + (x) * (npb) + 1)
#define _C_IDX(flow,x,y, npb) ((y) * ((flow)->w * (npb)) + (x) * (npb) + 2)
#define _U(flow, x, y, npb) ((flow)->data[_U_IDX((flow),(x),(y),(npb))])
#define _V(flow, x, y, npb) ((flow)->data[_V_IDX((flow),(x),(y),(npb))])
#define _C(flow, x, y, npb) ((flow)->data[_C_IDX((flow),(x),(y),(npb))])

static inline
float get_U(FLOfile *f, int x, int y)
{
	int npb = f->fmt == FF_FLO ? 2 : 3;
	return _U(f, x, y, npb);
}

static inline
float get_V(FLOfile *f, int x, int y)
{
	int npb = f->fmt == FF_FLO ? 2 : 3;
	return _V(f, x, y, npb);
}

static inline
float get_C(FLOfile *f, int x, int y)
{
	int npb = f->fmt == FF_FLO ? 2 : 3;
	return _C(f, x, y, npb);
}


char*
strdup(const char *str)
{
	int n = strlen(str) + 1;
	char *dup = malloc(n);
	if (dup) strcpy(dup, str);
	return dup;
}


void
usage(FILE *f, char *pname)
{
	fprintf(f, "Usage: %s left right out\n", pname);
}


bool
test_file(char *f)
{
	struct stat sb;
	if (stat(f, &sb)) {
		fprintf(stderr, "Error: File '%s' does not exist.\n", f);
		return false;
	}
	return true;
}


void
cmp(char *l, char *r, char *o)
{
	float a_mean = 0.0;
	float *a_err = NULL;
	FLOfile *left = read_flow(l);
	FLOfile *right = read_flow(r);

	if (left->w != right->w || left->h != right->h) {
		fprintf(stderr, "Error: Dimension mismatch\n");
		goto cleanup;
	}

	int i = 0;
	int n = 0;
	// int z = 0;
	a_err = calloc(left->w * left->h, sizeof(float));
	for (int y = 0; y < left->h; y++) {
		for (int x = 0; x < left->w; x++, i++) {
			float l_u = get_U(left, x, y);
			float l_v = get_V(left, x, y);

			float r_u = get_U(right, x, y);
			float r_v = get_V(right, x, y);

			float normL = sqrtf(l_u * l_u + l_v * l_v);
			float normR = sqrtf(r_u * r_u + r_v * r_v);


			// both vectors have zero norm -> no vector ->
			// everything ok, keep going
			if (normL == 0.0f && normR == 0.0f) {
				n++;
			}
			// left vector is zero, but right indicates
			// something -> mark in the output data with
			// -1e9
			else if (normL == 0.0f) {
				a_err[i] = (float)-1e9;

				a_mean += M_PI;
				n++;
			}
			// right vector is zero, but left indicates
			// something -> mark in the output data with
			// 1e9
			else if (normR == 0.0f) {
				a_err[i] = (float)1e9;

				a_mean += M_PI;
				n++;
			}
			// both vectors > 0, but resolve numerical
			// errors and clamp the cos value to [-1, 1]
			else {
				float tmp = (l_u * r_u + l_v * r_v) / (normL * normR);
				tmp = tmp > 1.0 ? 1.0 : tmp;
				tmp = tmp < -1.0 ? -1.0 : tmp;

				// store
				a_err[i] = acosf(tmp);
				a_mean += a_err[i];
				n++;
			}
		}
	}



	// normalize
	if (n != 0)
		a_mean /= n;
	else
		a_mean = INFINITY;

	// save to file
	FILE *fout = fopen(o, "w");

	// FERR file format description:
	// FERRwhmza0a1...an
	// where FERR is a string, w = width, h = height, m = mean
	// error, z = number of elements in averaging the error, a0 ...
	// an the element-wise errors.

	fprintf(fout, "%s", "FERR");
	fwrite(&left->w, sizeof(int), 1, fout);
	fwrite(&left->h, sizeof(int), 1, fout);
	fwrite(&a_mean, sizeof(float), 1, fout);
	fwrite(&n, sizeof(int), 1, fout);
	fwrite(a_err, 1, left->w * left->h * sizeof(float), fout);
	fclose(fout);

cleanup:
	free(a_err);
	free_flow(left);
	free_flow(right);
}


int
main(int argc, char *argv[])
{
	char *pname = strdup(argv[0]);
	pname = basename(pname);
	if (argc < 4) {
		usage(stderr, pname);
		return EXIT_FAILURE;
	}
	if (!test_file(argv[1]) || !test_file(argv[2]))
		return EXIT_FAILURE;
	cmp(argv[1], argv[2], argv[3]);

	return EXIT_SUCCESS;
}
