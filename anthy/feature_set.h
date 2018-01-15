/* 素性を扱う */
#ifndef _feature_set_h_included_
#define _feature_set_h_included_

#include <anthy/wtype.h>
#include <anthy/xstr.h>

/**/
#define FL_ALL_FEATURES 0
#define FL_CAND_FEATURES 1
#define FL_SEG_FEATURES 2
#define FL_SEG_TRANS_FEATURES 3
#define FL_SEG_LEN_FEATURES 4
#define FL_SEG_STRUCT_FEATURES 5
#define FL_CORE_STRUCT 6

/* hash collisionが出たら適宜増やす */
#define WORD_HASH_MAX 2048
/**/
#define HASH_DEP_LEN 3

/* 14+正例+負例 = 16になるように */
#define NR_EM_FEATURES 14

struct feature_list {
  /* いまのところ、素性は14個まで */
  int nr;
  int type;
  int size;
  union {
    int index[NR_EM_FEATURES];
    int *array;
  } u;
};

struct feature_freq {
  int f[NR_EM_FEATURES + 2];
};

void anthy_init_features(void);
struct feature_freq *
anthy_find_feature_freq(const void *array,
			const struct feature_list *fl,
			struct feature_freq *arg);
struct feature_freq *
anthy_find_array_freq(const void *array,
		      int *f, int nr,
		      struct feature_freq *arg);


/**/
void anthy_feature_list_init(struct feature_list *fl, int type);
void anthy_feature_list_clone(struct feature_list *fl, struct feature_list *src, int type);
void anthy_feature_list_free(struct feature_list *fl);
int anthy_feature_list_compare(struct feature_list *f1,
			       struct feature_list *f2);
/**/
void anthy_feature_list_add(struct feature_list *fl, int f);
int anthy_feature_list_nr(const struct feature_list *fl);
int anthy_feature_list_nth(const struct feature_list *fl, int nth);
void anthy_feature_list_sort(struct feature_list *fl);
/**/
void anthy_feature_list_set_cur_class(struct feature_list *fl, int cc);
void anthy_feature_list_set_class_trans(struct feature_list *fl, int pc, int cc);
void anthy_feature_list_set_dep_word(struct feature_list *fl, int h);
void anthy_feature_list_set_dep_class(struct feature_list *fl, int c);
void anthy_feature_list_set_core_wtype(struct feature_list *fl, wtype_t wt);
void anthy_feature_list_set_mw_features(struct feature_list *fl, int mask);
void anthy_feature_list_set_yomi_hash(struct feature_list *fl, int yomi);
void anthy_feature_list_set_word_hash(struct feature_list *fl, int word);
/**/
int anthy_feature_list_has_yomi(struct feature_list *fl);
/**/
void anthy_feature_list_print(struct feature_list *fl);

#endif
