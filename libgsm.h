#pragma once
#include <cstddef>
#include <cstdio>

#define GSM_NO_FLAGS 0
#define GSM_CREATE 1

struct gsm;

gsm *gsm_open(const char *gsmfile, int flags = 0);
void gsm_close(gsm *gsm);

void *gsm_bones_contents(gsm *file, size_t &len);
void *gsm_mesh_contents(gsm *file, size_t &len);

// These return nonzero on failure
int gsm_set_bones(gsm *file, FILE *f);
int gsm_set_mesh(gsm *file, FILE *f);

