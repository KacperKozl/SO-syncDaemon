#ifndef SYNCDAEMON_H
#define SYNCDAEMON_H

#include <dirent.h>
#include <sys/stat.h>
//Element listy
typedef struct element element;
struct element;
/*Funkcja prównująca elementy listy według nazwy elementów katalogu
a - pierwszy element
b - drugi element
Zwraca:
0 - gdy elementy są równe
<0 - jeżeli w porządku leksykografizcznym a jest przed b
>0 - jeżeli w porządku leksykografizcznym b jest przed a
*/
int compare(element *a,element *b);
//Lista jednokierunkowa
typedef struct list list;
struct list;
//Funkcja inicjalizuje listę
//zapisuje listę gotową do użycia
void list_initialize(list *l);
/*Funkcja dodaje element na koniec listy
l - lista do której dodajemy element
newEntry - element katalogu, który dodajemy
Zwraca:
0 - w przypadku poprawnego wykonania
-1 - gdy wystąpi błąd alokacji pamięci
*/
int add(list *l, struct dirent *newEntry);
/*Funkcja czyści listę
Modyfikuje liste usuwając z niej wszystkie elementy
*/
void clear(list *l);
/*Funkcja sortująca liste
Zapisuje posortowaną liste według funkcji int compare(element *a,element *b);
*/
void listSort(list *l);
/*Funkcja dopisująca na pozycji offset napisu dst, napis src
src - napis do wstawienia
dst - napis do którego zostanie wstawiony ciąg src
offset - pozycja na którą zostanie wstawiony ciąg src
*/
void stringAdd(char *dst, const size_t offset, const char *src);
/*Funkcja dodająca napis subName na koniec ścieżki katalogu path
path - ścieżka katalogu
pathLength - długość scieżki wyrażona w bajtach
subName - nazwa do dopisania na koniec katalogu
Funkcja zwraca długość nowej ścieżki w bajtach i
dopisuje do path napis subName i '/'
*/
size_t addtoSubDirName(char *path, const size_t pathLength, const char *subName);
/*Funkcja parsująca argumenty i opcje
argc - liczba parametrów i opcji
argv - tablica parametrów
funkcja zapisuje w:
source - ścieżkę synchronizowanego katalogu źródłowego
destination - ścieżkę synchronizowanego katalogu docelowego
sleepInterval - czas spania demona w sekundach
isRecursive - rekurencyjną sychronizację katalogów
copyThreshold(zmienna globalna) - minimalna wielkośc pliku, aby był uznany za duży
zwraca 0 jeśli nie wystąpił błąd w przeciwnym wypadku <0, jako kod błędu
*/
int argumentParse(int argc, char **argv, char **source, char **destination, unsigned int *sleepInterval, char* isRecursive);
/*Funkcja sprawdzająca, czy katalog na ścieżce path istnieje i można go synchronizować
zwraca 0 jeśli nie wystąpił błąd w przeciwnym wypadku <0, jako kod błędu
*/
int isDirectoryValid(const char *path);
/*Funkcja wykonuje kroki potrzebnya, aby program stał się demonem
source - ścieżkę synchronizowanego katalogu źródłowego
destination - ścieżkę synchronizowanego katalogu docelowego
sleepInterval - czas spania demona w sekundach
isRecursive - rekurencyjna sychronizacja katalogów
*/
void Daemon(char *source, char *destination, unsigned int sleepInterval, char isRecursive);
/*Funkcja obsługi sygnały SIGUSR1
signo-numer sygnału, którym zawsze jest sygnał SIGUSR1*/
void sigusr1Handler(int signo);
/*Funkcja obsługi sygnały SIGUTERM
signo-numer sygnału, którym zawsze jest sygnał SIGTERM*/
void sigtermHandler(int signo);
/*Funkcja wpisuje do listy wszystkie pliki z katalogu
directory - strumień katalog otwarty z użyciem opendir z któreog będą listowane pliki
files - lista do której zostaną zapisane inforamcje o plikach
zwraca 0 jeśli nie wystąpił błąd w przeciwnym wypadku <0, jako kod błędu
*/
int listFiles(DIR *directory, list *files);
/*Funkcja wpisuje do listy wszystkie pliki z katalogu
directory - strumień katalog otwarty z użyciem opendir z któreog będą listowane pliki
files - lista do której zostaną zapisane inforamcje o plikach
dirs - lista do której zostaną zapisane inforamcje o podkatalogach
zwraca 0 jeśli nie wystąpił błąd w przeciwnym wypadku <0, jako kod błędu
*/
int listFilesAndDir(DIR *directory, list *files, list *dirs);
/*Funkcja tworzy pusty katalog
path - ścieżka do utworzenia katalogu
mode - uprawnienia do utworzenia katalogu
zwraca 0 jeśli nie wystąpił błąd w przeciwnym wypadku <0, jako kod błędu
*/
int createEmptyDir(const char *path, mode_t mode);
/*Funkcja rekurencyjnie usuwa katalog
path - ścieżka katalogu do usunięcia
pathLength - długość ścieżki w bajtach
zwraca 0 jeśli nie wystąpił błąd, w przeciwnym wypadku <0 jeżeli błąd nie pozwala na dalsze kontynuowanie
>0 jeśli bład nie jest krytyczny
*/
int removeDirRecursively(const char *path, const size_t pathLength);
/*Funkcja kopiuje małe pliki(read i write)
srcFilePath - ścieżka pliku źródłowego do skopiowania
dstFilePath - ścieżka pliku docelowego
dstMode - tryb pliku do skopiowania
dstAccessTime - czas dostępu do ustawieniaplikowi docelowemu
dstModificationTime - czas modyfikacji do ustawieniaplikowi docelowemu
zwraca 0 jeśli nie wystąpił błąd, w przeciwnym wypadku <0 jeżeli błąd nie pozwala na dalsze kontynuowanie
>0 jeśli bład nie jest krytyczny
*/
int copySmallFile(const char *srcFilePath, const char *dstFilePath, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime);
/*Funkcja kopiuje duże pliki(mmap i write)
srcFilePath - ścieżka pliku źródłowego do skopiowania
dstFilePath - ścieżka pliku docelowego
fileSize - wielkość pliku
dstMode - tryb pliku do skopiowania
dstAccessTime - czas dostępu do ustawieniaplikowi docelowemu
dstModificationTime - czas modyfikacji do ustawieniaplikowi docelowemu
zwraca 0 jeśli nie wystąpił błąd, w przeciwnym wypadku <0 jeżeli błąd nie pozwala na dalsze kontynuowanie
>0 jeśli bład nie jest krytyczny
*/
int copyBigFile(const char *srcFilePath, const char *dstFilePath, const unsigned long long fileSize, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime);
/*Funkcja usuwa plik ze ścieżki path
zwraca -1 jeśli wystąpił błąd inaczej zwraca 0
*/
int removeFile(const char *path);
/*Funkcja aktualizuje pliki w miejscu docelowym na podstawie wykrytych zmian
srcDirPath - ścieżka katalogu źródłowego
srcDirPathLength - długość ścieżki katalogu źródłowego w bajtach
filesSrc - uporządkowana lista plików w katalogu źródłowym
dstDirPath- ścieżka katalogu docelowego
dstDirPathLength- długość ścieżki katalogu docelowego w bajtach
filesDst - uporządkowana lista plików w katalogu docelowym
zwraca 0 jeśli nie wystąpił błąd, w przeciwnym wypadku <0 jeżeli błąd nie pozwala na dalsze kontynuowanie
>0 jeśli bład nie jest krytyczny
*/
int updateDestFiles(const char *srcDirPath, const size_t srcDirPathLength, list *filesSrc, const char *dstDirPath, const size_t dstDirPathLength, list *filesDst);
/*Funkcja aktualizuje podkatalogi w miejscu docelowym na podstawie wykrytych zmian
srcDirPath - ścieżka katalogu źródłowego
srcDirPathLength - długość ścieżki katalogu źródłowego w bajtach
subDirsSrc - uporządkowana lista podkatalogów w katalogu źródłowym
dstDirPath- ścieżka katalogu docelowego
dstDirPathLength- długość ścieżki katalogu docelowego w bajtach
subDirsDst - uporządkowana lista podkatalogów w katalogu docelowym
isReady - tablica zawierająca inforamacje, czy katalog jest gotowy do sychronizacji
ma długoścć identyczna co liczba podtkalogów w katalogu źródlowym, przyjmuje wartości(1-gotowy,0-nie gotowy)
zwraca 0 jeśli nie wystąpił błąd, w przeciwnym wypadku <0 jeżeli błąd nie pozwala na dalsze kontynuowanie
>0 jeśli bład nie jest krytyczny
*/
int updateDestDir(const char *srcDirPath, const size_t srcDirPathLength, list *subDirsSrc, const char *dstDirPath, const size_t dstDirPathLength, list *subDirsDst, char *isReady);
/*Funkcja nierekurencyjnie synchronizuje katalogi
sourcePath - scieżka katalogu źródłowego
sourcePathLength-długość ścieżki katalogu źródłowego w bajtach
destinationPath-ścieżka katalogu docelowego
destinationPathLength-długość ścieżki katalogu docelowego w bajtach
zwraca 0 jeśli nie wystąpił błąd, w przeciwnym wypadku <0 jeżeli wystąpił błąd
*/
int syncNonRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength);
/*Funkcja ekurencyjnie synchronizuje katalogi
sourcePath - scieżka katalogu źródłowego
sourcePathLength-długość ścieżki katalogu źródłowego w bajtach
destinationPath-ścieżka katalogu docelowego
destinationPathLength-długość ścieżki katalogu docelowego w bajtach
zwraca 0 jeśli nie wystąpił błąd, w przeciwnym wypadku <0 jeżeli wystąpił błąd
*/
int syncRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength);
//Wskaźnik na funkcję sychronizującą, pozwala zmniejszyć ilość warunków
typedef int (*synchronizer)(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength);
#endif
