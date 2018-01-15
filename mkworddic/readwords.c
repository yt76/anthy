/*
 * 辞書ファイルを読み込むためのコード
 *
 * Copyright (C) 2000-2006 TABATA Yusuke
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
#include <ctype.h>
#include <anthy/anthy.h>
#include <anthy/word_dic.h>
#include "mkdic.h"

#define MAX_LINE_LEN 10240
#define MAX_WTYPE_LEN 20

static struct adjust_command *
parse_modify_freq_command(const char *buf)
{
  char *line = alloca(strlen(buf) + 1);
  char *yomi, *wt, *word, *type_str;
  struct adjust_command *cmd;
  int type = 0;
  strcpy(line, buf);
  yomi = strtok(line, " ");
  wt = strtok(NULL, " ");
  word = strtok(NULL, " ");
  type_str = strtok(NULL, " ");
  if (!yomi || !wt || !word || !type_str) {
    return NULL;
  }
  if (!strcmp(type_str, "up")) {
    type = ADJUST_FREQ_UP;
  }
  if (!strcmp(type_str, "down")) {
    type = ADJUST_FREQ_DOWN;
  }
  if (!strcmp(type_str, "kill")) {
    type = ADJUST_FREQ_KILL;
  }
  if (!type) {
    return NULL;
  }
  cmd = malloc(sizeof(struct adjust_command));
  cmd->type = type;
  cmd->yomi = anthy_cstr_to_xstr(yomi, ANTHY_EUC_JP_ENCODING);
  cmd->wt = get_wt_name(wt);
  cmd->word = anthy_conv_euc_to_utf8(word);
  return cmd;
}

static void
parse_adjust_command(const char *buf, struct adjust_command *ac_list)
{
  struct adjust_command *cmd = NULL;
  if (!strncmp("\\modify_freq ", buf, 13)) {
    cmd = parse_modify_freq_command(&buf[13]);
  }
  if (cmd) {
    cmd->next = ac_list->next;
    ac_list->next = cmd;
  }
}


/** cannadic形式の辞書の行からindexとなる部分を取り出す */
static xstr *
get_index_from_line(struct mkdic_stat *mds, char *buf)
{
  char *sp;
  xstr *xs;
  sp = strchr(buf, ' ');
  if (!sp) {
    /* 辞書のフォーマットがおかしい */
    return NULL;
  }
  *sp = 0;
  xs = anthy_cstr_to_xstr(buf, mds->input_encoding);
  *sp = ' ';
  return xs;
}

/** cannadic形式の辞書の行からindex以外の部分を取り出す */
static char *
get_entry_from_line(char *buf)
{
  char *sp;
  sp = strchr(buf, ' ');
  while(*sp == ' ') {
    sp ++;
  }
  return strdup(sp);
}

static char *
read_line(FILE *fp, char *buf)
{
  /* 長すぎる行を無視する */
  int toolong = 0;

  if (feof(fp)) {
    return NULL;
  }

  while (fgets(buf, MAX_LINE_LEN, fp)) {
    int len = strlen(buf);
    if (buf[0] == '#') {
      continue ;
    }
    if (buf[len - 1] != '\n') {
      toolong = 1;
      continue ;
    }

    buf[len - 1] = 0;
    if (toolong) {
      toolong = 0;
    } else {
      return buf;
    }
  }
  return NULL;
}

static int
parse_wtype(char *wtbuf, char *cur)
{
  /* 品詞 */
  char *t;
  int freq;
  if (strlen(cur) >= MAX_WTYPE_LEN) {
    return 0;
  }
  strcpy(wtbuf, cur);
  /* 頻度 */
  t = strchr(wtbuf, '*');
  freq = 1;
  if (t) {
    int tmp_freq;
    *t = 0;
    t++;
    tmp_freq = atoi(t);
    if (tmp_freq) {
      freq = tmp_freq;
    }
  }
  return freq;
}


static int
is_excluded_wtype(struct mkdic_stat *mds, char *wt)
{
  int i;
  for (i = 0; i < mds->nr_excluded; i++) {
    if (!strcmp(mds->excluded_wtypes[i], wt)) {
      return 1;
    }
  }
  return 0;
}

static char *
find_token_end(char *cur)
{
  char *n;
  for (n = cur; *n != ' ' && *n != '\0'; n++) {
    if (*n == '\\') {
      if (!n[1]) {
	return NULL;
      }
      n++;
    }
  }
  return n;
}

/** 複合候補の形式チェック */
static int
check_compound_candidate(struct mkdic_stat *mds, xstr *index, const char *cur)
{
  /* 読みの文字数の合計を数える */
  xstr *xs = anthy_cstr_to_xstr(cur, mds->input_encoding);
  int i, total = 0;
  for (i = 0; i < xs->len - 1; i++) {
    if (xs->str[i] == '_') {
      total += get_compound_element_len(xs->str[i+1]);
    }
  }
  anthy_free_xstr(xs);
  /* 比較する */
  if (total != index->len) {
    fprintf(stderr, "Invalid compound candidate (%s, length = %d).\n",
	    cur, total);
    return 0;
  }
  return 1;
}

/** 読みに対応する行を分割して、配列を構成する */
static void
push_back_word_entry_line(struct mkdic_stat *mds, struct yomi_entry *ye,
			  const char *ent)
{
  char *buf = alloca(strlen(ent) + 1);
  char *cur = buf;
  char *n;
  char wtbuf[MAX_WTYPE_LEN];
  int freq;
  int order = 0;

  if (mds->freq_order) {
    freq = 0;
  } else {
    freq = MAX_RAW_FREQ;
  }

  strcpy(buf, ent);
  wtbuf[0] = 0;

  while (1) {
    /* トークンを\0で切る。curの後の空白か\0を探す */
    n = find_token_end(cur);
    if (!n) {
      fprintf(stderr, "invalid \\ at the end of line (%s).\n",
	      ent);
      return ;
    }
    if (*n) {
      *n = 0;
    } else {
      n = NULL;
    }
    /**/
    if (cur[0] == '#') {
      if (isalpha((unsigned char)cur[1])) {
	/* #XX*?? をパース */
	freq = parse_wtype(wtbuf, cur);
	/**/
	if (!mds->freq_order) {
	  freq = MAX_RAW_FREQ - freq;
	}
      } else {
	if (cur[1] == '_' &&
	    check_compound_candidate(mds, ye->index_xstr, &cur[1])) {
	  /* #_ 複合候補 */
	  push_back_word_entry(mds, ye, wtbuf, freq, cur, order);
	  order ++;
	}
      }
    } else {
      /* 品詞が除去リストに入っているかをチェック */
      if (!is_excluded_wtype(mds, wtbuf)) {
	/* 単語を追加 */
	push_back_word_entry(mds, ye, wtbuf, freq, cur, order);
	order ++;
      }/* :to extract excluded words
	  else {
	  anthy_putxstr(ye->index_xstr);
	  printf(" %s*%d %s\n", wtbuf, freq, cur);
	  }*/
    }
    if (!n) {
      /* 行末 */
      return ;
    }
    cur = n;
    cur ++;
  }
}

/** 辞書を一行ずつ読み込んでリストを作る
 * このコマンドのコア */
static void
parse_traditional_dict_file(FILE *fin, struct mkdic_stat *mds)
{
  xstr *index_xs;
  char buf[MAX_LINE_LEN];
  char *ent;
  struct yomi_entry *ye = NULL;

  /* １行ずつ処理 */
  while (read_line(fin, buf)) {
    if (buf[0] == '\\' && buf[1] != ' ') {
      parse_adjust_command(buf, &mds->ac_list);
      continue ;
    }
    index_xs = get_index_from_line(mds, buf);
    if (!index_xs) {
      break;
    }
    ent = get_entry_from_line(buf);

    /* 読みが30文字を越える場合は無視 */
    if (index_xs->len < 31) {
      ye = find_yomi_entry(&mds->yl, index_xs, 1);
      push_back_word_entry_line(mds, ye, ent);
    }

    free(ent);
    anthy_free_xstr(index_xs);
  }
}

static void
parse_word_def(struct mkdic_stat *mds,
	       struct yomi_entry *ye,
	       const char *word)
{
  push_back_word_entry(mds, ye, "#T", 1, word, 0);
}

static void
chomp(char *buf)
{
  int len = strlen(buf);
  if (len > 0 && buf[len - 1] == '\n') {
    buf[len - 1] = 0;
  }
}

static void
parse_dict_file(FILE *fin, struct mkdic_stat *mds)
{
  char buf[MAX_LINE_LEN];
  char *idx = NULL;
  struct yomi_entry *ye = NULL;
  while (fgets(buf, MAX_LINE_LEN, fin)) {
    if (buf[0] == '#') {
      continue;
    }
    chomp(buf);
    /**/
    if (strlen(buf) == 0) {
      /* 空行 */
      continue;
    }
    /**/
    if (buf[0] == '[') {
      /* 見出し語 */
      char *p = strchr(buf, ']');
      if (p) {
	*p = 0;
      }
      free(idx);
      idx = strdup(buf);
      ye = NULL;
      continue;
    }
    /* 単語定義 */
    if (!ye) {
      xstr *xs = anthy_cstr_to_xstr(idx, mds->input_encoding);
      ye = find_yomi_entry(&mds->yl, xs, 1);
      anthy_free_xstr(xs);
    }
    parse_word_def(mds, ye, buf);
  }
  free(idx);
}

void
read_traditional_dict_file(struct mkdic_stat *mds, const char *fn)
{
  FILE *fp = fopen(fn, "r");
  if (fp) {
    printf("file = %s (traditional)\n", fn);
    parse_traditional_dict_file(fp, mds);
    fclose(fp);
  } else {
    printf("failed file = %s\n", fn);
  }
}

void
read_dict_file(struct mkdic_stat *mds, const char *fn)
{
  FILE *fp = fopen(fn, "r");
  if (fp) {
    printf("file = %s\n", fn);
    parse_dict_file(fp, mds);
    fclose(fp);
  } else {
    printf("failed file = %s\n", fn);
  }
}
