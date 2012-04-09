#include <stdlib.h>
#include <stdio.h>
#include <zip.h>
#include <unistd.h>

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
        fprintf(stderr, "Unable to open out.gsm for writing.\n");
        exit(1);
    }

    struct zip_source *s;
    if ((s = zip_source_buffer(zfile, objfile, 0, 0)) == NULL ||
            zip_add(zfile, "model.obj", s) < 0)
    {
        fprintf(stderr, "error adding file: %sn", zip_strerror(zfile));
        exit(1);
    }
    if ((s = zip_source_buffer(zfile, bonesfile, 0, 0)) == NULL ||
            zip_add(zfile, "skeleton.bones", s) < 0)
    {
        fprintf(stderr, "error adding file: %sn", zip_strerror(zfile));
        exit(1);
    }

    zip_close(zfile);

    return 0;
}
