#include "mycode_cli.h"
#define DIR_S_FLAG (S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))
//client
int conn_fd,fd;
char longest_path[512];
char wd_path[512][512];
hash hardlink_hash;
int fn(const char *file,const struct stat *sb,int flag,struct FTW *s){
	static int lev = 0;
    if(strcmp(file,".")==0)
        return 0;
	if(s->level>lev){
		lev = s->level;
		strcpy(longest_path,file);
		//printf("%s\n",longest_path);
	}
	struct stat statbuf;
    memset(&statbuf, 0, sizeof(struct stat));
    lstat(file,&statbuf);
    csiebox_protocol_header header;
    memset(&header,0,sizeof(header));
    if(statbuf.st_nlink>1){
    	char* source_path;
    //printf("try to get from hash\n");
    	if(get_from_hash(&hardlink_hash, &source_path,statbuf.st_ino)){
    		//printf("hash %s   file  %s\n",source_path,file);
    		csiebox_protocol_hardlink req;
    		memset(&req, 0, sizeof(req));
    		req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    		req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK;
    		req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    		int len = strlen(source_path);
    		req.message.body.srclen = len;
    		req.message.body.targetlen = strlen(file);
    		if(!send_message(conn_fd,&req,sizeof(req))){
    			fprintf(stderr,"hardlink fail\n");
			}
			send_message(conn_fd,source_path,len);
			send_message(conn_fd,file,strlen(file));
			if (recv_message(conn_fd, &header, sizeof(header))) {
    			if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK &&header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      			printf("hardlink ok!\n");
				}
			}
			return 0;
		}
		char *tmp = (char *)malloc(strlen(file));
		strcpy(tmp,file);
		put_into_hash(&hardlink_hash, tmp,statbuf.st_ino);
	}
    memset(&header,0,sizeof(header));
    header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  	header.req.op = CSIEBOX_PROTOCOL_OP_BUILD;
  	header.req.datalen = sizeof(header);
  	if (!send_message(conn_fd, &header, sizeof(header))) {
    		fprintf(stderr, "send fail\n");
    		return 0;
  	}
	int filelen = strlen(file);
  	send_message(conn_fd,&flag, sizeof(flag));
  	send_message(conn_fd,&filelen,sizeof(int));
  	send_message(conn_fd,file,filelen);
    if(flag==FTW_SL){
        char *target = (char*)malloc(512);
        readlink(file,target,sizeof(target));
        target[statbuf.st_size] = '\0';
    printf("symlink %s\n",target);
        char *temp;
        if((temp = strstr(target,"cdir"))!=NULL){
        	if(temp == target&&temp[4]=='/'){
        		temp[0] = 's';
			}
			else if(temp>target&&(*(temp-1))=='/'&&temp[4]=='/'){
				temp[0] = 's';
			}
		}
        int len = strlen(target);
        send_message(conn_fd,&len,sizeof(int));
        send_message(conn_fd,target,strlen(target));
        free(target);
	}
	else if(flag == FTW_D){
		strcpy(wd_path[inotify_add_watch(fd, file, IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY)],file);	
	}
	else if(flag==FTW_F){
		FILE*fp = fopen(file,"rb");
		fseek(fp,0,SEEK_END);
		int datalen = ftell(fp);
		fseek(fp,0,SEEK_SET);
		send_message(conn_fd,&datalen,sizeof(datalen));
		char *data = (char*)malloc(datalen);
		fread(data,sizeof(char),datalen,fp);
		send_message(conn_fd,data,datalen);
		free(data);
		fclose(fp);
	}
	else if(flag == FTW_NS){
			
	}
  	memset(&header, 0, sizeof(header));
  	if (recv_message(conn_fd, &header, sizeof(header))) {
    	if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&header.res.op == CSIEBOX_PROTOCOL_OP_BUILD &&header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      		printf("build ok!\n");
		}
	}
	return 0;
}
int fn_sync_meta(const char *file,struct stat *sb,int flag,struct FTW *s){
	if(strcmp(file,".")==0)
		return 0;

	csiebox_protocol_meta req;
    memset(&req, 0, sizeof(req));
    req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
    req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    req.message.body.pathlen = strlen(file);
    lstat(file, &req.message.body.stat);
    /*char *path_tmp;
    if(get_from_hash(&hardlink_hash, &path_tmp,req.message.body.stat.st_ino)){
    	if(strcmp(path_tmp,file)==0){
    		return 0;
		}
	}*/
    if(!S_ISDIR(req.message.body.stat.st_mode))
    	md5_file(file, req.message.body.hash);


    //send pathlen to server so that server can know how many charachers it should receive
    //Please go to check the samplefunction in server
    if (!send_message(conn_fd, &req, sizeof(req))) {
      	fprintf(stderr, "send fail\n");
      	return 0;
    }
  
    //send path
  	send_message(conn_fd, file, strlen(file));
  	csiebox_protocol_header header;
  	memset(&header, 0, sizeof(header));
  	if (recv_message(conn_fd, &header, sizeof(header))) {
    	if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        	header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META &&
        	header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      		printf("meta ok!\n");
    	}
  	}
    return 0;
}
void longest(){
	FILE *fp = fopen("longestPath.txt","w");
    fprintf(fp,"%s",longest_path);
    fclose(fp);
	csiebox_protocol_header header;
    header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  	header.req.op = CSIEBOX_PROTOCOL_OP_BUILD;
  	header.req.datalen = sizeof(header);
  	send_message(conn_fd,&header,sizeof(header));
  	int flag = FTW_F;
  	send_message(conn_fd,&flag,sizeof(int));
  	char *s = "./longestPath.txt";
  	int len = strlen(s);
  	send_message(conn_fd,&len,sizeof(len));
  	send_message(conn_fd,s,len);
  	fp = fopen(s,"rb");
	fseek(fp,0,SEEK_END);
	int datalen = ftell(fp);
	fseek(fp,0,SEEK_SET);
	send_message(conn_fd,&datalen,sizeof(int));
	char *data = (char*)malloc(datalen);
	fread(data,sizeof(char),datalen,fp);
	send_message(conn_fd,data,datalen);
	free(data);
	fclose(fp);
	memset(&header, 0, sizeof(header));
  	if (recv_message(conn_fd, &header, sizeof(header))) {
    	if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&header.res.op == CSIEBOX_PROTOCOL_OP_BUILD &&header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      		printf("build ok!\n");
		}
	}
	return;
}
int myfunction(csiebox_client* client){
	memset(&hardlink_hash, 0, sizeof(hash));
	if (!init_hash(&hardlink_hash, 1000)) {
    	fprintf(stderr, "hash init err\n");
    	exit(1);
  	}
	conn_fd = client->conn_fd;
	chdir("../cdir");
	fd = inotify_init();
	//add directory "." to watch list with specified events
  	strcpy(wd_path[inotify_add_watch(fd, ".", IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY)],".");
    nftw(".",fn,10000,FTW_PHYS);
    
    //longest path
    longest();
  	
	nftw(".",fn_sync_meta,10000,FTW_DEPTH|FTW_PHYS);
  	
	int length, i = 0;
  	char buffer[EVENT_BUF_LEN];
  	memset(buffer, 0, EVENT_BUF_LEN);


  	while ((length = read(fd, buffer, EVENT_BUF_LEN)) > 0) {
    	i = 0;
    	while (i < length) {
      		struct inotify_event* event = (struct inotify_event*)&buffer[i];
      		printf("event: (%d, %d, %s)\ntype: ", event->wd, strlen(event->name), event->name);
      		if(event->mask & IN_IGNORED){
      			printf("ignored\n");
      			inotify_rm_watch(fd, event->wd);
      			i += EVENT_SIZE + event->len;
      			continue;
			}
			char *path_temp = (char*)malloc(strlen(wd_path[event->wd])+1+strlen(event->name));
			strcpy(path_temp,wd_path[event->wd]);
        	strcat(path_temp,"/");
        	strcat(path_temp,event->name);
      		if (event->mask & IN_CREATE) {
        		printf("create \n");
        	//printf("%s\n",path_temp);
        		csiebox_protocol_header header;
    			header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  				header.req.op = CSIEBOX_PROTOCOL_OP_BUILD;
  				header.req.datalen = sizeof(header);
  				if (!send_message(conn_fd, &header, sizeof(header))) {
    				fprintf(stderr, "send fail\n");
  				}
  				int flag;
        		if(event->mask & IN_ISDIR){
        			strcpy(wd_path[inotify_add_watch(fd, path_temp, IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY)],path_temp);
        			flag = FTW_D;
				}
				else{
					flag = FTW_F;
				}
				send_message(conn_fd,&flag,sizeof(int));
				int len = strlen(path_temp);
				send_message(conn_fd,&len,sizeof(int));
				if(!send_message(conn_fd,path_temp,len))///
					printf("path not send\n");
				if(flag==FTW_F){
        			FILE*fp = fopen(path_temp,"rb");
					fseek(fp,0,SEEK_END);
					int datalen = ftell(fp);
					fseek(fp,0,SEEK_SET);
					send_message(conn_fd,&datalen,sizeof(int));
					char *data = (char*)malloc(datalen);
					fread(data,sizeof(char),datalen,fp);
					send_message(conn_fd,data,datalen);
					free(data);
					fclose(fp);
				}
  				memset(&header, 0, sizeof(header));
  				if (recv_message(client->conn_fd, &header, sizeof(header))) {
    				if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&header.res.op == CSIEBOX_PROTOCOL_OP_BUILD &&header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      					printf("build ok!\n");
					}
				}
				//sync_meta
				csiebox_protocol_meta req;
    			memset(&req, 0, sizeof(req));
    			req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    			req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
    			req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    			req.message.body.pathlen = len;
    			lstat(path_temp, &req.message.body.stat);
    			if(!S_ISDIR(req.message.body.stat.st_mode))
    				md5_file(path_temp, req.message.body.hash);


    			//send pathlen to server so that server can know how many charachers it should receive
    			//Please go to check the samplefunction in server
    			if (!send_message(conn_fd, &req, sizeof(req))) {
      				fprintf(stderr, "send fail\n");
      				return 0;
   				}
  				send_message(conn_fd, path_temp, len);
  				memset(&header, 0, sizeof(header));
  				if (recv_message(conn_fd, &header, sizeof(header))) {
    				if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        				header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META &&
        				header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      					printf("meta ok!\n");
    				}
  				}
      		}
      		if (event->mask & IN_DELETE) {
       			printf("delete \n");
       			csiebox_protocol_rm req;
       			req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    			req.message.header.req.op = CSIEBOX_PROTOCOL_OP_RM;
    			req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    			req.message.body.pathlen = strlen(path_temp);
    			if (!send_message(conn_fd, &req, sizeof(req))) {
      				fprintf(stderr, "send fail\n");
      				return 0;
    			}
    			send_message(conn_fd, path_temp, strlen(path_temp));
  				csiebox_protocol_header header;
  				memset(&header, 0, sizeof(header));
  				if (recv_message(conn_fd, &header, sizeof(header))) {
    				if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        				header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META &&
        				header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      					printf("rm ok!\n");
    				}
  				}
      		}
      		if (event->mask & IN_ATTRIB) {
        		printf("attrib \n");
        		csiebox_protocol_meta req;
    			req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    			req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
    			req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    			req.message.body.pathlen = strlen(path_temp);
    			lstat(path_temp, &req.message.body.stat);
    			if(!S_ISDIR(req.message.body.stat.st_mode))
    				md5_file(path_temp, req.message.body.hash);
    			if (!send_message(conn_fd, &req, sizeof(req))) {
      				fprintf(stderr, "send fail\n");
      				return 0;
    			}
  
    			//send path
  				send_message(conn_fd, path_temp, strlen(path_temp));
  				csiebox_protocol_header header;
  				memset(&header, 0, sizeof(header));
  				if (recv_message(conn_fd, &header, sizeof(header))) {
    				if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        				header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META &&
        				header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      					printf("meta ok!\n");
    				}
  				}
      		}
      		if (event->mask & IN_MODIFY) {
        		printf("modify \n");
        		csiebox_protocol_file req;
        		req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    			req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
    			req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    			req.message.body.pathlen = strlen(path_temp);
    			struct stat statbuf;
    			lstat(path_temp, &statbuf);
    			if(!S_ISDIR(statbuf.st_mode))
    				md5_file(path_temp, req.message.body.hash);
    			if (!send_message(conn_fd, &req, sizeof(req))) {
      				fprintf(stderr, "send fail\n");
      				return 0;
    			}
    			send_message(conn_fd, path_temp, strlen(path_temp));
    			csiebox_protocol_header header;
    			memset(&header,0,sizeof(header));
    			if (recv_message(conn_fd, &header, sizeof(header))) {
    				if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        				header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_FILE &&
        				header.res.status == CSIEBOX_PROTOCOL_STATUS_MORE) {
      					printf("send more!\n");
      					
      					FILE*fp = fopen(path_temp,"rb");
						fseek(fp,0,SEEK_END);
						int datalen = ftell(fp);
						fseek(fp,0,SEEK_SET);
						send_message(conn_fd,&datalen,sizeof(int));
						char *data = (char*)malloc(datalen);
						fread(data,sizeof(char),datalen,fp);
						send_message(conn_fd,data,datalen);
						free(data);
						fclose(fp);
    				}
  				}
  				csiebox_protocol_meta req2;
    			req2.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    			req2.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
    			req2.message.header.req.datalen = sizeof(req2) - sizeof(req2.message.header);
    			req2.message.body.pathlen = strlen(path_temp);
    			lstat(path_temp, &req2.message.body.stat);
    			if(!S_ISDIR(req2.message.body.stat.st_mode))
    				md5_file(path_temp, req2.message.body.hash);
    			if (!send_message(conn_fd, &req, sizeof(req))) {
      				fprintf(stderr, "send fail\n");
      				return 0;
    			}
  
    			//send path
  				send_message(conn_fd, path_temp, strlen(path_temp));
  				memset(&header, 0, sizeof(header));
  				if (recv_message(conn_fd, &header, sizeof(header))) {
    				if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        				header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META &&
        				header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      					printf("meta ok!\n");
    				}
  				}
      		}
      		free(path_temp);
      		i += EVENT_SIZE + event->len;
    	}
    memset(buffer, 0, EVENT_BUF_LEN);
  	}

  	//inotify_rm_watch(fd, wd);
  	close(fd);
}
