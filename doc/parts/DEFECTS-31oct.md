# StarLet 1.6.11 - defects found while writing the reference (31-OCT-2026, not fixed yet)

From the part-2 writer (starlet / cli_routines / avproto), several confirmed with AddressSanitizer:
1. FAO: a control string ending in a lone "!", "!A", "!%" or "!n*" - the scanner reads past the NUL (ASan).
2. !%T/!%D/__util$asctim: time 0 means "now" (the epoch cannot be shown); pre-1970 prints as a delta.
3. !%D year > 9999 reads past the digit table.
4. !n<...!> field > 512 bytes cut silently; __util$fao reads undefined stack values if the control string wants more parameters than passed.
5. __util$sub_time returns an unnormalised timespec; __util$cmp_time sign can flip past INT_MAX seconds.
6. __tlv$put overruns the buffer for word/long/quad/UUID records when a_valsz is 0 or too small (ASan, UUID with a_valsz 4).
7. __tlv$put UUID: header len 16 but adds 4+a_valsz to PDU len -> corrupt PDU.
8. __tlv$get does not check the wire length of numeric records (2/4/8) - over-read on malformed input.
9. __tlv$dump: a value of >= 256 octets writes one byte past its 512-byte stack buffer (bin2hex NUL).
10. __tlv$hget writes STS$K_ERROR into *a_u_csr on bad signature; hset silently cuts a signature > 8, hget rejects it; __tlv$put buffer size is unsigned short.
11. CLI: a verb that is a prefix of another (SET/SETUP) cannot be typed in full; a verb with no params/quals ignores extra words; sub-verb help unreachable; keyword value stored as typed, its val not exposed.
12. CLI$K_DATE / CLI$K_NUM lax (month 99 passes, minus wraps).
13. __cli$show_verbs does not restore level; __cli$show_ctx exported but not declared; CLI_PQDESC.flag commented "Reserved" but used.
14. __util$readparams declared, not defined; __util$fill loops forever with source size 0; __util$faohex dereferences a NULL a_outlen.

From the part-1 writer (utility_routines), each confirmed by a small test:
15. __util$bzero never clears the last (len mod 4) bytes.
16. __util$movqtail corrupts the queue (truncates, head not updated, asserts on the tail); __util$movqhead leaves tail stale when moving the tail.
17. clrqueue on empty leaves *count unset; remqhead/remqtail/remqent on empty return SUCCESS, *ent unset.
18. UTIL$S_INQUE == UTIL$S_NOLOCK (0x202); insqhead returns plain ERROR.
19. $ILE3INI does not compile with ordinary arguments (macro parameter names = member names).
20. __util$strelem wrong for index >= 2.
21. __util$pattern_match: a '*' in the subject facing a '*' in the pattern is taken literally ("*ba" !~ "*a"); the comment describes a backslash escape that does not exist.
22. logd/logd_fao print "[(null)\func:line]" with __MODULE__ NULL (debug build).
23. __util$log/log_fao: STS$M_SYSLOG or severity > 4 returns 5 (STS$K_UNDEF); in debug $LOG -> logd never syslogs.
24. __util$trace writes "mod\\func" (two backslashes).
25. __util$syslog without a host: "sendto: Invalid argument" on stderr + ERROR (source says no-op); perror calls; abort() on Windows; dead __SYSLOG__ block referencing undefined __li.
26. Log time fraction = ns >> 20 (000..953), not ms; Windows /1024 gives 6 digits.
27. isinrange2 excludes borders (comment: include); cmp_time sign opposite to comment; sub_time negative tv_nsec; uncomment ignores quotes, comment copied from trim.
28. Off-by-one: str2asc / OPTS$K_STR NUL at sts[255]; strcat_rx ERROR + NUL at dst[dstsz] on exact fill; mask2sym room check; collapse needs srclen+1.
29. iszero skips up to 4 bytes where long is 4 bytes (Windows).
30. getparams/readconfig write int for OPTS$K_INT of size != 8 (short overflows); OPTS$K_PORT host order (comment: network); no abbreviations (comment: yes); OPTS$K_CONF not handled in a config file; trims argv in place; returns SUCCESS after a bad value/unknown option.
31. __util$crc32c computes IEEE CRC-32 (0xCBF43926), not Castagnoli.
32. Not thread-safe: inimsg, first call of syslog.

From the Programmer's Guide writer (additional):
33. CLI verbs: no exact-match rule ("set" vs SET/SETUP -> Ambiguous); unknown --bogus becomes the next optional parameter; "/blocks 5" (value after a space) -> Unrecognized token; CLI$M_OPSIGNAL logs an error on every get_value probe of an absent item; __cli$parse returns WARN after printing help.
34. __tlv$get of a UUID TLV shorter than 16 calls movc5 with a NULL fill pointer (hostile packet crash); __tlv$put does not mask the tag id to 12 bits.
35. __util$logd under __SYSLOG__ cannot compile (va_start(..., __li), parameter is __line); __util$inimsg WARN for NULL and duplicate alike; default line of an unknown condition lacks the leading '%'.
36. Docs/comments: README -march advice obsolete (target attributes); tests/run.sh lacks lookup_key, logrotate; avproto.h disables three warnings for every includer; include order matters (utility_routines.h before starlet.h and cli_routines.h).
37. AUTHORSHIP: starlet.h author line "Claude for D.Novikov"; DN/SYS/Anton initials in some histories - contradicts the rule (StarLet Squad and Ruslan R. Laishev only).
38. LEGAL/PROVENANCE: utility_routines.h carries the LIB$MOVC5 header from a Compaq LIBMOVC5.C listing; utility_routines.c carries the C_STR$pattern_match header from DEBUG STRINGS.C (1989-1990 history). Must be rewritten in own words before an Apache release.
39. No tests: queues, bzero, strelem, out, getparams/readconfig.

Left for 1.7.1 (found by the 1.7.0 doc pass, 31-OCT-2026) - ALL EIGHT FIXED IN 1.7.1 (see Appendix D of the Programmer's Guide):
- [fixed in 1.7.1] __tlv$lookup does not check the wire length of fixed-size records (get does).
- [fixed in 1.7.1] __util$deflog comment says records go to the UDP host; nothing calls __util$syslog automatically.
- [fixed in 1.7.1] A severity above FATAL is clamped to 5 (UNDEF, odd) and returned - reads as success.
- [fixed in 1.7.1] $LOG_FAO/$PUTMSG_FAO ignore a bad FAO status (a typo in a control string goes unnoticed).
- [fixed in 1.7.1] __util$hex2bin does not validate its input.
- [fixed in 1.7.1] showparams: 4-octet/int options printed signed, 1/2/8 unsigned; a negative value is accepted for a 2-octet option.
- [fixed in 1.7.1] A quoted config value keeps its quote characters.
- [fixed in 1.7.1] __cli$get_keyword: /mode with an empty value is ERROR where get_value says WARN.
