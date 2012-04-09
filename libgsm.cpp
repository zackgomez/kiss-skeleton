#include "libgsm.h"
#include <zip.h>
#include <cstdlib>
#include <cassert>

// Struct declaration
struct gsm
{
    struct zip *archive;
};

// Helper functions
static void *zip_file_contents(struct zip *archive, const char *filename, size_t &len);


// Function definitions

gsm *gsm_open(const char *gsmfile)
{
    struct zip *zfile = zip_open(gsmfile, 0, NULL);
    if (!zfile)
    {
        printf("Unable to open gsmfile %s.\n", gsmfile);
        return NULL;
    }

    // TODO add checks for required files

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
    return zip_file_contents(file->archive, "skeleton.bones", len);
}

void *gsm_mesh_contents(gsm *file, size_t &len)
{
    return zip_file_contents(file->archive, "model.obj", len);
}

bool gsm_new(const char *gsmfile, FILE *bonesdata, FILE *meshdata)
{
    struct zip *zfile = zip_open(gsmfile, ZIP_CREATE, NULL);
    if (!zfile)
    {
        printf("Unable to open gsmfile %s.\n", gsmfile);
        return false;
    }

    // XXX some memory leaks here on errors
    struct zip_source *s;
    if ((s = zip_source_filep(zfile, meshdata, 0, -1)) == NULL ||
            zip_add(zfile, "model.obj", s) < 0)
    {
        zip_source_free(s);
        fprintf(stderr, "error adding file: %sn", zip_strerror(zfile));
        zip_close(zfile);
        return false;
    }
    if ((s = zip_source_filep(zfile, bonesdata, 0, -1)) == NULL ||
            zip_add(zfile, "skeleton.bones", s) < 0)
    {
        zip_source_free(s);
        fprintf(stderr, "error adding file: %sn", zip_strerror(zfile));
        zip_close(zfile);
        return false;
    }

    zip_close(zfile);

    return true;
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

