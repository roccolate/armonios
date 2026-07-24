#include <stdint.h>

#include "../kernel/app_image_source.h"
#include "../kernel/process.h"

process_t *process_current(void) {
    return 0;
}

process_t *process_find(uint32_t pid) {
    (void)pid;
    return 0;
}

static void check_true(int ok) {
    if (!ok) {
        __builtin_trap();
    }
}

#define CHECK_TRUE(expr) check_true((expr) != 0)
#define CHECK_EQ(expected, actual) check_true((expected) == (actual))

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

static void test_builtin_paths_resolve_to_bootfs(void) {
    app_image_source_t source;

    CHECK_EQ(0, app_image_source_resolve("/armonios/editor", &source));
    CHECK_EQ(APP_IMAGE_SOURCE_BOOTFS, source.kind);
    CHECK_TRUE(text_equal(source.name, "editor"));
    CHECK_TRUE(text_equal(source.path, "editor"));

    CHECK_EQ(0, app_image_source_resolve("/armonios/./clock", &source));
    CHECK_EQ(APP_IMAGE_SOURCE_BOOTFS, source.kind);
    CHECK_TRUE(text_equal(source.name, "clock"));
    CHECK_TRUE(text_equal(source.path, "clock"));
}

static void test_external_paths_resolve_to_canonical_vfs(void) {
    app_image_source_t source;

    CHECK_EQ(0, app_image_source_resolve(
                    "/fat//APPS/../HELLO.KLI", &source));
    CHECK_EQ(APP_IMAGE_SOURCE_VFS, source.kind);
    CHECK_TRUE(text_equal(source.path, "/fat/HELLO.KLI"));
    CHECK_TRUE(text_equal(source.name, "HELLO.KLI"));

    CHECK_EQ(0, app_image_source_resolve(
                    "/fat/APPS/HELLOWIN.KLI", &source));
    CHECK_EQ(APP_IMAGE_SOURCE_VFS, source.kind);
    CHECK_TRUE(text_equal(source.path, "/fat/APPS/HELLOWIN.KLI"));
    CHECK_TRUE(text_equal(source.name, "HELLOWIN.KLI"));

    CHECK_EQ(0, app_image_source_resolve(
                    "/armoniosx/HELLO.KLI", &source));
    CHECK_EQ(APP_IMAGE_SOURCE_VFS, source.kind);
    CHECK_TRUE(text_equal(source.name, "HELLO.KLI"));
}

static void test_shell_run_absolute_path_resolves_to_vfs(void) {
    app_image_source_t source;

    /* `run /fat/HELLO.KLI` is passed to spawn in this historical shape. */
    CHECK_EQ(0, app_image_source_resolve(
                    "/armonios//fat/HELLO.KLI", &source));
    CHECK_EQ(APP_IMAGE_SOURCE_VFS, source.kind);
    CHECK_TRUE(text_equal(source.path, "/fat/HELLO.KLI"));
    CHECK_TRUE(text_equal(source.name, "HELLO.KLI"));

    CHECK_EQ(0, app_image_source_resolve(
                    "/armonios///fat/APPS/../HELLO.KLI", &source));
    CHECK_EQ(APP_IMAGE_SOURCE_VFS, source.kind);
    CHECK_TRUE(text_equal(source.path, "/fat/HELLO.KLI"));
}

static void test_invalid_spawn_paths_are_rejected(void) {
    app_image_source_t source;
    const char *too_long =
        "/fat/ABCDEFGHIJKLMNOPQRSTUVWXYZ123456.KLI";

    CHECK_EQ(-1, app_image_source_resolve(0, &source));
    CHECK_EQ(-1, app_image_source_resolve("relative", &source));
    CHECK_EQ(-1, app_image_source_resolve("/", &source));
    CHECK_EQ(-1, app_image_source_resolve("/armonios", &source));
    CHECK_EQ(-1, app_image_source_resolve(
                     "/armonios/tools/editor", &source));
    CHECK_EQ(-1, app_image_source_resolve(too_long, &source));
    CHECK_EQ(-1, app_image_source_resolve("/fat/HELLO.KLI", 0));
}

int main(void) {
    test_builtin_paths_resolve_to_bootfs();
    test_external_paths_resolve_to_canonical_vfs();
    test_shell_run_absolute_path_resolves_to_vfs();
    test_invalid_spawn_paths_are_rejected();
    return 0;
}
