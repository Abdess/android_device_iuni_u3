/*
 * Copyright (C) 2015, The CyanogenMod Project
 * Copyright (C) 2017, The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "edify/expr.h"
#include "updater/install.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define ALPHABET_LEN 256

#define TZ_PART_PATH "/dev/block/platform/msm_sdcc.1/by-name/tz"
#define TZ_VER_STR "QC_IMAGE_VERSION_STRING="
#define TZ_VER_STR_LEN 24
#define TZ_VER_BUF_LEN 255

#define CACHE_PART_PATH    "/dev/block/platform/msm_sdcc.1/by-name/cache"
#define USERDATA_PART_PATH "/dev/block/platform/msm_sdcc.1/by-name/userdata"
#define SYSTEM_PART_PATH   "/dev/block/platform/msm_sdcc.1/by-name/system"

/* Boyer-Moore string search implementation from Wikipedia */

/* Return longest suffix length of suffix ending at str[p] */
static int max_suffix_len(const char *str, size_t str_len, size_t p) {
    uint32_t i;

    for (i = 0; (str[p - i] == str[str_len - 1 - i]) && (i < p); ) {
        i++;
    }

    return i;
}

/* Generate table of distance between last character of pat and rightmost
 * occurrence of character c in pat
 */
static void bm_make_delta1(int *delta1, const char *pat, size_t pat_len) {
    uint32_t i;
    for (i = 0; i < ALPHABET_LEN; i++) {
        delta1[i] = pat_len;
    }
    for (i = 0; i < pat_len - 1; i++) {
        uint8_t idx = (uint8_t) pat[i];
        delta1[idx] = pat_len - 1 - i;
    }
}

/* Generate table of next possible full match from mismatch at pat[p] */
static void bm_make_delta2(int *delta2, const char *pat, size_t pat_len) {
    int p;
    uint32_t last_prefix = pat_len - 1;

    for (p = pat_len - 1; p >= 0; p--) {
        /* Compare whether pat[p-pat_len] is suffix of pat */
        if (strncmp(pat + p, pat, pat_len - p) == 0) {
            last_prefix = p + 1;
        }
        delta2[p] = last_prefix + (pat_len - 1 - p);
    }

    for (p = 0; p < (int) pat_len - 1; p++) {
        /* Get longest suffix of pattern ending on character pat[p] */
        int suf_len = max_suffix_len(pat, pat_len, p);
        if (pat[p - suf_len] != pat[pat_len - 1 - suf_len]) {
            delta2[pat_len - 1 - suf_len] = pat_len - 1 - p + suf_len;
        }
    }
}

static char * bm_search(const char *str, size_t str_len, const char *pat,
        size_t pat_len) {
    int delta1[ALPHABET_LEN];
    int delta2[pat_len];
    int i;

    bm_make_delta1(delta1, pat, pat_len);
    bm_make_delta2(delta2, pat, pat_len);

    if (pat_len == 0) {
        return (char *) str;
    }

    i = pat_len - 1;
    while (i < (int) str_len) {
        int j = pat_len - 1;
        while (j >= 0 && (str[i] == pat[j])) {
            i--;
            j--;
        }
        if (j < 0) {
            return (char *) (str + i + 1);
        }
        i += MAX(delta1[(uint8_t) str[i]], delta2[j]);
    }

    return NULL;
}

static int get_tz_version(char *ver_str, size_t len) {
    int ret = 0;
    int fd;
    int tz_size;
    char *tz_data = NULL;
    char *offset = NULL;

    fd = open(TZ_PART_PATH, O_RDONLY);
    if (fd < 0) {
        ret = errno;
        goto err_ret;
    }

    tz_size = lseek64(fd, 0, SEEK_END);
    if (tz_size == -1) {
        ret = errno;
        goto err_fd_close;
    }

    tz_data = (char *) mmap(NULL, tz_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (tz_data == (char *)-1) {
        ret = errno;
        goto err_fd_close;
    }

    /* Do Boyer-Moore search across TZ data */
    offset = bm_search(tz_data, tz_size, TZ_VER_STR, TZ_VER_STR_LEN);
    if (offset != NULL) {
        strncpy(ver_str, offset + TZ_VER_STR_LEN, len);
    } else {
        ret = -ENOENT;
    }

    munmap(tz_data, tz_size);
err_fd_close:
    close(fd);
err_ret:
    return ret;
}

/* verify_trustzone("TZ_VERSION", "TZ_VERSION", ...) */
Value * VerifyTrustZoneFn(const char *name, State *state, int argc, Expr *argv[]) {
    char current_tz_version[TZ_VER_BUF_LEN];
    int i, ret;

    ret = get_tz_version(current_tz_version, TZ_VER_BUF_LEN);
    if (ret) {
        return ErrorAbort(state, kFreadFailure, "%s() failed to read current TZ version: %d",
                name, ret);
    }

    char** tz_version = ReadVarArgs(state, argc, argv);
    if (tz_version == NULL) {
        return ErrorAbort(state, kArgsParsingFailure, "%s() error parsing arguments", name);
    }

    ret = 0;
    for (i = 0; i < argc; i++) {
        uiPrintf(state, "Comparing TZ version %s to %s",
                tz_version[i], current_tz_version);
        if (strncmp(tz_version[i], current_tz_version, strlen(tz_version[i])) == 0) {
            ret = 1;
            break;
        }
    }

    for (i = 0; i < argc; i++) {
        free(tz_version[i]);
    }
    free(tz_version);

    return StringValue(strdup(ret ? "1" : "0"));
}

static int check_for_f2fs() {
    std::string blkid_output;
    int pipefd[2];
    pid_t child;
    FILE *fp;
    int ret = 1;

    if (pipe(pipefd) < 0)
        return -1;

    if ((child = fork()) < 0)
        return -1;

    if (child == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        execl("/sbin/blkid", "blkid", CACHE_PART_PATH,
            USERDATA_PART_PATH, SYSTEM_PART_PATH, NULL);
        return -1;
    }

    waitpid(child, NULL, 0);
    close(pipefd[1]);

    fp = fdopen(pipefd[0], "r");

    while (1) {
        char c = fgetc(fp);
        if (feof(fp))
            break;
        blkid_output += c;
    }

    fclose(fp);

    if (strstr(blkid_output.c_str(), "f2fs"))
        ret = 0;

    return ret;
}

Value * VerifyFsTypeFn(const char *name, State *state, int argc, Expr *argv[]) {
    int ret;

    ret = check_for_f2fs();
    if (ret < 0)
        uiPrintf(state, "Failed to check partitions for F2FS!");
    else if (ret == 0)
        uiPrintf(state, "Error, F2FS is not supported! Use EXT4 instead.");

    return StringValue(strdup(ret > 0 ? "1" : "0"));
}

void Register_librecovery_updater_u3() {
    RegisterFunction("u3.verify_trustzone", VerifyTrustZoneFn);
    RegisterFunction("u3.verify_fs_type", VerifyFsTypeFn);
}
