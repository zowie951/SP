#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <bsd/md5.h>
#include <stdint.h>
#include <sys/wait.h>

#include "boss.h"
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

int load_config_file(struct server_config *config, char *path)
{
    /* TODO finish your own config file parser */
    FILE *fp = fopen(path,"r");
    char str[8];
    config->pipes = (struct pipe_pair*)malloc(sizeof(struct pipe_pair)*8);
    int cnt = 0;
    while(fscanf(fp,"%s",str)==1){
    //printf("str = %s\n",str);
    	if(strcmp(str,"MINE:")==0){
    		config->mine_file = (char *)malloc(PATH_MAX);
    		fscanf(fp,"%s",config->mine_file);
    		fprintf(stderr,"%s\n",config->mine_file);
		}
		else if(strcmp(str,"MINER:")==0){
			config->pipes[cnt].input_pipe = (char*)malloc(PATH_MAX);
			config->pipes[cnt].output_pipe = (char*)malloc(PATH_MAX);
			fscanf(fp,"%s%s",config->pipes[cnt].input_pipe,config->pipes[cnt].output_pipe);
			//printf("%s %s\n",config->pipes[cnt].input_pipe,config->pipes[cnt].output_pipe);
			cnt++;
		}
		else{
			fprintf(stderr,"config wrong!\n");
			exit(1);
		}
	}
	config->num_miners = cnt;
	fclose(fp);
    return 0;
}

int assign_jobs(struct server_config *config,MD5_CTX ctx,struct fd_pair *client_fds,int find_n)
{
    /* TODO design your own (1) communication protocol (2) job assignment algorithm */
    unsigned char begin = 0;
    int divide = 256/config->num_miners;
    unsigned char end = divide-1;
    int i;
    for(i=0;i<config->num_miners-1;i++){
    	struct job job;
    	job.n_tr = find_n;
    	memcpy(&job.ctx,&ctx,sizeof(MD5_CTX));
    	job.begin = begin;
    	job.end = end;
    	begin = end+1;
    	end += divide;
    	write(client_fds[i].input_fd,&job,sizeof(struct job));
	}
	struct job job;
    job.n_tr = find_n;
    memcpy(&job.ctx,&ctx,sizeof(MD5_CTX));
    job.begin = begin;
    job.end = 255;
	write(client_fds[i].input_fd,&job,sizeof(struct job));
	return 1;
}

int handle_command(int best_n,uint8_t treasure[MD5_DIGEST_LENGTH],int bytes,struct server_config *config,struct fd_pair *client_fds,unsigned char *mine_base,int mine_base_len)
{
    /* TODO parse user commands here */
    char cmd[16]; /* command string */
    fscanf(stdin,"%s",cmd);

    if (strcmp(cmd, "status")==0)
    {
        /* TODO show status */
        fprintf(stdout,"best %d-treasure ",best_n);
        for(int i=0;i<MD5_DIGEST_LENGTH&&best_n>0;i++){
		fprintf(stdout,"%02x",treasure[i]);
	}
		fprintf(stdout," in %d bytes\n",bytes);
		if(bytes==0)
			return 0;
        struct packet pac;
        pac.type = status;
        pac.number = 0;
        for(int i=0;i<config->num_miners;i++){
        	write(client_fds[i].input_fd,&pac,sizeof(struct packet));
		}
    }
    else if (strcmp(cmd, "dump")==0)
    {
        /* TODO write best n-treasure to specified file */
        char dump_path[PATH_MAX] = {};
        fscanf(stdin,"%s",dump_path);
        
        int pid = fork();
        //fprintf(stderr,"fork: %d\n",pid);
        if(pid==0){
        	for(int i=0;i<config->num_miners;i++){
        		close(client_fds[i].input_fd);
				close(client_fds[i].output_fd);
			}
        	int dump_file = open(dump_path,O_WRONLY|O_TRUNC|O_CREAT|O_SYNC,0666);
        	assert (dump_file >= 0);
        	write(dump_file,mine_base,bytes);
        /*for(int i=0;i<mine_base_len;i++)
        	fprintf(stderr,"\\x%02x",mine_base[i]);
        fprintf(stderr,"\n");*/
        	close(dump_file);
        	exit(0);
    	}
    }
    else
    {
        assert(strcmp(cmd, "quit")==0);
        /* TODO tell clients to cease their jobs and exit normally */
        struct packet pac;
        pac.type = quit;
        pac.number = 0;
        for(int i=0;i<config->num_miners;i++){
        	write(client_fds[i].input_fd,&pac,sizeof(struct packet));
        	close(client_fds[i].input_fd);
		}
		return 1;
    }
    return 0;
}
unsigned char mine_base[1024*1024*1024+2048] = {};
int main(int argc, char **argv)
{
    /* sanity check on arguments */
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }

    /* load config file */
    struct server_config config;
    load_config_file(&config, argv[1]);
	fprintf(stderr,"config success\n");
    /* open the named pipes */
    struct fd_pair client_fds[config.num_miners];

    for (int ind = 0; ind < config.num_miners; ind += 1)
    {
        struct fd_pair *fd_ptr = &client_fds[ind];
        struct pipe_pair *pipe_ptr = &config.pipes[ind];

		fd_ptr->output_fd = open(pipe_ptr->output_pipe, O_RDONLY);
        assert (fd_ptr->output_fd >= 0);
        
        fd_ptr->input_fd = open(pipe_ptr->input_pipe, O_WRONLY);
        assert (fd_ptr->input_fd >= 0);

    }

    /* initialize data for select() */
    int maxfd = 0;
    fd_set readset;
    fd_set working_readset;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    FD_ZERO(&readset);
    FD_SET(STDIN_FILENO, &readset);
    // TODO add input pipes to readset, setup maxfd
	for(int i=0;i<config.num_miners;i++){
		FD_SET(client_fds[i].output_fd,&readset);
		if(client_fds[i].output_fd>maxfd)
			maxfd = client_fds[i].output_fd;
	}
	maxfd++;
	//init
	uint8_t treasure[32][MD5_DIGEST_LENGTH] = {};
	int treasure_bytes[32] = {};
	
	//get mine base
	///*******
	FILE *fp = fopen(config.mine_file,"rb");
	fseek(fp,0,SEEK_END);
	int mine_base_len = ftell(fp);
	//fprintf(stderr,"%d\n",mine_base_len);
	fseek(fp,0,SEEK_SET);
	fread(mine_base,1,mine_base_len,fp);
	fclose(fp);
	
	//init MD5
	MD5_CTX ctx;
  	MD5Init(&ctx);
  	MD5Update(&ctx, (const uint8_t*)mine_base, mine_base_len);
  	MD5_CTX tempp = ctx;
  	uint8_t mine_base_MD5[MD5_DIGEST_LENGTH];
  	MD5Final(mine_base_MD5,&tempp);
  	/*for(int i=0;i<mine_base_len;i++)
  		fprintf(stderr,"\\x%02x",mine_base[i]);
  	fprintf(stderr,"\n");
  	for(int i=0;i<16;i++)
  		fprintf(stderr,"%02x",mine_base_MD5[i]);
  	fprintf(stderr,"\n");*/
  	fprintf(stderr,"init md5 finish!\n");
	int best_n = 0;
	for(int i=0;i<16;i++){
		if(mine_base_MD5[i]>15)
			break;
		if(mine_base_MD5[i]==0){
			best_n+=2;
		}
		else{
			best_n++;
			break;
		}
	}
	if(best_n){
		memcpy(treasure[best_n-1],mine_base_MD5,16);
		treasure_bytes[best_n-1] = mine_base_len;
	}
    /* assign jobs to clients */
    assign_jobs(&config,ctx,client_fds,best_n+1);
	
    while (1)
    {
        memcpy(&working_readset, &readset, sizeof(readset)); // why we need memcpy() here?
        select(maxfd, &working_readset, NULL, NULL, &timeout);

        if (FD_ISSET(STDIN_FILENO, &working_readset))
        {
            /* TODO handle user input here */
            int shutdown;
	    	if(best_n>0)
	    		shutdown=handle_command(best_n,treasure[best_n-1],treasure_bytes[best_n-1],&config,client_fds,mine_base,mine_base_len);
	    	else
				shutdown = handle_command(best_n,treasure[best_n],0,&config,client_fds,mine_base,mine_base_len);
            if(shutdown){
            	memcpy(&working_readset, &readset, sizeof(readset));
        		select(maxfd, &working_readset, NULL, NULL, &timeout);
        		for(int i=0;i<config.num_miners;i++){
        			if(FD_ISSET(client_fds[i].output_fd,&working_readset)){
        				struct message temp;
						read(client_fds[i].output_fd,&temp,sizeof(struct message));
					}
				}
            	break;
            }
        }

        /* TODO check if any client send me some message
           you may re-assign new jobs to clients*/
        int get_mes = 0;
        for(int i=0;i<config.num_miners;i++){
			if(FD_ISSET(client_fds[i].output_fd,&working_readset)){
				//check if get mes
				if(get_mes){
					struct message temp;
					read(client_fds[i].output_fd,&temp,sizeof(struct message));
					continue;
				}
				//get append
				struct message mes;
				read(client_fds[i].output_fd,&mes,sizeof(struct message));
				if(mes.n_tre !=best_n+1)
					continue;
				get_mes = 1;
				
				for(int j=0;j<config.num_miners;j++){
					struct packet pac;
					if(j==i){
						pac.type = win;
					}
					else
						pac.type = lose;
					pac.number = 0;
					write(client_fds[j].input_fd,&pac,sizeof(struct packet));
				}
				
				fprintf(stderr,"winner: %s\n",mes.name);
				/*for(int i=0;i<mes.append_len;i++)
					fprintf(stderr,"\\x%02x",mes.append[i]);
				fprintf(stderr,"\n");*/
				//compute MD5
				mes.append[mes.append_len] = '\0';
				for(int i=0;i<mes.append_len;i++)
					mine_base[mine_base_len+i] = mes.append[i];
				mine_base_len += mes.append_len;
				//fprintf(stderr,"bytes = %d\n",mine_base_len);
  				MD5Update(&ctx, (const uint8_t*)mes.append, mes.append_len);
  				MD5_CTX temp;
  				memcpy(&temp,&ctx,sizeof(MD5_CTX));
  				MD5Final(treasure[best_n], &temp);
  				treasure_bytes[best_n] = mine_base_len;
  				best_n++;

				//broadcast
				fprintf(stdout,"A %d-treasure discovered! ",best_n);
				for(int j=0;j<MD5_DIGEST_LENGTH;j++){
					fprintf(stdout,"%02x",treasure[best_n-1][j]);
				}
				fprintf(stdout,"\n");
				for(int j=0;j<config.num_miners;j++){
					if(j==i)
						continue;
					write(client_fds[j].input_fd,&mes,sizeof(struct message));
					write(client_fds[j].input_fd,treasure[best_n-1],MD5_DIGEST_LENGTH);
				}
				assign_jobs(&config,ctx,client_fds,best_n+1);
			}
		}
    }
	wait(NULL);
    /* TODO close file descriptors */
    for(int i=0;i<config.num_miners;i++){
        close(client_fds[i].output_fd);
	}
    return 0;
}
