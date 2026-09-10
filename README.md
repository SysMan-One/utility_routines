# StarLet (utility_routines)

Delivered as `StarLet-<ident>.tar.gz`, unpacking into `starlet-<ident>/`.

A small, freestanding-friendly C library of OpenVMS-flavoured building blocks for
systems and embedded work. Portable C11 (GCC/Clang); tested on x86-64 and ARMv8
(RK33xx/RK35xx), with a hardware CRC-32 path on ARMv8.

## Modules

| Module | Prefix | What it provides |
|--------|--------|------------------|
| `utility_routines` | `__util$` / `$LOG` | Logging (`$LOG`, `$LOG_FAO`, `$TRACE`, `$DUMPHEX`), OpenVMS-style message catalogues (`$PUTMSG_FAO`), ASCIC counted strings, hex/UUID conversion, base64, CRC-32, command-line **and** config-file option parsing, spinlock-protected queues, wildcard matching (`*` a run, `%`/`?` one octet). |
| `avproto` | `__tlv$` | A compact binary Tag-Length-Value protocol: a fixed header plus TLV records, all in network byte order. |
| `cli_routines` | `__cli$` | A DCL-style command-line parser: `verb [p1..p8] [/qualifier[=value]]`, with per-value type validation. |
| `starlet` | `__util$` | Portable analogues of OpenVMS system services: the FAO formatter (`__util$faol`, driving the `$FAO`/`$PUTMSG_FAO`/`$LOG_FAO` macros) with numeric, string, time and address directives (MAC, IPv4, RFC 5952 IPv6, UUID), and binary-time routines (`__util$bintim`/`asctim`/`add_times`/`sub_times`). |

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

## Tests

Regression tests under `tests/` (all exit non-zero on failure, so they drop
straight into a build pipeline):

- **`test_status_convention`** — locks in the OpenVMS condition-value
  convention (`1 & status` — odd is success, even is failure): `__util$bintim`,
  `__util$asctim`, the `CLI$K_TIME`/`CLI$K_MAC` value types and the severity of
  the `STS$K_*`/`FAO$K_*` codes.
- **`test_cli_features`** — qualifier defaults, negatable qualifiers (including
  "last wins" for a repeated `/X` / `/NOX`), value lists, `__cli$present` and
  required qualifiers.
- **`test_tlv`** — a full `__tlv$` PDU round-trip (header, word/longword/qword
  byte-order conversion, octet blocks, lookup and a missing tag).
- **`test_cli_help`** — the built-in `/HELP`, `-h`, `--help` detection, and that
  a bare `help` token stays an ordinary verb or parameter value.
- **`test_log_overflow`** — the logging routines truncate oversized records in
  bounds and never write past their buffers.
- **`test_setlogfd`** — `__util$setlogfd()` points the log at a given descriptor
  without opening a file or seizing the standard streams.
- **`test_putmsg_fao`** — the `$PUTMSG_FAO` catalogue path: FAO-formatted records,
  severity-independent lookup, and the facility name stamped into each record.
- **`test_cli_nested`** / **`test_cli_filepath`** — `SHOW STATISTICS`-style sub-verbs;
  `/`-paths staying parameter values while `/qualifiers` keep parsing.
- **`test_fao_addr`** — the `!%M`/`!%I`/`!%6`/`!%U` address directives, RFC 5952
  corner cases, byte parity with `inet_ntop`.
- **`test_log_prefix`** — the cached log-record prefix stays byte-identical to the
  historical rendering; the tid column matches the kernel's.
- **`test_pattern_match`** — fixed vectors, a 500k-case fuzz against the former
  recursive matcher, the `?` alias, and the pathological multi-star pattern
  returning at once.
- **`test_log_fao`** — the `$LOG_FAO` envelope, FAO body, address directives and
  the zero-parameter form.
- **`test_base64`** — `__util$base64`/`__util$unbase64`: the RFC 4648 vectors,
  a random round-trip over every length, unpadded input, the courtesy NUL, and
  the rejections (a foreign octet, a misplaced `=`, a dangling sextet, a short
  buffer with the needed length still reported).

A GitHub Actions workflow (`.github/workflows/ci.yml`) runs them on every push:
via CTest and standalone under ASan/UBSan with glibc, then again with `musl-gcc`
(CTest plus the standalone suite statically linked against musl, `-Werror` throughout).

With CMake:

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

Or standalone, without CMake:

```sh
sh tests/run.sh          # honours CC and CFLAGS
```

## Install as a CMake package

Build and install once:

```sh
cmake -B build
cmake --build build
cmake --install build --prefix /usr/local     # or any prefix
```

Then consume it from another project with no `add_subdirectory()` /
`FetchContent()` - the package name is **StarLet**, the target is
**`StarLet::starlet`** (the static library of all four modules):

```cmake
find_package(StarLet REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE StarLet::starlet)
```

The install ships the four public headers, `libstarlet.a`, and the
`StarLetConfig.cmake` / `StarLetConfigVersion.cmake` / `StarLetTargets.cmake`
files under `<prefix>/lib/cmake/StarLet`. Version requests are honoured with
same-major-version compatibility, e.g. `find_package(StarLet 1.0 REQUIRED)`.
Binary or source packages can be produced with `cpack` from the build dir.

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

## Manuals

Bilingual reference manuals ship with the tree: `Routines_Reference_EN.pdf` and
`Routines_Reference_RU.pdf`. Appendix A is the FAO directives reference (including
the address directives); Appendix B covers the additions since the main corpus -
`$LOG_FAO`, the wildcard set (`*`, `%`, `?`), the facility-name stamping and the
cached log-record prefix.
