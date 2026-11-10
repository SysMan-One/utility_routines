#!/usr/bin/env python3
"""
**++
**  FACILITY:		STARLET - Utility Routines
**
**  MODULE:		doc/mkpdf.py
**
**  ABSTRACT:		Renders a manual of the library as PDF from its markdown source.
**
**			The markdown follows the layout of an OpenVMS manual, where the number of a
**			chapter and its title stand on separate lines. That reads well as text and
**			badly as a document, so the two are joined here and the identification block of
**			the title page is lifted out into metadata.
**
**			An entry of the command dictionary or of the messages is a line in bold with
**			its description indented under it. As text that is the look of an OpenVMS
**			manual; to pandoc an indented paragraph is code. The entry is therefore made a
**			definition list here, and what is indented further stays code inside it.
**
**			The faces are named explicitly: the template of pandoc otherwise reaches for
**			Latin Modern, which not every installation of TeX carries, and the DejaVu family
**			is present everywhere a modern system is. A dollar sign is everywhere in the
**			names of OpenVMS and would otherwise open mathematics in LaTeX, so that extension
**			of the markdown reader is turned off.
**
**			Taken from the renderer of the manuals of MSCP-Hub (and BATCH before it).
**
**  AUTHOR:		StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:	22-OCT-2026
**
**  MODIFICATION HISTORY:
**
**		31-OCT-2026	CLD	1.6.11 : Taken over from MSCP-Hub for the manuals of StarLet; long
**					lines of code are folded (fvextra).
**
**--
"""

import re
import subprocess
import sys
import tempfile
from pathlib import Path

#  The reader: markdown without the extensions that would take a dollar sign for mathematics
READER = "markdown-tex_math_dollars-tex_math_single_backslash-raw_tex"

#  A manual exists in two languages, and each names its divisions and its title page in its own
#  words. Both vocabularies are recognized so that one renderer serves both.
DIVISIONS = (r"Chapter \d+|Appendix [A-Z]"
		r"|\u0413\u043b\u0430\u0432\u0430 \d+|\u041f\u0440\u0438\u043b\u043e\u0436\u0435\u043d\u0438\u0435 [A-Z]")

LABELS = {
	"order number":		"order",
	"software version":	"version",
	"revision/update":	"revision",
	"date":			"date",
	"\u043d\u043e\u043c\u0435\u0440 \u0438\u0437\u0434\u0430\u043d\u0438\u044f":	"order",
	"\u0432\u0435\u0440\u0441\u0438\u044f \u043f\u0440\u043e\u0433\u0440\u0430\u043c\u043c\u044b":	"version",
	"\u043f\u0440\u0430\u0432\u043a\u0430":	"revision",
	"\u0434\u0430\u0442\u0430":			"date",
	"num\u00e9ro de commande":	"order",
	"version du logiciel":	"version",
	"r\u00e9vision":		"revision",
}

#  Preamble. The manual is set in a serif face with monospace for everything the reader would type,
#  a running head carrying the chapter, and the identification of the software in the foot.
PREAMBLE = r"""
\usepackage{fancyhdr}
\usepackage{titlesec}
\usepackage{needspace}

\pagestyle{fancy}
\fancyhf{}
\fancyhead[L]{\small\nouppercase{\leftmark}}
\fancyhead[R]{\small\thepage}
\fancyfoot[L]{\footnotesize %(facility)s %(version)s}
\fancyfoot[R]{\footnotesize %(kind)s}
\renewcommand{\headrulewidth}{0.4pt}
\renewcommand{\footrulewidth}{0.4pt}

\fancypagestyle{plain}{%%
	\fancyhf{}%%
	\fancyfoot[L]{\footnotesize %(facility)s %(version)s}%%
	\fancyfoot[R]{\footnotesize %(kind)s}%%
	\renewcommand{\headrulewidth}{0pt}%%
	\renewcommand{\footrulewidth}{0.4pt}}

%%  A chapter opens a page of its own, as it does in a printed manual
\titleformat{\section}[display]{\normalfont\Large\bfseries}{}{0pt}{}
\newcommand{\sectionbreak}{\clearpage}

%%  A long line of code is folded at the margin instead of running off the page
\usepackage{fvextra}
\DefineVerbatimEnvironment{Highlighting}{Verbatim}{breaklines,breakanywhere,commandchars=\\\{\}}
\fvset{breaklines=true,breakanywhere=true}

%%  A message entry is not to be split from its explanation
\titlespacing*{\subsubsection}{0pt}{2ex plus 1ex minus .2ex}{1ex plus .2ex}
"""


def split_title(text):
	"""Lift the title page out of the body and return it as metadata."""
	lines = text.split("\n")
	meta = {}

	m = re.match(r"^# (.+)$", lines[0])
	meta["title"] = m.group(1) if m else "STARLET"

	for line in lines[1:12]:
		m = re.match(r"^## (.+)$", line)

		if m:
			meta["kind"] = m.group(1)
			break

	#  An indented line that is not a field of the identification block is the notice - the copyright
	#  and the terms - which goes on the back of the title page; a blank line ends a paragraph of it
	notice = [""]

	for line in lines[:30]:
		if line.strip() == "---":
			break

		m = re.match(r"^\s{4}(\S[^:]*):\s+(.+?)\s*$", line)
		key = LABELS.get(m.group(1).strip().lower()) if m else None

		if key:
			meta[key] = m.group(2)
		elif m:
			continue
		elif line.startswith("    "):
			notice[-1] = (notice[-1] + " " + line.strip()).strip()
		elif not line.strip() and notice[-1]:
			notice.append("")

	meta["notice"] = [para for para in notice if para]

	#  The body starts after the rule that closes the title page
	for i, line in enumerate(lines):
		if line.strip() == "---" and i > 3:
			return meta, "\n".join(lines[i + 1:])

	return meta, text


def back_of_title(paras):
	"""The notice as LaTeX: at the foot of a page of its own, after the title page."""
	SPECIAL = { "\\": r"\textbackslash{}", "&": r"\&", "%": r"\%", "$": r"\$", "#": r"\#",
			"_": r"\_", "{": r"\{", "}": r"\}", "~": r"\textasciitilde{}", "^": r"\textasciicircum{}" }

	text = "\n\n".join("\\noindent " + "".join(SPECIAL.get(c, c) for c in para).replace("(C)", r"\copyright{}")
			for para in paras)

	return "\\thispagestyle{empty}\n\\vspace*{\\fill}\n{\\small\n%s\n}\n\\clearpage\n" % text


def join_headings(text):
	"""Join a chapter number with its title, which the source keeps on separate lines."""
	text = re.sub(r"^# (%s)\n# (.+)$" % DIVISIONS,
			lambda m: "# %s. %s" % (m.group(1), m.group(2)), text, flags=re.M)

	#  A rule between chapters is a page break in the document, which the chapter opening already
	#  provides, so it is dropped rather than drawn
	return re.sub(r"^---$\n", "", text, flags=re.M)


def entries(text):
	"""Make an entry - a line in bold, its description indented under it - a definition list."""
	return re.sub(r"^( {0,3}\*\*.+)\n\n {4}(?=\S)", r"\1\n\n:   ", text, flags=re.M)


def build(src, out):
	text = Path(src).read_text(encoding="utf-8")
	meta, body = split_title(text)
	body = entries(join_headings(body))

	#  The language of a manual is marked by the suffix of its name, and is handed to the typesetter
	#  so that it hyphenates and names its table of contents accordingly. A fourth language costs a
	#  line of the table.
	SUFFIXES = { "_ru": "ru" }

	lang = "en"

	for suffix, code in SUFFIXES.items():
		if suffix in Path(src).stem:
			lang = code
			break

	with tempfile.TemporaryDirectory() as tmp:
		hdr = Path(tmp) / "preamble.tex"
		hdr.write_text(PREAMBLE % {
				"facility": meta.get("title", "STARLET").split()[0],
				"version": meta.get("version", "").replace("STARLET Version ", ""),
				"kind": meta.get("kind", "")}, encoding="utf-8")

		back = Path(tmp) / "back.tex"
		back.write_text(back_of_title(meta["notice"]) if meta["notice"] else "", encoding="utf-8")

		md = Path(tmp) / "body.md"
		md.write_text(body, encoding="utf-8")

		cmd = [
			"pandoc", "-f", READER, "-o", out,
			"--pdf-engine=xelatex",
			"--toc", "--toc-depth=2",
			"-V", "documentclass=report",
			"-V", "papersize=a4",
			"-V", "geometry:margin=25mm",
			"-V", "fontsize=10pt",
			"-V", "mainfont=DejaVu Serif",
			"-V", "sansfont=DejaVu Sans",
			"-V", "monofont=DejaVu Sans Mono",
			"-V", "monofontoptions=Scale=0.85",
			#  The template reaches for Latin Modern when no font package is named, and not
			#  every installation of TeX carries it. Naming one that is always present keeps
			#  the template off that path; the faces themselves come from fontspec above.
			"-V", "fontfamily=charter",
			"-V", "colorlinks=true",
			"-V", "linkcolor=black",
			"-V", "toccolor=black",
			"-V", "title=%s" % meta.get("title", "STARLET"),
			"-V", "subtitle=%s" % meta.get("kind", ""),
			"-V", "author=%s" % meta.get("order", ""),
			"-V", "date=%s   ***   %s" % (meta.get("version", ""), meta.get("date", "")),
			"-V", "lang=%s" % lang,
			"--include-in-header=%s" % hdr,
			"--include-before-body=%s" % back,
			str(md),
		]

		res = subprocess.run(cmd, capture_output=True, text=True)

		if res.returncode:
			sys.stderr.write(res.stdout + res.stderr)
			return 1

		if res.stderr.strip():
			sys.stderr.write(res.stderr)

	print("%%STARLET-S-PDF, %s -> %s" % (src, out))

	return 0


if __name__ == "__main__":
	rc = 0

	for pair in sys.argv[1:]:
		src, _, out = pair.partition(":")
		rc |= build(src, out)

	sys.exit(rc)
