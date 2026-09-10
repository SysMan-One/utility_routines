#+++
#       DESCRIPTION: Uitility Routines --- a set of routines for plain C coders
#
#       AUTHOR: Ruslan R. Laishev
#
#       CREATION DATE:  8-MAR-2024
#
#       MODIFICATION HISTORY:
#
#		24-MAR-2026	RRL	Added "__MAIN_FOR_DEBUG__" for debug and developing purpose;
#					usage: cmake ... -D__MAIN_FOR_DEBUG__=1
#
#		01-AUG-2026	RRL	The installation and the packaging rules apply to a top-level build
#					only. A consumer that add_subdirectory()es this set got its CPack
#					configuration written into ITS build directory, so a 'cpack' there
#					produced a package named StarLet rather than the consumer's own.
#
#		01-AUG-2026	RRL	Made the set installable as a CMake package: install(TARGETS EXPORT),
#					public headers, and a StarLet:: package config. Consumers now use
#					find_package(StarLet) + StarLet::starlet, with no add_subdirectory()/
#					FetchContent(). CPack enabled for binary/source packages.
#
#		 7-AUG-2026	RRL	Package VERSION -> 1.1.0. This project() version is the single delivery
#					number - independent of the per-module __IDENT__ - and is bumped on
#					every delivery that changes any module, so the tarball name (and the
#					CPack/StarLetConfigVersion) identify the delivery without a diff.
#
#		 7-AUG-2026	RRL	VERSION -> 1.1.1. Bilingual manuals brought up to date: putmsg_fao /
#					PUTMSG_FAO, and the CLI command syntax (sub-verbs, /name or --name
#					qualifiers, positional/qualifier ordering, /-paths, the unrecognized-token
#					error, and the /HELP, -h, --help forms).
#
#		 7-AUG-2026	RRL	VERSION -> 1.2.0. Message lookup made severity-independent (s_msgcmp by
#					message number): one catalogue record may be signalled at different
#					severities. Manual notes the behaviour.
#
#		 7-AUG-2026	RRL	VERSION -> 1.2.1. s_msgcmp() compares facility then message number; a found
#					record is prefixed with the caller's severity (%<S>-). Manual updated.
#
#		 7-AUG-2026	RRL	VERSION -> 1.2.2. facname field restored in EMSG_RECORD_DESC; a signalled
#					record is now framed %<facility>-<S>-<text>. Manual updated.
#
#		 7-AUG-2026	RRL	VERSION -> 1.3.0. FAO address directives added: !%M (MAC), !%I (IPv4),
#					!%6 (IPv6), !%U (UUID) - see the starlet.c FAO header and test_fao_addr.
#
#		13-AUG-2026	RRL	VERSION -> 1.4.0. The !%6 directive now emits the RFC 5952 canonical
#					IPv6 form (:: compression, leftmost run on a tie). The bilingual manuals
#					gained Appendix A - the FAO Directives Reference (pages 39-40).
#
#		13-AUG-2026	RRL	VERSION -> 1.4.1. The log-record prefix is rendered by the cached
#					the prefix helper (byte-identical, ~4x faster end-to-end logging:
#					putmsg_fao 1153 -> 259 ns, LOG 1216 -> 302 ns). test_log_prefix added.
#
#		13-AUG-2026	RRL	VERSION -> 1.4.2. !%6 shows an IPv4-mapped address in the RFC 5952
#					mixed notation (::ffff:a.b.c.d), byte-identical to inet_ntop - the
#					parity is pinned by test_fao_addr. starlet __REV__ re-synced. Appendix A
#					refreshed.
#
#		14-AUG-2026	RRL	VERSION -> 1.4.3. project() now says LANGUAGES C: without it CMake
#					enables C and CXX by default and the configure fails on a host with no
#					C++ compiler ("No CMAKE_CXX_COMPILER could be found") - the library is
#					pure C and never needed one.
#
#		14-AUG-2026	RRL	VERSION -> 1.4.4. The ident rule is codified: __IDENT__ Xaa-bbECOc maps
#					to __REV__ aa.bb.c (a functional change bumps bb and clears the ECO; a
#					small fix appends ECOc). Two strays brought in line as ECOs: cli_routines
#					X.00-16ECO1 (__REV__ 0.02.0 -> 0.16.1), avproto X.94-11ECO1 (__REV__ was
#					absent -> 94.11.1).
#
#		14-AUG-2026	RRL	VERSION -> 1.4.5. House rules applied across the package: the author
#					line is "StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)" in
#					every module, history entries are marked RRL, and the delivery archive
#					is named <package>-<ident>.tar.gz (StarLet-1.4.5.tar.gz).
#
#		14-AUG-2026	RRL	VERSION -> 1.4.6. pattern_match() is iterative now: the hostile
#					multi-star pattern went from 8.6 s to 318 ns, typical matches 1.6-3.6x
#					faster; 500k-case fuzz against the old matcher agrees. test_pattern_match
#					added.
#
#		14-AUG-2026	RRL	VERSION -> 1.4.7. CI gained the musl job: CMake/CTest with musl-gcc
#					(-Werror) plus the standalone suite statically linked against musl, so
#					the musl compatibility verified by hand cannot creep away unnoticed.
#
#		14-AUG-2026	RRL	VERSION -> 1.4.8. The session's eight routines audited against the
#					routine-body template and the seven strays brought in line (ECOs:
#					utility V.01-26ECO1, starlet X.01-15ECO1). Legacy layout untouched.
#
#		29-AUG-2026	RRL	VERSION -> 1.5.0. LOG_FAO added: a LOG whose body is an FAO control
#					string, with the PUTMSG_FAO ergonomics (auto-cast, auto-count).
#					test_log_fao added (12 suites / 165 checks now).
#
#		29-AUG-2026	RRL	VERSION -> 1.5.1. The delivery unpacks into starlet-<ident>/ (was
#					utility_routines-master/), matching the <package>-<ident> archive rule -
#					each delivery lands in its own versioned directory.
#
#		31-AUG-2026	RRL	VERSION -> 1.5.2. The facility name is stamped into each message record
#					at inimsg() time (new trailing EMSG_RECORD.facname), removing the
#					second descriptor-list walk from every putmsg[d]_fao() call.
#
#		 1-SEP-2026	RRL	VERSION -> 1.5.3. House rule: no dollar-sign identifiers inside cmake
#					files - the history mentions of C names are rewritten without the sign
#					(the cmake-own brace expansions are unaffected).
#
#		 1-SEP-2026	RRL	VERSION -> 1.6.0. pattern_match() accepts the question mark as an alias
#					of the percent wildcard (exactly one octet).
#
#		 1-SEP-2026	RRL	VERSION -> 1.6.1. Documentation pulled up to date: README reflects the
#					delivery naming, LOG_FAO, the wildcard set, address directives, the
#					musl CI and the current test roster; the manuals gained Appendix B
#					(additions since the corpus) beside the refreshed Appendix A.
#
#		10-SEP-2026	RRL	VERSION -> 1.6.2. base64 / unbase64 added to utility_routines
#					(V.01-30): RFC 4648 with the bounded-buffer calling sequence,
#					strict decoder. The pair that BroProxy and BroAgent each kept in
#					a private header moves here. test_base64 added.
#---


cmake_minimum_required(VERSION 3.10)

cmake_policy(SET CMP0048 NEW)
project(StarLet  VERSION 1.6.2 DESCRIPTION "Utility Routines" LANGUAGES C)

# Included AFTER project() so a language (and thus the target architecture) is
# known - GNUInstallDirs needs it to pick the default lib dir.
include(CMakePrintHelpers)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_MAKEFILE   ON)
set(CMAKE_C_STANDARD 11)
#set (CMAKE_C_COMPILER /usr/bin/clang)                                                  # Only for debug\test purpose


# Predefine project
set( CPACK_PACKAGE_NAME  "${PROJECT_NAME}")
set( CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set( CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set( CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})


add_definitions( -D__ARCH__NAME__="${CMAKE_HOST_SYSTEM_PROCESSOR}" )


if (__MAIN_FOR_DEBUG__)
	add_definitions( -D__MAIN_FOR_DEBUG__=1)
endif()

if (CMAKE_BUILD_TYPE)
	string(TOLOWER ${CMAKE_BUILD_TYPE} build_type)                                  # Additional definitions for DEBUG builds
endif()

if (build_type STREQUAL debug)
	add_definitions( -D_DEBUG=1 -D__TRACE__=1 -DDEBUG=1)                            # Add "-w" to disable warnings
endif()




include_directories (
		./
)

set (SRC_LIST
	avproto.c
	avproto.h
	cli_routines.c
	cli_routines.h
	utility_routines.c
	utility_routines.h
	starlet.c
	starlet.h
)

add_library (starlet STATIC
		${SRC_LIST}
)

# Export the header directory so consumers that add_subdirectory()/FetchContent() this
# project get the include path automatically when they link against 'starlet'.
target_include_directories (starlet PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	)

	# In-tree consumers (add_subdirectory / FetchContent) get the SAME namespaced
	# target as find_package() consumers.
	add_library (StarLet::starlet ALIAS starlet)


if (__MAIN_FOR_DEBUG__)
	add_executable ( starlet.exe ${SRC_LIST})
endif()


# ---------------------------------------------------------------------------
# Regression tests.  Enabled by default; run with 'ctest' after building.
# Locks in the OpenVMS condition-value convention (1 & status) across the
# time facility - see tests/test_status_convention.c.
# ---------------------------------------------------------------------------
option (BUILD_TESTING "Build the regression tests" ON)

if (BUILD_TESTING)
	enable_testing()

	foreach (t status_convention cli_features tlv cli_help log_overflow setlogfd putmsg_fao cli_nested cli_filepath fao_addr log_prefix pattern_match log_fao base64)
		add_executable (test_${t} tests/test_${t}.c)
		target_link_libraries (test_${t} starlet)
		add_test (NAME ${t} COMMAND test_${t})
	endforeach()
endif()


# ---------------------------------------------------------------------------
# Installation and packaging.
#
#   cmake -B build
#   cmake --build build
#   cmake --install build --prefix /usr/local
#
# A consumer then uses, with no add_subdirectory()/FetchContent():
#   find_package(StarLet REQUIRED)
#   target_link_libraries(app PRIVATE StarLet::starlet)
# ---------------------------------------------------------------------------

#
#	A consumer that add_subdirectory()es this set wants the target, not the
#	packaging: its own install rules and its own CPack configuration must not
#	be joined by ours. PROJECT_IS_TOP_LEVEL would say it in one word, but it
#	arrived in CMake 3.21 and the minimum here is 3.10.
#
if ( CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR )

# The static library plus its usage requirements (the include dir) go into an export set.
install (TARGETS starlet
	EXPORT   StarLetTargets
	ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Public headers.
install (FILES
	utility_routines.h
	avproto.h
	cli_routines.h
	starlet.h
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# The exported targets, under the StarLet:: namespace.
install (EXPORT StarLetTargets
	FILE        StarLetTargets.cmake
	NAMESPACE   StarLet::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/StarLet
)

# The package config + version files, so find_package(StarLet) resolves.
configure_package_config_file (
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/StarLetConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/StarLetConfig.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/StarLet
)
write_basic_package_version_file (
	${CMAKE_CURRENT_BINARY_DIR}/StarLetConfigVersion.cmake
	VERSION       ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion
)
install (FILES
	${CMAKE_CURRENT_BINARY_DIR}/StarLetConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/StarLetConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/StarLet
)

# Optional: binary/source packages via 'cpack' in the build dir.
include (CPack)

endif()					# CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR
