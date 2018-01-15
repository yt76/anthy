/*
 * 確率を評価しビタビアルゴリズム(viterbi algorithm)によって
 * 文節の区切りを決定してマークする。
 *
 *
 * 外部から呼び出される関数
 *  anthy_mark_borders()
 *
 * Copyright (C) 2006-2007 TABATA Yusuke
 * Copyright (C) 2004-2006 YOSHIDA Yuichi
 * Copyright (C) 2006 HANAOKA Toshiyuki
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
/*
 * コンテキスト中に存在するmeta_wordをつないでグラフを構成します。
 * (このグラフのことをラティス(lattice/束)もしくはトレリス(trellis)と呼びます)
 * meta_wordどうしの接続がグラフのノードとなり、構造体lattice_nodeの
 * リンクとして構成されます。
 *
 * ここでの処理は次の二つの要素で構成されます
 * (1) グラフを構成しつつ、各ノードへの到達確率を求める
 * (2) グラフを後ろ(右)からたどって最適なパスを求める
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <anthy/alloc.h>
#include <anthy/xstr.h>
#include <anthy/segclass.h>
#include <anthy/splitter.h>
#include <anthy/feature_set.h>
#include <anthy/diclib.h>
#include "wordborder.h"

static void *trans_info_array;
static void *seg_info_array;
static void *yomi_info_array;
static void *seg_len_info_array;

#define NODE_MAX_SIZE 50

/* グラフのノード(遷移状態) */
struct lattice_node {
  int node_id;
  int border; /* 文字列中のどこから始まるノードか */
  enum seg_class seg_class; /* この状態の品詞 */


  double node_probability; /* このノードが正解の確率 */
  double path_probability;  /* ここに至るまでの確率 */

  struct lattice_node* before_node; /* 一つ前のノード */
  struct meta_word* mw; /* このノードに対応するmeta_word */

  struct lattice_node* next; /* リスト構造のためのポインタ */
};

struct node_list_head {
  struct lattice_node *head;
  int nr_nodes;
};

struct lattice_info {
  /* 遷移状態のリストの配列 */
  int node_id;
  struct node_list_head *lattice_node_list;
  struct splitter_context *sc;
  /* ノードのアロケータ */
  allocator node_allocator;
  int last_node_id;
};

static double get_transition_probability(struct lattice_node *node);
/*
 */
static void
print_lattice_node(struct lattice_info *info, struct lattice_node *node)
{
  struct lattice_node *before_node;
  if (!node) {
    printf("**lattice_node (null)*\n");
    return ;
  }
  before_node = node->before_node;
  printf("**lattice_node id:%d(<-%d) path_p=%f,node_p=%f(<-%f)\n",
	 node->node_id, (before_node ? before_node->node_id : 0),
	 node->path_probability,
	 node->node_probability,
	 (before_node ? before_node->path_probability : 1.0f));
  if (node->mw) {
    anthy_print_metaword(info->sc, node->mw);
  }
  printf("\n");
}

static void
build_feature_list(struct lattice_node *node,
		   struct lattice_node *before_node,
		   struct feature_list *features)
{
  int pc, cc;
  if (node) {
    cc = node->seg_class;
  } else {
    cc = SEG_TAIL;
  }
  anthy_feature_list_set_cur_class(features, cc);
  if (before_node) {
    pc = before_node->seg_class;
  } else {
    pc = SEG_HEAD;
  }
  anthy_feature_list_set_class_trans(features, pc, cc);
  
  if (node && node->mw) {
    struct meta_word *mw = node->mw;
    anthy_feature_list_set_dep_class(features, mw->dep_class);
    anthy_feature_list_set_dep_word(features,
				    mw->dep_word_hash);
    anthy_feature_list_set_mw_features(features, mw->mw_features);
    anthy_feature_list_set_core_wtype(features, mw->core_wt);
    anthy_feature_list_set_yomi_hash(features, mw->yomi_hash);
  }
  anthy_feature_list_sort(features);
}

static double
search_probability(void *array, struct feature_list *fl, double noscore)
{
  double prob;
  struct feature_freq *res, arg;

  /* 確率を計算する */
  res = anthy_find_feature_freq(array, fl, &arg);
  if (res) {
    double pos = res->f[15];
    double neg = res->f[14];
    prob = 1 - (neg) / (double) (pos + neg);
    if (prob < 0) {
      prob = noscore;
    }
  } else {
    prob = noscore;
  }
  return prob;
}

static void
debug_ln(struct lattice_node *node,
	 double probability, double p)
{
  if (node) {
    printf(" cc=%d(%s), P=%f(%f)\n", node->seg_class,
	   anthy_seg_class_name(node->seg_class), probability, p);
  } else {
    printf(" cc=%d(%s), P=%f(%f)\n", SEG_TAIL,
	   anthy_seg_class_name(SEG_TAIL), probability, p);
  }
}

static double
get_transition_probability(struct lattice_node *node)
{
  struct feature_list seg_features;
  struct feature_list seg_trans_features;
  struct feature_list seg_struct_features;
  struct feature_list seg_len_features;
  double probability, p;
  double trans_p, seg_p, all_p, len_p;

  /* 全部の素性のリストを作る */
  anthy_feature_list_init(&seg_features, FL_SEG_FEATURES);
  build_feature_list(node, node->before_node, &seg_features);
  /* 文節接続に関する素性リスト */
  anthy_feature_list_clone(&seg_trans_features, &seg_features, FL_SEG_TRANS_FEATURES);
  anthy_feature_list_clone(&seg_struct_features, &seg_features, FL_SEG_STRUCT_FEATURES);
  /**/
  anthy_feature_list_clone(&seg_len_features, &seg_features, FL_SEG_LEN_FEATURES);

  /* それぞれの積で確率を計算する */
  probability = 1;
  trans_p = search_probability(trans_info_array, &seg_trans_features, 0.4f);
  seg_p = search_probability(seg_info_array, &seg_struct_features, 0.5f);
  all_p = search_probability(yomi_info_array, &seg_features, 1.0f);
  if (trans_p > 0.4f) {
    probability *= trans_p;
  } else {
    probability *= seg_p;
  }
  all_p = (2 + all_p) / 3;
  probability *= all_p;
  
  p = probability;
  len_p = search_probability(seg_len_info_array, &seg_len_features, 0.5f);
  probability *= len_p;


  /**/
  if (anthy_splitter_debug_flags() & SPLITTER_DEBUG_LN) {
    printf("trans_f=");anthy_feature_list_print(&seg_trans_features);
    printf("seg_f=");anthy_feature_list_print(&seg_struct_features);
    printf("all_f=");anthy_feature_list_print(&seg_features);
    printf("len_f=");anthy_feature_list_print(&seg_len_features);
    debug_ln(node, probability, p);
    printf(" trans_p=%f, seg_p=%f, all_p=%f, len_p=%f\n",
	   trans_p, seg_p, all_p, len_p);
  }

  /**/
  anthy_feature_list_free(&seg_features);
  anthy_feature_list_free(&seg_struct_features);
  anthy_feature_list_free(&seg_len_features);
  anthy_feature_list_free(&seg_trans_features);

  if (1) {
    /* length biasを順方向にかける */
    probability = (1 - probability) / node->mw->len;
    probability = 1 - probability;
  }

  return probability;
}

static struct lattice_info*
alloc_lattice_info(struct splitter_context *sc, int size)
{
  int i;
  struct lattice_info* info = (struct lattice_info*)malloc(sizeof(struct lattice_info));
  info->sc = sc;
  info->lattice_node_list = (struct node_list_head*)
    malloc((size + 1) * sizeof(struct node_list_head));
  for (i = 0; i < size + 1; i++) {
    info->lattice_node_list[i].head = NULL;
    info->lattice_node_list[i].nr_nodes = 0;
  }
  info->node_allocator = anthy_create_allocator(sizeof(struct lattice_node),
						NULL);
  info->last_node_id = 0;
  return info;
}

static void
calc_node_parameters(struct lattice_node *node)
{
  /* 対応するmetawordが無い場合は文頭と判断する */
  node->seg_class = node->mw ? node->mw->seg_class : SEG_HEAD; 

  if (node->before_node) {
    /* 左に隣接するノードがある場合 */
    if (node->mw && (node->mw->mw_features & MW_FEATURE_OCHAIRE)) {
      node->node_probability = 1.0f;
    } else {
      node->node_probability = get_transition_probability(node);
    }
    node->path_probability =
      node->before_node->path_probability *
      node->node_probability;
  } else {
    /* 左に隣接するノードが無い場合 */
    node->path_probability = 1.0f;
  }
}

static struct lattice_node*
alloc_lattice_node(struct lattice_info *info,
		   struct lattice_node* before_node,
		   struct meta_word* mw, int border)
{
  struct lattice_node* node;
  node = anthy_smalloc(info->node_allocator);
  info->last_node_id ++;
  node->node_id = info->last_node_id;
  node->before_node = before_node;
  node->border = border;
  node->next = NULL;
  node->mw = mw;

  calc_node_parameters(node);

  return node;
}

static void 
release_lattice_node(struct lattice_info *info, struct lattice_node* node)
{
  anthy_sfree(info->node_allocator, node);
}

static void
release_lattice_info(struct lattice_info* info)
{
  anthy_free_allocator(info->node_allocator);
  free(info->lattice_node_list);
  free(info);
}

static int
cmp_node_by_type(struct lattice_node *lhs, struct lattice_node *rhs,
		 enum metaword_type type)
{
  if (lhs->mw->type == type && rhs->mw->type != type) {
    return 1;
  } else if (lhs->mw->type != type && rhs->mw->type == type) {
    return -1;
  } else {
    return 0;
  }
}

static int
cmp_node_by_type_to_type(struct lattice_node *lhs, struct lattice_node *rhs,
			 enum metaword_type type1, enum metaword_type type2)
{
  if (lhs->mw->type == type1 && rhs->mw->type == type2) {
    return 1;
  } else if (lhs->mw->type == type2 && rhs->mw->type == type1) {
    return -1;
  } else {
    return 0;
  } 
}

/*
 * ノードを比較する
 *
 ** 返り値
 * 1: lhsの方が確率が高い
 * 0: 同じ
 * -1: rhsの方が確率が高い
 */
static int
cmp_node(struct lattice_node *lhs, struct lattice_node *rhs)
{
  struct lattice_node *lhs_before = lhs;
  struct lattice_node *rhs_before = rhs;
  int ret;

  if (lhs && !rhs) return 1;
  if (!lhs && rhs) return -1;
  if (!lhs && !rhs) return 0;

  while (lhs_before && rhs_before) {
    if (lhs_before->mw && rhs_before->mw &&
	lhs_before->mw->from + lhs_before->mw->len == rhs_before->mw->from + rhs_before->mw->len) {
      /* 学習から作られたノードかどうかを見る */
      ret = cmp_node_by_type(lhs_before, rhs_before, MW_OCHAIRE);
      if (ret != 0) return ret;

      /* COMPOUND_PARTよりはCOMPOUND_HEADを優先 */
      ret = cmp_node_by_type_to_type(lhs_before, rhs_before,
				     MW_COMPOUND_HEAD, MW_COMPOUND_PART);
      if (ret != 0) return ret;
    } else {
      break;
    }
    lhs_before = lhs_before->before_node;
    rhs_before = rhs_before->before_node;
  }

  /* 最後に遷移確率を見る */
  if (lhs->path_probability > rhs->path_probability) {
    return 1;
  } else if (lhs->path_probability < rhs->path_probability) {
    return -1;
  } else {
    return 0;
  }
}

/*
 * 構成中のラティスにノードを追加する
 */
static void
push_node(struct lattice_info* info, struct lattice_node* new_node,
	  int position)
{
  struct lattice_node* node;
  struct lattice_node* previous_node = NULL;

  if (anthy_splitter_debug_flags() & SPLITTER_DEBUG_LN) {
    print_lattice_node(info, new_node);
  }

  /* 先頭のnodeが無ければ無条件に追加 */
  node = info->lattice_node_list[position].head;
  if (!node) {
    info->lattice_node_list[position].head = new_node;
    info->lattice_node_list[position].nr_nodes ++;
    return;
  }

  while (node->next) {
    /* 余計なノードを追加しないための枝刈り */
    if (new_node->seg_class == node->seg_class &&
	new_node->border == node->border) {
      /* segclassが同じで、始まる位置が同じなら */
      switch (cmp_node(new_node, node)) {
      case 0:
      case 1:
	/* 新しい方が確率が大きいか学習によるものなら、古いのと置き換え*/
	if (previous_node) {
	  previous_node->next = new_node;
	} else {
	  info->lattice_node_list[position].head = new_node;
	}
	new_node->next = node->next;
	release_lattice_node(info, node);
	break;
      case -1:
	/* そうでないなら削除 */
	release_lattice_node(info, new_node);
	break;
      }
      return;
    }
    previous_node = node;
    node = node->next;
  }

  /* 最後のノードの後ろに追加 */
  node->next = new_node;
  info->lattice_node_list[position].nr_nodes ++;
}

/* 一番確率の低いノードを消去する*/
static void
remove_min_node(struct lattice_info *info, struct node_list_head *node_list)
{
  struct lattice_node* node = node_list->head;
  struct lattice_node* previous_node = NULL;
  struct lattice_node* min_node = node;
  struct lattice_node* previous_min_node = NULL;

  /* 一番確率の低いノードを探す */
  while (node) {
    if (cmp_node(node, min_node) < 0) {
      previous_min_node = previous_node;
      min_node = node;
    }
    previous_node = node;
    node = node->next;
  }

  /* 一番確率の低いノードを削除する */
  if (previous_min_node) {
    previous_min_node->next = min_node->next;
  } else {
    node_list->head = min_node->next;
  }
  release_lattice_node(info, min_node);
  node_list->nr_nodes --;
}

/* いわゆるビタビアルゴリズムを使用して経路を選ぶ */
static void
choose_path(struct lattice_info* info, int to)
{
  /* 最後まで到達した遷移のなかで一番確率の大きいものを選ぶ */
  struct lattice_node* node;
  struct lattice_node* best_node = NULL;
  int last = to; 
  while (!info->lattice_node_list[last].head) {
    /* 最後の文字まで遷移していなかったら後戻り */
    --last;
  }
  for (node = info->lattice_node_list[last].head; node; node = node->next) {
    if (cmp_node(node, best_node) > 0) {
      best_node = node;
    }
  }
  if (!best_node) {
    return;
  }

  /* 遷移を逆にたどりつつ文節の切れ目を記録 */
  node = best_node;
  if (anthy_splitter_debug_flags() & SPLITTER_DEBUG_LP) {
    printf("choose_path()\n");
  }
  while (node->before_node) {
    info->sc->word_split_info->best_seg_class[node->border] =
      node->seg_class;
    anthy_mark_border_by_metaword(info->sc, node->mw);
    /**/
    if (anthy_splitter_debug_flags() & SPLITTER_DEBUG_LP) {
      get_transition_probability(node);
      print_lattice_node(info, node);
    }
    /**/
    node = node->before_node;
  }
}

static void
build_graph(struct lattice_info* info, int from, int to)
{
  int i;
  struct lattice_node* node;
  struct lattice_node* left_node;

  /* 始点となるノードを追加 */
  node = alloc_lattice_node(info, NULL, NULL, from);
  push_node(info, node, from);

  /* info->lattice_node_list[index]にはindexまでの遷移が入っているのであって、
   * indexからの遷移が入っているのではない 
   */

  /* 全ての遷移を左から試す */
  for (i = from; i < to; ++i) {
    for (left_node = info->lattice_node_list[i].head; left_node;
	 left_node = left_node->next) {
      struct meta_word *mw;
      /* i文字目に到達するlattice_nodeのループ */

      for (mw = info->sc->word_split_info->cnode[i].mw; mw; mw = mw->next) {
	int position;
	struct lattice_node* new_node;
	/* i文字目からのmeta_wordのループ */

	if (mw->can_use != ok) {
	  continue; /* 決められた文節の区切りをまたぐmetawordは使わない */
	}
	position = i + mw->len;
	new_node = alloc_lattice_node(info, left_node, mw, i);
	push_node(info, new_node, position);

	/* 解の候補が多すぎたら、確率の低い方から削る */
	if (info->lattice_node_list[position].nr_nodes >= NODE_MAX_SIZE) {
	  remove_min_node(info, &info->lattice_node_list[position]);
	}
      }
    }
  }

  /* 文末補正 */
  for (node = info->lattice_node_list[to].head; node; node = node->next) {
    struct feature_list features;
    double prob;
    anthy_feature_list_init(&features, FL_SEG_TRANS_FEATURES);
    build_feature_list(NULL, node, &features);
    prob = search_probability(trans_info_array, &features, 0.5);
    node->path_probability = node->path_probability *
      prob;
    if (anthy_splitter_debug_flags() & SPLITTER_DEBUG_LN) {
      printf("trans_f=");anthy_feature_list_print(&features);
      debug_ln(NULL, prob, 0);
    }
    anthy_feature_list_free(&features);
  }
}

void
anthy_mark_borders(struct splitter_context *sc, int from, int to)
{
  struct lattice_info* info = alloc_lattice_info(sc, to);
  trans_info_array = anthy_file_dic_get_section("trans_info");
  seg_info_array = anthy_file_dic_get_section("seg_info");
  yomi_info_array = anthy_file_dic_get_section("yomi_info");
  seg_len_info_array = anthy_file_dic_get_section("seg_len_info");
  build_graph(info, from, to);
  choose_path(info, to);
  release_lattice_info(info);
}
