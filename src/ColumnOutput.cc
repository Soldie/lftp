/*
 * lftp and utils
 *
 * Copyright (c) 2001 by Alexander V. Lukyanov (lav@yars.free.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id$ */

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "SMTask.h"
#include "ColumnOutput.h"
#include "ResMgr.h"
#include "misc.h"

ResDecl res_color	("color:use-color",		"auto",ResMgr::TriBoolValidate,ResMgr::NoClosure);
ResDecl res_color_begin	("color:term-color-begin",	"\\e[", 0, 0);
ResDecl res_color_end	("color:term-color-end",	"m", 0, 0);
ResDecl res_color_reset	("color:term-color-reset",	"\\e[0m", 0, 0);

ColumnOutput::ColumnOutput()
{
   lst = 0;
   lst_cnt = lst_alloc = 0;
}

ColumnOutput::~ColumnOutput()
{
   for(int i = 0; i < lst_cnt; i++)
      delete lst[i];
   free(lst);
}

void ColumnOutput::add(const char *name, const char *color)
{
   lst[lst_cnt-1]->append(name, color);
}

void ColumnOutput::append()
{
   if(lst_cnt >= lst_alloc) {
      if(!lst_alloc) lst_alloc = 16;
      else lst_alloc += lst_alloc / 2;

      lst = (datum **) xrealloc(lst, sizeof(datum *) * lst_alloc);
   }

   lst[lst_cnt++] = new datum;
}

/* The minimum width of a colum is 3: 1 character for the name and 2
 * for the separating white space.  */
#define MIN_COLUMN_WIDTH        3

/* Assuming cursor is at position FROM, indent up to position TO.
 * Use a TAB character instead of two or more spaces whenever possible.  */
static void
indent (int from, int to, Buffer *o)
{
   // TODO
#define tabsize 8
   while (from < to) {
      if (tabsize > 0 && to / tabsize > (from + 1) / tabsize) {
	 o->Put("\t");
	 from += tabsize - from % tabsize;
      } else {
	 o->Put(" ");
	 from++;
      }
   }
}

void ColumnOutput::get_print_info(unsigned width, int *&col_arr, int *&ws_arr, int &cols) const
{
   /* Maximum number of columns ever possible for this display.  */
   int max_idx = width / MIN_COLUMN_WIDTH;
   if (max_idx == 0) max_idx = 1;

   col_arr = (int *) xmalloc (max_idx * sizeof (int));
   ws_arr = (int *) xmalloc (max_idx * sizeof (int));

   /* Normally the maximum number of columns is determined by the
    * screen width.  But if few files are available this might limit it
    * as well. */
   int max_cols = max_idx > lst_cnt ? lst_cnt : max_idx;
   if(max_cols < 1) max_cols = 1;

   /* Compute the maximum number of possible columns.  */
   for (cols = max_cols; cols >= 1; cols--) {
      for (int j = 0; j < max_idx; ++j) {
	 col_arr[j] = MIN_COLUMN_WIDTH;
	 ws_arr[j] = 99999999;
      }

      /* Find the amount of whitespace shared by every entry in the column. */
      for (int filesno = 0; filesno < lst_cnt; ++filesno) {
	 int idx = filesno / ((lst_cnt + cols - 1) / cols);
	 int ws = lst[filesno]->whitespace();
	 if(ws < ws_arr[idx]) ws_arr[idx] = ws;
      }

      /* Strip as much whitespace off the left as possible, but strip
       * the same amount from each entry (per column) to keep each
       * column aligned with itself. */
      unsigned line_len = cols * MIN_COLUMN_WIDTH;
      for (int filesno = 0; filesno < lst_cnt; ++filesno) {
	 int idx = filesno / ((lst_cnt + cols - 1) / cols);
	 int name_length = lst[filesno]->width();

	 /* all but the last column get 2 spaces of padding */
	 int real_length = name_length + (idx == cols-1 ? 0 : 2) - ws_arr[idx];

	 if (real_length <= col_arr[idx]) continue;

	 line_len += (real_length - col_arr[idx]);
	 col_arr[idx] = real_length;
      }
      if(line_len < width)
	 break; /* found it */
   }
   if(cols == 0) cols = 1;


}

void ColumnOutput::print(Buffer *o, unsigned width, bool color) const
{
   if(!lst_cnt) return; /* we have nothing to display */

   subst_t subst[] = {
      { 'e', "\033" },
      { 0, "" }
   };

   char *color_pref = Subst(res_color_begin.Query(getenv("TERM")), subst);
   char *color_suf = Subst(res_color_end.Query(getenv("TERM")), subst);
   char *color_reset = Subst(res_color_reset.Query(getenv("TERM")), subst);

   int cols;
   int *col_arr, *ws_arr;

   get_print_info(width, col_arr, ws_arr, cols);

   /* Calculate the number of rows that will be in each column except possibly
    * for a short column on the right. */
   int rows = lst_cnt / cols + (lst_cnt % cols != 0);

   for (int row = 0; row < rows; row++) {
      int col = 0;
      int filesno = row;
      int pos = 0;                      /* Current character column. */
      /* Print the next row.  */
      while (1) {
	 lst[filesno]->print(o, color, ws_arr[col], color_pref, color_suf, color_reset);
	 int name_length = lst[filesno]->width() - ws_arr[col];
	 int max_name_length = col_arr[col++];

	 filesno += rows;
	 if (filesno >= lst_cnt)
	    break;

	 indent (pos + name_length, pos + max_name_length, o);
	 pos += max_name_length;
      }
      o->Put("\n");
   }

   xfree(ws_arr);
   xfree(col_arr);

   xfree(color_pref);
   xfree(color_suf);
   xfree(color_reset);
}

datum::datum(): names(0), colors(0), num(0), ws(0), curwidth(0) { }
datum::~datum()
{
   for(int i = 0; i < num; i++) {
      xfree(names[i]);
      xfree(colors[i]);
   }
   xfree(names);
   xfree(colors);
}

int datum::width() const
{
   return curwidth;
}

void datum::append(const char *name, const char *color)
{
   num++;
   names = (char **) xrealloc(names, sizeof(char *) * num);
   colors = (char **) xrealloc(colors, sizeof(char *) * num);

   names[num-1] = xstrdup(name);
   colors[num-1] = xstrdup(color);
   if(num == 1) {
      ws = 0;
      for(int c = 0; name[c]; c++) {
	 if(name[c] != ' ') break;
	 ws++;
      }
   }

   /* XXX: UTF8 */
   curwidth += strlen(name);
}

void datum::print(Buffer *o, bool color, int skip,
		const char *color_pref, const char *color_suf, const char *color_reset) const
{
   const char *cur_color = 0;

   for(int i = 0; i < num; i++) {
      int len = strlen(names[i]);
      if(len < skip) {
	 skip -= len;
	 continue;
      }

      if(color) {
	 if(colors[i][0]) {
	    /* if it's the same color, don't bother */
	    if(!cur_color || !strcmp(cur_color, colors[i])) {
	       o->Put(color_pref);
	       o->Put(colors[i]);
	       o->Put(color_suf);

	       cur_color = colors[i];
	    }
	 } else {
	    /* reset color, if we have one */
	    if(cur_color) {
	       o->Put(color_reset);
	       cur_color = 0;
	    }
	 }
      }
      o->Put(names[i]+skip);
      skip = 0;
   }

   if(cur_color)
      o->Put(color_reset);
}

int datum::whitespace() const
{
   return ws;
}
