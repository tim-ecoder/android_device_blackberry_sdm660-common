/*
 * BB OREO Camera Gralloc Compatibility Shim
 *
 * The BB OREO camera HAL reads gralloc private_handle_t at Android 8 (V1)
 * field offsets.  In LOS23 (V4) the fields layer_count / id / usage sit
 * BEFORE size/offset/base, shifting them by 20 bytes.
 *
 * Strategy:
 *   1.  Intercept native_handle_clone() via LD_PRELOAD.  Every buffer
 *       handle that enters the camera-provider process is cloned from the
 *       Binder parcel.  After cloning, if the handle carries the gralloc
 *       magic ('gmsm'), we rearrange the int[] payload from V4 order to V1
 *       order **in place**.  The BB camera HAL blob then reads the correct
 *       values at its compiled-in offsets.
 *
 *   2.  Keep the mmap() shim as a safety net: if anything still passes
 *       size=1 for a dmabuf mapping, expand to the real dmabuf size.
 *
 *   3.  Keep the --wrap=ioctl shim for V4L2 buffer-map fixups.
 *
 * V4 int layout (words after the two fds, i.e. data[2..]):
 *   [0] magic  [1] flags  [2] width  [3] height
 *   [4] unaligned_width  [5] unaligned_height  [6] format  [7] buffer_type
 *   [8] layer_count
 *   [9..10] id (uint64)
 *   [11..12] usage (uint64)
 *   [13] size  [14] offset  [15] offset_metadata
 *   [16..17] base (uint64)  [18..19] base_metadata (uint64)
 *   [20..21] gpuaddr (uint64)
 *
 * V1 int layout (what the BB blob expects):
 *   [0] magic  [1] flags  [2] width  [3] height
 *   [4] unaligned_width  [5] unaligned_height  [6] format  [7] buffer_type
 *   [8] size  [9] offset  [10] offset_metadata
 *   [11..12] base (uint64)  [13..14] base_metadata (uint64)
 *   [15..16] gpuaddr (uint64)
 *   [17] layer_count
 *   [18..19] id (uint64)
 *   [20..21] usage (uint64)
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <android/log.h>

/* cutils/native_handle.h equivalent */
#include <cutils/native_handle.h>

#define LOG_TAG "CameraCompat"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define GRALLOC_MAGIC  0x676d736d   /* 'gmsm' */
#define NUM_FDS        2            /* fd + fd_metadata */

__attribute__((constructor))
static void cameracompat_init(void) {
    ALOGE("libcameracompat loaded (V4->V1 handle rearrange active)");
    /* Debug: write marker file to prove we're loaded */
    int marker_fd = open("/data/local/tmp/cameracompat_loaded", O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (marker_fd >= 0) {
        write(marker_fd, "loaded\n", 7);
        close(marker_fd);
    }
}

/* Word indices into native_handle_t.data[] (after the two fds).
 * V4 layout:  data[0]=magic, data[1]=flags, ... data[8]=layer_count,
 *             data[9..10]=id, data[11..12]=usage, data[13]=size, ...
 */
#define V4_MAGIC_IDX        0
#define V4_LAYER_COUNT_IDX  8
#define V4_ID_LO_IDX        9
#define V4_ID_HI_IDX       10
#define V4_USAGE_LO_IDX   11
#define V4_USAGE_HI_IDX   12
#define V4_SIZE_IDX        13

/* After rearrange, V1 layout:
 *   data[8]=size, ..., data[17]=layer_count, data[18..19]=id, data[20..21]=usage
 */

/*
 * Rearrange handle data[] from V4 order to V1 order.
 * V4 words [8..21]:  lc, id_lo, id_hi, usg_lo, usg_hi, size, off, off_meta,
 *                     base_lo, base_hi, basem_lo, basem_hi, gpu_lo, gpu_hi
 * V1 words [8..21]:  size, off, off_meta, base_lo, base_hi, basem_lo, basem_hi,
 *                     gpu_lo, gpu_hi, lc, id_lo, id_hi, usg_lo, usg_hi
 *
 * i.e. move the 5 words [lc, id_lo, id_hi, usg_lo, usg_hi] from positions 8-12
 *      to positions 17-21, and shift words 13-21 down to 8-16.
 */
static void v4_to_v1(int *data, int numInts) {
    if (numInts < 22) return;   /* sanity: need at least 22 int words */

    int saved[5];
    /* Save V4 words 8..12 (layer_count, id, usage) */
    saved[0] = data[NUM_FDS + 8];   /* layer_count */
    saved[1] = data[NUM_FDS + 9];   /* id low */
    saved[2] = data[NUM_FDS + 10];  /* id high */
    saved[3] = data[NUM_FDS + 11];  /* usage low */
    saved[4] = data[NUM_FDS + 12];  /* usage high */

    /* Shift V4 words 13..21 down to positions 8..16 */
    /* These are: size, offset, offset_metadata, base(2), base_metadata(2), gpuaddr(2) */
    memmove(&data[NUM_FDS + 8], &data[NUM_FDS + 13], 9 * sizeof(int));

    /* Place saved layer_count/id/usage at positions 17..21 */
    data[NUM_FDS + 17] = saved[0];  /* layer_count */
    data[NUM_FDS + 18] = saved[1];  /* id low */
    data[NUM_FDS + 19] = saved[2];  /* id high */
    data[NUM_FDS + 20] = saved[3];  /* usage low */
    data[NUM_FDS + 21] = saved[4];  /* usage high */
}

/* --- native_handle_clone interception --- */

static native_handle_t* (*real_native_handle_clone)(const native_handle_t *) = NULL;
static int clone_hook_logged = 0;

native_handle_t* native_handle_clone(const native_handle_t *handle) {
    if (!real_native_handle_clone)
        real_native_handle_clone = (native_handle_t*(*)(const native_handle_t*))
            dlsym(RTLD_NEXT, "native_handle_clone");

    native_handle_t *cloned = real_native_handle_clone(handle);
    if (!cloned) return cloned;

    ALOGI("native_handle_clone: numFds=%d numInts=%d", cloned->numFds, cloned->numInts);

    /* Check if this is a gralloc handle by magic word */
    if (cloned->numFds >= NUM_FDS && cloned->numInts >= 22) {
        int magic = cloned->data[NUM_FDS + V4_MAGIC_IDX];
        if (magic == (int)GRALLOC_MAGIC) {
            /* Verify it's V4 layout: word[8] should be layer_count (small, 1-4)
             * and word[13] should be size (large, > 4096 typically) */
            uint32_t w8  = (uint32_t)cloned->data[NUM_FDS + 8];
            uint32_t w13 = (uint32_t)cloned->data[NUM_FDS + 13];
            ALOGI("gralloc handle: w8=%u w13=%u", w8, w13);
            if (w8 <= 4 && w13 > 64) {
                /* Looks like V4: w8=layer_count, w13=size */
                ALOGI("V4->V1 handle rearrange: layer_count=%u size=%u",
                      w8, w13);
                v4_to_v1(cloned->data, cloned->numInts);
            }
        }
    }
    return cloned;
}

/* Also hook native_handle_create for completeness — used in AIDL unflattening */
static native_handle_t* (*real_native_handle_create)(int, int) = NULL;
static int create_hook_count = 0;

native_handle_t* native_handle_create(int numFds, int numInts) {
    if (!real_native_handle_create)
        real_native_handle_create = (native_handle_t*(*)(int, int))
            dlsym(RTLD_NEXT, "native_handle_create");

    native_handle_t *h = real_native_handle_create(numFds, numInts);
    if (create_hook_count < 3) {
        ALOGI("native_handle_create: numFds=%d numInts=%d -> %p", numFds, numInts, h);
        create_hook_count++;
    }
    return h;
}

/* --- mmap interception (safety net) --- */

#define MMAP_MIN_THRESHOLD (64 * 1024)

static int is_dmabuf_fd(int fd) {
    char path[64];
    char link[256];
    ssize_t len;
    if (fd < 0) return 0;
    snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    len = readlink(path, link, sizeof(link) - 1);
    if (len <= 0) return 0;
    link[len] = '\0';
    return (strstr(link, "dmabuf") != NULL);
}

static size_t get_dmabuf_size(int fd) {
    off_t size = lseek(fd, 0, SEEK_END);
    if (size > 0) {
        lseek(fd, 0, SEEK_SET);
        return (size_t)size;
    }
    return 0;
}

static void* (*real_mmap)(void*, size_t, int, int, int, off_t) = NULL;
static void* (*real_mmap64)(void*, size_t, int, int, int, off64_t) = NULL;

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    if (!real_mmap)
        real_mmap = (void*(*)(void*,size_t,int,int,int,off_t))dlsym(RTLD_NEXT, "mmap");

    if (length < MMAP_MIN_THRESHOLD && fd >= 0 && is_dmabuf_fd(fd)) {
        size_t actual = get_dmabuf_size(fd);
        if (actual > length) {
            ALOGI("Fix mmap: fd=%d %zu->%zu prot=0x%x", fd, length, actual, prot);
            length = actual;
        }
    }
    return real_mmap(addr, length, prot, flags, fd, offset);
}

void* mmap64(void* addr, size_t length, int prot, int flags, int fd, off64_t offset) {
    if (!real_mmap64)
        real_mmap64 = (void*(*)(void*,size_t,int,int,int,off64_t))dlsym(RTLD_NEXT, "mmap64");

    if (length < MMAP_MIN_THRESHOLD && fd >= 0 && is_dmabuf_fd(fd)) {
        size_t actual = get_dmabuf_size(fd);
        if (actual > length) {
            ALOGI("Fix mmap64: fd=%d %zu->%zu prot=0x%x", fd, length, actual, prot);
            length = actual;
        }
    }
    return real_mmap64(addr, length, prot, flags, fd, offset);
}

/* ioctl interception removed — handle rearrange makes it unnecessary */
