#include "SyncDaemon.h"
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
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
                *isRecursive=(char)1;
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
