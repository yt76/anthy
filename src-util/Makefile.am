## $Id: Makefile.am,v 1.10 2002/08/08 07:20:45 gniibe Exp $

SUBDIRS =

# Files
# Dictionary utilities
ELISP_FILES = anthy.el anthy-dic.el anthy-conf.el leim-list.el \
 anthy-isearch.el anthy-azik.el anthy-kyuri.el
EXTRA_DIST = $(ELISP_FILES) typetab dic-tool-usage.txt
bin_PROGRAMS = anthy-dic-tool anthy-agent anthy-morphological-analyzer
ELCFILES = anthy.elc anthy-dic.elc anthy-azik.elc anthy-conf.elc anthy-isearch.elc anthy-kyuri.elc leim-list.elc
if ELISP
lisp_LISP = $(ELISP_FILES)
endif

noinst_LTLIBRARIES = libconvdb.la
libconvdb_la_SOURCES = convdb.c



AM_CPPFLAGS = -I$(top_srcdir)/
anthy_dic_tool_SOURCES = dic-tool.c
anthy_dic_tool_LDADD = ../src-main/libanthy.la
anthy_agent_SOURCES = agent.c egg.c input.c input.h rkconv.c rkhelper.c\
 rkconv.h rkmap.h rkhelper.h
anthy_agent_LDADD = ../src-main/libanthy.la
anthy_morphological_analyzer_SOURCES= morph-main.c
anthy_morphological_analyzer_LDADD = libconvdb.la ../src-main/libanthy.la

pkgdata_DATA = typetab dic-tool-usage.txt
