# Files
noinst_SCRIPTS =
CLEANFILES = anthy.*
AM_CPPFLAGS = -I$(top_srcdir)/ -DSRCDIR=\"$(srcdir)\"

noinst_PROGRAMS = mkfiledic
mkfiledic_SOURCES = mkfiledic.c
mkfiledic_LDADD = ../src-diclib/libdiclib.la

anthy.dic : mkfiledic ../mkworddic/anthy.wdic ../depgraph/anthy.dep  $(top_srcdir)/calctrans/corpus_info
	./mkfiledic -c $(top_srcdir)/calctrans/corpus_info


# To install 
pkgdata_DATA = anthy.dic
