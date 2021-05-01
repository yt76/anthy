/*
 * 文節の遷移行列を作成する
 *
 * morphological-analyzerの出力には下記のマークが付けてある
 * ~ 候補の誤り
 * ! 文節長の誤り
 * ^ 複合文節の2つめ以降の要素
 *
 * generate transition matrix
 *
 * Copyright (C) 2006 HANAOKA Toshiyuki
 * Copyright (C) 2006-2007 TABATA Yusuke
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
#include <math.h>

#include <anthy/anthy.h>
#include <anthy/xstr.h>
#include <anthy/feature_set.h>
#include <anthy/diclib.h>
#include "input_set.h"
#include <anthy/corpus.h>

#define FEATURE_SET_SIZE NR_EM_FEATURES

#define ARRAY_SIZE 16

struct array {
  int len;
  int f[ARRAY_SIZE];
};

#define MAX_SEGMENT 64

struct segment_info {
  int orig_hash;
  int hash;
};

struct sentence_info {
  int nr_segments;
  struct segment_info segs[MAX_SEGMENT];
};

/* 確率のテーブル */
struct input_info {
  /* 候補全体の素性 */
  struct input_set *raw_cand_is;
  struct input_set *cand_is;
  /* 文節の素性 */
  struct input_set *raw_seg_is;
  /**/
  struct input_set *trans_is;
  struct input_set *seg_struct_is;
  struct input_set *yomi_is;
  struct input_set *seg_len_is;
  /* 自立語の全文検索用情報 */
  struct corpus *indep_corpus;

  /**/
  struct array missed_cand_features;

  /* 入力された例文の量に関する情報 */
  int nr_sentences;
  int nr_connections;
};

static struct input_info *
init_input_info(void)
{
  struct input_info *m;
  m = malloc(sizeof(struct input_info));
  /**/
  m->raw_cand_is = input_set_create();
  m->cand_is = input_set_create();
  /**/
  m->raw_seg_is = input_set_create();
  m->trans_is = input_set_create();
  m->seg_struct_is = input_set_create();
  m->yomi_is = input_set_create();
  m->seg_len_is = input_set_create();
  /**/
  m->indep_corpus = corpus_new();
  /**/
  m->missed_cand_features.len = 0;
  m->nr_sentences = 0;
  m->nr_connections = 0;
  return m;
}

/* features=1,2,3,,の形式をparseする */
static void
parse_features(struct array *features, char *s)
{
  char *tok, *str = s;
  tok = strtok(str, ",");
  features->len = 0;
  do {
    features->f[features->len] = atoi(tok);
    features->len++;
    tok = strtok(NULL, ",");
  } while(tok);
}

static void
add_seg_struct_info(struct input_info *m,
		    struct array *features,
		    int weight)
{
  input_set_set_features(m->raw_cand_is, features->f, features->len, weight);
}

static void
set_hash(struct sentence_info *sinfo, int error_class,
	 char tag, int hash)
{
  if (tag == '~') {
    sinfo->segs[sinfo->nr_segments].orig_hash = hash;
  } else {
    sinfo->segs[sinfo->nr_segments].hash = hash;
  }
  if (!error_class) {
    sinfo->nr_segments++;
  }
}

static int
compare_cand_feature_array(struct array *a1, struct array *a2)
{
  struct feature_list f1, f2;
  int i, r;
  anthy_feature_list_init(&f1, FL_CAND_FEATURES);
  anthy_feature_list_init(&f2, FL_CAND_FEATURES);
  for (i = 0; i < a1->len; i++) {
    anthy_feature_list_add(&f1, a1->f[i]);
  }
  for (i = 0; i < a2->len; i++) {
    anthy_feature_list_add(&f2, a2->f[i]);
  }
  r = anthy_feature_list_compare(&f1, &f2);
  anthy_feature_list_free(&f1);
  anthy_feature_list_free(&f2);
  return r;
}

/* 自立語の行をparseする */
static void
parse_indep(struct input_info *m, struct sentence_info *sinfo,
	    char *line, char *buf, int error_class)
{
  struct array features;
  char *s;
  int weight = 1;
  /**/
  s = strstr(buf, "features=");
  if (s) {
    s += 9;
    parse_features(&features, s);
    m->nr_connections ++;
  }
  s = strstr(buf, "hash=");
  if (s) {
    s += 5;
    set_hash(sinfo, error_class, line[0], atoi(s));
  }

  /* 加算する */
  if (error_class) {
    if (line[0] == '~') {
      /* 誤った候補の構造を保存 */
      m->missed_cand_features = features;
    }
    if (line[0] == '!') {
      /* 文節長の誤り */
      input_set_set_features(m->raw_seg_is, features.f, features.len, -weight);
    }
  } else {
    /* 接続行列 */
    input_set_set_features(m->raw_seg_is, features.f, features.len, weight);
    /* 候補の構造 */
    if (m->missed_cand_features.len != 0 &&
	compare_cand_feature_array(&features, &m->missed_cand_features)) {
      /* 正解と異なる構造なら分母に加算 */
      add_seg_struct_info(m, &m->missed_cand_features, -weight);
    }
    m->missed_cand_features.len = 0;
    add_seg_struct_info(m, &features, weight);
  }
}

static void
init_sentence_info(struct sentence_info *sinfo)
{
  int i;
  sinfo->nr_segments = 0;
  for (i = 0; i < MAX_SEGMENT; i++) {
    sinfo->segs[i].orig_hash = 0;
    sinfo->segs[i].hash = 0;
  }
}

/* 一つの文を読んだときに全文検索用のデータを作る
 */
static void
complete_sentence_info(struct input_info *m, struct sentence_info *sinfo)
{
  int i;
  for (i = 0; i < sinfo->nr_segments; i++) {
    int flags = ELM_NONE;
    int nr = 1;
    int buf[2];
    if (i == 0) {
      flags |= ELM_BOS;
    }
    /**/
    buf[0] = sinfo->segs[i].hash;
    if (sinfo->segs[i].orig_hash) {
      /*
      buf[1] = sinfo->segs[i].orig_hash;
      nr ++;
      */
    }
    corpus_push_back(m->indep_corpus, buf, nr, flags);
  }
}

static void
do_read_file(struct input_info *m, FILE *fp)
{
  char line[1024];
  struct sentence_info sinfo;

  init_sentence_info(&sinfo);

  while (fgets(line, 1024, fp)) {
    char *buf = line;
    int error_class = 0;
    if (!strncmp(buf, "eos", 3)) {
      m->nr_sentences ++;
      complete_sentence_info(m, &sinfo);
      init_sentence_info(&sinfo);
    }
    if (line[0] == '~' || line[0] == '!' ||
	line[0] == '^') {
      buf ++;
      error_class = 1;
    }
    if (!strncmp(buf, "indep_word", 10) ||
	!strncmp(buf, "eos", 3)) {
      parse_indep(m, &sinfo, line, buf, error_class);
    }
  }
}

static void
read_file(struct input_info *m, char *fn)
{
  FILE *ifp;
  ifp = fopen(fn, "r");
  if (!ifp) {
    return ;
  }
  do_read_file(m, ifp);
  fclose(ifp);
}

static void
dump_line(FILE *ofp, struct input_line *il)
{
  int i;
  for (i = 0; i < FEATURE_SET_SIZE || i < il->nr_features; i++) {
    if (i) {
      fprintf(ofp, ", ");
    }
    if (i < il->nr_features) {
      fprintf(ofp, "%d", il->features[i]);
    } else {
      fprintf(ofp, "0");
    }
  }
  fprintf(ofp,",%d,%d\n", (int)il->negative_weight, (int)il->weight);
}

static int
compare_line(const void *p1, const void *p2)
{
  const struct input_line *const *il1 = p1;
  const struct input_line *const *il2 = p2;
  int i;
  for (i = 0; i < (*il1)->nr_features &&
	 i < (*il2)->nr_features; i++) {
    if ((*il1)->features[i] !=
	(*il2)->features[i]) {
      return (*il1)->features[i] - (*il2)->features[i];
    }
  }
  return (*il1)->nr_features - (*il2)->nr_features;
}

static void
dump_features(FILE *ofp, struct input_set *is)
{
  struct input_line *il, **lines;
  int i, nr = 0;
  int weight = 0;

  /* count lines */
  for (il = input_set_get_input_line(is); il; il = il->next_line) {
    nr ++;
    weight += (int)il->weight;
  }
  /* copy lines */
  lines = malloc(sizeof(struct input_line *) * nr);
  for (il = input_set_get_input_line(is), i = 0; i < nr;
       i++, il = il->next_line) {
    lines[i] = il;
  }
  /* sort */
  qsort(lines, nr, sizeof(struct input_line *), compare_line);
  /* output */
  fprintf(ofp, "%d %d total_line_weight,count\n", weight, nr);
  /**/
  for (i = 0; i < nr; i++) {
    dump_line(ofp, lines[i]);
  }
}

static void
dump_input_info(FILE *ofp, struct input_info *m)
{
  fprintf(ofp, "section anthy.trans_info ");
  dump_features(ofp, m->trans_is);
  fprintf(ofp, "section anthy.cand_info ");
  dump_features(ofp, m->cand_is);
  fprintf(ofp, "section anthy.seg_info ");
  dump_features(ofp, m->seg_struct_is);
  fprintf(ofp, "section anthy.yomi_info ");
  dump_features(ofp, m->yomi_is);
  fprintf(ofp, "section anthy.seg_len_info ");
  dump_features(ofp, m->seg_len_is);
  fprintf(ofp, "section anthy.corpus_bucket ");
  corpus_write_bucket(ofp, m->indep_corpus);
  fprintf(ofp, "section anthy.corpus_array ");
  corpus_write_array(ofp, m->indep_corpus);
}

static int
word_fl_filter(struct feature_list *fl, int pw, int nw)
{
  (void)pw;
  if (anthy_feature_list_has_yomi(fl) && nw > 1 && nw > pw * 2) {
    /* 誤変換であった割合が十分に大きいとき */
    return 0;
  }
  return 1;
}

/**/
static void
extract_is(struct input_set *src_is, struct input_set *dst_is, int type,
	   int (*filter)(struct feature_list *fl, int pw, int nw))
{
  struct input_line *il = input_set_get_input_line(src_is);
  for (; il; il = il->next_line) {
    struct feature_list fl;
    int i, nr;
    int ar[FEATURE_SET_SIZE];
    anthy_feature_list_init(&fl, type);
    for (i = 0; i < il->nr_features; i++) {
      anthy_feature_list_add(&fl, il->features[i]);
    }
    nr = anthy_feature_list_nr(&fl);
    for (i = 0; i < nr; i++) {
      ar[i] = anthy_feature_list_nth(&fl, i);
    }
    if (filter && filter(&fl, il->weight, il->negative_weight)) {
      anthy_feature_list_free(&fl);
      continue;
    }
    anthy_feature_list_free(&fl);
    input_set_set_features(dst_is, ar, nr,
			   il->weight);
    input_set_set_features(dst_is, ar, nr,
			   -il->negative_weight);
  }
}

/* 変換結果から確率のテーブルを作る */
static void
proc_corpus(int nr_fn, char **fns, FILE *ofp)
{
  int i;
  struct input_info *iinfo;
  /**/
  iinfo = init_input_info();
  /**/
  for (i = 0; i < nr_fn; i++) {
    read_file(iinfo, fns[i]);
  }

  /* 全文検索のデータベースを作る */
  corpus_build(iinfo->indep_corpus);
  /* 確率を出力する */
  extract_is(iinfo->raw_cand_is, iinfo->cand_is, FL_CAND_FEATURES, NULL);
  extract_is(iinfo->raw_seg_is, iinfo->seg_struct_is, FL_SEG_STRUCT_FEATURES, NULL);
  extract_is(iinfo->raw_seg_is, iinfo->trans_is, FL_SEG_TRANS_FEATURES, NULL);
  extract_is(iinfo->raw_seg_is, iinfo->yomi_is, FL_SEG_FEATURES,
	     word_fl_filter);
  extract_is(iinfo->raw_seg_is, iinfo->seg_len_is, FL_SEG_LEN_FEATURES, NULL);
  dump_input_info(ofp, iinfo);

  /* 統計情報 */
  fprintf(stderr, " %d sentences\n", iinfo->nr_sentences);
  fprintf(stderr, " %d connections\n", iinfo->nr_connections);
  fprintf(stderr, " %d segments\n", iinfo->nr_connections - iinfo->nr_sentences);
}

int
main(int argc, char **argv)
{
  FILE *ofp;
  int i;
  int nr_input = 0;
  char **input_files;

  ofp = NULL;
  input_files = malloc(sizeof(char *) * argc);
  anthy_init_features();
  
  for (i = 1; i < argc; i++) {
    char *arg = argv[i];
    if (!strcmp(arg, "-o")) {
      ofp = fopen(argv[i+1], "w");
      if (!ofp) {
	fprintf(stderr, "failed to open (%s)\n", argv[i+1]);
      }
      i ++;
    } else {
      input_files[nr_input] = arg;
      nr_input ++;
    }
  }
  if (ofp) {
    /* コーパスからテキスト形式の辞書を作る */
    printf(" -- generating dictionary in text form\n");
    proc_corpus(nr_input, input_files, ofp);
    fclose(ofp);
  }

  return 0;
}
