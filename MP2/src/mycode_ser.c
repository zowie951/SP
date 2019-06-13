#include "mycode_ser.h"
#define DIR_S_FLAG (S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))
//server
int build(int conn_fd){
	int *buf;
	buf = (int*)malloc(sizeof(int));
	recv_message(conn_fd,buf,sizeof(int));
//printf("\tflag\n");
	int flag = *buf;
	recv_message(conn_fd,buf,sizeof(int));
//printf("\tlen \n",*buf);
	int filelen = (*buf) ;

	free(buf);
	char *file = (char*)malloc(filelen + 1);
	recv_message(conn_fd,file,filelen);
	file[filelen] = '\0';
	printf("build %s\n",file);
	if(flag==FTW_SL){
		int *buff = (int*)malloc(sizeof(int));
        recv_message(conn_fd,buff,sizeof(int));
        int target_len = (*buff);
        char* target = (char*)malloc(target_len+1);
        free(buff);
        recv_message(conn_fd,target,target_len);
		target[target_len] = '\0';
	printf("target %s\n",target);
        symlink(target,file);
        free(target);
	}
	else if(flag==FTW_F){
		int *buf = (int *)malloc(sizeof(int));
		recv_message(conn_fd,buf,sizeof(int));
		int datalen = *buf;
		free(buf);
		char* data = (char*)malloc(datalen);
		recv_message(conn_fd,data,datalen);
		FILE *fp;
		fp = fopen(file,"wb");
		fwrite(data,sizeof(char),datalen,fp);
		fclose(fp);
		free(data);
	}
	else if(flag==FTW_D||flag==FTW_DNR){
		mkdir(file,DIR_S_FLAG);
	}
	else if(flag == FTW_NS){
			
	}
	free(file);
	csiebox_protocol_header header;
  	memset(&header, 0, sizeof(header));
  	header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  	header.res.op = CSIEBOX_PROTOCOL_OP_BUILD;
  	header.res.datalen = 0;
  	header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
  	if(!send_message(conn_fd, &header, sizeof(header))){
    	fprintf(stderr, "send fail\n");
    	return 0;
  	}
	return 1;
}
int sync_file(int conn_fd,csiebox_protocol_file *file){
	char *path = (char*)malloc((file->message.body.pathlen)+1);
	recv_message(conn_fd,path,file->message.body.pathlen);
	path[file->message.body.pathlen] = '\0';
	struct stat statbuf;
	memset(&statbuf,0,sizeof(struct stat));
	lstat(path,&statbuf);
	if(S_ISDIR(statbuf.st_mode)){
		csiebox_protocol_header header;
  		memset(&header, 0, sizeof(header));
  		header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  		header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
  		header.res.datalen = 0;
  		header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
	}
	else{
		uint8_t hash[MD5_DIGEST_LENGTH];
		memset(hash,0,sizeof(hash));
		md5_file(path, hash);
		int same = 1;
		for(int i=0;i<MD5_DIGEST_LENGTH;i++){
			if(hash[i]!=file->message.body.hash[i]){
				same = 0;
				break;
			}
		}
		csiebox_protocol_header header;
  		memset(&header, 0, sizeof(header));
  		header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  		header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
  		header.res.datalen = 0;
		if(same){
			header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
			if(!send_message(conn_fd, &header, sizeof(header))){
    			fprintf(stderr, "send fail\n");
    		}
		}
		else{
			header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
			if(!send_message(conn_fd, &header, sizeof(header))){
    			fprintf(stderr, "send fail\n");
    			return 0;
    		}
    		int *buf = (int *)malloc(sizeof(int));
			recv_message(conn_fd,buf,sizeof(int));
			int datalen = *buf;
			free(buf);
			char* data = (char*)malloc(datalen);
			recv_message(conn_fd,data,datalen);
			FILE *fp;
			fp = fopen(path,"wb");
			fwrite(data,sizeof(char),datalen,fp);
			fclose(fp);
			free(data);
		}
	}
	return 0;
}
int sync_meta(int conn_fd,csiebox_protocol_meta *meta){
	char *path = (char*)malloc((meta->message.body.pathlen)+1);
	recv_message(conn_fd,path,meta->message.body.pathlen);
	path[meta->message.body.pathlen] = '\0';
	//printf("the meta file:  %s\n",path);
	//printf("mode  %d\n",meta->message.body.stat.st_mode);
	chmod(path,meta->message.body.stat.st_mode);
	chown(path,meta->message.body.stat.st_uid,meta->message.body.stat.st_gid);
	struct utimbuf buf = {meta->message.body.stat.st_atime,meta->message.body.stat.st_mtime};
	utime(path,&buf);
	free(path);
	csiebox_protocol_header header;
  	memset(&header, 0, sizeof(header));
  	header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  	header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
  	header.res.datalen = 0;
  	header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
  	if(!send_message(conn_fd, &header, sizeof(header))){
    	fprintf(stderr, "send fail\n");
  	}
  	return 1;
}
int sync_hardlink(int conn_fd,csiebox_protocol_hardlink *hardlink){
	//printf("build hardlink\n");
	int src_len = (hardlink->message.body.srclen);
	int tar_len = (hardlink->message.body.targetlen);
	char *src = (char*)malloc(src_len+1);
	char *tar = (char*)malloc(tar_len+1);
	recv_message(conn_fd,src,src_len);
	recv_message(conn_fd,tar,tar_len);
	src[src_len] = '\0';
	tar[tar_len] = '\0';
	//printf("src %s\ntar %s\n",src,tar);
	link(src,tar);
	csiebox_protocol_header header;
  	memset(&header, 0, sizeof(header));
  	header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  	header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK;
  	header.res.datalen = 0;
  	header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
  	if(!send_message(conn_fd, &header, sizeof(header))){
    	fprintf(stderr, "send fail\n");
	}
	return 0;
}
int sync_rm(int conn_fd,csiebox_protocol_rm *rm){
	char *path = (char*)malloc((rm->message.body.pathlen)+1);
	recv_message(conn_fd,path,rm->message.body.pathlen);
	path[rm->message.body.pathlen] = '\0';
	struct stat statbuf;
    memset(&statbuf, 0, sizeof(struct stat));
    lstat(path,&statbuf);
    if(S_ISDIR(statbuf.st_mode)){
    	rmdir(path);
	}
	else{
		unlink(path);
	}
	csiebox_protocol_header header;
  	memset(&header, 0, sizeof(header));
  	header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  	header.res.op = CSIEBOX_PROTOCOL_OP_RM;
  	header.res.datalen = 0;
  	header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
  	if(!send_message(conn_fd, &header, sizeof(header))){
    	fprintf(stderr, "send fail\n");
	}
	return 0;
}
