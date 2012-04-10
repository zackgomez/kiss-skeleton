#include "libgsm.h"
#include <zip.h>
#include <cstdlib>
#include <cassert>

static const char *BONES_FILE = "skeleton.bones";
static const char *MESH_FILE  = "model.obj";

// Struct declaration
struct gsm
{
    struct zip *archive;
};

// Helper functions
static void *zip_file_contents(struct zip *archive, const char *filename, size_t &len);
static int zip_set_file(struct zip *archive, const char *filename, FILE *f);


// Function definitions

gsm *gsm_open(const char *gsmfile, int flags)
{
    int zflags = 0;
    if (flags | GSM_CREATE)
        zflags |= ZIP_CREATE;

    struct zip *zfile = zip_open(gsmfile, zflags, NULL);
    if (!zfile)
    {
        printf("Unable to open gsmfile %s.\n", gsmfile);
        return NULL;
    }

    // checks for expected files
    /*
    if (zip_name_locate(zfile, "model.obj", 0) < 0)
    {
        printf("Bad gsm archive: 'model.obj' not found.\n");
        zip_close(zfile);
        return NULL;
    }
    if (zip_name_locate(zfile, "skeleton.bones", 0) < 0)
    {
        printf("Bad gsm archive: 'skeleton.bones' not found.\n");
        zip_close(zfile);
        return NULL;
    }
    */

    gsm *ret = (gsm *) malloc(sizeof(gsm));
    ret->archive = zfile;

    return ret;
}

void gsm_close(gsm *gsm)
{
    zip_close(gsm->archive);
    free(gsm);
}

void *gsm_bones_contents(gsm *file, size_t &len)
{
    return zip_file_contents(file->archive, BONES_FILE, len);
}

void *gsm_mesh_contents(gsm *file, size_t &len)
{
    return zip_file_contents(file->archive, MESH_FILE, len);
}

int gsm_set_bones(gsm *file, FILE *f)
{
    return zip_set_file(file->archive, BONES_FILE, f);
}

int gsm_set_mesh(gsm *file, FILE *f)
{
    return zip_set_file(file->archive, MESH_FILE, f);
}

void *zip_file_contents(struct zip *archive, const char *filename, size_t &len)
{
    struct zip_stat stats;
    if (zip_stat(archive, filename, 0, &stats))
    {
        fprintf(stderr, "Unable to stat %s from archive\n", filename);
        return NULL;
    }
    len = stats.size;

    struct zip_file *f = zip_fopen(archive, filename, 0);
    if (!f)
    {
        fprintf(stderr, "Unable to open %s from archive\n", filename);
        return NULL;
    }

    void *contents = malloc(len);
    assert(contents);

    int bytes_read = zip_fread(f, contents, len);
    assert(len == bytes_read);

    zip_fclose(f);

    return contents;
}

int zip_set_file(struct zip *archive, const char *filename, FILE *f)
{
    zip_source *s;
    if ((s = zip_source_filep(archive, f, 0, -1)) == NULL)
        return -1;

    // if it exists..
    uint64_t idx;
    if ((idx = zip_name_locate(archive, filename, 0)) != -1)
    {
        // try to replace
        if (zip_replace(archive, idx, s) < 0)
        {
            zip_source_free(s);
            return -1;
        }
    }
    else
    {
        // just add
        if (zip_add(archive, filename, s) < 0)
        {
            zip_source_free(s);
            return -1;
        }
    }

    return 0;
}
