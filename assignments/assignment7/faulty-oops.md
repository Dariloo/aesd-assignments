# Assignment 7 - Faulty Kernel Oops Analysis

The kernel oops was generated with the command:

`echo "hello_world" > /dev/faulty`

The important part of the kernel message was:

- `Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000`
- `WnR = 1`
- `pc : faulty_write+0x18/0x20 [faulty]`

The call trace starts with:

- `faulty_write+0x18/0x20 [faulty]`
- `ksys_write+0x74/0x110`
- `__arm64_sys_write+0x24/0x30`

The message shows that the kernel tried to access address `0x0`, which means a NULL pointer was dereferenced.

The value `WnR = 1` shows that the fault occurred during a write operation.

The program counter points to the `faulty_write()` function in the `faulty` kernel module.

Looking at `misc-modules/faulty.c`, the faulty instruction is on line 53:

`*(int *)0 = 0;`

This instruction intentionally writes to address zero and causes the kernel oops.

The call trace shows that the write system call eventually reached `faulty_write()`. This matches the command used to write `"hello_world"` to `/dev/faulty`.

The function name and offset shown in the oops help locate the failing function. The source code can then be inspected to find the faulty line. Tools such as `objdump` can also be used to relate the instruction offset to the compiled kernel module when necessary.
