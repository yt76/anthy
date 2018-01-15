CORPUS_FILES = corpus.0.txt corpus.1.txt corpus.2.txt\
 corpus.3.txt corpus.4.txt corpus.5.txt
RAW_FILES = raw.1.txt


EXTRA_DIST = $(CORPUS_FILES) $(RAW_FILES) \
 corpus_info

noinst_PROGRAMS = calctrans proccorpus
INCLUDES = -I$(top_srcdir)/

calctrans_SOURCES = calctrans.c input_set.c input_set.h corpus.c
calctrans_LDADD = ../src-main/libanthy.la -lm
proccorpus_SOURCES = proccorpus.c
proccorpus_LDADD = ../src-util/libconvdb.la ../src-main/libanthy.la

init_params:
	rm -f parsed_data
	./proccorpus $(srcdir)/corpus.0.txt > parsed_data
	make do_update_params

update_params:
	./proccorpus $(srcdir)/corpus.?.txt -e missed_data >> parsed_data
	make do_update_params

update_params2:
	mv missed_data missed_data.src
	./proccorpus missed_data.src -e missed_data >> parsed_data
	make do_update_params

do_update_params:
	./calctrans parsed_data -o $(srcdir)/corpus_info
	./calctrans -c $(srcdir)/corpus_info

proc_raw:
	../src-util/anthy-morphological-analyzer --corpus-format $(srcdir)/raw.?.txt >> raw_morph_data
	./proccorpus raw_morph_data -e missed_data >> parsed_data
	make do_update_params

noinst_DATA =
CLEANFILES = parsed_data
