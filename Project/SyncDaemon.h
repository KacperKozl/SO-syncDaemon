#ifndef SYNCDAEMON_H
#define SYNCDAEMON_H

#include <dirent.h>
#include <sys/stat.h>

typedef struct element element;
struct element;
int compare(element *a,element *b);
typedef struct list list;
struct list;
void list_initialize(list *l);
int add(list *l, struct dirent *newEntry);
void clear(list *l);
void listSort(list *l);
void stringAdd(char *dst, const size_t offset, const char *src);
size_t addtoSubDirName(char *path, const size_t pathLength, const char *subName);
int argumentParse(int argc, char **argv, char **source, char **destination, unsigned int *sleepInterval, char* isRecursive, unsigned long long* copyThreshold);
int isDirectoryValid(const char *path);
void Daemon(char *source, char *destination, unsigned int sleepInterval, char isRecursive,unsigned long long* copyThreshold);
void sigusr1Handler(int signo);
void sigtermHandler(int signo);

int listFiles(DIR *directory, list *files);
int listFilesAndDir(DIR *directory, list *files, list *dirs);
int createEmptyDir(const char *path, mode_t mode);
int removeDirRecursively(const char *path, const size_t pathLength);

int copySmallFile(const char *srcFilePath, const char *dstFilePath, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime);
int copyBigFile(const char *srcFilePath, const char *dstFilePath, const unsigned long long fileSize, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime);
int removeFile(const char *path);

int updateDestFiles(const char *srcDirPath, const size_t srcDirPathLength, list *filesSrc, const char *dstDirPath, const size_t dstDirPathLength, list *filesDst);
int updateDestDir(const char *srcDirPath, const size_t srcDirPathLength, list *subdirsSrc, const char *dstDirPath, const size_t dstDirPathLength, list *subdirsDst, char *isReady);

int syncNonRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength);
int syncRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength);
typedef int (*synchronizer)(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength);
#endif
