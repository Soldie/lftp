/*
 * lftp and utils
 *
 * Copyright (c) 1998 by Alexander V. Lukyanov (lav@yars.free.net)
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
#include "FileSet.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>

#include "misc.h"
#include "ResMgr.h"

void  FileInfo::Merge(const FileInfo& f)
{
   if(strcmp(name,f.name))
      return;
// int sim=defined&f.defined;
   int dif=(~defined)&f.defined;
   if(dif&MODE)
      SetMode(f.mode);
   if(dif&DATE)
      SetDate(f.date);
   if(dif&DATE_UNPREC && !(defined&DATE))
      SetDateUnprec(f.date);
   if(dif&TYPE)
      SetType(f.filetype);
   if(dif&SYMLINK)
      SetSymlink(f.symlink);
}

void FileInfo::SetName(const char *n)
{
   if(n==name)
      return;
   xfree(name);
   name=xstrdup(n);
   defined|=NAME;
}


void FileSet::Add(FileInfo *fi)
{
   assert(!sorted);
   if(!(fi->defined & fi->NAME))
   {
      delete fi;
      return;
   }
   /* add sorted */
   int pos = FindGEIndByName(fi->name);
   if(pos < fnum && !strcmp(files[pos]->name,fi->name)) {
      files[pos]->Merge(*fi);
      delete fi;
      return;
   }
   files=files_sort=(FileInfo**)xrealloc(files,(++fnum)*sizeof(*files));
   memmove(files+pos+1, files+pos, sizeof(*files)*(fnum-pos-1));
   files[pos]=fi;
}

void FileSet::Sub(int i)
{
   assert(!sorted);
   if(i>=fnum)
      abort();
   delete files[i];
   memmove(files+i,files+i+1,(--fnum-i)*sizeof(*files));
   if(ind>i)
      ind--;
}

void FileSet::Merge(const FileSet *set)
{
   int i,j;
   for(i=0; i<set->fnum; i++)
   {
      for(j=0; j<fnum; j++)
      {
      	 if(!strcmp(files[j]->name,set->files[i]->name))
	 {
	    files[j]->Merge(*(set->files[i]));
	    break;
	 }
      }
      if(j==fnum)
      {
	 Add(new FileInfo(*set->files[i]));
      }
   }
}

void FileSet::Merge(char **list)
{
   if(list==0)
      return;

   int j;
   for( ; *list; list++)
   {
      for(j=0; j<fnum; j++)
      {
      	 if(!strcmp(files[j]->name,*list))
	    break;
      }
      if(j==fnum)
      {
	 FileInfo *fi=new FileInfo();
	 fi->SetName(*list);
	 Add(fi);
      }
   }
}

/* we don't copy the sort state--nothing needs it, and it'd
 * be a bit of a pain to implement. */
FileSet::FileSet(FileSet const *set)
{
   ind=set->ind;
   fnum=set->fnum;
   sorted = false;
   if(fnum==0)
      files=files_sort=0;
   else
      files=files_sort=(FileInfo**)xmalloc(fnum*sizeof(*files));
   for(int i=0; i<fnum; i++)
      files[i]=new FileInfo(*(set->files[i]));
}

static int (*compare)(const char *s1, const char *s2);

static int sort_name(const void *s1, const void *s2)
{
   const FileInfo *p1 = *(const FileInfo **) s1;
   const FileInfo *p2 = *(const FileInfo **) s2;
   return compare(p1->name, p2->name);
}

static int sort_size(const void *s1, const void *s2)
{
   const FileInfo *p1 = *(const FileInfo **) s1;
   const FileInfo *p2 = *(const FileInfo **) s2;
   if(p1->size > p2->size) return -1;
   if(p1->size < p2->size) return 1;
   return 0;
}

static int sort_dirs(const void *s1, const void *s2)
{
   const FileInfo *p1 = *(const FileInfo **) s1;
   const FileInfo *p2 = *(const FileInfo **) s2;
   if(p1->filetype == FileInfo::DIRECTORY && !p2->filetype == FileInfo::DIRECTORY) return -1;
   if(!p1->filetype == FileInfo::DIRECTORY && p2->filetype == FileInfo::DIRECTORY) return 1;
   return 0;
}

/* files_sort is an alias of files when sort == NAME (since
 * files is always sorted by name), and an independant array
 * of pointers (pointing to the same data) otherwise. */
void FileSet::Sort(sort_e newsort, bool casefold)
{
   if(newsort == BYNAME && !casefold) {
      Unsort();
      return;
   }
   
   if(files_sort == files) {
      files_sort=(FileInfo**)xmalloc(fnum*sizeof(FileInfo *));
      for(int i=0; i < fnum; i++)
	 files_sort[i] = files[i];
   }

   sorted = true;

   if(casefold) compare = strcasecmp;
   else compare = strcmp;

   switch(newsort) {
   case BYNAME: qsort(files_sort, fnum, sizeof(FileInfo *), sort_name); break;
   case BYSIZE: qsort(files_sort, fnum, sizeof(FileInfo *), sort_size); break; 
   case DIRSFIRST: qsort(files_sort, fnum, sizeof(FileInfo *), sort_dirs); break; 
   }
}

/* Remove the current sort, allowing new entries to be added.
 * (Nothing uses this ... */
void FileSet::Unsort()
{
   if(!sorted) return;
   xfree(files_sort);
   files_sort = files;
   sorted = false;
}

void FileSet::Empty()
{
   for(int i=0; i<fnum; i++)
      delete files[i];
   xfree(files);
   files=0;
   fnum=0;
   ind=0;
}

FileSet::~FileSet()
{
   Empty();
}

void FileSet::SubtractSame(const FileSet *set,
      const TimeInterval *prec,const TimeInterval *loose_prec,int ignore)
{
   for(int i=0; i<fnum; i++)
   {
      FileInfo *f=set->FindByName(files[i]->name);
      if(f && files[i]->SameAs(f,prec,loose_prec,ignore))
	 Sub(i--);
   }
}

void FileSet::SubtractAny(const FileSet *set)
{
   for(int i=0; i<fnum; i++)
      if(set->FindByName(files[i]->name))
	 Sub(i--);
}

void FileSet::SubtractNotIn(const FileSet *set)
{
   for(int i=0; i<fnum; i++)
      if(!set->FindByName(files[i]->name))
	 Sub(i--);
}

void FileSet::SubtractOlderThan(time_t t)
{
   for(int i=0; i<fnum; i++)
   {
      if(files[i]->defined&FileInfo::TYPE
      && files[i]->filetype!=FileInfo::NORMAL)
	 continue;
      if(files[i]->OlderThan(t))
      {
	 Sub(i);
	 i--;
      }
   }
}

void FileSet::ExcludeDots()
{
   for(int i=0; i<fnum; i++)
   {
      if(!strcmp(files[i]->name,".") || !strcmp(files[i]->name,".."))
      {
	 Sub(i);
	 i--;
      }
   }
}

bool  FileInfo::SameAs(const FileInfo *fi,
	 const TimeInterval *prec,const TimeInterval *loose_prec,int ignore)
{
   if(defined&NAME && fi->defined&NAME)
      if(strcmp(name,fi->name))
	 return false;
   if(defined&TYPE && fi->defined&TYPE)
      if(filetype!=fi->filetype)
	 return false;

   if((defined&TYPE && filetype==DIRECTORY)
   || (fi->defined&TYPE && fi->filetype==DIRECTORY))
      return false;  // can't guarantee directory is the same (recursively)

   if(defined&SYMLINK_DEF && fi->defined&SYMLINK_DEF)
      return (strcmp(symlink,fi->symlink)==0);

   if(defined&(DATE|DATE_UNPREC) && fi->defined&(DATE|DATE_UNPREC)
   && !(ignore&DATE))
   {
      time_t p;
      bool inf;
      if((defined&DATE_UNPREC) || (fi->defined&DATE_UNPREC))
      {
	 p=loose_prec->Seconds();
	 inf=loose_prec->IsInfty();
      }
      else
      {
	 p=prec->Seconds();
	 inf=prec->IsInfty();
      }
      if(!(ignore&IGNORE_DATE_IF_OLDER && date<fi->date)
      && (!inf && labs((long)date-(long)(fi->date))>p))
	 return false;
   }

   if(defined&SIZE && fi->defined&SIZE && !(ignore&SIZE))
   {
      if(!(ignore&IGNORE_SIZE_IF_OLDER
      && defined&(DATE|DATE_UNPREC) && fi->defined&(DATE|DATE_UNPREC)
      && date<fi->date)
      && (size!=fi->size))
	 return false;
   }

   return true;
}

bool  FileInfo::OlderThan(time_t t)
{
   return ((defined&(DATE|DATE_UNPREC)) && date<t);
}

void FileSet::Count(int *d,int *f,int *s,int *o)
{
   for(int i=0; i<fnum; i++)
   {
      if(!(files[i]->defined&FileInfo::TYPE))
      {
	 if(o) (*o)++;
      }
      else switch(files[i]->filetype)
      {
      case(FileInfo::DIRECTORY):
	 if(d) (*d)++; break;
      case(FileInfo::NORMAL):
	 if(f) (*f)++; break;
      case(FileInfo::SYMLINK):
	 if(s) (*s)++; break;
      }
   }
}

/* assumes sorted by name. binary search for name, returning the first name
 * >= name; returns fnum if name is greater than all names. */
int FileSet::FindGEIndByName(const char *name) const
{
   int l = 0, u = fnum - 1;

   /* no files or name is greater than the max file: */
   if(!fnum || strcmp(files[u]->name, name) < 0)
      return fnum;

   /* we have files, and u >= name (meaning l <= name <= u); loop while
    * this is true: */
   while(l < u) {
      /* find the midpoint: */
      int m = (l + u) / 2;
      int cmp = strcmp(files[m]->name, name);

      /* if files[m]->name >= name, update the upper bound: */
      if (cmp >= 0)
	 u = m;

      /* if files[m]->name < name, update the lower bound: */
      if (cmp < 0)
	 l = m+1;
   }

   return u;
}

FileInfo *FileSet::FindByName(const char *name) const
{
   int n = FindGEIndByName(name);

   if(n < fnum && !strcmp(files[n]->name,name))
      return files[n];
   
   return 0;
}

void  FileSet::Exclude(const char *prefix,regex_t *exclude,regex_t *include)
{
   for(int i=0; i<fnum; i++)
   {
      const char *name=dir_file(prefix,files[i]->name);
      if(!(include && regexec(include,name,0,0,0)==0)
       && ((exclude && regexec(exclude,name,0,0,0)==0)
	   || (include && !exclude)))
      {
	 Sub(i);
	 i--;
      }
   }
}


// *** Manipulations with set of local files

#if 0
void FileSet::LocalRemove(const char *dir)
{
   FileInfo *file;
   for(int i=0; i<fnum; i++)
   {
      file=files[i];
      if(file->defined & (file->DATE|file->DATE_UNPREC))
      {
	 const char *local_name=dir_file(dir,file->name);

	 if(!(file->defined & file->TYPE)
	 || file->filetype==file->DIRECTORY)
	 {
	    int res=rmdir(local_name);
	    if(res==0)
	       continue;
	    res=remove(local_name);
	    if(res==0)
	       continue;
	    truncate_file_tree(local_name);
	    continue;
	 }
	 remove(local_name);
      }
   }
}
#endif

void FileSet::LocalUtime(const char *dir,bool only_dirs)
{
   FileInfo *file;
   for(int i=0; i<fnum; i++)
   {
      file=files[i];
      if(file->defined & (file->DATE|file->DATE_UNPREC))
      {
	 if(!(file->defined & file->TYPE))
	    continue;
	 if(file->filetype==file->SYMLINK)
	    continue;
	 if(only_dirs && file->filetype!=file->DIRECTORY)
	    continue;

	 const char *local_name=dir_file(dir,file->name);
	 struct utimbuf ut;
	 struct stat st;
	 ut.actime=ut.modtime=file->date;

	 if(stat(local_name,&st)!=-1 && st.st_mtime!=file->date)
	    utime(local_name,&ut);
      }
   }
}
void FileSet::LocalChmod(const char *dir,mode_t mask)
{
   FileInfo *file;
   for(int i=0; i<fnum; i++)
   {
      file=files[i];
      if(file->defined & file->MODE)
      {
	 if(file->defined & file->TYPE
	 && file->filetype==file->SYMLINK)
	    continue;

	 const char *local_name=dir_file(dir,file->name);

	 struct stat st;
	 mode_t new_mode=file->mode&~mask;

	 if(stat(local_name,&st)!=-1 && st.st_mode!=new_mode)
	    chmod(local_name,new_mode);
      }
   }
}

FileInfo * FileSet::operator[](int i) const
{
   if(i >= fnum) return 0;
   return files_sort[i];
}

FileInfo *FileSet::curr()
{
   return (*this)[ind];
}
FileInfo *FileSet::next()
{
   if(ind<fnum)
   {
      ind++;
      return curr();
   }
   return 0;
}

void FileInfo::Init()
{
   name=NULL;
   defined=0;
   symlink=NULL;
   data=0;
}
FileInfo::FileInfo(const FileInfo &fi)
{
   Init();
   name=xstrdup(fi.name);
   symlink=xstrdup(fi.symlink);
   defined=fi.defined;
   filetype=fi.filetype;
   mode=fi.mode;
   date=fi.date;
   size=fi.size;
}

#ifndef S_ISLNK
# define S_ISLNK(mode) (S_IFLNK==(mode&S_IFMT))
#endif

void FileInfo::LocalFile(const char *name, bool follow_symlinks)
{
   SetName(name);
   struct stat st;
   if(lstat(name,&st)==-1)
      return;

check_again:
   FileInfo::type t;
   if(S_ISDIR(st.st_mode))
      t=FileInfo::DIRECTORY;
   else if(S_ISREG(st.st_mode))
      t=FileInfo::NORMAL;
#ifdef HAVE_LSTAT
   else if(S_ISLNK(st.st_mode))
   {
      if(follow_symlinks)
      {
	 if(stat(name,&st)!=-1)
	    goto check_again;
	 // dangling symlink, don't follow it.
      }
      t=FileInfo::SYMLINK;
   }
#endif
   else
      return;   // ignore other type files

   SetSize(st.st_size);
   SetDate(st.st_mtime);
   SetMode(st.st_mode&07777);
   SetType(t);

#ifdef HAVE_LSTAT
   if(t==SYMLINK)
   {
      char *buf=(char*)alloca(st.st_size+1);
      int res=readlink(name,buf,st.st_size);
      if(res!=-1)
      {
	 buf[res]=0;
	 SetSymlink(buf);
      }
   }
#endif /* HAVE_LSTAT */
}

FileInfo::~FileInfo()
{
   xfree(name);
   xfree(symlink);
   xfree(data);
}
