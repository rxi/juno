/** 
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */


#ifndef FS_H
#define FS_H

typedef struct fs_FileListNode {
  char *name;
  struct fs_FileListNode *next;
} fs_FileListNode;

enum {
  FS_ESUCCESS     = 0,
  FS_EFAILURE     = -1,
  FS_EOUTOFMEM    = -2,
  FS_EBADPATH     = -3,
  FS_EBADFILENAME = -4,
  FS_ENOWRITEPATH = -5,
  FS_ECANTOPEN    = -6,
  FS_ECANTREAD    = -7,
  FS_ECANTWRITE   = -8,
  FS_ECANTDELETE  = -9,
  FS_ECANTMKDIR   = -10,
  FS_ENOTEXIST    = -11,
};

const char *fs_errorStr(int err);
void fs_deinit(void);
int fs_mount(const char *path);
int fs_unmount(const char *path);
int fs_setWritePath(const char *path);
int fs_exists(const char *filename);
int fs_modified(const char *filename, unsigned *mtime);
int fs_size(const char *filename, size_t *size);
void *fs_read(const char *filename, size_t *size);
int fs_isDir(const char *filename);
fs_FileListNode *fs_listDir(const char *path);
void fs_freeFileList(fs_FileListNode *list);
int fs_write(const char *filename, const void *data, int size);
int fs_append(const char *filename, const void *data, int size);
int fs_delete(const char *filename);
int fs_makeDirs(const char *path);

#endif
