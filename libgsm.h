#pragma once
#include <cstddef>
#include <cstdio>

struct gsm;

gsm *gsm_open(const char *gsmfile);
void gsm_close(gsm *gsm);

void *gsm_bones_contents(gsm *file, size_t &len);
void *gsm_mesh_contents(gsm *file, size_t &len);

// Will overwrite existing files
// returns true on success, false otherwise
bool gsm_new(const char *gsmfile, FILE *bonesdata, FILE *meshdata);
