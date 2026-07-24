#include "kernel/app_image_source.h"

#include <stdint.h>

#define APP_IMAGE_BOOTFS_PREFIX "/armonios/"

static void clear_source(app_image_source_t *source) {
    if (source == 0) {
        return;
    }

    source->kind = APP_IMAGE_SOURCE_BOOTFS;
    source->path = 0;
    for (uint32_t i = 0; i < VFS_MAX_PATH; i++) {
        source->path_storage[i] = '\0';
    }
    for (uint32_t i = 0; i < APP_IMAGE_NAME_MAX; i++) {
        source->name[i] = '\0';
    }
}

static int prefix_matches(const char *text, const char *prefix,
                          uint32_t *matched_length) {
    uint32_t i = 0;

    if (text == 0 || prefix == 0) {
        return 0;
    }
    while (prefix[i] != '\0') {
        if (text[i] != prefix[i]) {
            return 0;
        }
        i++;
    }
    if (matched_length != 0) {
        *matched_length = i;
    }
    return 1;
}

static int text_equal(const char *left, const char *right) {
    uint32_t i = 0;

    if (left == 0 || right == 0) {
        return left == right;
    }
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) {
            return 0;
        }
        i++;
    }
    return left[i] == right[i];
}

static int copy_name(app_image_source_t *source, const char *name) {
    uint32_t length = 0;

    if (source == 0 || name == 0 || name[0] == '\0') {
        return -1;
    }
    while (name[length] != '\0') {
        if (name[length] == '/' || length + 1U >= APP_IMAGE_NAME_MAX) {
            return -1;
        }
        source->name[length] = name[length];
        length++;
    }
    source->name[length] = '\0';
    return 0;
}

int app_image_source_resolve(const char *path, app_image_source_t *source) {
    uint32_t prefix_length = 0;
    uint32_t basename = 1U;

    if (source == 0) {
        return -1;
    }
    clear_source(source);

    if (vfs_normalize_path(path, source->path_storage) != 0 ||
        source->path_storage[0] != '/' ||
        source->path_storage[1] == '\0') {
        return -1;
    }

    if (prefix_matches(source->path_storage, APP_IMAGE_BOOTFS_PREFIX,
                       &prefix_length)) {
        if (copy_name(source, &source->path_storage[prefix_length]) != 0) {
            clear_source(source);
            return -1;
        }
        source->kind = APP_IMAGE_SOURCE_BOOTFS;
        source->path = source->name;
        return 0;
    }

    if (text_equal(source->path_storage, "/armonios")) {
        clear_source(source);
        return -1;
    }

    for (uint32_t i = 1U; source->path_storage[i] != '\0'; i++) {
        if (source->path_storage[i] == '/') {
            basename = i + 1U;
        }
    }
    if (copy_name(source, &source->path_storage[basename]) != 0) {
        clear_source(source);
        return -1;
    }

    source->kind = APP_IMAGE_SOURCE_VFS;
    source->path = source->path_storage;
    return 0;
}
