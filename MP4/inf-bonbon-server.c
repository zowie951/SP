#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "cJSON.h"

enum status{
	QUIT = 0,
	FD1_DISCONNECT = 1,
	FD2_DISCONNECT = 2
};
typedef struct User{
	char name[33];
	unsigned int age;
	char gender[7];
	char introduction[1025];
}User;

typedef struct wait_list{
	struct wait_list *next,*before;
	int fd;
	User user;
}Wait_list;
Wait_list *root = NULL;//root
Wait_list *end = NULL;

typedef struct try_list{
	struct try_list *next,*before;
	int fd;
	User user;
	Wait_list *next_try,*myself;
}Try_list;
Try_list *try_root = NULL,*try_end = NULL;

typedef struct chat_pid_list{
	struct chat_pid_list *next,*before;
	int pid,fd1,fd2;
}Chat_pid_list;
Chat_pid_list *chat_root = NULL;
Chat_pid_list *chat_end = NULL;
char buf_data[1500][6000];
int buf_rest[1500] = {};//record incomplete data size
int match(int readfd,int writefd){
	int maxfd;
    fd_set readset;
    fd_set working_readset;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 300000;

    FD_ZERO(&readset);
    FD_SET(readfd, &readset);
    maxfd = readfd + 1;
    
    int tryer_size = 0,waiter_size = 0;
    while(1){
    	memcpy(&working_readset, &readset, sizeof(readset)); 
        if(select(maxfd, &working_readset, NULL, NULL, &timeout)==-1){
        	fprintf(stderr,"select fail!\n");
        	exit(EXIT_FAILURE);
		}
		if(FD_ISSET(readfd,&working_readset)){
			Wait_list *waiter = (Wait_list*)malloc(sizeof(Wait_list));
			if(read(readfd,waiter,sizeof(Wait_list))==0){
				close(readfd);
				fprintf(stderr,"match pipe err!\n");
				sleep(10);
				exit(0);
			}
			//fprintf(stderr,"match process: waiter->fd: %d!\n",waiter->fd);
			if(waiter->fd>=1000000){
				waiter->fd -= 1000000;
				Try_list *try_ptr = try_root;
				while(try_ptr!=NULL){
					if(try_ptr->fd==waiter->fd){
						if(try_ptr->before!=NULL){
							try_ptr->before->next = try_ptr->next;
						}
						else
							try_root = try_ptr->next;
						if(try_ptr->next!=NULL){
							try_ptr->next->before = try_ptr->before;
						}
						else
							try_end = try_ptr->before;
						break;
					}
					try_ptr = try_ptr->next;
				}
				Wait_list *wait_ptr = root;
				while(wait_ptr!=NULL){
					if(wait_ptr->fd==waiter->fd){
						if(wait_ptr->before!=NULL){
							wait_ptr->before->next = wait_ptr->next;
						}
						else
							root = wait_ptr->next;
						if(wait_ptr->next!=NULL){
							wait_ptr->next->before = wait_ptr->before;
						}
						else
							end = wait_ptr->before;
						break;
					}
					wait_ptr=wait_ptr->next;
				}
				continue;
			}
			if(end==NULL){
				root = waiter;
				end = waiter;
				waiter->before = NULL;
				waiter->next = NULL;
			}
			else{
				end->next = waiter;
				waiter->before = end;
				end = waiter;
			}
			
			Try_list *tryer = (Try_list*)malloc(sizeof(Try_list));
			tryer->fd = waiter->fd;
			tryer->myself = waiter;
			tryer->next_try = root;
			tryer->user = waiter->user;
			tryer->next = NULL;
			if(try_end==NULL){
				try_root = tryer;
				try_end = tryer;
				tryer->before = NULL;
			}
			else{
				try_end->next = tryer;
				tryer->before = try_end;
				try_end = tryer;
			}
			tryer_size++;
			waiter_size++;
		}
		Try_list *loc = try_root;
		int pid_list[2048],pid_size = 0;
		while(loc!=NULL){
			if(loc->next_try == loc->myself){
				try_root = loc->next;
				if(try_root==NULL)
					try_end = NULL;
				Try_list *temp = loc;
				loc = loc->next;
				free(temp);
				continue;
			}
			int pid = fork();
			if(pid<0){
				fprintf(stderr,"match fork fail!\n");
				exit(1);
			}
			else if(pid==0){//child
				char lib1[64]="./",lib2[64]="./";
				sprintf(lib1+2,"%d",loc->fd);
				sprintf(lib2+2,"%d",loc->next_try->fd);
				//fprintf(stderr,"       name %s\n",loc->next_try->user.name);
				strcat(lib1,"lib.so");
				strcat(lib2,"lib.so");
				//fprintf(stderr,"%s\n",lib2);
				//sleep(5);
				void* handle1 = dlopen(lib1, RTLD_LAZY);
				assert(NULL != handle1);
				void* handle2 = dlopen(lib2, RTLD_LAZY);
				fprintf(stderr,"lib2 = %s\n",lib2);
				assert(NULL != handle2);
				
				int (*func1)(struct User user) = (int (*)(struct User user)) dlsym(handle1, "filter_function");
				int (*func2)(struct User user) = (int (*)(struct User user)) dlsym(handle2, "filter_function");
				const char *dlsym_error = dlerror();
    			if (dlsym_error){
        			fprintf(stderr, "Cannot load function!\n");
        			dlclose(handle1);
        			dlclose(handle2);
    			}
    			int match1 = func1(loc->next_try->user);
    			int match2 = func2(loc->user);
    			dlclose(handle1);
    			dlclose(handle2);
    			if(match1!=0&&match2!=0){
    				exit(1);
				}
				exit(0);
			}
			pid_list[pid_size] = pid;
			pid_size++;
			loc = loc->next;
		}
		//check have match or not
		loc = try_root;
		for(int i=0;i<pid_size/*&&loc!=NULL*/;i++){
			int status;
			waitpid(pid_list[i],&status,0);
			if(WIFEXITED(status)){
				if(WEXITSTATUS(status)==1){
					/*int pair_fd[2];
					pair_fd[0] = loc->fd;
					pair_fd[1] = loc->next_try->fd;
					fprintf(stderr,"pair1: %d pair2: %d\n",pair_fd[0],pair_fd[1]);*/
					write(writefd,loc->myself,sizeof(Wait_list));
					write(writefd,loc->next_try,sizeof(Wait_list));
					//delete from two list
					if(loc->before!=NULL)
						loc->before->next = loc->next;
					else{
						try_root = loc->next;
						/*if(try_root==NULL)
							try_end = NULL;*/
					}
					if(loc->next!=NULL)
						loc->next->before = loc->before;
					else
						try_end = loc->before;
					if(loc->myself->before!=NULL)
						loc->myself->before->next = loc->myself->next;
					else
						root = loc->myself->next;
					if(loc->myself->next!=NULL)
						loc->myself->next->before = loc->myself->before;
					else
						end = loc->myself->before;
					
					if(loc->next_try->before!=NULL)
						loc->next_try->before->next = loc->next_try->next;
					else{
						root = loc->next_try->next;
						/*if(root==NULL)
							end = NULL;*/
					}
					if(loc->next_try->next!=NULL)
						loc->next_try->next->before = loc->next_try->before;
					else
						end = loc->next_try->before;
					
					//free(loc->myself);
					//free(loc->next_try);
					Try_list *temp = loc;
					loc = loc->next;
					free(temp);
					continue;
				}
			}
			loc->next_try = loc->next_try->next;
			loc = loc->next;
		}
	}
	
	return 0;
}

fd_set readset;

int chat(int fd1,int fd2){
	fd_set chat_readset;
	fd_set workset;
	FD_ZERO(&chat_readset);
	FD_SET(fd1,&chat_readset);
	FD_SET(fd2,&chat_readset);
	int maxfd;
	struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
	if(fd2>fd1){
		maxfd = fd2+1;
	}
	else
		maxfd = fd1 +1;
	int fd[2];
	fd[0] = fd1;
	fd[1] = fd2;
	char buf_last[2][6000];
	int last_size[2]={};
	while(1){
		memcpy(&workset, &chat_readset, sizeof(chat_readset));
        if(select(maxfd, &workset, NULL, NULL, &timeout)==-1){
        	fprintf(stderr,"chat select fail!\n");
        	exit(EXIT_FAILURE);
		}
		for(int i=0;i<2;i++){
			if(FD_ISSET(fd[i],&workset)){
				char buff[6000]={},cmd[32]={};
				int buff_len = recv(fd[i],buff,sizeof(buff),0);
				if(buff_len == 0){
					FD_CLR(fd1,&readset);
					FD_CLR(fd2,&readset);
					fprintf(stderr,"client%d disconnect!\n",i);
					char req[32] = "{\"cmd\":\"other_side_quit\"}\n";
					if(i==0)
						send(fd2,req,strlen(req),0);
					else
						send(fd1,req,strlen(req),0);
					close(fd1);
					close(fd2);
					if(i==0)
						exit(FD1_DISCONNECT);
					else
						exit(FD2_DISCONNECT);
				}
				char* newline_pos=buff,*last = buff;
				while((newline_pos = strchr(newline_pos,'\n'))!=NULL){
				
				int data_len = newline_pos - last;
				buff_len -= data_len+1;
				char buf[6000];
				if(last_size[i]!=0){
					strncpy(buf,buf_last[i],last_size[i]);
					strncpy(buf+last_size[i],last,data_len);
					data_len += last_size[i];
					last_size[i] = 0;
				}
				else
					strncpy(buf,last,data_len);
				buf[data_len] = '\0';
				cJSON * root = cJSON_Parse(buf);
				cJSON *cmd_json = cJSON_GetObjectItemCaseSensitive(root, "cmd");
				strcpy(cmd,cmd_json->valuestring);
				if(!strcmp(cmd,"send_message")){
					buf[data_len] = '\n';
					send(fd[i],buf,data_len+1,0);//respond client
					buf[data_len] = '\0';
					cJSON *mes_json = cJSON_GetObjectItemCaseSensitive(root, "message");
					cJSON *seq_json = cJSON_GetObjectItemCaseSensitive(root, "sequence");
					char req_buf[2048];
					sprintf(req_buf,"{\"cmd\":\"receive_message\",\"message\":\"%s\",\"sequence\":%d}\n",
					mes_json->valuestring,seq_json->valuedouble);
					if(i==0)
						send(fd2,req_buf,strlen(req_buf),0);
					else
						send(fd1,req_buf,strlen(req_buf),0);
				}
				else if(!strcmp(cmd,"quit")){
					char *res = "{\"cmd\":\"quit\"}\n";
					send(fd[i],res,strlen(res),0);//respond client
					char req[32] = "{\"cmd\":\"other_side_quit\"}\n";
					if(i==0)
						send(fd2,req,strlen(req),0);
					else
						send(fd1,req,strlen(req),0);
					close(fd1);
					close(fd2);
					exit(QUIT);
				}
				last = ++newline_pos;
				
				}
				if(buff_len>0){
					last_size[i] = buff_len;
					strncpy(buf_last[i],last,buff_len);
				}
			}
		}
		
	}
	return 0;
}
int maxfd;
void sig_handle(int sig){
	int status,pid;
	pid = waitpid(0,&status,WNOHANG);
	fprintf(stderr,"pid = %d\n",pid);
	if(pid==0)
		return;
	Chat_pid_list *loc = chat_root;
	while(loc!=NULL){
		if(loc->pid==pid)
			break;
		loc = loc->next;
	}
	if(loc==NULL){
		fprintf(stderr,"no pid in chat list!\n");
		exit(0);
	}
	if(!WIFEXITED(status)){
		fprintf(stderr,"chatroom broken!\n");
		exit(0);
	}
	int value = WEXITSTATUS(status);
	if(value==QUIT){
		FD_SET(loc->fd1,&readset);
		FD_SET(loc->fd2,&readset);
		if(loc->fd1>=maxfd)
			maxfd = loc->fd1;
		if(loc->fd2>=maxfd)
			maxfd = loc->fd2;
	}
	else if(value==FD1_DISCONNECT){
		FD_SET(loc->fd2,&readset);
		if(loc->fd2>=maxfd)
			maxfd = loc->fd2+1;
		close(loc->fd1);
	}
	else if(value==FD2_DISCONNECT){
		FD_SET(loc->fd1,&readset);
		if(loc->fd1>=maxfd)
			maxfd = loc->fd1+1;
		close(loc->fd2);
	}
	else{
		fprintf(stderr,"unexpected exit value %d!\n",value);
		exit(0);
	}
	if(loc->before!=NULL){
		loc->before->next = loc->next;
	}
	else
		chat_root = loc->next;
	if(loc->next!=NULL)
		loc->next->before = loc->before;
	else
		chat_end = loc->before;
	free(loc);
	return;
}
int main(int argc, char *argv[]){
	if(argc<2){
		fprintf(stderr,"no port!\n");
		exit(1);
	}
	int port = atoi(argv[1]);
	if(port>65536||port<0){
		fprintf(stderr,"invalid port!\n");
		exit(1);
	}
	
	int waiterfd[2],pairfd[2];
	pipe(waiterfd);
	pipe(pairfd);
	int match_pid = fork();
	if(match_pid<0){
		fprintf(stderr,"match_pid fail!\n");
		exit(1);
	}
	else if(match_pid==0){
		close(waiterfd[1]);
		close(pairfd[0]);
		match(waiterfd[0],pairfd[1]);		
		exit(1);
	}
	close(waiterfd[0]);
	close(pairfd[1]);
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	assert(sockfd >= 0);

	// 利用 struct sockaddr_in 設定伺服器"自己"的位置
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr)); // 清零初始化，不可省略
	server_addr.sin_family = PF_INET;              // 位置類型是網際網路位置
	server_addr.sin_addr.s_addr = INADDR_ANY;      // INADDR_ANY 是特殊的IP位置，表示接受所有外來的連線
	server_addr.sin_port = htons(port);
	
// 使用 bind() 將伺服socket"綁定"到特定 IP 位置
	int retval = bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr));
	if(retval==-1){
		fprintf(stdout,"socket fail\n");
		exit(1);
	}

		// 呼叫 listen() 告知作業系統這是一個伺服socket
	retval = listen(sockfd, 5);
	assert(!retval);
	
    fd_set working_readset;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 300000;

    FD_ZERO(&readset);
    FD_SET(sockfd, &readset);
    //FD_SET(waiterfd[1], &readset);
    FD_SET(pairfd[0], &readset);
    maxfd = sockfd + 1;
    
    signal (SIGCHLD,sig_handle);//ignore child signal
    //int chat_pair[2048] = {};
    fprintf(stderr,"success\n");
	while (1)
	{
    // 呼叫 accept() 會 block 住程式直到有新連線進來，回傳值代表新連線的描述子
    	memcpy(&working_readset, &readset, sizeof(readset)); // why we need memcpy() here?
        if(select(maxfd, &working_readset, NULL, NULL, &timeout)==-1){
        	if(errno == EINTR)
        		continue;
        	perror("select failed!");
        	fprintf(stderr,"select fail!\n");
        	exit(EXIT_FAILURE);
		}
		for(int i=1;i<maxfd;i++){
			if(!FD_ISSET(i,&working_readset))
				continue;
			if(i==pairfd[0]){
				//handle the matched pair
				Wait_list *waiter1 = (Wait_list*)malloc(sizeof(Wait_list));
				Wait_list *waiter2 = (Wait_list*)malloc(sizeof(Wait_list));
				read(pairfd[0],waiter1,sizeof(Wait_list));
				read(pairfd[0],waiter2,sizeof(Wait_list));
				fprintf(stderr,"match fd: %d %d\n",waiter1->fd,waiter2->fd);
				int pairfd[2];
				pairfd[0] = waiter1->fd;
				pairfd[1] = waiter2->fd;
				
					char file[32],func[4097]={},file2[32];
					
					sprintf(file,"%dfunc.c",pairfd[1]);
					/*int file_fd = open(file,O_RDONLY);
					if(file_fd==-1){
						fprintf(stderr,"open fail\n");
						exit(0);
					}
					int file_size = lseek(file_fd,0,SEEK_END);
					fprintf(stderr,"file size:%d\n",file_size);
					file_size -= 20;
					lseek(file_fd,20,SEEK_SET);
					read(file_fd,func,file_size);
					close(file_fd);*/
					FILE *fp = fopen(file,"rb");
					fseek(fp,0,SEEK_END);
					int file_size = ftell(fp);
					//fprintf(stderr,"file size:%d\n",file_size);
					file_size -= 84;
					fseek(fp,84,SEEK_SET);
					fread(func,sizeof(char),file_size,fp);
					fclose(fp);
					//fprintf(stderr,"func: %s\n",func);
					//sleep(5);
					
					
					char req_buf[6000];
					sprintf(req_buf,"{\"cmd\":\"matched\",\"name\":\"%s\",\"age\":%u,\"gender\":\"%s\",\"introduction\":\"%s\",\"filter_function\":\"%s\"}\n",waiter2->user.name,waiter2->user.age,waiter2->user.gender,waiter2->user.introduction,func);

					fprintf(stderr,"send mes: %s",req_buf);
					send(pairfd[0],req_buf,strlen(req_buf),0);
					
					sprintf(file2,"%dfunc.c",pairfd[0]);
					/*file_fd = open(file2,O_RDONLY);
					file_size = lseek(file_fd,0,SEEK_END);
					file_size -= 20;
					lseek(file_fd,20,SEEK_SET);
					read(file_fd,func,file_size);
					close(file_fd);*/
					fp = fopen(file2,"rb");
					fseek(fp,0,SEEK_END);
					file_size = ftell(fp);
					//fprintf(stderr,"file size:%d\n",file_size);
					file_size -= 84;
					fseek(fp,84,SEEK_SET);
					fread(func,sizeof(char),file_size,fp);
					fclose(fp);
					//fprintf(stderr,"func: %s\n",func);
					//sleep(5);
					sprintf(req_buf,"{\"cmd\":\"matched\",\"name\":\"%s\",\"age\":%u,\"gender\":\"%s\",\"introduction\":\"%s\",\"filter_function\":\"%s\"}\n",waiter1->user.name,waiter1->user.age,waiter1->user.gender,waiter1->user.introduction,func);
					fprintf(stderr,"send mes: %s",req_buf);
					send(pairfd[1],req_buf,strlen(req_buf),0);
		
				int pid = fork();
				if(pid<0){
					fprintf(stderr,"chat room fork fail!\n");
					exit(0);
				}
				else if(pid == 0){
					close(sockfd);
					chat(pairfd[0],pairfd[1]);
					exit(0);
				}
				Chat_pid_list *chater = (Chat_pid_list*)malloc(sizeof(Chat_pid_list));
				chater->fd1 = pairfd[0];
				chater->fd2 = pairfd[1];
				chater->pid = pid;
				if(chat_end==NULL){
					chat_root = chater;
					chat_end = chater;
					chater->before=NULL;
					chater->next=NULL;
				}
				else{
					chat_end->next = chater;
					chater->before = chat_end;
					chater->next = NULL;
					chat_end = chater;
				}
				FD_CLR(pairfd[0],&readset);
				FD_CLR(pairfd[1],&readset);
			}
			else if(i==sockfd){
				fprintf(stderr,"new client!\n");
				struct sockaddr_in client_addr;
    			socklen_t addrlen = sizeof(client_addr);
   	 			int client_fd = accept(sockfd, (struct sockaddr*) &client_addr, &addrlen);
   	 			assert(client_fd >= 0);
   	 			FD_SET(client_fd,&readset);
   	 			if(client_fd >= maxfd)
   	 				maxfd = client_fd +1;
			}
			else{
				char buff[6000]={},cmd[32]={};
				int buff_len = recv(i,buff,sizeof(buff),0);
				if(buff_len == 0){
					Wait_list *waiter = (Wait_list*)malloc(sizeof(Wait_list));
					waiter->fd = i+1000000;
					write(waiterfd[1],waiter,sizeof(Wait_list));
					free(waiter);
					FD_CLR(i,&readset);
					fprintf(stderr,"client disconnect!\n");
					close(i);
					buf_rest[i] = 0;
					continue;
				}
				//fprintf(stderr,"rec %s\n",buf);
				char* newline_pos=buff,*last = buff;
				while((newline_pos = strchr(newline_pos,'\n'))!=NULL){
				char buf[6000];
				int data_len = newline_pos - last;
				buff_len -= data_len+1;
				if(buf_rest[i]!=0){
					strncpy(buf,buf_data[i],buf_rest[i]);
					strncpy(buf+buf_rest[i],last,data_len);
					data_len += buf_rest[i];
					buf_rest[i] = 0;
				}
				else
					strncpy(buf,last,data_len);
				buf[data_len] = '\0';
				cJSON * root = cJSON_Parse(buf);
				cJSON *cmd_json = cJSON_GetObjectItemCaseSensitive(root, "cmd");
				if(!cJSON_IsString(cmd_json)){
					fprintf(stderr,"cmd is not string!\n");
					exit(1);
				}
				strcpy(cmd,cmd_json->valuestring);
				fprintf(stderr,"get cmd: %s\n",cmd);
				
				if(!strcmp(cmd,"try_match")){
					char res[32] = "{\"cmd\":\"try_match\"}\n";
					send(i,res,strlen(res),0);//respond client
					//cJSON *name_json,*age_json,*gender_json,*intro_json,*func_json;
					cJSON *name_json = cJSON_GetObjectItemCaseSensitive(root, "name");
					cJSON *age_json = cJSON_GetObjectItemCaseSensitive(root, "age");
					cJSON *gender_json = cJSON_GetObjectItemCaseSensitive(root, "gender");
					cJSON *intro_json = cJSON_GetObjectItemCaseSensitive(root, "introduction");
					cJSON *func_json = cJSON_GetObjectItemCaseSensitive(root, "filter_function");
					
					Wait_list *waiter = (Wait_list*)malloc(sizeof(Wait_list));
					waiter->user.age = (unsigned int)age_json->valuedouble;
					strcpy(waiter->user.name,name_json->valuestring);
					strcpy(waiter->user.gender,gender_json->valuestring);
					strcpy(waiter->user.introduction,intro_json->valuestring);
					waiter->next = NULL;
					waiter->fd = i;
					
					char func[4097];
					strcpy(func,func_json->valuestring);
					//fprintf(stderr,"func: %s\nget: %s",func_json->valuestring,func);
					//sleep(30);
					//build lib
					char func_file[32];
					sprintf(func_file,"%d",i);
					strcat(func_file,"func.c");
					char header[] = "struct User{char name[33];unsigned int age;char gender[7];char introduction[1025];};";
					int fd = open(func_file,O_CREAT|O_WRONLY|O_TRUNC,0666);
					write(fd,header,sizeof(header)-1);
					write(fd,func,strlen(func));
					close(fd);
					char lib_name[64];
					sprintf(lib_name,"%d",i);
					strcat(lib_name,"lib.so ");
					char bash_cmd[64] = "gcc -fPIC -shared -o ";
					strcat(bash_cmd,lib_name);
					strcat(bash_cmd,func_file);
					fprintf(stderr," system %s\n",bash_cmd);
					//sleep(10);
					system(bash_cmd);
					
					write(waiterfd[1],waiter,sizeof(Wait_list));
					free(waiter);
				}
				/*else if(!strcmp(cmd,"send_message")){
					buf[data_len] = '\n';
					send(i,buf,data_len+1,0);//respond client
					buf[data_len] = '\0';
					cJSON *mes_json = cJSON_GetObjectItemCaseSensitive(root, "message");
					cJSON *seq_json = cJSON_GetObjectItemCaseSensitive(root, "sequence");
					
					cJSON *req_json = cJSON_CreateObject();
					cJSON_AddStringToObject(req_json, "cmd", "receive_message");
					cJSON_AddStringToObject(req_json, "message", mes_json->valuestring);
					cJSON_AddNumberToObject(req_json, "sequence", seq_json->valuedouble);
					char *req = cJSON_Print(req_json);
					char req_buf[6000];
					strcpy(req_buf,req);
					int req_len = strlen(req);
					req_buf[req_len] = '\n';
					req_buf[req_len+1] = '\0';
					fprintf(stderr,"send mes: %s",req_buf);
					send(chat_pair[i],req_buf,req_len+1,0);
					cJSON_Delete(req_json);
				}*/
				else if(!strcmp(cmd,"quit")){
					char *res = "{\"cmd\":\"quit\"}\n";
					send(i,res,strlen(res),0);//respond client
					Wait_list *waiter = (Wait_list*)malloc(sizeof(Wait_list));
					waiter->fd = i+1000000;
					write(waiterfd[1],waiter,sizeof(Wait_list));
					free(waiter);
					//FD_CLR(i,&readset);
					//close(i);
				}
				last = ++newline_pos;
				}
				if(buff_len>0){
					buf_rest[i] = buff_len;
					strncpy(buf_data[i],last,buff_len);
				}
			}
		}
    	

    /* 注意實作上細節，呼叫 accept() 時 addrlen 必須是 client_addr 結構的長度
       accept() 回傳之後會將客戶端的 IP 位置填到 client_addr
       並將新的 client_addr 結構長度填寫到 addrlen                        */
    
	}

	// 結束程式前記得關閉 sockfd
	close(sockfd);	
	return 0;
}
