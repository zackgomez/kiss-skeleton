#pragma once
#include <cstddef>

struct gsm;

gsm *gsm_open(const char *gsmfile);
void gsm_close(gsm *gsm);

void *gsm_bones_contents(gsm *file, size_t &len);
void *gsm_mesh_contents(gsm *file, size_t &len);
