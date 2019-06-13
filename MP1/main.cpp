#include <iostream>
#include <algorithm>
#include <string.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <unordered_map>
#include <vector>
#include "md5.h"
#include "find_commit.h"
#include "list_file.h"
using namespace std;
int main(int argc,char *argv[]){
	fstream file;
	string dir = argv[1];
	string config = argv[2];
	config = config + "/.loser_config";
	file.open(config.c_str(),ios::in);
	if(file){
		string nick_name,origin_name;
		char equal;
		while(file>>nick_name>>equal>>origin_name){
			if(nick_name == argv[1]){
				dir = origin_name;
				break;
			}
		}
		file.close();
	}
	if(strcmp(dir.c_str(),"status")==0){
		FILE *fptr;
		string record = argv[2];
		record = record + "/.loser_record";
		fptr = fopen(record.c_str(),"r");
		char *str =(char *)malloc(sizeof(char)*(strlen(argv[2])+8));
		strcpy(str,"./");
		strcat(str,argv[2]);
		struct FileNames f_name = list_file(str);
		if(fptr){
			fseek(fptr,find_commit(1,record.c_str()),SEEK_SET);
			char buf[512];
			unordered_map<string,string> f_map,md5_map;
			//vector<string> MD5_output;
			vector<string> new_f,mod_f,cop_f;
			while((fgets(buf,sizeof(buf),fptr)!=NULL)){
				if(strcmp(buf,"(MD5)\n")==0){
					char md5_id[64];
					while(fscanf(fptr,"%s %s",buf,md5_id) == 2){
						if(strcmp(buf,"#")==0)
							break;
						else{
							string a = buf,b = md5_id;
							//cout<<buf<<"  md5 "<<md5_id<<endl;
							f_map[a] = b;
							md5_map[b] = a;
	
						}
					}
					break;
				}
				
			}
			for(int i=0;i<f_name.length;i++){
				string f_temp = f_name.names[i];
				string temp = argv[2];
				fstream file;
				file.open(temp+'/'+f_temp,ios::in);
				file.seekg(0,file.end);
				int len = file.tellg();
				//cout<<len<<endl;
				file.seekg(0,file.beg);
				//char file_data[1024];
				char *file_data = new char[len];
				file.read(file_data,len);
				//file_data[len] = '\0';
				file.close();
				string md5_id = md5(file_data,len);
				//MD5_output.push_back(f_temp+" "+md5_id);
				if(f_map.count(f_temp)==1&&f_map[f_temp]==md5_id)
					continue;
				else if (f_map.count(f_temp)==1&&f_map[f_temp]!=md5_id)
					mod_f.push_back(f_temp);
				else if(md5_map.count(md5_id)==1&&md5_map[md5_id]!=f_temp)
					cop_f.push_back(md5_map[md5_id]+" => "+f_temp);
				else
					new_f.push_back(f_temp);
				delete file_data;
			}

			cout<<"[new_file]"<<endl;
			sort(new_f.begin(),new_f.end());
			for(int i=0;i<new_f.size();i++)
				cout<<new_f[i]<<endl;
			cout<<"[modified]"<<endl;
			sort(mod_f.begin(),mod_f.end());
			for(int i=0;i<mod_f.size();i++)
				cout<<mod_f[i]<<endl;
			cout<<"[copied]"<<endl;
			sort(cop_f.begin(),cop_f.end());
			for(int i=0;i<cop_f.size();i++)
				cout<<cop_f[i]<<endl;
		/*	cout<<"(MD5)"<<endl;
			sort(MD5_output.begin(),MD5_output.end());
			for(int i=0;i<MD5_output.size();i++)
				cout<<MD5_output[i]<<endl;  */
			fclose(fptr);
		}
		else{
			string f_str[f_name.length];
			for(int i=0;i<f_name.length;i++)
				f_str[i] = f_name.names[i];
			sort(f_str,f_str+f_name.length);
			printf("[new_file]\n");
			for(int i=0;i<f_name.length;i++)
				printf("%s\n",f_str[i].c_str());
			printf("[modified]\n[copied]\n");
		}
		free(str);
		free_file_names(f_name);
	}
	else if(strcmp(dir.c_str(),"commit")==0){
		FILE *fptr;
		string record = argv[2];
		record = record + "/.loser_record";
		fptr = fopen(record.c_str(),"r");
		char *str =(char *)malloc(sizeof(char)*(strlen(argv[2])+8));
		strcpy(str,"./");
		strcat(str,argv[2]);
		struct FileNames f_name = list_file(str);
		if(f_name.length != 0){
			if(fptr){
				fseek(fptr,find_commit(1,record.c_str()),SEEK_SET);
				char buf[512],commit[8],n[8],c[4];
				unordered_map<string,string> f_map,md5_map;
				vector<string> MD5_output;
				vector<string> new_f,mod_f,cop_f;
				fscanf(fptr,"%s%s%s",c,commit,n);
			//	cout<<c<<endl;
				int num = atoi(n);
				while((fgets(buf,sizeof(buf),fptr)!=NULL)){
					if(strcmp(buf,"(MD5)\n")==0){
						char md5_id[64];
						while(fscanf(fptr,"%s %s",buf,md5_id) == 2){
							if(strcmp(buf,"#")==0)
								break;
							else{
								string a = buf,b = md5_id;
								f_map[a] = b;
								md5_map[b] = a;
	
							}
						}
						break;
					}
				
				}
				for(int i=0;i<f_name.length;i++){
					string f_temp = f_name.names[i];
					string temp = argv[2];
					fstream file;
					file.open(temp+'/'+f_temp,ios::in);
					file.seekg(0,file.end);
					int len = file.tellg();
					//cout<<len<<endl;
					file.seekg(0,file.beg);
					//char file_data[1024];
					char *file_data = new char[len];
					file.read(file_data,len);
					//file_data[len] = '\0';
					file.close();
					string md5_id = md5(file_data,len);
					MD5_output.push_back(f_temp+" "+md5_id);
					if(f_map.count(f_temp)==1&&f_map[f_temp]==md5_id)
						continue;
					else if (f_map.count(f_temp)==1&&f_map[f_temp]!=md5_id)
						mod_f.push_back(f_temp);
					else if(md5_map.count(md5_id)==1&&md5_map[md5_id]!=f_temp)
						cop_f.push_back(md5_map[md5_id]+" => "+f_temp);
					else
						new_f.push_back(f_temp);
					delete file_data;
				}
				if(new_f.size()==0&&mod_f.size()==0&&cop_f.size()==0)
					return 0;
				fstream f_record;
				f_record.open(record,ios::out|ios::app);
				f_record<<endl;
				f_record<<"# commit "<<num+1<<endl;	
				f_record<<"[new_file]"<<endl;
				sort(new_f.begin(),new_f.end());
				for(int i=0;i<new_f.size();i++)
					f_record<<new_f[i]<<endl;
				f_record<<"[modified]"<<endl;
				sort(mod_f.begin(),mod_f.end());
				for(int i=0;i<mod_f.size();i++)
					f_record<<mod_f[i]<<endl;
				f_record<<"[copied]"<<endl;
				sort(cop_f.begin(),cop_f.end());
				for(int i=0;i<cop_f.size();i++)
					f_record<<cop_f[i]<<endl;
				f_record<<"(MD5)"<<endl;
				sort(MD5_output.begin(),MD5_output.end());
				for(int i=0;i<MD5_output.size();i++)
					f_record<<MD5_output[i]<<endl;
				f_record.close();
				fclose(fptr);
			}
			else{
				vector<string> MD5_output;
				for(int i=0;i<f_name.length;i++){
					string f_temp = f_name.names[i];
					string temp = argv[2];
					fstream file;
					file.open(temp+'/'+f_temp,ios::in);
					file.seekg(0,file.end);
					int len = file.tellg();
					//cout<<len<<endl;
					file.seekg(0,file.beg);
					//char file_data[1024];
					char *file_data = new char[len];
					file.read(file_data,len);
					//file_data[len] = '\0';
					file.close();
					string md5_id = md5(file_data,len);
					MD5_output.push_back(f_temp+" "+md5_id);
					delete file_data;
				}
				fstream f_record;
				f_record.open(record,ios::out);
				string f_str[f_name.length];
				for(int i=0;i<f_name.length;i++)
					f_str[i] = f_name.names[i];
				sort(f_str,f_str+f_name.length);
				f_record<<"# commit 1"<<endl;	
				f_record<<"[new_file]"<<endl;;
				for(int i=0;i<f_name.length;i++)
					f_record<<f_str[i]<<endl;
				f_record<<"[modified]"<<endl<<"[copied]"<<endl;
				f_record<<"(MD5)"<<endl;
				sort(MD5_output.begin(),MD5_output.end());
				for(int i=0;i<MD5_output.size();i++)
					f_record<<MD5_output[i]<<endl;
				f_record.close();
			}
		}
		free(str);
		free_file_names(f_name);

	}
	else if(strcmp(dir.c_str(),"log")==0){
		int num = atoi(argv[2]);
		if(num<=0)
			return 0;
		FILE *fptr;
		string record = argv[3];
		record = record + "/.loser_record";
		fptr = fopen(record.c_str(),"r");
		if(!fptr)
			return 0;	
		fseek(fptr,-1,SEEK_END);
		int cnt = 0,flag = 0;
		long int loc;
		do{
			if(fgetc(fptr)=='#'){
				cnt++;
				loc = ftell(fptr);
				fseek(fptr,-1,SEEK_CUR);
				char buf[1024];
				while(fgets(buf,sizeof(buf),fptr)!=NULL){
					if(strcmp(buf,"\n")==0){
						if(cnt!=num)
							cout<<endl;
						break;
					}
					else
						cout<<buf;
				}
				fseek(fptr,loc,SEEK_SET);
				if(cnt == num){
					break;
				}
				if(cnt==1)
					cout<<endl;
			}
		}while((flag=fseek(fptr,-2L,SEEK_CUR)) == 0);
		fclose(fptr);
	}
	return 0;
}
