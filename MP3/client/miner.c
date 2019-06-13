#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <bsd/md5.h>
#include <string.h>
#include <linux/limits.h>
#include <stdint.h>
enum{
	status = 3,
	quit = 1,
	win = 2,
	lose = 4,
};
struct packet {
    int type;
    int number;
};
struct job{
	int n_tr;
	unsigned char begin;
	unsigned char end;
	MD5_CTX ctx;
};
struct message{
	char name[1024];
	int append_len;
	unsigned char append[128];
	int n_tre;
};

int main(int argc, char **argv)
{
    /* parse arguments */
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }

    char *name = argv[1];
    char *input_pipe = argv[2];
    char *output_pipe = argv[3];

    /* create named pipes */
    int ret;
    ret = mkfifo(input_pipe, 0644);
    assert (!ret);

    ret = mkfifo(output_pipe, 0644);
    assert (!ret);

    /* open pipes */
    int output_fd = open(output_pipe, O_WRONLY);
    assert (output_fd >= 0);
    
    int input_fd = open(input_pipe, O_RDONLY);
    assert (input_fd >= 0);

    
	fprintf(stdout,"BOSS is mindful.\n");
	
    /* TODO write your own (1) communication protocol (2) computation algorithm */
    /* To prevent from blocked read on input_fd, try select() or non-blocking I/O */
    struct job job;
    memset(&job,0,sizeof(struct job));
    read(input_fd,&job,sizeof(struct job));
    unsigned char append[128] = {0};
    append[0] = job.begin;
    int append_lev = 0;
    
	struct timeval tv;
    fd_set readset;

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    
    int time_check = 10,time_cnt = 0;
    uint8_t ans[MD5_DIGEST_LENGTH] = {};
	while(1){
		if(time_cnt==time_check){
			time_cnt=0;
			FD_ZERO(&readset);
    		FD_SET(input_fd, &readset);
    		int retval = select(input_fd + 1, &readset, NULL, NULL, &tv);
    		/* Be aware that readset can be modified by select(). Do not select() it twice! */

    		if (retval == -1){
        		perror("select()");
    		}
    		else if (FD_ISSET(input_fd, &readset)){
    			struct packet pack;
    			memset(&pack,0,sizeof(struct packet));
        		read(input_fd,&pack,sizeof(struct packet));
        		switch(pack.type){
					case status:{
						fprintf(stdout,"I'm working on ");
						for(int i=0;i<16;i++){
							fprintf(stdout,"%02x",ans[i]);
						}
						fprintf(stdout,"\n");
						break;
					}
					case lose:{
						uint8_t winner_md5[MD5_DIGEST_LENGTH] = {};
						struct message mes;
						read(input_fd,&mes,sizeof(struct message));
						read(input_fd,winner_md5,MD5_DIGEST_LENGTH);//****
						fprintf(stdout,"%s wins a %d-treasure! ",mes.name,mes.n_tre);
						for(int i=0;i<16;i++){
							fprintf(stdout,"%02x",winner_md5[i]);
						}
						fprintf(stdout,"\n");
						memset(&job,0,sizeof(struct job));
    					read(input_fd,&job,sizeof(struct job));
    					//fprintf(stderr,"job:find %d-tre\n",job.n_tr);
						
    					memset(append,0,sizeof(append));
    					append[0] = job.begin;
    					append_lev = 0;
						continue;
						//break;
					}
					case quit:{
						fprintf(stdout,"BOSS is at rest.\n");
						close(input_fd);
						close(output_fd);
						
						sleep(1);
    					int output_fd = open(output_pipe, O_WRONLY);
    					assert (output_fd >= 0);
    					
    					int input_fd = open(input_pipe, O_RDONLY);
    					assert (input_fd >= 0);
    					
						memset(&job,0,sizeof(struct job));
    					fprintf(stderr,"read: %d\n",read(input_fd,&job,sizeof(struct job)));
    					fprintf(stdout,"BOSS is mindful.\n");
    					memset(append,0,sizeof(append));
    					append[0] = job.begin;
    					append_lev = 0;
						continue;
						//break;
					}
					case win:{
						/*for(int i=0;i<=append_lev;i++)
							fprintf(stderr,"\\x%02x",append[i]);
						fprintf(stderr,"\n");*/
						fprintf(stdout,"I win a %d-treasure! ",job.n_tr);
						for(int i=0;i<16;i++){
							fprintf(stdout,"%02x",ans[i]);
						}
						fprintf(stdout,"\n");
						memset(&job,0,sizeof(struct job));
    					read(input_fd,&job,sizeof(struct job));
    					/*fprintf(stderr,"job:find %d-tre\n",job.n_tr);
    					
    					MD5_CTX temp = job.ctx;
    					uint8_t temp_MD5[MD5_DIGEST_LENGTH] = {};
    					MD5Final(temp_MD5,&temp);
    					for(int j=0;j<MD5_DIGEST_LENGTH;j++){
							fprintf(stderr,"%02x",temp_MD5[j]);
						}
						fprintf(stderr,"\n");*/
						
    					memset(append,0,sizeof(append));
    					append[0] = job.begin;
    					append_lev = 0;
						continue;
					}
				}
    		}
		}
		MD5_CTX ctx_temp = job.ctx;
		MD5Update(&ctx_temp, (const uint8_t*)append, append_lev+1);
		
		MD5Final(ans, &ctx_temp);//****
		int find = 0;
		for(int i=0;i<16;i++){
			if(ans[i]>15){
				break;
			}
			if(ans[i]==0){
				find+=2;
			}
			else{
				find++;
				break;
			}
		}
		if(find==job.n_tr){
			/*for(int i=0;i<=append_lev;i++)
				fprintf(stderr,"\\x%02x",append[i]);
			fprintf(stderr,"\n");*/
    		//send mes
			
			struct message mes;
			mes.append_len = append_lev+1;
			memcpy(mes.append,append,sizeof(append));
			strcpy(mes.name,name);
			mes.n_tre = job.n_tr;
			write(output_fd,&mes,sizeof(struct message));
			//fprintf(stderr,"write: %d\n",job.n_tr);
			//check win or not
			struct packet pack;
    		memset(&pack,0,sizeof(struct packet));
        	read(input_fd,&pack,sizeof(struct packet));
        	switch(pack.type){
				case status:{
					fprintf(stdout,"I'm working on ");
					for(int i=0;i<16;i++){
						fprintf(stdout,"%02x",ans[i]);
					}
					fprintf(stdout,"\n");
					break;
				}
				case win:{
					/*for(int i=0;i<=append_lev;i++)
						fprintf(stderr,"\\x%02x",append[i]);
					fprintf(stderr,"\n");*/
						fprintf(stdout,"I win a %d-treasure! ",job.n_tr);
						for(int i=0;i<16;i++){
							fprintf(stdout,"%02x",ans[i]);
						}
						fprintf(stdout,"\n");
						memset(&job,0,sizeof(struct job));
    					read(input_fd,&job,sizeof(struct job));
    					/*fprintf(stderr,"job:find %d-tre\n",job.n_tr);
    					
    					MD5_CTX temp = job.ctx;
    					uint8_t temp_MD5[MD5_DIGEST_LENGTH] = {};
    					MD5Final(temp_MD5,&temp);
    					for(int j=0;j<MD5_DIGEST_LENGTH;j++){
							fprintf(stderr,"%02x",temp_MD5[j]);
						}
						fprintf(stderr,"\n");*/
						
    					memset(append,0,sizeof(append));
    					append[0] = job.begin;
    					append_lev = 0;
						continue;
				}
				case quit:{
					fprintf(stdout,"BOSS is at rest.\n");
					close(input_fd);
					close(output_fd);
					
					sleep(1);
    				int output_fd = open(output_pipe, O_WRONLY);
    				assert (output_fd >= 0);
    				int input_fd = open(input_pipe, O_RDONLY);
    				assert (input_fd >= 0);
    				
					memset(&job,0,sizeof(struct job));
    				read(input_fd,&job,sizeof(struct job));
   					fprintf(stdout,"BOSS is mindful.\n");
   					memset(append,0,sizeof(append));
   					append[0] = job.begin;
   					append_lev = 0;						
					continue;
				}
				case lose:{
					uint8_t winner_md5[MD5_DIGEST_LENGTH] = {};
					struct message mes;
					read(input_fd,&mes,sizeof(struct message));
					read(input_fd,winner_md5,MD5_DIGEST_LENGTH);//****
					fprintf(stdout,"%s wins a %d-treasure! ",mes.name,mes.n_tre);
					for(int i=0;i<16;i++){
							fprintf(stdout,"%02x",winner_md5[i]);
					}
					fprintf(stdout,"\n");
					memset(&job,0,sizeof(struct job));
   					read(input_fd,&job,sizeof(struct job));
    				/*fprintf(stderr,"job:find %d-tre\n",job.n_tr);
    				
    				MD5_CTX temp = job.ctx;
    					uint8_t temp_MD5[MD5_DIGEST_LENGTH] = {};
    					MD5Final(temp_MD5,&temp);
    					for(int j=0;j<MD5_DIGEST_LENGTH;j++){
							fprintf(stderr,"%02x",temp_MD5[j]);
						}
					fprintf(stderr,"\n");*/
					
   					memset(append,0,sizeof(append));
    				append[0] = job.begin;
    				append_lev = 0;
					continue;
				}
			}
			memset(&job,0,sizeof(struct job));
    		read(input_fd,&job,sizeof(struct job));
    		memset(append,0,sizeof(append));
    		append[0] = job.begin;
    		append_lev = 0;
    		time_cnt = 0;
			continue;
		}
		int j;
		for(j=append_lev;j>0;j--){
			append[j]++;
			if(append[j]>0)
				break;
		}
		if(j==0){
			append[0]++;
			if(append[0]>job.end||append[0]==0){
				append[0] = job.begin;
				append_lev++;
			}
		}
		time_cnt++;
	///////////////
	}
	close(input_fd);
	close(output_fd);
	unlink(argv[2]);
	unlink(argv[3]);
    return 0;
}
