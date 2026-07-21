#!/usr/bin/env python3

from pathlib import Path

path = Path("kernel/syscall_gui.c")
text = path.read_text()

replacements = [
    (
        """    sys_copy_to_user_validated(out_ptr, out, sizeof(out));
    return 0;
""",
        """    return sys_copy_to_user(process, out_ptr, out, sizeof(out));
""",
    ),
    (
        """    sys_copy_to_user_validated(out_ptr, &state, sizeof(state));
    return 0;
""",
        """    return sys_copy_to_user(process, out_ptr, &state, sizeof(state));
""",
    ),
    (
        """        sys_copy_to_user_validated(buf_ptr + n * sizeof(out),
                                   out, sizeof(out));
        n++;
""",
        """        status = sys_copy_to_user(process, buf_ptr + n * sizeof(out),
                                  out, sizeof(out));
        if (status != 0) {
            return status;
        }
        n++;
""",
    ),
]

for old, new in replacements:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected one match, found {count}")
    text = text.replace(old, new, 1)

path.write_text(text)
Path(__file__).unlink()
