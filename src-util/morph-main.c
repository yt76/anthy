/* コーパスから遷移行列を作るためのコード 
 *
 * Copyright (C) 2005-2006 TABATA Yusuke
 * Copyright (C) 2005-2006 YOSHIDA Yuichi
 *
 */
/*
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <anthy/anthy.h>
/* for print_context_info() */
#include <anthy/convdb.h>

struct test_context {
  anthy_context_t ac;
};

static void read_file(struct test_context *tc, const char *fn);
extern void anthy_reload_record(void);

int verbose;
int use_utf8;
int use_corpus_format;

/**/
static void
init_test_context(struct test_context *tc)
{
  tc->ac = anthy_create_context();
  anthy_set_reconversion_mode(tc->ac, ANTHY_RECONVERT_ALWAYS);
  if (use_utf8) {
    anthy_context_set_encoding(tc->ac, ANTHY_UTF8_ENCODING);
  }
  anthy_reload_record();
}

static void
select_candidates(anthy_context_t ac, const char *str)
{
  int i, j;
  struct anthy_conv_stat acs;
  anthy_get_stat(ac, &acs);

  for (i = 0; i < acs.nr_segment; i++) {
    struct anthy_segment_stat ass;
    const char *next_str = NULL;
    char buf[1024];
    anthy_get_segment_stat(ac, i, &ass);
    for (j = 0; j < ass.nr_candidate; j++) {
      int len;
      anthy_get_segment(ac, i, j, buf, 1024);
      len = strlen(buf);
      if (!strncmp(str, buf, len)) {
	anthy_commit_segment(ac, i, j);
	next_str = &str[len];
	break;
      }
    }
    if (!next_str) {
      break;
    }
    str = next_str;
  }
}

static void
print_corpus_format(anthy_context_t ac, const char *str)
{
  int i;
  struct anthy_conv_stat acs;
  char buf[1024];
  anthy_get_stat(ac, &acs);

  if (strchr(str, ' ') || strchr(str, '|') ||
      acs.nr_segment == 0) {
    printf("#");
  }

  printf("|");
  for (i = 0; i < acs.nr_segment; i++) {
    anthy_get_segment(ac, i, NTH_UNCONVERTED_CANDIDATE, buf, 1024);
    printf("%s", buf);
    printf("|");
  }
  printf(" |");
  for (i = 0; i < acs.nr_segment; i++) {
    anthy_get_segment(ac, i, NTH_COMMITTED_CANDIDATE, buf, 1024);
    printf("%s", buf);
    printf("|");
  }
  printf("\n");
}

static void
conv_sentence(struct test_context *tc, const char *str)
{
  anthy_set_string(tc->ac, str);
  if (verbose) {
    anthy_print_context(tc->ac);
  }
  /**/
  select_candidates(tc->ac, str);
  if (use_corpus_format) {
    print_corpus_format(tc->ac, str);
  } else {
    print_context_info(tc->ac, NULL);
  }
}

/* 行末の改行を削除 */
static void
chomp(char *buf)
{
  int len = strlen(buf);
  while (len > 0) {
    char c = buf[len - 1];
    if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
      buf[len - 1] = 0;
      len --;
    } else {
      return ;
    }
  }
}

static void
read_fp(struct test_context *tc, FILE *fp)
{
  char buf[1024];
  while (fgets(buf, 1024, fp)) {
    if (buf[0] == '#') {
      continue;
    }

    if (!strncmp(buf, "\\include ", 9)) {
      read_file(tc, &buf[9]);
      continue;
    }
    chomp(buf);
    conv_sentence(tc, buf);
  }
}

static void
read_file(struct test_context *tc, const char *fn)
{
  FILE *fp;
  fp = fopen(fn, "r");
  if (!fp) {
    printf("failed to open (%s)\n", fn);
    return ;
  }
  read_fp(tc, fp);
  fclose(fp);
}

static void
print_usage(void)
{
  printf("morphological analyzer\n");
  printf(" $ ./morphological analyzer < [text-file]\n or");
  printf(" $ ./morphological analyzer [text-file]\n");
  exit(0);
}

static void
parse_args(int argc, char **argv)
{
  int i;
  for (i = 1; i < argc; i++) {
    char *arg = argv[i];
    if (!strcmp(arg, "--utf8")) {
      use_utf8 = 1;
    } else if (!strcmp(arg, "--corpus-format")) {
      use_corpus_format = 1;
    } else if (arg[i] == '-') {
      print_usage();
    }
  }
}

int
main(int argc, char **argv)
{
  struct test_context tc;
  int i, nr;
  anthy_init();
  anthy_set_personality("");
  init_test_context(&tc);

  parse_args(argc, argv);

  nr = 0;
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      continue;
    }
    read_file(&tc, argv[i]);
    nr ++;
  }
  if (nr == 0) {
    read_fp(&tc, stdin);
  }

  return 0;
}
