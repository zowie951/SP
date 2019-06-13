#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include<ctype.h>
int main(int argc,char *argv[]){
	int len = strlen(argv[1]),i,cnt = 0;
	char alp[len+1],data;
	bool table[58] = {},space=0;
	strcpy(alp,argv[1]);
	for(i=0;i<len;i++){
		if(isalpha(alp[i])){
			table[alp[i]-65]=1;
		}
		else if(isspace(alp[i]))
			space = 1;
	}
	FILE *fptr;
	fptr = fopen(argv[2],"r");
	if(!fptr){
		fprintf(stderr,"error\n");
	}
	else{
		while((data = fgetc(fptr)) != EOF){
			if(data=='\n'){
				printf("%d\n",cnt);
				cnt = 0;
				continue;
			}
			else if(isalpha(data)){
				if(table[data-65])
					cnt++;
			}
			else if(space&&isspace(data))
				cnt++;
		}
		fclose(fptr);
	}
	return 0;
}
