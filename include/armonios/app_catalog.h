#ifndef ARMONIOS_INCLUDE_ARMONIOS_APP_CATALOG_H
#define ARMONIOS_INCLUDE_ARMONIOS_APP_CATALOG_H

/*
 * Shared visible application catalog.
 *
 * Entry fields:
 *   id, visible label, bootfs path, pinned taskbar flag, clock-widget flag.
 *
 * The build and linker scripts still list app objects/sections explicitly:
 * those files define build and link boundaries, while this catalog defines
 * runtime metadata consumed by bootfs, boot_program, and the panel.
 */
#define ARMONIOS_APP_CATALOG(X) \
    X(shell, "Shell", "/armonios/shell", 1, 0) \
    X(editor, "Editor", "/armonios/editor", 1, 0) \
    X(files, "Files", "/armonios/files", 1, 0) \
    X(monitor, "Monitor", "/armonios/monitor", 1, 0) \
    X(control, "Control Panel", "/armonios/control", 0, 0) \
    X(clock, "Clock", "/armonios/clock", 0, 1)

/* The panel is a boot image and bootfs file, not a visible panel menu app. */
#define ARMONIOS_BOOT_ONLY_APP_CATALOG(X) \
    X(panel, "Panel", "/armonios/panel", 0, 0)

#define ARMONIOS_BOOT_APP_CATALOG(X) \
    ARMONIOS_APP_CATALOG(X) \
    ARMONIOS_BOOT_ONLY_APP_CATALOG(X)

#endif
