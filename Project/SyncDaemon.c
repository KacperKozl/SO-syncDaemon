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
static unsigned long long copyThreshold;
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
int argumentParse(int argc, char** argv, char** source, char** destination, unsigned int* sleepInterval, char* isRecursive)
{
    if(argc<=1) return -1;
    *isRecursive= 0;
    *sleepInterval=300;
    copyThreshold=ULLONG_MAX;
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
void stringAdd(char *dst, const size_t offset, const char *src){
    strcpy(dst+offset,src);
}
size_t addtoSubDirName(char*path, const size_t pathLen,const char*name){
    //Dopisujemny na końcu ścieżki, nazwę nowego katalogu
    stringAdd(path,pathLen,name);
    size_t subPathLen=pathLen+strlen(name);
    stringAdd(path,subPathLen,"/");
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
void Daemon(char *source, char *destination, unsigned int sleepInterval, char isRecursive, unsigned long long* copyThreshold){
    pid_t pid=fork();
    if(pid==-1){
        perror("fork");
        exit(-1);
    } else if(pid>0){
        printf("PID procesu potomnego %i\n",pid);
        exit(0);
    }
    int returnCode=0;
    char*sourcePath=NULL;
    char*destPath=NULL;
    if((sourcePath=malloc(sizeof(char)*4096))==NULL) returnCode=-1;
    else if((destPath=malloc(sizeof(char)*4096))==NULL) returnCode=-2;
    else if(realpath(source,sourcePath)==NULL){
        perror("realpath-source");
        returnCode=-3;
    }
    else if(realpath(destination,destPath)==NULL){
        perror("realpath-destination");
        returnCode=-4;
    }
    else if(setsid()==-1) returnCode=-5;
    else if(chdir("/")==-1) returnCode=-6;
    else{
        if(close(0)==-1) returnCode=-7;
        if(close(1)==-1) returnCode=-8;
        if(close(2)==-1) returnCode=-9;
    }
    if(returnCode>=0){
        for(int i=3;i<1024;i++) close(i);
        sigset_t set;
        //stdin na dev/null
        if(open("/dev/null",O_RDWR)==-1) returnCode=-10;
        //stdout na dev/null
        else if (dup(0)==-1) returnCode=-11;
        //stderr na dev/null
        else if (dup(0)==-1) returnCode=-12;
        else if (signal(SIGUSR1,sigusr1Handler)==SIG_ERR) returnCode=-13;
        else if (signal(SIGTERM,sigtermHandler)==SIG_ERR) returnCode=-14;
        else if(sigemptyset(&set)==-1) returnCode=-15;
        else if(sigaddset(&set,SIGUSR1)==1) returnCode=-16;
        else if(sigaddset(&set,SIGTERM)==1) returnCode=-17;
        else{
            size_t srcPathLen=strlen(sourcePath);
            if(sourcePath[srcPathLen-1]!='/') stringAdd(sourcePath,srcPathLen++,"/");
            size_t destPathLen=strlen(destPath);
            if(destPath[destPathLen-1]!='/') stringAdd(destPath,destPathLen++,"/");
            synchronizer sync;
            if(isRecursive==0){
                sync=syncNonRecursively;
            } else sync=syncRecursively;
            stopDaemon=0;
            forcedSyncro=0;
            while(1){
                if(forcedSyncro==0){
                    openlog("SyncDaemon",LOG_ODELAY | LOG_PID, LOG_DAEMON);
                    syslog(LOG_INFO,"sleep");
                    closelog();
                    unsigned int timeLeft=sleep(sleepInterval);
                    openlog("SyncDaemon",LOG_ODELAY | LOG_PID, LOG_DAEMON);
                    syslog(LOG_INFO,"wake_up - slept %u seconds",sleepInterval-timeLeft);
                    closelog();
                    if(stopDaemon==1) break;
                }
                if(sigprocmask(SIG_BLOCK,&set,NULL)==-1) {returnCode=-18;break;}
                int status=sync(sourcePath,srcPathLen,destPath,destPath);
                openlog("SyncDaemon",LOG_ODELAY | LOG_PID, LOG_DAEMON);
                syslog(LOG_INFO,"end of synchronization - %d",status);
                closelog();
                forcedSyncro=0;
                if(sigprocmask(SIG_UNBLOCK,&set,NULL)==-1) {returnCode=-19;break;}
                if(forcedSyncro==0&&stopDaemon=1) break;
            }
        }
    }
    if(sourcePath!=NULL) free(sourcePath);
    if(destPath!=NULL) free(destPath);
    openlog("SyncDaemon",LOG_ODELAY | LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO,"end of Deamon - %d",returnCode);
    closelog();
    exit(returnCode);
}
int updateDestFiles(const char *srcDirPath, const size_t srcDirPathLength, list *filesSrc, const char *dstDirPath, const size_t dstDirPathLength, list *filesDst){
    int returnCode=0;
    int status=0;
    int compare;
    char *srcFilePath=NULL;
    char *dstFilePath=NULL;
    if((srcFilePath=malloc(sizeof(char)*4096))==NULL) return -1;
    if((dstFilePath=malloc(sizeof(char)*4096))==NULL) {free(srcFilePath);return -2;}
    strcpy(srcFilePath,srcDirPath);
    strcpy(dstFilePath,dstDirPath);
    element *currentSrc=filesSrc->first;
    element *currentDst=filesDst->first;
    struct stat srcFile,dstFile;
    openlog("SyncDaemon",LOG_ODELAY | LOG_PID, LOG_DAEMON);
    char* srcFileName;
    char* dstFileName;
    while (currentDst!=NULL&&currentSrc!=NULL)
    {
        srcFileName=currentSrc->value->d_name;
        dstFileName=currentDst->value->d_name;
        compare=strcmp(srcFileName,dstFileName);
        if(compare>0){
            stringAdd(dstFilePath,dstDirPathLength,dstFileName);
            status=removeFile(dstFilePath);
            syslog(LOG_INFO,"delete file %s -status:%d\n",dstFilePath,status);
            if(status!=0) returnCode=1;
            currentDst=currentDst->next;
        }
        else{
            stringAdd(srcFilePath,srcDirPathLength,srcFileName);
            if(stat(srcFilePath,&srcFile)==-1){
                if(compare<0){
                    syslog(LOG_INFO,"failed copying file %s to directory %s-status:%d\n",srcFilePath,dstDirPath,errno);
                    returnCode=2;
                }else{
                    syslog(LOG_INFO,"failed metadata check of source file %s -status:%d\n",srcFilePath,errno);
                    currentDst=currentDst->next;
                    returnCode=3;
                }
                currentSrc=currentSrc->next;
            }
            else{
                if(compare<0){
                stringAdd(dstFilePath,dstDirPathLength,srcFileName);
                if(srcFile.st_size<copyThreshold)
                    status=copySmallFile(srcFilePath,dstFilePath,srcFile.st_mode,&srcFile.st_atim,&srcFile.st_mtim);
                    else status=copyBigFile(srcFilePath,dstFilePath,srcFile.st_size,srcFile.st_mode,&srcFile.st_atim,&srcFile.st_mtim);
                syslog(LOG_INFO,"copying file %s to directory %s -status:%d\n",srcFilePath,dstDirPath,status);
                if(status!=0) returnCode=4;
                currentSrc=currentSrc->next;
                
                }
                else{
                    stringAdd(dstFilePath,dstDirPathLength,dstFileName);
                    if(stat(dstFilePath,&dstFile)==-1){
                        syslog(LOG_INFO,"failed metadata check of destination file %s -status:%d\n",dstFilePath,errno);
                        returnCode=5;
                    }
                    else if(srcFile.st_mtim.tv_nsec != dstFile.st_mtim.tv_nsec||srcFile.st_mtim.tv_sec != dstFile.st_mtim.tv_sec){
                        if(srcFile.st_size<copyThreshold)
                        status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
                        else status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
                        syslog(LOG_INFO,"replacing file %s to directory %s -status:%d\n",srcFilePath,dstDirPath,status);
                        if(status!=0) returnCode=6;
                    }
                    else if(srcFile.st_mode!=dstFile.st_mode){
                        if(chmod(dstFilePath,srcFile.st_mode)==-1){
                            syslog(LOG_INFO,"failed copying file rights from %s to %s -status:%d\n",srcFilePath,dstDirPath,errno);
                            returnCode=7;
                        } 
                        else{ 
                            status=0;
                            syslog(LOG_INFO,"copying file rights from %s to %s -status:%d\n",srcFilePath,dstDirPath,status);
                        }
                    }
                    currentDst=currentDst->next;
                    currentSrc=currentSrc->next;
                }
            }
            
        }
    }
    while(currentDst!=NULL){
        dstFileName=currentDst->value->d_name;
        stringAdd(dstFilePath,dstDirPathLength,dstFileName);
        status=removeFile(dstFilePath);
        syslog(LOG_INFO,"deleting file %s - status %d\n",dstFilePath,status);
        if(status!=0) returnCode=8;
        currentDst=currentDst->next;
    }
    while(currentSrc!=NULL){
        srcFileName=currentSrc->value->d_name;
        stringAdd(srcFilePath,srcDirPathLength,srcFileName);
        if(stat(srcFilePath,&srcFile)==-1)
        {
            syslog(LOG_INFO,"failed copying file %s to directory %s-status:%d\n",srcFilePath,dstDirPath,errno);
            returnCode=9;
        }
        else{
            stringAdd(dstFilePath,dstDirPathLength,srcFileName);
            if(srcFile.st_size<copyThreshold)
                    status=copySmallFile(srcFilePath,dstFilePath,srcFile.st_mode,&srcFile.st_atim,&srcFile.st_mtim);
                    else status=copyBigFile(srcFilePath,dstFilePath,srcFile.st_size,srcFile.st_mode,&srcFile.st_atim,&srcFile.st_mtim);
                syslog(LOG_INFO,"copying file %s to directory %s -status:%d\n",srcFilePath,dstDirPath,status);
                if(status!=0) returnCode=11;
        }
        currentSrc=currentSrc->next;
    }
    free(dstFilePath);
    free(srcFilePath);
    closelog();
    returnCode;
}
int updateDestDir(const char *srcDirPath, const size_t srcDirPathLength, list *subDirsSrc, const char *dstDirPath, const size_t dstDirPathLength, list *subDirsDst, char *isReady){
    int status=0;
    int returnCode=0;
    int compare;
    int iterator=0;
    char* srcSubDirPath=NULL;
    if((srcSubDirPath=malloc(sizeof(char)*4096))==NULL) return -1;
    char* dstSubDirPath=NULL;
    if((dstSubDirPath=malloc(sizeof(char)*4069))==NULL){
        free(srcSubDirPath);
        return -2;
    }
    strcpy(dstSubDirPath,dstDirPath);
    strcpy(srcSubDirPath,srcDirPath);
    element *currentSrc=subDirsSrc->first;
    element *currentDst=subDirsDst->first;
    struct stat srcSubDir, dstSubDir;
    openlog("SyncDaemon",LOG_ODELAY | LOG_PID, LOG_DAEMON);
    char* srcSubDirName;
    char* dstSubDirName;
    while(currentDst!=NULL&&currentSrc!=NULL){
        srcSubDirName=currentSrc->value->d_name;
        dstSubDirName=currentDst->value->d_name;
        compare=strcmp(srcSubDirName,dstSubDirName);
        if(compare>0){
            size_t len=addtoSubDirName(dstDirPath,dstDirPathLength,dstSubDirName);
            status=removeDirRecursively(dstSubDirPath,len);
            syslog(LOG_INFO,"delete directory %s - status %d\n",dstSubDirPath,status);
            if(syslog!=0) returnCode=1;
            currentDst=currentDst->next;
        }
        else{
            stringAdd(srcSubDirPath,srcDirPathLength,srcSubDirName);
            if(stat(srcSubDirPath,&srcSubDir)==-1){
                if(compare<0){
                    syslog(LOG_INFO,"failed copying directory %s - status %d\n",dstSubDirPath,errno);
                    isReady[iterator]=0;
                    iterator++;
                    returnCode=2;
                }
                else{
                    syslog(LOG_INFO,"failed accessing metadata src directory %s - status %d\n",srcSubDirPath,errno);
                    isReady[iterator]=1;
                    iterator++;
                    returnCode=3;
                    currentDst=currentDst->next;
                }
                currentSrc=currentSrc->next;
            }
            else{
                if(compare<0){
                    stringAdd(dstSubDirPath,dstDirPathLength,srcSubDirName);
                    status=createEmptyDir(dstSubDirPath,srcSubDir.st_mode);
                    
                    if(status!=0){
                        isReady[iterator]=0;
                        iterator++;
                        returnCode=4;
                        syslog(LOG_INFO,"failed creating directory %s - status %d\n",dstSubDirPath,status);
                    }
                    else{
                        isReady[iterator]=1;
                        iterator++;
                        syslog(LOG_INFO,"created directory %s - status %d\n",dstSubDirPath,status);
                    }
                    currentSrc=currentSrc->next;
                }
                else{
                    isReady[iterator]=1;
                    iterator++;
                    stringAdd(dstSubDirPath,dstDirPathLength,dstSubDirName);
                    if(stat(dstSubDirPath,&dstSubDir)==-1){
                        syslog(LOG_INFO,"failed accessing metadata dst directory %s - status %d\n",dstSubDirPath,errno);
                        returnCode=6;
                    }
                    else if(dstSubDir.st_mode!=srcSubDir.st_mode){
                        if(chmod(dstSubDirPath,srcSubDir.st_mode)==-1){
                            returnCode=7;
                            syslog(LOG_INFO,"failed copying directory rights from %s to %s -status:%d\n",srcSubDirPath,dstSubDirPath,errno);
                        }
                        else{ 
                            status=0;
                            syslog(LOG_INFO,"copying directory rights from %s to %s -status:%d\n",srcSubDirPath,dstSubDirPath,status);
                        }
                    }
                    currentDst=currentDst->next;
                    currentSrc=currentSrc->next;
                }
            }
        }
    }
    while(currentDst!=NULL){
        dstSubDirName=currentDst->value->d_name;
        size_t len = addtoSubDirName(dstSubDirPath,dstDirPathLength,dstSubDirName);
        status = removeDirRecursively(dstSubDirPath,len);
        if(status!=0){
            syslog(LOG_INFO,"failed deleting directory %s - status %d\n",dstSubDirPath,status);
            returnCode=8;
        }else syslog(LOG_INFO,"deleting directory %s - status %d\n",dstSubDirPath,status);
    }
    while(currentSrc!=NULL){
        srcSubDirName=currentSrc->value->d_name;
        stringAdd(srcSubDirPath,srcDirPathLength,srcSubDirName);
        if(stat(srcSubDirPath,&srcSubDir)==-1){
            syslog(LOG_INFO,"failed accessing metadata of directory %s -status %d\n",srcSubDirPath,errno);
            isReady[iterator]=0;
            iterator++;
            returnCode=9;
        }
        else{
            stringAdd(dstSubDirPath,dstDirPathLength,srcSubDirName);
            status=createEmptyDir(dstSubDirPath,srcSubDir.st_mode);
            if(status!=0){
                isReady[iterator]=0;
                iterator++;
                returnCode=10;
                syslog(LOG_INFO,"failed creating directory %s -status %d\n",dstSubDirPath,status);
            }else{
                isReady[iterator]=1;
                iterator++;
                syslog(LOG_INFO,"created directory %s -status %d\n",dstSubDirPath,status);
            }
        }
        currentSrc=currentSrc->next;
    }
    free(dstSubDirPath);
    free(srcSubDirPath);
    return returnCode;
}