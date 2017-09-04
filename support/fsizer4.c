#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define NAMFIL_MAX 80

static char local_path[NAMFIL_MAX] = { 0, };
static const char jpleph_name[] = "JPLEPH";
static const char astro_dir[] = ".astro";

static void make_local_path()
{
    if (*local_path != 0) {
        return;
    }
    struct stat buffer;
    int status;
    const char *description = "";
    const char *home = getenv("HOME");

    if (home == NULL) {
        if (getcwd(local_path, sizeof(local_path)) != NULL) {
            description = "current";
        } else {
            printf("error: getcwd failed.\n");
            exit(1);
        }
    } else {
        strncpy(local_path, home, sizeof(local_path));
        local_path[sizeof(local_path)-1] = 0;
        description = "home";
    }
    char *p = strchr(local_path, 0);
    if (p == NULL || p < local_path || (size_t)(p - local_path) >= NAMFIL_MAX) {
        printf("error: invalid %s value in environment.\n", description);
        exit(1);
    }
    status = stat(local_path, &buffer);
    if (status == 0) {
        if (!S_ISDIR(buffer.st_mode)) {
            printf("error: %s is not a directory.\n", description);
            exit(1);
        }
    } else {
        printf("error: %s directory missing.\n", description);
        exit(1);
    }

    size_t worksz = NAMFIL_MAX - (size_t)(p - local_path);
    int sz = snprintf(p, worksz, "/%s", astro_dir);
    if (sz < 0 || (size_t)sz >= worksz) {
        printf("error: unable to form astro directory path.\n");
        exit(1);
    }
    p += sz;
    worksz -= sz;

    status = stat(local_path, &buffer);
    if (status == 0) {
        if (!S_ISDIR(buffer.st_mode)) {
            printf("error: local path \"%s\" exists, but is not a directory.\n", local_path);
            exit(1);
        }
    } else {
        if (mkdir(local_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
            printf("error: unable to create work directory \"%s\".\n", local_path);
            exit(1);
        }
    }
}

void fsizer4_(uint32_t *nrecl, uint32_t *ksize, uint32_t *nrfile, char *namfil)
{
    *nrecl = 4;
    *ksize = 2036;
    *nrfile = 12;
    make_local_path();
    int sz = snprintf(namfil, NAMFIL_MAX, "%s/%s", local_path, jpleph_name);
    if (sz < 0 || (size_t)sz >= NAMFIL_MAX) {
        printf("error: unable to fit %s path into %u byte limit of NOVA-C.\n",
                jpleph_name, NAMFIL_MAX);
        exit(1);
    }
}

