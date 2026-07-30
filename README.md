# utility_routines

A small, freestanding-friendly C library of OpenVMS-flavoured building blocks for
systems and embedded work. Portable C11 (GCC/Clang); tested on x86-64 and ARMv8
(RK33xx/RK35xx), with a hardware CRC-32 path on ARMv8.

## Modules

| Module | Prefix | What it provides |
|--------|--------|------------------|
| `utility_routines` | `__util$` / `$LOG` | Logging (`$LOG`, `$TRACE`, `$DUMPHEX`), ASCIC counted strings, hex/UUID conversion, CRC-32, command-line **and** config-file option parsing, spinlock-protected queues, wildcard matching. |
| `avproto` | `__tlv$` | A compact binary Tag-Length-Value protocol: a fixed header plus TLV records, all in network byte order. |
| `cli_routines` | `__cli$` | A DCL-style command-line parser: `verb [p1..p8] [/qualifier[=value]]`, with per-value type validation. |
| `starlet` | `__util$` | Portable analogues of OpenVMS system services: the FAO formatter (`__util$fao`) and binary-time routines (`__util$bintim`/`asctim`/`add_times`/`sub_times`). |

Routines follow the OpenVMS condition-value convention: the **low bit of the returned
status is set on success**.

## Building

```sh
cc -std=gnu11 -O2 -c utility_routines.c avproto.c cli_routines.c starlet.c
cc *.o -o your_app                 # link with your application
```

- Define `__MODULE__` (and optionally `__FAC__`) before including a header, so log
  records carry the right prefix.
- Build with `-DNDEBUG` to compile out `$TRACE`/asserts in release.
- On ARMv8, add `-march=armv8-a+crc` to enable the hardware CRC-32 in `__util$crc32c`.

A `CMakeLists.txt` is also provided. It builds the static-library target **`starlet`**
(all four modules) and exports its include directory, so consumers get the headers
automatically when they link it. The executable target needs a `main()` of your own.

## Quick integration (git + CMake)

**As a git submodule.** Vendor the sources under your tree and add the subdirectory:

```sh
git submodule add https://<host>/utility_routines.git external/utility_routines
```

```cmake
add_subdirectory(external/utility_routines)

add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE starlet)   # headers come with the target
```

**Without a submodule (CMake 3.11+, FetchContent).** Let CMake fetch it at configure time:

```cmake
include(FetchContent)
FetchContent_Declare(utility_routines
    GIT_REPOSITORY https://<host>/utility_routines.git
    GIT_TAG        main)          # or a release tag / commit
FetchContent_MakeAvailable(utility_routines)

add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE starlet)
```

Then in your code:

```c
#define __MODULE__ "MYAPP$"
#include "utility_routines.h"     /* and avproto.h / cli_routines.h / starlet.h as needed */
```

For ARMv8 hardware CRC, pass `-march=armv8-a+crc` (e.g.
`target_compile_options(starlet PRIVATE -march=armv8-a+crc)`).

## Documentation

Full reference manuals (VSI OpenVMS RTL style) are included:

- `Routines_Reference_EN.pdf` — English
- `Routines_Reference_RU.pdf` — Russian

Both cover every public routine (Format / Returns / parameter tables / condition
values) plus worked, compilable examples.

## Coding style

Whitesmiths indentation, tabs, `$`-in-identifier VMS naming (a GCC extension),
`s_` prefix for static routines, VMS-style file/history headers. The tree builds
clean under `-Wall -Wextra -std=gnu11`.

## License

Apache License 2.0 — see `LICENSE`.

## Author

Ruslan R. Laishev
