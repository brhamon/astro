#include "ephutil.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static const char obs_file_name[] = "obs.dat";

void load_obs(struct obs *obs)
{
    make_local_path();
    char workpath[PATH_MAX];
    int sz = snprintf(workpath, sizeof(workpath), "%s/%s", g_local_path, obs_file_name);
    if (sz < 0 || (size_t)sz >= sizeof(workpath)) {
        printf("error: unable to form observer pathname.\n");
        exit(1);
    }
    printf_if(2, "Observer pathname: \"%s\"\n", workpath);
    int fd = open(workpath, O_RDONLY);
    if (fd < 0) {
        printf_if(2, "No observer defaults found.\n");
    } else {
        ssize_t bytes_read = read(fd, obs, sizeof(struct obs));
        if (bytes_read < 0) {
            printf_if(1, "warning: observer defaults read error.\n");
        } else {
            if ((size_t)bytes_read != sizeof(struct obs)) {
                printf_if(1, "warning: observer defaults read %zd bytes (expecting %zu).\n",
                        bytes_read, sizeof(struct obs));
            } else {
                printf_if(2, "observer defaults loaded.\n");
            }
        }
        close(fd);
    }
}

void save_obs(const struct obs *obs)
{
    char workpath[PATH_MAX];
    int sz = snprintf(workpath, sizeof(workpath), "%s/%s", g_local_path, obs_file_name);
    if (sz < 0 || (size_t)sz >= sizeof(workpath)) {
        printf("error: unable to form observer pathname.\n");
        exit(1);
    }
    printf_if(2, "Observer pathname: \"%s\"\n", workpath);
    int fd = open(workpath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        printf("Warning: Cannot save observer defaults.\n");
    } else {
        ssize_t bytes_written = write(fd, obs, sizeof(struct obs));
        if (bytes_written < 0) {
            printf("Warning: observer defaults write error.\n");
        } else {
            if ((size_t)bytes_written != sizeof(struct obs)) {
                printf("warning: observer defaults wrote %zd bytes (expecting %zu).\n",
                        bytes_written, sizeof(struct obs));
            } else {
                printf_if(1, "info: observer defaults saved.\n");
            }
        }
        close(fd);
    }
}

