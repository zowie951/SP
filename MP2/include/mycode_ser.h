#define _XOPEN_SOURCE 500
#include "csiebox_server.h"
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
//server
int build(int conn_fd);
int sync_file(int conn_fd,csiebox_protocol_file *file);
int sync_meta(int conn_fd,csiebox_protocol_meta *meta);
int sync_hardlink(int conn_fd,csiebox_protocol_hardlink *hardlink);
int sync_rm(int conn_fd,csiebox_protocol_rm *rm);
