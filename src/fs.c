/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
/* Force miniz.c to use regular fseek/ftell functions */
#define fseeko fseek
#define ftello ftell
#define fseeko64 fseek
#define ftello64 ftell
#include "lib/miniz.c"
#include "fs.h"

#if _WIN32
  #define mkdir(path, mode) mkdir(path)
#endif


typedef struct PathNode {
  struct PathNode *next;
  int type;
  mz_zip_archive zip;
  char path[1];
} PathNode;

static PathNode *mounts;
static PathNode *writePath;

enum {
  PATH_TDIR,
  PATH_TZIP
};


static char *concat(const char *str, ...) {
  va_list args;
  const char *s;
  /* Get len */
  int len = strlen(str);
  va_start(args, str); 
  while ((s = va_arg(args, char*))) {
    len += strlen(s);
  }
  va_end(args);
  /* Build string */
  char *res = malloc(len + 1);
  if (!res) return NULL;
  strcpy(res, str);
  va_start(args, str); 
  while ((s = va_arg(args, char*))) {
    strcat(res, s); 
  }
  va_end(args);
  return res;
}


static int isDir(const char *path) {
  struct stat s;
  int res = stat(path, &s);
  return (res == FS_ESUCCESS) && S_ISDIR(s.st_mode);
}


static int isSeparator(int chr) {
  return (chr == '/' || chr == '\\');
}


static int makeDirs(const char *path) {
  int err = FS_ESUCCESS;
  char *str = concat(path, "/", NULL);
  char *p = str;
  if (!str) {
    err = FS_EOUTOFMEM;
    goto end;
  }
  if (p[0] == '/') p++;
  if (p[0] && p[1] == ':' && p[2] == '\\') p += 3;
  while (*p) {
    if (isSeparator(*p)) {
      *p = '\0';
      if (!isDir(str)) {
        if (mkdir(str, S_IRWXU) == -1) {
          err = FS_ECANTMKDIR;
          goto end;
        }
      }
      *p = '/';
    }
    p++;
  }
end:
  free(str);
  return err;
}


static PathNode *newNode(const char *path) {
  int res;
  int len = strlen(path);
  PathNode *p = calloc(1, sizeof(*p) + len + 1);
  if (!p) return NULL;
  strcpy(p->path, path);
  /* Trim trailing path seperator */
  if (isSeparator(p->path[len - 1])) {
    p->path[len - 1] = '\0';
  }
  /* Get type */
  if (isDir(path)) {
    p->type = PATH_TDIR;
  } else {
    p->type = PATH_TZIP;
    res = mz_zip_reader_init_file(&p->zip, path, 0);
    assert(res);
  }
  return p;
}


static void destroyNode(PathNode *p) {
  if (p->type == PATH_TZIP) {
    mz_zip_reader_end(&p->zip);
  }
  free(p);
}


static int checkFilename(const char *filename) {
  if (*filename == '/' || strstr(filename, "..") || strstr(filename, ":\\")) {
    return FS_EFAILURE;
  }
  return FS_ESUCCESS;
}


static const char *skipDotSlash(const char *filename) {
  if (filename[0] == '.' && isSeparator(filename[1])) {
    return filename + 2;
  }
  return filename;
}


const char *fs_errorStr(int err) {
  switch(err) {
    case FS_ESUCCESS      : return "success";
    case FS_EFAILURE      : return "failure";
    case FS_EOUTOFMEM     : return "out of memory";
    case FS_EBADPATH      : return "bad path";
    case FS_EBADFILENAME  : return "bad filename";
    case FS_ENOWRITEPATH  : return "no write path set";
    case FS_ECANTOPEN     : return "could not open file";
    case FS_ECANTREAD     : return "could not read file";
    case FS_ECANTWRITE    : return "could not write file";
    case FS_ECANTDELETE   : return "could not delete file";
    case FS_ECANTMKDIR    : return "could not make directory";
    case FS_ENOTEXIST     : return "file or directory does not exist";
    default               : return "unknown error";
  }
}


void fs_deinit(void) {
  while (mounts) {
    PathNode *p = mounts->next;
    destroyNode(mounts);
    mounts = p;
  }
  if (writePath) {
    destroyNode(writePath);
    writePath = NULL;
  }
}


int fs_mount(const char *path) {
  /* Check if path is valid directory or archive */
  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));
  if (!isDir(path) && !mz_zip_reader_init_file(&zip, path, 0)) {
    return FS_EBADPATH;
  }
  mz_zip_reader_end(&zip);
  /* Check path isn't already mounted*/
  PathNode *p = mounts;
  while (p) {
    if (!strcmp(p->path, path)) {
      return FS_ESUCCESS;
    }
    p = p->next;
  }
  /* Construct node */
  p = newNode(path);
  if (!p) return FS_EOUTOFMEM;
  /* Add to start of list (highest priority) */
  p->next = mounts;
  mounts = p;
  return FS_ESUCCESS;
}


int fs_unmount(const char *path) {
  PathNode **next = &mounts;
  while (*next) {
    if (!strcmp((*next)->path, path)) {
      PathNode *p = *next;
      *next = (*next)->next;
      destroyNode(p);
      break;
    }
    next = &(*next)->next;
  }
  return FS_ESUCCESS;
}


int fs_setWritePath(const char *path) {
  int res = makeDirs(path);
  if (!isDir(path)) {
    return (res != FS_ESUCCESS) ? FS_ECANTMKDIR : FS_EBADPATH;
  }
  PathNode *p = newNode(path);
  if (!p) return FS_EOUTOFMEM;
  if (writePath) {
    destroyNode(writePath);
  }
  writePath = p;
  return FS_ESUCCESS;
}


static int fileInfo(
  const char *filename, unsigned *mtime, size_t *size, int *isdir
) {
  if (checkFilename(filename) != FS_ESUCCESS) return FS_EBADFILENAME;
  filename = skipDotSlash(filename);
  PathNode *p = mounts;
  while (p) {

    if (p->type == PATH_TDIR) {
      struct stat s;
      char *r = concat(p->path, "/", filename, NULL);
      if (!r) return FS_EOUTOFMEM;
      int res = stat(r, &s);
      free(r);
      if (res == 0) {
        if (mtime) *mtime = s.st_mtime;
        if (size) *size = s.st_size;
        if (isdir) *isdir = S_ISDIR(s.st_mode);
        return 0;
      }

    } else if (p->type == PATH_TZIP) {
      int idx = mz_zip_reader_locate_file(&p->zip, filename, NULL, 0);
      if (idx != -1) {
        if (mtime || size) {
          mz_zip_archive_file_stat s;
          mz_zip_reader_file_stat(&p->zip, idx, &s);
          if (mtime) *mtime = s.m_time;
          if (size) *size = s.m_uncomp_size;
        }
        if (isdir) {
          *isdir = mz_zip_reader_is_file_a_directory(&p->zip, idx);
        }
        return 0;
      }
    }
    p = p->next;
  }
  return FS_ENOTEXIST;
}



int fs_exists(const char *filename) {
  return fileInfo(filename, NULL, NULL, NULL) == FS_ESUCCESS;
}


int fs_modified(const char *filename, unsigned *mtime) {
  return fileInfo(filename, mtime, NULL, NULL);
}


int fs_size(const char *filename, size_t *size) {
  return fileInfo(filename, NULL, size, NULL);
}


void *fs_read(const char *filename, size_t *len) {
  size_t len_ = 0;
  if (!len) len = &len_;
  if (checkFilename(filename) != FS_ESUCCESS) return NULL;
  filename = skipDotSlash(filename);
  PathNode *p = mounts;
  while (p) {
    if (p->type == PATH_TDIR) {
      char *r = concat(p->path, "/", filename, NULL);
      if (!r) return NULL;
      FILE *fp = fopen(r, "rb");
      free(r);
      if (!fp) goto next;
      /* Get file size */
      fseek(fp, 0, SEEK_END);
      *len = ftell(fp);
      /* Load file */
      fseek(fp, 0, SEEK_SET);
      char *res = malloc(*len + 1);
      if (!res) return NULL;
      res[*len] = '\0';
      if (fread(res, 1, *len, fp) != *len) {
        free(res);
        fclose(fp);
        return NULL;
      }
      fclose(fp);
      return res;

    } else if (p->type == PATH_TZIP) {
      char *res = mz_zip_reader_extract_file_to_heap(
        &p->zip, filename, len, 0);
      if (res) {
        return res;
      }
    }
next:
    p = p->next;
  }
  return NULL;
}


int fs_isDir(const char *filename) {
  int res;
  int err = fileInfo(filename, NULL, NULL, &res);
  if (err) return 0;
  return res;
}


static fs_FileListNode *newFileListNode(const char *path) {
  fs_FileListNode *n = malloc(sizeof(*n) + strlen(path) + 1);
  if (!n) return NULL;
  n->next = NULL;
  n->name = (void*) (n + 1);
  strcpy(n->name, path);
  return n;
}

static int appendFileListNode(fs_FileListNode **list, const char *path) {
  fs_FileListNode **n = list;
  while (*n) {
    if (!strcmp(path, (*n)->name)) return FS_ESUCCESS;
    n = &(*n)->next;
  }
  *n = newFileListNode(path);
  if (!*n) return FS_EOUTOFMEM;
  return FS_ESUCCESS;
}

static int containsSeperator(const char *str) {
  const char *p = str;
  while (*p) {
    if (isSeparator(*p++)) return 1;
  }
  return 0;
}

fs_FileListNode *fs_listDir(const char *path) {
  char *pathTrimmed = NULL;
  int pathTrimmedLen;
  fs_FileListNode *res = NULL;
  PathNode *p = mounts;
  if (checkFilename(path) != FS_ESUCCESS) return NULL;
  /* Copy path string, trim separator from end if it exists */
  pathTrimmed = concat(path, NULL);
  if (!pathTrimmed) goto outOfMem;
  pathTrimmedLen = strlen(pathTrimmed);
  if (isSeparator(pathTrimmed[pathTrimmedLen - 1])) {
    pathTrimmed[--pathTrimmedLen] = '\0';
  }
  if (!strcmp(pathTrimmed, ".")) {
    pathTrimmed[0] = '\0';
    pathTrimmedLen = 0;
  }
  /* Fill result list */
  while (p) {
    if (p->type == PATH_TDIR) {
      DIR *dp;
      struct dirent *ep;
      char *r = concat(p->path, "/", pathTrimmed, NULL);
      if (!r) goto outOfMem;
      dp = opendir(r);
      free(r);
      if (!dp) goto next;
      while ((ep = readdir(dp))) {
        /* Skip ".." and "." */
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, "..")) {
          continue;
        }
        if (appendFileListNode(&res, ep->d_name) != FS_ESUCCESS) {
          closedir(dp);
          goto outOfMem;
        }
      }
      closedir(dp);

    } else if (p->type == PATH_TZIP) {
      /* Iterate *all* the files and append those in the correct directory */
      int i;
      mz_zip_archive_file_stat s;
      int nfiles = mz_zip_reader_get_num_files(&p->zip);
      for (i = 0; i < nfiles; i++) {
        mz_zip_reader_file_stat(&p->zip, i, &s);
        if (
          ( pathTrimmedLen == 0 ) ||
          ( strstr(s.m_filename, pathTrimmed) == s.m_filename &&
            isSeparator(s.m_filename[pathTrimmedLen]) )
        ) {
          char *filename = s.m_filename + pathTrimmedLen;
          int filenameLen = strlen(filename);
          if (isSeparator(filename[0])) filename++;
          /* Strip trailing seperator if it exists -- end of dir names contain
           * a seperator. If this leaves us with nothing then this file was the
           * path itself, which we skip */
          if (isSeparator(filename[filenameLen - 1])) {
            filename[filenameLen - 1] = '\0';
          }
          if (*filename == '\0') continue;
          /* If the filename still contains seperators its the contents of a
           * sub directory -- we don't want to include this */
          if (containsSeperator(filename)) {
            continue;
          }
          if (appendFileListNode(&res, filename) != FS_ESUCCESS) {
            goto outOfMem;
          }
        }
      }
    }
next:
    p = p->next;
  }
  free(pathTrimmed);
  return res;
outOfMem:
  free(pathTrimmed);
  fs_freeFileList(res);
  return NULL;
}


void fs_freeFileList(fs_FileListNode *list) {
  fs_FileListNode *next;
  while (list) {
    next = list->next;
    free(list);
    list = next;
  }
}


static int writeUsingMode(
  const char *filename, const char *mode, const void *data, int size
) {
  if (!writePath) return FS_ENOWRITEPATH;
  if (checkFilename(filename) != FS_ESUCCESS) return FS_EBADFILENAME;
  char *name = concat(writePath->path, "/", filename, NULL);
  if (!name) return FS_EOUTOFMEM;
  FILE *fp = fopen(name, mode);
  free(name);
  if (!fp) return FS_ECANTOPEN;
  int res = fwrite(data, size, 1, fp);
  fclose(fp);
  return (res == 1) ? FS_ESUCCESS : FS_ECANTWRITE;
}

int fs_write(const char *filename, const void *data, int size) {
  return writeUsingMode(filename, "wb", data, size);
}


int fs_append(const char *filename, const void *data, int size) {
  return writeUsingMode(filename, "ab", data, size);
}


int fs_delete(const char *filename) {
  if (!writePath) return FS_ENOWRITEPATH;
  if (checkFilename(filename) != FS_ESUCCESS) return FS_EBADFILENAME;
  char *name = concat(writePath->path, "/", filename, NULL);
  if (!name) return FS_EOUTOFMEM;
  int res = remove(name);
  free(name);
  return (res == 0) ? FS_ESUCCESS : FS_ECANTDELETE;
}


int fs_makeDirs(const char *path) {
  if (!writePath) return FS_ENOWRITEPATH;
  if (checkFilename(path) != FS_ESUCCESS) return FS_EBADFILENAME;
  char *name = concat(writePath->path, "/", path, NULL);
  if (!name) return FS_EOUTOFMEM;
  int res = makeDirs(name);
  free(name);
  return res;
}
