#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>

int main (void)
{
	char buffer[256];
	while (1)
	{
		int i;
		printf("OSLab2->");
		gets(buffer);
		int len=strlen(buffer);
		int begin=0,end=0;
		int pipe_flag=0;
		char addr[256];
		for (i=0;i<256;++i)
			addr[i]=0;//all mem in argv is set to NULL
		addr[0]=0;
		int count=0;
		for (end=0;end<=len;++end){
			if (buffer[end]=='|'){
				pipe_flag=end;
			}
			else if (buffer[end]==';' || buffer[end]=='\0'){
				buffer[end]='\0';
				if (pipe_flag==0){
					char **argv = (char **)malloc(sizeof(char *)*(count+2));
					for (i=0;i<=count;++i){
						argv[i]=&buffer[addr[i]];
					}
					argv[count+1]=NULL;
					pid_t pid = fork();
					if (pid==0){
						execvp(argv[0],argv);
					}
					else {
						waitpid(pid,NULL,0);
					}
					free(argv);
				}
				else {
					for (i=begin;i<pipe_flag-1;++i){
						if (buffer[i]=='\0')
							buffer[i]=' ';
					}
					for (i=pipe_flag+2;i<end;++i){
						if (buffer[i]=='\0')
							buffer[i]=' ';
					}
					char *argv1=&buffer[begin];
					char *argv2=&buffer[pipe_flag+2];
					FILE *f1 = popen(argv1, "r");
					FILE *f2 = popen(argv2, "w");
					char data[1000];
					fread(data,1000*sizeof(char),1,f1);
					fwrite(data,1000*sizeof(char),1,f2);
					pclose(f1);
					pclose(f2);

				}
				begin=end+1;
				pipe_flag=0;
				for (i=0;i<256;++i)
					addr[i]=0;
				count=-1;
				
			}
			else if (buffer[end]==' '){
				buffer[end]='\0';
			}
			else {
				if (end>0 && buffer[end-1]=='\0')
				{
					count++;
					addr[count]=end;
				}
			}
		}
		
	}
}
