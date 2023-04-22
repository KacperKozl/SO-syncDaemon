#include "SyncDaemon.h"
int main(int argc, char** argv)
{

}
int argumentParse(int argc, char** argv, char** source, char** destination, unsigned int* sleepInterval, bool* isRecursive, unsigned long long* copyThreshold)
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
                *isRecursive=true;
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
