## $Id: Makefile.am,v 1.4 2002/02/25 07:25:03 yusuke Exp $

AM_CPPFLAGS = -I$(top_srcdir)/ -DCONF_DIR=\"$(sysconfdir)\"

libanthydic_la_SOURCES = \
	word_dic.c dic_util.c \
	wtype.c\
	textdict.c record.c\
	word_lookup.c use_dic.c \
	priv_dic.c mem_dic.c \
	ext_ent.c matrix.c\
	feature_set.c\
	dic_main.h\
	ptab.h wtab.h dic_ent.h \
	mem_dic.h dic_personality.h

libanthydic_la_LIBADD = ../src-diclib/libdiclib.la
libanthydic_la_LDFLAGS = -version-info 1:0:1
noinst_LTLIBRARIES = libanthydic.la
