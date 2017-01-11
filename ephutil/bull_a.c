#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <stdint.h>
#include <curl/curl.h>
#include "bull_a.h"

static const char bull_a_file_name[] = "finals2000A.daily";
static const char *bull_a_url[] = {
    "http://maia.usno.navy.mil/ser7/finals2000A.daily",
    "http://toshi.nofs.navy.mil/ser7/finals2000A.daily",
    NULL
};

static bull_a_entry_t *entries = NULL;
static size_t nbr_of_entries = 0;

#define BUFFER_SIZE (256 * 1024)
#define MAX_AGE_IN_SECS   86400

struct write_result {
    char* data;
    size_t pos;
    size_t sz;
};

static size_t write_response(void* ptr, size_t size, size_t nmemb, void* stream)
{
    struct write_result *result = (struct write_result *)stream;

    size *= nmemb;
    if (result->pos + size >= result->sz - 1) {
        printf("error: too small buffer\n");
        return 0;
    }
    memcpy(result->data + result->pos, ptr, size);
    result->pos += size;
    return size;
}

static int fetch_url(const char* url, char* data, size_t data_size,
        size_t* nbr_of_bytes) {
    CURL* curl=NULL;
    CURLcode status;
    struct curl_slist *headers=NULL;
    long code;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (!curl) {
        goto error;
    }
    struct write_result write_result = {
        .data = data,
        .pos = 0,
        .sz = data_size
    };
    curl_easy_setopt(curl, CURLOPT_URL, url);

    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 "
            "(Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) "
            "Chrome/48.0.2564.48 Safari/537.36");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

    status = curl_easy_perform(curl);
    if (status != 0) {
        printf("error: unable to request data from %s:\n", url);
        printf("%s\n", curl_easy_strerror(status));
        goto error;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if (code != 200) {
        printf("error: server responded with code %ld\n", code);
        goto error;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    curl_global_cleanup();

    data[write_result.pos] = '\0';
    *nbr_of_bytes = write_result.pos;
    return 0;

error:
    if (curl) {
        curl_easy_cleanup(curl);
    }
    if (headers) {
        curl_slist_free_all(headers);
    }
    curl_global_cleanup();
    return -1;
}

static int get_int(char* p, size_t len) {
    char* p_end;
    long int val = 0;
    char ch = p[len];
    p[len] = '\0';
    val = strtol(p, &p_end, 10);
    p[len] = ch;
    return val;
}

static double get_double(char* p, size_t len) {
    char* p_end;
    double val = 0.0;
    char ch = p[len];
    p[len] = '\0';
    val = strtod(p, &p_end);
    p[len] = ch;
    return val;
}

int bull_a_init()
{
    struct stat buffer;
    time_t age;
    size_t file_size, offset;
    ssize_t nbr_bytes;
    char* file_buffer=NULL;
    char* p;
    char* p1;
    const char** url_iter;
    int status;
    int fd;
    bull_a_entry_t *entry;

    age = time(NULL);
    status = stat(bull_a_file_name, &buffer);
    if (status == 0) {
        file_size = (size_t)buffer.st_size;
        age -= buffer.st_mtime;
        if (file_size < BUFFER_SIZE && age < MAX_AGE_IN_SECS) {
            file_buffer = (char*)malloc(file_size);
            if (file_buffer == NULL) { // malloc failed
                status = -1;
            } else {
                fd = open(bull_a_file_name, O_RDONLY);
                if (fd >= 0) {
                    offset = 0;
                    while (status == 0 && offset < file_size) {
                        nbr_bytes = read(fd, file_buffer + offset, file_size -
                                offset);
                        if (nbr_bytes < 0) { // read failed
                            status = -1;
                        } else {
                            offset += (size_t)nbr_bytes;
                        }
                    }
                    close(fd);
                    if (status == 0) {
                        printf("Read %9jd bytes from %s\n", offset,
                                bull_a_file_name);
                    }
                } else { // open failed
                    status = -1;
                }
                if (status != 0) { // if any errors occurred, don't leak file_buffer
                    free(file_buffer);
                    file_buffer = NULL;
                }
            }
        } else { // The file is too large or too old.
            status = -1;
        }
    }
    if (status != 0) {
        printf("Fetching a new copy of %s\n", bull_a_file_name);
        file_buffer = (char*)malloc(BUFFER_SIZE);
        if (file_buffer == NULL) {
            status = -1;
        } else {
            url_iter = &bull_a_url[0];
            status = -1;
            while (status != 0 && *url_iter != NULL) {
                printf("  from %s\n", *url_iter);
                status = fetch_url(*url_iter, file_buffer, BUFFER_SIZE,
                        &file_size);
                if (status != 0) {
                    ++url_iter;
                }
            }
            if (status == 0) {
                printf("Downloaded %9jd bytes.\n", file_size);
                fd = open(bull_a_file_name, O_CREAT|O_WRONLY, 0644);
                if (fd >= 0) {
                    offset = 0;
                    while (status == 0 && offset < file_size) {
                        nbr_bytes = write(fd, file_buffer + offset,
                                file_size - offset);
                        if (nbr_bytes < 0) {
                            status = -1;
                        } else {
                            offset += (size_t)nbr_bytes;
                        }
                    }
                    close(fd);
                    if (status == 0) {
                        printf("Wrote %9jd bytes to %s\n", offset, bull_a_file_name);
                    }
                } else { // There was a problem opening the file.
                    status = -1;
                }
            }
            if (status != 0) { // if any errors occurred, don't leak file_buffer
                free(file_buffer);
                file_buffer = NULL;
            }
        }
    }
    if (status == 0) {
        // pass one: count lines
        offset = 0;
        for (p = file_buffer; *p; p = p1 + 1) {
            p1 = strchr(p, '\n');
            if (p1 == NULL) {
                break;
            }
            ++offset;
        }
        printf("%9jd lines\n", offset);
        nbr_of_entries = 0;
        entries = (bull_a_entry_t*)malloc(offset * sizeof(bull_a_entry_t));
        if (entries == NULL) {
            status = -1;
        } else {
            entry = entries;
            for (p = file_buffer; *p; p = p1 + 1) {
                p1 = strchr(p, '\n');
                if (p1 == NULL) {
                    break;
                }
                if (p1 - p > 78) {
                    entry->year = get_int(p, 2);
                    entry->month = get_int(p+2, 2);
                    entry->day = get_int(p+4, 2);
                    entry->mjd = get_double(p+7, 8);
                    entry->pm_quality = p[16];
                    entry->pm_x = get_double(p+18, 9);
                    entry->pm_x_err = get_double(p+27, 9);
                    entry->pm_y = get_double(p+37, 9);
                    entry->pm_y_err = get_double(p+46, 9);
                    entry->ut1_utc_quality = p[57];
                    entry->ut1_utc = get_double(p+58, 10);
                    entry->ut1_utc_err = get_double(p+68, 10);
                    /* to get true calendar year, add 1900 for MJD<=51543
                     * or add 2000 for MJD>=51544
                     */
                    if (entry->mjd >= 51544.0) {
                        entry->year += 2000;
                    } else {
                        entry->year += 1900;
                    }
                    ++entry;
                    ++nbr_of_entries;
                }
            }
        }
        free(file_buffer);
    }
    return status;
}

bull_a_entry_t* bull_a_find(double mjd)
{
    bull_a_entry_t* entry;
    bull_a_entry_t* entry_end = entries + nbr_of_entries;
    bull_a_entry_t* prev_entry = NULL;
    for (entry = entries; entry != NULL && entry != entry_end && entry->mjd <= mjd;
            ++entry) {
        prev_entry = entry;
    }
    return prev_entry;
}

void bull_a_cleanup()
{
    if (entries != NULL) {
        free(entries);
        entries = NULL;
        nbr_of_entries = 0;
    }
}

/* vim:set ts=4 sts=4 sw=4 cindent expandtab: */
