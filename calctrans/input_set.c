/* 入力のセットを管理するコード
 *
 * Copyright (C) 2006 HANAOKA Toshiyuki
 * Copyright (C) 2006-2007 TABATA Yusuke
 *
 * Special Thanks: Google Summer of Code Program 2006
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "input_set.h"

#define HASH_SIZE 1024

struct input_set {
  /**/
  struct input_line *lines;
  struct input_line *buckets[HASH_SIZE];
  /**/
};

static int
line_hash(const int *ar, int nr)
{
  int i;
  unsigned h = 0;
  for (i = 0; i < nr; i++) {
    h += ar[i];
  }
  return (h % HASH_SIZE);
}

static struct input_line *
find_same_line(struct input_set *is, int *features, int nr)
{
  struct input_line *il;
  int h = line_hash(features, nr);
  for (il = is->buckets[h]; il; il = il->next_in_hash) {
    int i;
    if (il->nr_features != nr) {
      continue;
    }
    for (i = 0; i < nr; i++) {
      if (il->features[i] != features[i]) {
	break;
      }
    }
    if (i >= nr) {
      return il;
    }
  }
  return NULL;
}

static struct input_line *
add_line(struct input_set *is, int *features, int nr)
{
  int i, h;
  struct input_line *il;
  il = malloc(sizeof(struct input_line));
  il->nr_features = nr;
  il->features = malloc(sizeof(int) * nr);
  for (i = 0; i < nr; i++) {
    il->features[i] = features[i];
  }
  il->weight = 0;
  il->negative_weight = 0;
  /* link */
  il->next_line = is->lines;
  is->lines = il;
  /**/
  h = line_hash(features, nr);
  il->next_in_hash = is->buckets[h];
  is->buckets[h] = il;
  return il;
}

/* input_setに入力を一つ加える */
void
input_set_set_features(struct input_set *is, int *features,
		       int nr, int weight)
{
  struct input_line *il;
  int abs_weight;
  if (weight == 0) {
    return ;
  }
  abs_weight = abs(weight);

  /**/
  il = find_same_line(is, features, nr);
  if (!il) {
    il = add_line(is, features, nr);
  }
  /**/
  if (weight > 0) {
    il->weight += weight;
  } else {
    il->negative_weight += abs_weight;
  }
}

struct input_set *
input_set_create(void)
{
  int i;
  struct input_set *is;
  is = malloc(sizeof(struct input_set));
  is->lines = NULL;
  /**/
  for (i = 0; i < HASH_SIZE; i++) {
    is->buckets[i] = NULL;
  }
  /**/
  return is;
}

struct input_line *
input_set_get_input_line(struct input_set *is)
{
  return is->lines;
}

struct input_set *
input_set_filter(struct input_set *is,
		 double pos, double neg)
{
  struct input_set *new_is = input_set_create();
  struct input_line *il;
  for (il = is->lines; il; il = il->next_line) {
    if (il->weight > pos ||
	il->negative_weight > neg) {
      input_set_set_features(new_is, il->features,
			     il->nr_features, il->weight);
      input_set_set_features(new_is, il->features,
			     il->nr_features, -il->negative_weight);
    }
  }
  return new_is;
}
