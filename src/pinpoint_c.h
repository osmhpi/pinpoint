#pragma once

#include <time.h>

typedef void *pinpoint_source_t;

// if energy -> joules (no prefix)
// if power -> watt (no prefix)
typedef struct {
	struct timespec timestamp;
	double value;
} pinpoint_sample_t;

extern int pinpoint_setup(void);

// returns list of pointers, terminated with nullptr
// callee is responsible for two-level cleanup
extern char **pinpoint_available_counters(void);

// returns list of pointers, terminated with nullptr
// callee is responsible for two-level cleanup
extern char **pinpoint_available_aliases(void);

extern pinpoint_source_t pinpoint_open_source(const char *name);
extern void pinpoint_close_source(pinpoint_source_t src);

// Read data source name into buffer and return expected len
// Follows strncpy semantics, will not zero-terminate dst_buf, if name length >= buflen
extern size_t pinpoint_source_name(pinpoint_source_t src, char *dst_buf, size_t buflen);

extern void pinpoint_read_energy(pinpoint_source_t src, pinpoint_sample_t *dst);
extern void pinpoint_read_power(pinpoint_source_t src, pinpoint_sample_t *dst);

extern void pinpoint_reset_acc(pinpoint_source_t src);
