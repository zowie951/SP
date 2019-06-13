#include "find_commit.h"

//you can use it only if the .record file exist
int find_commit(int num,const char* record){
	FILE *fptr;
	fptr = fopen(record,"r");
	fseek(fptr,-1,SEEK_END);
	int ans,cnt = 0,flag = 0;
	do{
		int loc,nth;
		if(fgetc(fptr)=='#'){
			cnt++;
			loc = (int)ftell(fptr);
			if(cnt == num){
				ans = loc - 1;
				break;
			}
		}
	}while((flag=fseek(fptr,-2L,SEEK_CUR)) == 0);
	fclose(fptr);
	if(flag == 1)
		return 0;
	else
		return ans;
}
