/*
 * ファイルをまとめて辞書ファイルを生成する
 *
 * デフォルトではひとつ上のディレクトリ「..」に各ファイルの
 * パス名を付けるが、このコマンドに対する -p オプションで
 * 変更することができる。
 *
 * entry_num個のファイルに対して
 *  0: entry_num ファイルの個数
 *  1: 各ファイルの情報
 *    n * 3    : name_offset
 *    n * 3 + 1: strlen(key)
 *    n * 3 + 2: contents_offset
 *  [name_of_section]*entry_num
 *   : 各ファイルの名前
 *  [file]*entry_num
 *   : 各ファイルの内容
 *
 * Copyright (C) 2005-2006 YOSHIDA Yuichi
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
#include <sys/stat.h>

#include <anthy/xstr.h>
#include <anthy/feature_set.h>
#include <anthy/diclib.h>

#define SECTION_ALIGNMENT 64
#define DIC_NAME "anthy.dic"

struct header_entry {
  const char* key;
  const char* file_name;
};

static void
write_nl(FILE* fp, int i)
{
  i = anthy_dic_htonl(i);
  fwrite(&i, sizeof(int), 1, fp);
}


/** ファイルのサイズを取得する */
static int
get_file_size(const char* fn)
{
  struct stat st;
  if (stat(fn, &st) < 0) {
    return -1;
  }
  return (st.st_size + SECTION_ALIGNMENT - 1) & (-SECTION_ALIGNMENT);
}

static char *
get_file_name(const char *prefix, struct header_entry* entry)
{
  char *fn = malloc(strlen(prefix) + strlen(entry->file_name) + 4);
  if (entry->file_name[0] == '/') {
    sprintf(fn, "%s/%s", prefix, entry->file_name);
  } else {
    sprintf(fn, "./%s", entry->file_name);
  }
  return fn;
}

static int
write_header(FILE* fp, const char *prefix,
	     int entry_num, struct header_entry* entries)
{
  int i;
  int name_offset;
  int contents_offset;

  name_offset = sizeof(int) * (1 + entry_num * 3);
  contents_offset = name_offset;

  for (i = 0; i < entry_num; ++i) {
    contents_offset += strlen(entries[i].key);
  }
  contents_offset =
    (contents_offset + SECTION_ALIGNMENT - 1) & (-SECTION_ALIGNMENT);

  /* ファイルの数 */
  write_nl(fp, entry_num);

  /* 各ファイルの場所を出力する */
  for (i = 0; i < entry_num; ++i) {
    char *fn = get_file_name(prefix, &entries[i]);
    int file_size = get_file_size(fn);
    if (file_size == -1) {
      fprintf(stderr, "failed to get file size of (%s).\n",
	      fn);
      free(fn);
      return -1;
    }
    free(fn);
    /**/
    write_nl(fp, name_offset);
    write_nl(fp, strlen(entries[i].key));
    write_nl(fp, contents_offset);
    /**/
    name_offset += strlen(entries[i].key);
    contents_offset += file_size;
  }

  /* 各ファイルの名前を出力する */
  for (i = 0; i < entry_num; ++i) {
    fprintf(fp, "%s", entries[i].key);
  }
  return 0;
}



static void
copy_file(FILE *in, FILE *out)
{
  int i;
  size_t nread;
  char buf[BUFSIZ];

  /* Pad OUT to the next aligned offset.  */
  for (i = ftell (out); i & (SECTION_ALIGNMENT - 1); i++) {
    fputc (0, out);
  }

  /* Copy the contents.  */
  rewind(in);
  while ((nread = fread (buf, 1, sizeof buf, in)) > 0) {
    if (fwrite (buf, 1, nread, out) < nread) {
      exit (1);
    }
  }
}

static void
write_contents(FILE* fp, const char *prefix,
	       int entry_num, struct header_entry* entries)
{
  int i;
  for (i = 0; i < entry_num; ++i) {
    FILE* in_fp;
    char *fn = get_file_name(prefix, &entries[i]);

    in_fp = fopen(fn, "r");
    if (in_fp == NULL) {
      printf("failed to open %s\n", fn);
      free(fn);
      break;
    }
    printf("  copying %s (%s)\n", fn, entries[i].key);
    free(fn);
    copy_file(in_fp, fp);
    fclose(in_fp);
  }
}


static void
create_file_dic(const char* fn, const char *prefix,
		int entry_num, struct header_entry* entries)
{
  FILE* fp = fopen(fn, "w");
  int res;
  if (!fp) {
    fprintf(stderr, "failed to open file dictionary file (%s).\n", fn);
    exit(1);
  }
  /* ヘッダを書き出す */
  res = write_header(fp, prefix, entry_num, entries);
  if (res) {
    exit(1);
  }

  /* ファイルの中身を書き出す */
  write_contents(fp, prefix, entry_num, entries);
  fclose(fp);
}

static void
convert_line(FILE *ofp, char *buf)
{
  char *tok;
  tok = strtok(buf, ",");
  do {
    int n = atoi(tok);
    write_nl(ofp, n);
    tok = strtok(NULL, ",");
  } while (tok);
}

static void
convert_file(FILE *ifp)
{
  char buf[1024];
  FILE *ofp = NULL;
  while (fgets(buf, 1024, ifp)) {
    /**/
    if (buf[0] == '#') {
      continue;
    }
    if (!strncmp("section", buf, 7)) {
      int w, n, i;
      char fn[1024];
      if (ofp) {
	fclose(ofp);
	ofp = NULL;
      }
      sscanf(buf, "section %s %d %d", fn, &w, &n);
      ofp = fopen(fn, "w");
      if (!ofp) {
	fprintf(stderr, "failed to open (%s)\n", fn);
	abort();
      }
      write_nl(ofp, w);
      write_nl(ofp, n);
      for (i = 0; i < NR_EM_FEATURES; i++) {
	write_nl(ofp, 0);
      }
    } else {
      convert_line(ofp, buf);
    }
  }
  if (ofp) {
    fclose(ofp);
  }
}

static void
convert_data(const char *fn)
{
  FILE *ifp;
  ifp = fopen(fn, "r");
  if (!ifp) {
    fprintf(stderr, "failed to open (%s)\n", fn);
    abort();
  }
  convert_file(ifp);
  fclose(ifp);
}

int
main(int argc, char* argv[])
{
  int i;
  const char *prefix = "..";
  const char *prev_arg = "";
  const char *dict_source = NULL;

  struct header_entry entries[] = {
    {"word_dic", "/mkworddic/anthy.wdic"},
    {"dep_dic", "/depgraph/anthy.dep"},
    {"trans_info", "anthy.trans_info"},
    {"seg_info", "anthy.seg_info"},
    {"cand_info", "anthy.cand_info"},
    {"yomi_info", "anthy.yomi_info"},
    {"seg_len_info", "anthy.seg_len_info"},
    {"corpus_bucket", "anthy.corpus_bucket"},
    {"corpus_array", "anthy.corpus_array"},
  };

  for (i = 1; i < argc; i++) {
    if (!strcmp("-p", prev_arg)) {
      prefix = argv[i];
    }
    if (!strcmp("-c", prev_arg)) {
      dict_source = argv[i];
    }
    /**/
    prev_arg = argv[i];
  }
  if (dict_source) {
    convert_data(dict_source);
  }
  printf("file name prefix=[%s] you can change this by -p option.\n", prefix);

  create_file_dic(DIC_NAME, prefix,
		  sizeof(entries)/sizeof(struct header_entry),
		  entries);

  printf("%s done.\n", argv[0]);
  return 0;
}
