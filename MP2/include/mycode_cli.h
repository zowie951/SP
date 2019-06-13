#define _XOPEN_SOURCE 500
#include "csiebox_client.h"
#include "csiebox_common.h"
#include "connect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/inotify.h>
#include <dirent.h>
#include <utime.h>
#include "hash.h"

//client
int fn(const char *file,const struct stat *sb,int flag,struct FTW *s);
int fn_sync_meta(const char *file,struct stat *sb,int flag,struct FTW *s);
int myfunction(csiebox_client* client);
