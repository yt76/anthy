/* features
 *
 * 素性の番号と意味を隠蔽して管理する
 *
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
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <anthy/segclass.h>
#include <anthy/feature_set.h>
/* for MW_FEATURE* constants */
#include <anthy/splitter.h>

/* 素性の番号 
 *
 * 0-19 クラス素性
 * 30- 付属語の種類
 * - その他
 * 1000-(SEG_SIZE^2) クラス遷移属性
 * 2000- (2048個) 付属語の種類
 * 10000- 文節の読みの素性
 */

#define CUR_CLASS_BASE 0
#define DEP_TYPE_FEATURE_BASE 30
#define FEATURE_SV 542
#define FEATURE_WEAK 543
#define FEATURE_SUFFIX 544
#define FEATURE_NUM 546
#define FEATURE_HIGH_FREQ 548
#define FEATURE_CORE1 550
#define FEATURE_CORE2 551
#define FEATURE_CORE3 552
#define FEATURE_SEG1 560
#define FEATURE_SEG2 561
#define FEATURE_SEG3 562
#define COS_FEATURE_BASE 570
#define CLASS_TRANS_BASE 1000
#define DEP_FEATURE_BASE 2000
#define WORD_HASH_BASE 10000

/* 素性のクラス */
#define FC_MISC 0x1
#define FC_SEG 0x2
#define FC_TRANS 0x4
#define FC_YOMI 0x8
#define FC_WORD 0x10
#define FC_SHORT_LEN 0x20
#define FC_SEG_LEN 0x40
#define FC_SEGCLASS 0x80
#define FC_CORE 0x100
#define FC_NOUN_COS 0x200
#define FC_ALL 0x3ff

/* 素性リストの種類(FL_*)から素性への対応を得る */
static const int fc_tab[] = {
  /* FL_ALL_FEATURES */
  FC_ALL,
  /* FL_CAND_FEATURES */
  FC_MISC | FC_SEG | FC_TRANS | FC_SEGCLASS | FC_CORE,
  /* FL_SEG_FEATURES */
  FC_MISC | FC_SEG | FC_TRANS | FC_YOMI | FC_SHORT_LEN |
  FC_SEG_LEN | FC_SEGCLASS | FC_CORE | FC_NOUN_COS,
  /* FL_SEG_TRANS_FEATURES */
  FC_MISC | FC_SEG | FC_TRANS | FC_SHORT_LEN | FC_NOUN_COS,
  /* FL_SEG_LEN_FEATURES */
  FC_SEG_LEN,
  /* FL_SEG_STRUCT_FEATURES */
  FC_MISC | FC_SEG | FC_SHORT_LEN | FC_SEGCLASS,
  /* FL_CORE_STRUCT */
  FC_CORE
};
/*
 * 文節境界の決定にはFL_SEG_FEATURESとそのsubsetである
 * FL_SEG_TRANS_FEATURES,
 * FL_SEG_STRUCT_FEATURES,
 * FL_SEG_LEN_FEATURESを用いる
 *
 * 候補の決定にはFL_CAND_FEATURESを用いる
 *
 * FC_COREは自立語の構造(未使用)
 */

static int
get_feature_class(int c)
{
  if (c >= DEP_FEATURE_BASE && c < DEP_FEATURE_BASE + WORD_HASH_MAX) {
    /* 付属語 */
    return FC_SEG;
  }
  if (c >= FEATURE_SEG1 && c <= FEATURE_SEG3) {
    return FC_SEG_LEN;
  }
  if (c >= FEATURE_CORE1 && c <= FEATURE_CORE2) {
    return FC_SHORT_LEN;
  }
  if (c >= CUR_CLASS_BASE && c < CUR_CLASS_BASE + SEG_SIZE) {
    return FC_SEGCLASS;
  }
  if (c >= CLASS_TRANS_BASE && c < CLASS_TRANS_BASE + SEG_SIZE * SEG_SIZE) {
    return FC_TRANS;
  }
  if (c >= COS_FEATURE_BASE && c < COS_FEATURE_BASE + COS_NR) {
    return FC_NOUN_COS;
  }
  switch (c) {
  case FEATURE_WEAK:
    return FC_CORE;
  default:
    break;
  }
  if (c >= WORD_HASH_BASE) {
    if (c & 1) {
      return FC_WORD;
    } else {
      return FC_YOMI;
    }
  }
  return FC_MISC;
}

void
anthy_feature_list_init(struct feature_list *fl,int type)
{
  fl->nr = 0;
  fl->size = NR_EM_FEATURES;
  fl->type = type;
}

void
anthy_feature_list_clone(struct feature_list *fl, struct feature_list *src,
			 int type)
{
  int i, nr;
  anthy_feature_list_init(fl, type);
  nr = anthy_feature_list_nr(src);
  for (i = 0; i < nr; i++) {
    int f = anthy_feature_list_nth(src, i);
    anthy_feature_list_add(fl, f);
  }
}

void
anthy_feature_list_free(struct feature_list *fl)
{
  (void)fl;
}

int
anthy_feature_list_compare(struct feature_list *f1,
			   struct feature_list *f2)
{
  int i;
  if (f1->nr != f2->nr) {
    return 1;
  }
  for (i = 0; i < f1->nr; i++) {
    if (f1->u.index[i] != f2->u.index[i]) {
      return 1;
    }
  }
  return 0;
}

void
anthy_feature_list_add(struct feature_list *fl, int f)
{
  /* 素性のクラスを取得 */
  int fc = get_feature_class(f);
  /* 素性リストに入れることのできる素性かチェックする */
  if (!(fc_tab[fl->type] & fc)) {
    return ;
  }
  /* リストに追加する */
  if (fl->nr < NR_EM_FEATURES) {
    fl->u.index[fl->nr] = f;
    fl->nr++;
  }
}

int
anthy_feature_list_nr(const struct feature_list *fl)
{
  return fl->nr;
}

int
anthy_feature_list_nth(const struct feature_list *fl, int nth)
{
  return fl->u.index[nth];
}

static int
cmp_int(const void *p1, const void *p2)
{
  return *((int *)p1) - *((int *)p2);
}

void
anthy_feature_list_sort(struct feature_list *fl)
{
  qsort(fl->u.index, fl->nr, sizeof(fl->u.index[0]),
	cmp_int);
}


void
anthy_feature_list_set_cur_class(struct feature_list *fl, int cl)
{
  anthy_feature_list_add(fl, CUR_CLASS_BASE + cl);
}

void
anthy_feature_list_set_class_trans(struct feature_list *fl, int pc, int cc)
{
  anthy_feature_list_add(fl, CLASS_TRANS_BASE + pc * SEG_SIZE + cc);
}

void
anthy_feature_list_set_dep_word(struct feature_list *fl, int h)
{
  anthy_feature_list_add(fl, h + DEP_FEATURE_BASE);
}

void
anthy_feature_list_set_dep_class(struct feature_list *fl, int c)
{
  anthy_feature_list_add(fl, c + DEP_TYPE_FEATURE_BASE);
}

void
anthy_feature_list_set_core_wtype(struct feature_list *fl, wtype_t wt)
{
  int pos = anthy_wtype_get_pos(wt);
  if (pos == POS_NOUN) {
    int c;
    c = anthy_wtype_get_cos(wt);
    if (c == COS_POSTFIX || c == COS_PREFIX || c == COS_SVSUFFIX) {
      anthy_feature_list_add(fl, COS_FEATURE_BASE + c);
    }
  }
}

void
anthy_feature_list_set_mw_features(struct feature_list *fl, int mask)
{
  if (mask & MW_FEATURE_WEAK_CONN) {
    anthy_feature_list_add(fl, FEATURE_WEAK);
  }
  if (mask & MW_FEATURE_SUFFIX) {
    anthy_feature_list_add(fl, FEATURE_SUFFIX);
  }
  if (mask & MW_FEATURE_SV) {
    anthy_feature_list_add(fl, FEATURE_SV);
  }
  if (mask & MW_FEATURE_NUM) {
    anthy_feature_list_add(fl, FEATURE_NUM);
  }
  if (mask & MW_FEATURE_CORE1) {
    anthy_feature_list_add(fl, FEATURE_CORE1);
  }
  if (mask & MW_FEATURE_CORE2) {
    anthy_feature_list_add(fl, FEATURE_CORE2);
  }
  if (mask & MW_FEATURE_SEG1) {
    anthy_feature_list_add(fl, FEATURE_SEG1);
  }
  if (mask & MW_FEATURE_SEG2) {
    anthy_feature_list_add(fl, FEATURE_SEG2);
  }
  if (mask & MW_FEATURE_SEG3) {
    anthy_feature_list_add(fl, FEATURE_SEG3);
  }
}

void
anthy_feature_list_set_yomi_hash(struct feature_list *fl, int h)
{
  /* use even number */
  int f = (h + WORD_HASH_BASE) & 0x3ffffffe;
  anthy_feature_list_add(fl, f);
}

void
anthy_feature_list_set_word_hash(struct feature_list *fl, int h)
{
  /* use odd number */
  int f = (h + WORD_HASH_BASE) & 0x3ffffffe;
  f += 1;
  anthy_feature_list_add(fl, f);
}

int
anthy_feature_list_has_yomi(struct feature_list *fl)
{
  int i;
  for (i = 0; i < fl->nr; i++) {
    if (fl->u.index[i] >= WORD_HASH_BASE) {
      return 1;
    }
  }
  return 0;
}

void
anthy_feature_list_print(struct feature_list *fl)
{
  int i;
  printf("features=");
  for (i = 0; i < fl->nr; i++) {
    if (i) {
      printf(",");
    }
    printf("%d", fl->u.index[i]);
  }
  printf("\n");
}

static int
compare_line(const void *kp, const void *cp)
{
  const int *f = kp;
  const struct feature_freq *c = cp;
  int i;
  for (i = 0; i < NR_EM_FEATURES; i++) {
    if (f[i] != (int)ntohl(c->f[i])) {
      return f[i] - ntohl(c->f[i]);
    }
  }
  return 0;
}

struct feature_freq *
anthy_find_array_freq(const void *image, int *f, int nr,
		      struct feature_freq *arg)
{
  struct feature_freq *res;
  int nr_lines, i;
  const int *array = (int *)image;
  int n[NR_EM_FEATURES];
  if (!image) {
    return NULL;
  }
  /* コピーする */
  for (i = 0; i < NR_EM_FEATURES; i++) {
    if (i < nr) {
      n[i] = f[i];
    } else {
      n[i] = 0;
    }
  }
  /**/
  nr_lines = ntohl(array[1]);
  res = bsearch(n, &array[16], nr_lines,
		sizeof(struct feature_freq),
		compare_line);
  if (!res) {
    return NULL;
  }
  for (i = 0; i < NR_EM_FEATURES + 2; i++) {
    arg->f[i] = ntohl(res->f[i]);
  }
  return arg;
}

struct feature_freq *
anthy_find_feature_freq(const void *image,
			const struct feature_list *fl,
			struct feature_freq *arg)
{
  int i, nr;
  int f[NR_EM_FEATURES + 2];

  /* 配列にコピーする */
  nr = anthy_feature_list_nr(fl);
  for (i = 0; i < NR_EM_FEATURES + 2; i++) {
    if (i < nr) {
      f[i] = anthy_feature_list_nth(fl, i);
    } else {
      f[i] = 0;
    }
  }
  return anthy_find_array_freq(image, f, NR_EM_FEATURES, arg);
}

void
anthy_init_features(void)
{
}
