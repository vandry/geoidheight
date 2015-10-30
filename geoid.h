#ifndef __GASN_TAMC_GEOID_H__
#define __GASN_TAMC_GEOID_H__

/* takes a NULL-terminated list of filenames to try to open geoid data.
   Uses the first one that exists */
struct geoid_ctx *geoid_init(const char *geoid_pgm_filename, ...);

void geoid_free(struct geoid_ctx *);

/* latitudes and longitudes in degrees */
double geoid_height_linear(struct geoid_ctx *, double lat, double lon);
double geoid_height_cubic(struct geoid_ctx *, double lat, double lon);

#endif
