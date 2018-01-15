#ifndef _mkdic_h_included_
#define _mkdic_h_included_

#include <stdio.h>
#include <anthy/xstr.h>

/** 単語 */
struct word_entry {
  /** 品詞名 */
  const char *wt_name;
  /** 頻度 */
  int raw_freq;
  int source_order;
  /* コスト(頻度の逆) */
  int cost;
  /* 素性 */
  int feature;
  /** 単語 */
  char *word_utf8;
  /** 辞書ファイル中のオフセット */
  int offset;
  /** 属すyomi_entry*/
  struct yomi_entry* ye;
};

/** ある読み */
struct yomi_entry {
  /* 読みの文字列 */
  xstr *index_xstr;
  /* 読みの文字列(辞書ファイル内のインデックス) */
  char *index_str;
  /* 辞書ファイル中のページ中のオフセット */
  int offset;
  /* 各エントリ */
  int nr_entries;
  struct word_entry *entries;
  /**/
  struct yomi_entry *next;
  struct yomi_entry *hash_next;
};

#define YOMI_HASH (16384 * 16)

/* 辞書全体 */
struct yomi_entry_list {
  /* 見出しのリスト */
  struct yomi_entry *head;
  /* 辞書ファイル中の見出しの数 */
  int nr_entries;
  /* 見出しの中で単語を持つものの数 */
  int nr_valid_entries;
  /* 単語の数 */
  int nr_words;
  /**/
  struct yomi_entry *hash[YOMI_HASH];
  struct yomi_entry **ye_array;
};

#define ADJUST_FREQ_UP 1
#define ADJUST_FREQ_DOWN 2
#define ADJUST_FREQ_KILL 3

/* 頻度補正用コマンド */
struct adjust_command {
  int type;
  xstr *yomi;
  const char *wt;
  char *word;
  struct adjust_command *next;
};


/* 辞書生成の状態 */
struct mkdic_stat {
  /* 単語のリスト */
  struct yomi_entry_list yl;
  /**/
  struct adjust_command ac_list;
  /* 用例辞書 */
  struct uc_dict *ud;
  /**/
  const char *output_fn;
  /**/
  int input_encoding;
  /**/
  int nr_excluded;
  char **excluded_wtypes;
  /**/
  int freq_order;
};

#define INVALID_FREQ 99999
#define MAX_RAW_FREQ 1000

/**/
struct yomi_entry *find_yomi_entry(struct yomi_entry_list *yl,
				   xstr *index, int create);

/* 辞書書き出し用の補助 */
void write_nl(FILE *fp, int i);

/**/
const char *get_wt_name(const char *name);
void push_back_word_entry(struct mkdic_stat *mds,
			  struct yomi_entry *ye, const char *wt_name,
			  int freq, const char *word, int order);
int get_compound_element_len(xchar xc);

/* mkudic.c
 * 用例辞書を作る */
struct uc_dict *create_uc_dict(void);
void read_uc_file(struct uc_dict *ud, const char *fn);
void make_ucdict(FILE *out, struct uc_dict *uc);
/**/

/* writewords.c */
void output_word_dict(struct yomi_entry_list *yl);

/* readwords.c */
void read_traditional_dict_file(struct mkdic_stat *mds, const char *fn);
void read_dict_file(struct mkdic_stat *mds, const char *fn);

/* calcfreq.c */
void calc_freq(struct yomi_entry_list *yl);

#endif
