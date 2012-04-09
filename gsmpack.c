#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zip.h>
#include <unistd.h>
#include <assert.h>

int main(int argc, char **argv)
{
    const char *objfile, *bonesfile, *outfile;
    if (argc != 4)
    {
        printf("usage: %s OUTFILE OBJFILE BONESFILE\n", argv[0]);
        exit(1);
    }

    objfile = argv[2];
    bonesfile = argv[3];
    outfile = argv[1];

    if (access(objfile, R_OK))
    {
        fprintf(stderr, "Cannot open %s for reading.\n", objfile);
        exit(1);
    }
    if (access(bonesfile, R_OK))
    {
        fprintf(stderr, "Cannot open %s for reading.\n", bonesfile);
        exit(1);
    }
    
    struct zip *zfile = zip_open(outfile, ZIP_CREATE | ZIP_EXCL, NULL);
    if (!zfile)
    {
        fprintf(stderr, "Unable to open %s for writing.\n", outfile);
        exit(1);
    }

    /*
    const char *comment = "GSM FILE???";
    zip_set_archive_comment(zfile, comment, strlen(comment));
    */

    struct zip_source *s;
    if ((s = zip_source_file(zfile, objfile, 0, -1)) == NULL ||
            zip_add(zfile, "model.obj", s) < 0)
    {
        fprintf(stderr, "error adding file: %sn", zip_strerror(zfile));
        exit(1);
    }
    if ((s = zip_source_file(zfile, bonesfile, 0, -1)) == NULL ||
            zip_add(zfile, "skeleton.bones", s) < 0)
    {
        fprintf(stderr, "error adding file: %sn", zip_strerror(zfile));
        exit(1);
    }

    if (zip_close(zfile))
    {
        fprintf(stderr, "unable to write zipfile %s\n", zip_strerror(zfile));
    }

    return 0;
}

