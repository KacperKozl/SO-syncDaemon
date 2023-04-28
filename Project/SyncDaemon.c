#include "SyncDaemon.h"
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stddef.h>
//#include <linux/fs.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
//#include <sys/mman.h>
//#include <syslog.h>
#define BUFFER 4096
char forcedSyncro;
char stopDaemon;
int main(int argc, char** argv)
{

}
struct element{
    element *next;
    struct dirent *value;
};
int compare(element *a, element *b){
    return strcmp(a->value->d_name,b->value->d_name);
}
struct list{
    element *first,*last;
    unsigned int number;
};
void list_initialize(list *l){
    l->first=NULL;
    l->last=NULL;
    l->number=0;
}
int add(list *l, struct dirent *newEntry){
    element* elem =NULL;
    if((elem=malloc(sizeof(element)))==NULL) return -1;
    elem->value=newEntry;
    elem->next=NULL;
    if(l->first=NULL)
    {
        l->first=elem;
        l->last=elem;
        l->number=1;
    }
    else
    {
        l->last->next=elem;
        l->last=elem;
        ++l->number;
    }
    return 0;
}
void clear(list *l){
    element* elem=l->first;
    element* next;
    while(elem!=NULL){
        next=elem->next;
        free(elem);
        elem=next;
    }
    list_initialize(l);
}
int argumentParse(int argc, char** argv, char** source, char** destination, unsigned int* sleepInterval, char* isRecursive, unsigned long long* copyThreshold)
{
    if(argc<=1) return -1;
    *isRecursive= 0;
    *sleepInterval=300;
    *copyThreshold=ULLONG_MAX;
    int options;
    while((options=getopt(argc,argv,":Ri:t:"))!=-1)
    {
        switch(options)
        {
            case'i':
                if(sscanf(optarg,"%u",sleepInterval)<1) return -2;
                break;
            case'R':
                *isRecursive=1;
            break;
            case't':
                if(sscanf(optarg,"%llu",copyThreshold)<1) return -3;
                break;
            case':':
                printf("Opcja wymaga podania wartosci");
                return -4;
                break;
            case'?':
                printf("Nieznana opcja %c",optopt);
                return -5;
                break;
            default:
                printf("Nieznany blad");
                return -6;
                break;
        }
    }
    if(argc-optind!=2) return -7;
    *source=argv[optind];
    *destination=argv[optind+1];
    return 0;
}
size_t addtoSubDirName(char*path, const size_t pathLen,const char*name){
    //Dopisujemny na końcu ścieżki, nazwę nowego katalogu
    strcpy(path+pathLen,name);
    size_t subPathLen=pathLen+strlen(name);
    strcpy(path+subPathLen,"/");
    subPathLen++;
    return subPathLen;
}
int isDirectoryValid(const char *path){
    DIR *directory=opendir(path);
    if(directory==NULL) return -1;
    if(closedir(directory)==-1) return -2;
    return 0;
}
void sigusr1Handler(int signo){
    forcedSyncro=1;
}
void sigtermHandler(int signo){
    stopDaemon=1;
}
int listFiles(DIR *directory, list* files){
    struct dirent *item;
    errno=0;
    while((item=readdir(directory))!=NULL){
        if(item->d_type==DT_REG){
            if(add(files,item)<0) return -1;
        }
    }
    if(errno!=0) return -2;
    return 0;
}
int listFilesAndDir(DIR *directory, list *files, list *dirs){
    struct dirent* item;
    errno=0;
    while((item=readdir(directory))!=NULL){
        if(item->d_type==DT_REG){
            if(add(files,item)<0) return -1;
        }
        else if(item->d_type==DT_DIR){
            if(strcmp(item->d_name,"..")!=0&&strcmp(item->d_name,".")){
                if(add(dirs,item)<0) return -3;
            }
        }
    }
    if(errno!=0) return -2;
    return 0;
}
