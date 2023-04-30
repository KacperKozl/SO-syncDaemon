/*
Wykorzystanie:
SyncDaemon [-R] [-i <czas_spania>] [-t <minimalna_wielkosc_kopiowania_duzych_plikow>] sciezka_zrodlowa sciezka_docelowa
argumenty:
sciezka_docelowa - ścieżka do katalogu z którego będziemy kopiować
sciezka_docelowa - ścieżka do katalogu do którego będziemy kopiować
opcje:
-i <czas_spania> - czas spania w sekunach
-R - powoduje sychronizowanie rekurencyjne katalogów
-t <minimalna_wielkosc_kopiowania_duzych_plikow> - próg, który określa minimalną wielkość pliku, od którego będzie onn uważany za duży
Demona kotrolujemy za pomocą sygnałów:
wysłanie SIGURS1:
-w trakcie spania powoduje wczesne obudzenie
-w trakcie synchronizacji powoduje jej ponowienie od razu po zakończeniu synchronizacji
wysłanie SIGTERM:
-kończy demona, jeśli dojdzie w trakcie synchronizacji, to najpierw zakończy się sychronizacja, a potem demon
*/
#include "SyncDaemon.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/fs.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
//Definiujemy stałą określającą wielkość bufora dla funkcji kopiujących
#define BUFFER 4096
//Tworzymy zmienną statyczną przetrzymującą minimalną wielkość od której plik jest uważany za duży
static unsigned long long copyThreshold;
//Tworzymy zmienną, która przetrzymuje status wymuszonej synchronizacji sygnałem SIGUSR1
char forcedSyncro;
//Tworzymy zmienną, która przetrzymuje status wymuszonego zakończenia sygnałem SIGTERM
char stopDaemon;
int main(int argc, char** argv)
{
    //Deklarujemy zmiennę na przechowywanie parametrów
    char* src,*dst;
    unsigned int sleepInterval;
    char isRecursive;
    //Pardujemy parametry podane w trakcie uruchamiania programu. W przypadku wystąpienia błędu wypisujemy poprawne użycie i kończymy program
    if(argumentParse(argc,argv,&src,&dst,&sleepInterval,&isRecursive)<0){
        printf("Prawidlowy sposob uzycia: SyncDaemon [-i <czas_spania>] [-R] [-t <minimalna_wielkosc_kopiowania_duzych_plikow>] sciezka_zrodlowa sciezka_docelowa\n");
        return -1;
    }
    //Jeśli ścieżka do folderu docelowego jest nieprawidłowa, to wypisujemy błąd na wyjście błędów i kończymy program
    if(isDirectoryValid(dst)<0){
        perror(dst);
        return -2;
    }
    //Jeśli ścieżka do folderu źródłowego jest nieprawidłowa, to wypisujemy błąd na wyjście błędów i kończymy program
    if(isDirectoryValid(src)<0){
        perror(src);
        return -3;
    }
    //Uruchamiamy demona
    Daemon(src,dst,sleepInterval,isRecursive);
    return 0;
}
//element listy
struct element{
    //wskaźnik na następny
    element *next;
    //Wskaźnik na elementy katalogu, mogą być to pliki lub katalogi
    struct dirent *value;
};
//Funkcja porównująca elementy listy w porządku leksykografucznym na podstawie nazw
int compare(element *a, element *b) {
  return strcmp(a->value->d_name, b->value->d_name);
}
//lista do przechowywania folderów lub plików
struct list {
  //Wskaźniki na pierwszy i ostatni element listy
  element *first, *last;
  //Zmienna na zliczanie elementów listy
  unsigned int number;
};
//Inicjalizacja listy
void list_initialize(list *l) {
  //Ustawiamy wskaźniki na NULL i ustawiamy liczbę elementów na 0
  l->first = NULL;
  l->last = NULL;
  l->number = 0;
}
//Dodawanie elementów do listy, elementy są dodawane na koniec
int add(list *l, struct dirent *newEntry) {
  element *elem = NULL;
  //Rezerwujemy pamięć na nowy element, w przypadku błędu kończymy funkcje z kodem błędu
  if ((elem = malloc(sizeof(element))) == NULL)
    return -1;
  //Dodajemy element katalogu jako wartość elmentu listy
  elem->value = newEntry;
  //Ustawiamy wskaźnik nowego elementu listy na NULL
  elem->next = NULL;
  //Gdy lista jest pusta, to jako pierwszy i ostatni element ustawiamy aktualnie dodawany element
  if (l->first == NULL) {
    l->first = elem;
    l->last = elem;
    l->number = 1;
  }//Gdy lista nie jest pusta to dodajemy nowy element na koniec i zwiększamy liczbę o jeden 
  else {
    l->last->next = elem;
    l->last = elem;
    ++l->number;
  }
  //kończymy funkcje bez błędu
  return 0;
}
//Funkcja czyszcząca listę
void clear(list *l) {
  element *elem = l->first;
  element *next;
  //przechodzimy po kolei po wszystkich elementach listy i zwalniamy pamięć
  while (elem != NULL) {
    next = elem->next;
    free(elem);
    elem = next;
  }
  //Czyścimy początkowe wskaźniki listy poprzez funkcję inicjalizacyjną  
  list_initialize(l);
}
void listSort(list *l) {
  int swapped;
  element *ptr1 = l->first;
  element *lptr = NULL;

  if (l == NULL)
    return;
  if (l->first == NULL)
    return;

  do {
    swapped = 0;
    ptr1 = l->first;

    while (ptr1->next != lptr) {
      if (compare(ptr1, ptr1->next) > 0) {
        element *temp = ptr1;
        ptr1 = ptr1->next;
        temp->next = ptr1->next;
        ptr1->next = temp;

        if (temp == l->first)
          l->first = ptr1;
        swapped = 1;
      } else {
        ptr1 = ptr1->next;
      }
    }
    lptr = ptr1;
  } while (swapped);
}
//Funkcja parsująca argumenty
int argumentParse(int argc, char **argv, char **source, char **destination,
                  unsigned int *sleepInterval, char *isRecursive) {
  //Gdy argumentów jest mniej niż jeden to kończymy program
  if (argc <= 1) return -1;
  //Podstawowo ustawiamy, że nie robimy rekurencyjnej synchronizacji i podstawowy czas spania na 300s
  *isRecursive = 0;
  *sleepInterval = 300;
  //Domyślny próg dla dużych plików ustawiamy na maksymalną wielkość zmiennej unsigned long long
  copyThreshold = ULLONG_MAX;
  //Zmienna na opcje
  int options;
  //Wczytujemy opcję funkcję getopt,':' na początku rozróżnia nieznana opcję od braku wartości dla opcji
  //Po wczytaniu w zależności od wartości options ustawiamy inne zmienne statusowe
  while ((options = getopt(argc, argv, ":Ri:t:")) != -1) {
    switch (options) {
      //Funkcja sscanf formatuje zwracane argumenty opcji z napisów na wymagane formaty
    case 'i':
      if (sscanf(optarg, "%u", sleepInterval) < 1)
        return -2;
      break;
    case 'R':
      *isRecursive = 1;
      break;
    case 't':
      if (sscanf(optarg, "%llu", &copyThreshold) < 1)
        return -3;
      break;
    //Gdy nie podano wartości do jakiejś opcji wypisujemy komunikat
    case ':':
      printf("Opcja wymaga podania wartosci");
      return -4;
      break;
    //Gdy nie podano opcji wypisujemy komunikat
    case '?':
      printf("Nieznana opcja %c", optopt);
      return -5;
      break;
    //Gdy inny błąd to wypisujemy komunikat o nieznanym błędzie
    default:
      printf("Nieznany blad");
      return -6;
      break;
    }
  }
  //Gdy po wczytaniu opcji jest inna liczba argumentów niż 2(ścieżka docelowa i źródłowa) kończymy z błędem
  if (argc - optind != 2)
    return -7;
  //Ustawiamy wartości zmiennych przetrzymujące ścieżki do synchronizowanych folderów
  *source = argv[optind];
  *destination = argv[optind + 1];
  //Kończymy z poprawnym kodem
  return 0;
}
//Funkcja dopisująca do napisu dst na pozycji offset tekst z src, powód powstania funkcji to kasowanie wartości offset, dlatego podawjemy przez constant
void stringAdd(char *dst, const size_t offset, const char *src) {
  strcpy(dst + offset, src);
}
//Funkcja dopisująca na końcu ścieżki, nazwę nowego katalogu
size_t addtoSubDirName(char *path, const size_t pathLen, const char *name) {
  //dodajemy nazwę na koniec
  stringAdd(path, pathLen, name);
  //zwiększamy długość ścieżki
  size_t subPathLen = pathLen + strlen(name);
  //dodajemy slash na koniec
  stringAdd(path, subPathLen, "/");
  subPathLen++;
  //zwracamy długość ścieżki
  return subPathLen;
}
//Funkcja sprawdzająca czy można otworzyć katalog na ścieżce
int isDirectoryValid(const char *path) {
  //Próbujemy otworzyć i zamknąć katalog, jeśli wystąpi błąd to w którejś operacji zwracamy kod błedu
  DIR *directory = opendir(path);
  if (directory == NULL)
    return -1;
  if (closedir(directory) == -1)
    return -2;
  //Kończymy bez błędu
  return 0;
}
// Funkcja obsługi SIGUSR1 - ustawiamy flagę, odpowiadającą za wymuszenie synchronizacji
void sigusr1Handler(int signo) { forcedSyncro = 1; }
// Funkcja obsługi SIGTERM - ustawiamy flagę, odpowiadającą za wymuszenie zamknięcia demona
void sigtermHandler(int signo) { stopDaemon = 1; }
//Funkcja listująca pliki w katalogu
int listFiles(DIR *directory, list *files) {
  //tworzymy zmienną na nowy element w katalogu
  struct dirent *item;
  //zerujemy errno
  errno = 0;
  //Dopóki nie wczytano wszystkich elmentów w katalogu
  while ((item = readdir(directory)) != NULL) {
    //Jeśli element jest plikiem to go dodajemy
    if (item->d_type == DT_REG) {
      //W przypadku błędu kończymy funkcję
      if (add(files, item) < 0)
        return -1;
    }
  }
  //jeśli raaddir ustawił zmienną errno na kod błędu kończymy funkcję z kodem błędu
  if (errno != 0)
    return -2;
  //Kończymy bez błędu
  return 0;
}
//Funkcja listująca pliki i foldery
int listFilesAndDir(DIR *directory, list *files, list *dirs) {
  //tworzymy zmienną na nowy element w katalogu
  struct dirent *item;
  //zerujemy errno
  errno = 0;
  //Dopóki nie wczytano wszystkich elmentów w katalogu
  while ((item = readdir(directory)) != NULL) {
    //Jeśli element jest plikiem lub katalogiem to dodajemy go do odpowiedniej listy
    //W przypadku błędu, zwracamy odpowiedni kod
    if (item->d_type == DT_REG) {
      if (add(files, item) < 0)
        return -1;
    } else if (item->d_type == DT_DIR) {
      if (strcmp(item->d_name, "..") != 0 && strcmp(item->d_name, ".")) {
        if (add(dirs, item) < 0)
          return -3;
      }
    }
  }
  //jeśli raaddir ustawił zmienną errno na kod błędu kończymy funkcję z kodem błędu
  if (errno != 0)
    return -2;
  //Kończymy bez błędu
  return 0;
}
//Funkcja do tworzenia katalagów, w przypadku gdyby trzeba było zmodyfikować tę procedurę
int createEmptyDir(const char *path, mode_t mode) { return mkdir(path, mode); }
//
int removeDirRecursively(const char *path, const size_t pathLength) {
  int ret = 0;
  DIR *dir = opendir(path);
  if (!dir) {
    ret = -1;
    goto exit;
  }

  list files, subdirs;
  list_initialize(&files);
  list_initialize(&subdirs);

  if (listFilesAndDir(dir, &files, &subdirs) < 0) {
    ret = -2;
    goto clean_up;
  }

  char subPath[PATH_MAX];
  strcpy(subPath, path);

  for (element *cur = subdirs.first; cur != NULL; cur = cur->next) {
    size_t subPathLength =
        addtoSubDirName(subPath, pathLength, cur->value->d_name);
    if (removeDirRecursively(subPath, subPathLength) < 0) {
      ret = -4;
      goto clean_up;
    }
  }

  for (element *cur = files.first; cur != NULL; cur = cur->next) {
    stringAdd(subPath, pathLength, cur->value->d_name);
    if (removeFile(subPath) == -1) {
      ret = -5;
      goto clean_up;
    }
  }

  if (rmdir(path) == -1) {
    ret = -6;
    goto clean_up;
  }

clean_up:
  clear(&files);
  clear(&subdirs);

exit:
  if (dir) {
    closedir(dir);
  }

  if (ret >= 0 && ret <= 2) {
    ret = -ret;
  }

  return ret;
}

// Funkcja kopiuje plik z podanej ścieżki źródłowej na podaną ścieżkę docelową
// z zachowaniem trybu dostępu oraz czasów dostępu i modyfikacji.
// Argumenty funkcji:
// - srcFilePath: ścieżka źródłowa pliku do skopiowania
// - dstFilePath: ścieżka docelowa pliku skopiowanego
// - dstMode: tryb dostępu do ustawienia dla skopiowanego pliku
// - dstAccessTime: czas dostępu do ustawienia dla skopiowanego pliku
// - dstModificationTime: czas modyfikacji do ustawienia dla skopiowanego pliku
// Zwraca:
// - 0 w przypadku powodzenia
// - wartość mniejszą od zera w przypadku błędu

int copySmallFile(const char *srcFilePath, const char *dstFilePath,
                  const mode_t dstMode, const struct timespec *dstAccessTime,
                  const struct timespec *dstModificationTime) {
  int ret = 0, in = -1, out = -1;
  // Otwórz plik źródłowy do odczytu i zapisz jego deskryptor pliku
  if ((in = open(srcFilePath, O_RDONLY)) == -1) {
    ret = -1;
    goto cleanup;
  }

  // Otwórz plik docelowy do zapisu, utwórz go, jeśli nie istnieje,
  // oraz wyczyść jego zawartość, jeśli istnieje. Zapisz jego deskryptor pliku.
  if ((out = open(dstFilePath, O_WRONLY | O_CREAT | O_TRUNC, dstMode)) == -1) {
    ret = -2;
    goto cleanup;
  }

  // Ustawienie czasu dostępu i modyfikacji dla pliku docelowego
  const struct timespec time[2] = {*dstAccessTime, *dstModificationTime};

  // Odczytaj dane z pliku źródłowego i zapisz je do pliku docelowego
  char buffer[BUFFER];
  ssize_t bytesRead, bytesWritten;

  while ((bytesRead = read(in, buffer, BUFFER)) > 0) {
    bytesWritten = write(out, buffer, bytesRead);
    if (bytesWritten != bytesRead) {
      ret = -4;
      goto cleanup;
    }
  }

  if (bytesRead == -1) {
    ret = -5;
    goto cleanup;
  }

  // Ustaw czas dostępu i modyfikacji dla pliku docelowego
  if (futimens(out, time) == -1) {
    ret = -3;
    goto cleanup;
  }

cleanup:
  // Zamknij deskryptory plików i zwróć wynik
  if (in != -1)
    close(in);
  if (out != -1)
    close(out);

  return ret;
}

// Funkcja copyBigFile służy do kopiowania dużych plików.
// Funkcja przyjmuje pięć argumentów:
// - srcFilePath: ścieżka pliku źródłowego, który chcemy skopiować
// - dstFilePath: ścieżka pliku docelowego, do którego chcemy skopiować plik
// źródłowy
// - fileSize: rozmiar pliku źródłowego w bajtach
// - dstMode: prawa dostępu, jakie zostaną nadane plikowi docelowemu
// - dstAccessTime: czas dostępu, jaki zostanie nadany plikowi docelowemu
// - dstModificationTime: czas modyfikacji, jaki zostanie nadany plikowi
// docelowemu
// Zwraca:
// - 0 w przypadku powodzenia
// - wartość ujemną w przypadku błędu

int copyBigFile(const char *srcFilePath, const char *dstFilePath,
                const unsigned long long fileSize, const mode_t dstMode,
                const struct timespec *dstAccessTime,
                const struct timespec *dstModificationTime) {
  // Wstępnie zapisujemy status oznaczający brak błędu.
  int ret = 0, in = -1, out = -1;
  // Otwieramy plik źródłowy w trybie tylko do odczytu
  if ((in = open(srcFilePath, O_RDONLY)) == -1)
    // Ustawiamy kod błędu. Po tym program natychmiast przechodzi na koniec
    // funkcji.
    ret = -1;
  else {
    // Pobierz czas modyfikacji pliku docelowego
    struct stat dstStat;
    if (stat(dstFilePath, &dstStat) == -1) {
      // Jeśli plik docelowy nie istnieje, utwórz go
      if (errno == ENOENT) {
        out = open(dstFilePath, O_WRONLY | O_CREAT | O_TRUNC, dstMode);
        if (out == -1) {
          // Ustawiamy kod błędu.
          ret = -2;
        }
      } else {
        ret = -2;
      }
    } else {
      // Jeśli plik docelowy istnieje, sprawdź, czy wymaga aktualizacji
      if (dstModificationTime->tv_sec >= dstStat.st_mtim.tv_sec &&
          dstModificationTime->tv_nsec >= dstStat.st_mtim.tv_nsec) {
        // Plik docelowy jest już aktualny, nie trzeba kopiować
        return ret;
      }
      out = open(dstFilePath, O_WRONLY | O_TRUNC);
      if (out == -1) {
        // Ustawiamy kod błędu.
        ret = -2;
      }
    }
    if (ret == 0) {
      // Ustaw atrybuty pliku docelowego
      if (fchmod(out, dstMode) == -1) {
        ret = -3;
      } else {
        // Utwórz mapowanie pamięci wirtualnej dla pliku źródłowego
        char *map;
        // Odwzorowujemy (mapujemy) w pamięci plik źródłowy w trybie do odczytu.
        // Jeżeli wystąpił błąd
        if ((map = mmap(0, fileSize, PROT_READ, MAP_SHARED, in, 0)) ==
            MAP_FAILED)
          // Ustawiamy kod błędu.
          ret = -4;
        else {
          // Wysyłamy jądru wskazówkę (poradę), że plik źródłowy będzie
          // odczytywany sekwencyjnie.
          if (madvise(map, fileSize, MADV_SEQUENTIAL) == -1)
            ret = 1;
          // Bufor odczytu pliku
          char buffer[BUFFER];
          unsigned long long b; // Numer bajtu w pliku źródłowym.
          char *position;       // Pozycja w buforze.
          size_t remainingBytes;
          ssize_t bytesWritten;
          for (b = 0; b + BUFFER < fileSize; b += BUFFER) {
            // Kopiujemy BUFFER (rozmiar bufora) bajtów ze zmapowanej pamięci
            // do bufora.
            memcpy(buffer, map + b, BUFFER);
            position = buffer;
            // Zapisujemy całkowitą bajtów pozostałych do zapisania, która
            // zawsze
            // jest równa rozmiarowi bufora.
            remainingBytes = BUFFER;
            // Dopóki liczby bajtów pozostałych do zapisania i bajtów zapisanych
            // w
            // aktualnej iteracji są niezerowe.
            while (remainingBytes != 0 &&
                   (bytesWritten = write(out, position, remainingBytes)) != 0) {
              // Obsłuż błędy zapisu
              if (bytesWritten == -1) {
                // Jeżeli funkcja write została przerwana odebraniem sygnału.
                // Blokujemy SIGUSR1 i SIGTERM na czas synchronizacji, więc te
                // sygnały nie mogą spowodować tego błędu.
                if (errno == EINTR)

                  continue; // Ponawiamy próbę zapisu.
                            // Jeżeli wystąpił inny błąd
                            // Ustawiamy kod błędu.
                ret = -6;
                b = ULLONG_MAX - BUFFER +
                    1; // Ustawiamy b aby przerwać pętlę for.

                break; // Przerywamy pętlę.
              }
              // O liczbę bajtów zapisanych w aktualnej iteracji zmniejszamy
              // liczbę pozostałych bajtów i przesuwamy pozycję w buforze.
              remainingBytes -= bytesWritten;
              position += bytesWritten;
            }
          }
          if (ret == 0) {
            // Zapisujemy liczbę bajtów z końca pliku, które nie zmieściły się w
            // jednym całym buforze.
            remainingBytes = fileSize - b;
            // Kopiujemy je ze zmapowanej pamięci do bufora.
            memcpy(buffer, map + b, remainingBytes);
            // Zapisujemy pozycję pierwszego bajtu bufora.
            position = buffer;
            // Dopóki liczby bajtów pozostałych do zapisania i bajtów zapisanych
            // w aktualnej iteracji są niezerowe.
            while (remainingBytes != 0 &&
                   (bytesWritten = write(out, position, remainingBytes)) != 0) {
              // Jeżeli wystąpił błąd w funkcji write.
              if (bytesWritten == -1) {
                // Jeżeli funkcja write została przerwana odebraniem sygnału.
                if (errno == EINTR)
                  // Ponawiamy próbę zapisu.
                  continue;
                // Ustawiamy kod błędu.
                ret = -7;
                // Przerywamy pętlę.
                break;
              }
              // O liczbę bajtów zapisanych w aktualnej iteracji zmniejszamy
              // liczbę pozostałych bajtów i
              remainingBytes -= bytesWritten;
              // przesuwamy pozycję w buforze.
              position += bytesWritten;
            }
          }
          munmap(map, fileSize);
        }
      }
      close(out);
    }
  }
  // Jeśli udało się otworzyć plik wejściowy, to go zamykamy
  if (in != -1)
    close(in);

  // Jeśli kopiowanie pliku zakończyło się sukcesem, to aktualizujemy czas
  // dostępu i modyfikacji pliku docelowego Otwieramy plik docelowy do zapisu i
  // ustawiamy czas dostępu i modyfikacji
  if (ret == 0) {
    int fd = open(dstFilePath, O_WRONLY);
    if (fd != -1) {
      futimens(fd, dstAccessTime);
      close(fd);
    } else {
      ret = -5;
    }
  }

  // Zwracamy status kopiowania pliku
  return ret;
}

/*
Funkcja removeFile służy do usuwania pliku o podanej ścieżce.
Jako argument przyjmuje wskaźnik do stałego łańcucha znaków reprezentującego
ścieżkę pliku. Funkcja zwraca wartość całkowitą oznaczającą powodzenie lub
niepowodzenie operacji usuwania pliku.
*/
int removeFile(const char *path) {
  int result =
      unlink(path); // Usuwamy plik o podanej ścieżce przy pomocy funkcji
                    // unlink() i przypisujemy wynik do zmiennej result.
  if (result == -1) { // Sprawdzamy, czy usunięcie pliku zakończyło się błędem.
    fprintf(
        stderr, "Error removing file: %s\n",
        path); // Wypisujemy komunikat o błędzie na standardowe wyjście błędów.
  }
  return result; // Zwracamy wartość zwróconą przez funkcję unlink(), która
                 // oznacza powodzenie lub niepowodzenie operacji usuwania
                 // pliku.
}
//Główna funkcja odpowiadająca za działanie Demona
void Daemon(char *source, char *destination, unsigned int sleepInterval,
            char isRecursive) {
  //tworzymy proces potomny
  pid_t pid = fork();
  //Jeśłi nie udało się stworzyć poprawnie procesu potomnego
  if (pid == -1) {
    //Wypisujemy na wyjście błędów błąd związąny z funkcją fork
    perror("fork");
    //zamykamy proces rodzicielski z kodem błędu
    exit(-1);
  }
  //Jeśli poprawnie stworzymy proces potomny to wypisujemy jego numer
  //i zamykamy proces rodzicielski
  else if (pid > 0) {
    printf("PID procesu potomnego %i\n", pid);
    exit(0);
  }
  //Dalsza część wykonuje się już jako proces potomny
  int returnCode = 0;
  char *sourcePath = NULL;
  char *destPath = NULL;
  //Maksymalna wielkość ścieżek w systemach z systemem plików ext to 4098 bajtów
  //Dlatego taką maksymalną pamięć rezerwujemy
  //W przypadku błędu funkcji malloc, to ustawiamy odpowiednie kody błędów.
  if ((sourcePath = malloc(sizeof(char) * 4096)) == NULL)
    returnCode = -1;
  else if ((destPath = malloc(sizeof(char) * 4096)) == NULL)
    returnCode = -2;
  //Wyznaczamy bezwzględną ściężkę do katalogu źródłowego i docelowego
  //W przypakdu błędu wypisujemy na wyjście błędu odpowiednią informajcę i ustawiamy kod błędu
  else if (realpath(source, sourcePath) == NULL) {
    perror("realpath-source");
    returnCode = -3;
  } else if (realpath(destination, destPath) == NULL) {
    perror("realpath-destination");
    returnCode = -4;
  }//Tworzymy nową sesję i grupę procesów, jęśli wystąpił błąd ustawiamy kod błędu
  else if (setsid() == -1)
    returnCode = -5;
  //zmieniamy katalog roboczy na root, w przypadku błedu ustawiamy odpowiedni kod
  else if (chdir("/") == -1)
    returnCode = -6;
  //Zamykamy strumienie o deskryptorach 0,1,2 czyli stdin,stdout i stderr
  else {
    if (close(0) == -1)
      returnCode = -7;
    if (close(1) == -1)
      returnCode = -8;
    if (close(2) == -1)
      returnCode = -9;
  }
  //Dalej przechodzimy tylko jeśli do tego momentu nie wystąpił żaden błąd
  if (returnCode >= 0) {
    //Zamykamy pozostałe deskryptory
    for (int i = 3; i < 1024; i++)
      close(i);
    //Tworzymy zbiór sygnałów
    sigset_t set;
    //Przekierowujemy deskryptory 0,1,2 na dev/null, jeśli błąd ustawiamy odpowiedni kod błędu
    //Najpierw deskrytor 0
    if (open("/dev/null", O_RDWR) == -1)
      returnCode = -10;
    //Teraz deskryptor 1 na to samo co desktryptor 0
    else if (dup(0) == -1)
      returnCode = -11;
    //Teraz deskryptor 2 na to samo co desktryptor 0
    else if (dup(0) == -1)
      returnCode = -12;
    //W tym momencie proces stał się Daemonem
    //Przypisujemy funkcję do ogsługi sygnału SIGUSR1, w przypadku błędu ustawiamy odpowiedni kod
    else if (signal(SIGUSR1, sigusr1Handler) == SIG_ERR)
      returnCode = -13;
    //Przypisujemy funkcję do ogsługi sygnału SIGTERM, w przypadku błędu ustawiamy odpowiedni kod
    else if (signal(SIGTERM, sigtermHandler) == SIG_ERR)
      returnCode = -14;
    //Inicjalizujemy zbiór sygnałów jako zbiór pusty, jeśli błąd ustawiamy odpowiedni kod błędu
    else if (sigemptyset(&set) == -1)
      returnCode = -15;
    //Dodajemy sygnały SIGUSR1 i SIGTERM do zbioru, w przypadku błędu ustawiamy odpowiedni kod
    else if (sigaddset(&set, SIGUSR1) == 1)
      returnCode = -16;
    else if (sigaddset(&set, SIGTERM) == 1)
      returnCode = -17;
    else {
      //ustawiamy długośc ścieżki katalogu źródłowego
      size_t srcPathLen = strlen(sourcePath);
      //Jeśli ścieżka nie jest zakończna '/' to dodajemy go na końcu i odpowiednio zwiększamy długość
      if (sourcePath[srcPathLen - 1] != '/')
        stringAdd(sourcePath, srcPathLen++, "/");
      //ustawiamy długośc ścieżki katalogu docelowego
      size_t destPathLen = strlen(destPath);
      //Jeśli ścieżka nie jest zakończna '/' to dodajemy go na końcu i odpowiednio zwiększamy długość
      if (destPath[destPathLen - 1] != '/')
        stringAdd(destPath, destPathLen++, "/");
      //Tworzymy zmienną synchronizer zadeklarowana w pliku SyncDaemon.h
      //wskazuje na odpowiednią funkcję synchronizującą w zależnosci od tego
      //czy wybrana została opcja z rekurencyjną synchronizacją katalogów
      synchronizer sync;
      if (isRecursive == 0) {
        sync = syncNonRecursively;
      } else
        sync = syncRecursively;
      //Ustawiamy flagi wymuszonego zakończenia i synchornizacji na 0
      //co oznacza, że początkowo nie ma wymuszenia
      stopDaemon = 0;
      forcedSyncro = 0;
      //Rozpoczynamy nieskończoną pętlę w trakcie której będzie wykonywana synchronizacja
      while (1) {
        //Jeśli nie wymuszono synchronizacji
        if (forcedSyncro == 0) {
          //Otwieramy log /var/log/syslog
          openlog("SyncDaemon", LOG_ODELAY | LOG_PID, LOG_DAEMON);
          //Wpisujemy informację o uśpieniu demona
          syslog(LOG_INFO, "sleep");
          //Zamykamy log
          closelog();
          //Usypiamy demona na czas określony w zmiennej sleepInterval
          unsigned int timeLeft = sleep(sleepInterval);
          //Otwieramy syslog i wpisujemy informację ile czasu spał demon
          openlog("SyncDaemon", LOG_ODELAY | LOG_PID, LOG_DAEMON);
          syslog(LOG_INFO, "wake_up - slept %u seconds",
                 sleepInterval - timeLeft);
          //zamykamy log
          closelog();
          //Jeśli wysłano SIGTERM kończymy pętlę
          if (stopDaemon == 1)
            break;
        }
        //Blokujemy odbieranie sygnałów SIGTERM I SIGUSR1 do czasu zakończenia synchronizacji
        //W przypadku błędu kończymy pętlę i ustawiamy odpowiedni kod błędu
        if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
          returnCode = -18;
          break;
        }
        //Uruchamiamy synchornizację wybraną wcześniej funkcję
        //W zmiennej status zapisujemy kod zwrócony przez funkcję
        //Wartość inna od 0 oznacza, że wystąpił błąd
        int status = sync(sourcePath, srcPathLen, destPath, destPathLen);
        //Zapisujemy do syslog informację o synchronizacji i jej statusie końcowym
        openlog("SyncDaemon", LOG_ODELAY | LOG_PID, LOG_DAEMON);
        syslog(LOG_INFO, "end of synchronization - %d", status);
        closelog();
        //Zerujemy flagę wymuszenia synchronizacji
        forcedSyncro = 0;
        //Odblokowujemy wcześniej zablokowane sygnały,
        //jeśli wystąpił błąd to ustawiamy kod błędu i wychodzimy z pętli
        if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1) {
          returnCode = -19;
          break;
        }
        //Jeśli któryś z zablokowanych sygnałów został odebrany w trakcie, gdy były zablokowane to
        //ich zostaną wywołane ich funkcje obsługi. Jeżeli została wymuszona synchronizacja, to zaraz potem
        //wykona się kolejna. Jeśli odebraliśmy tylko SIGTERM to demon się zakończy natomiast
        //Jeśli forcedSyncro == 1 && stopDaemon == 1 to demon zakończy się po następnej synchronizacji
        //pod warunkiem, że w trakcie później nie zostanie odebrany kolejny SIGUSR1, wtedy wykonają się powyższe procedury
        if (forcedSyncro == 0 && stopDaemon == 1)
          break;
      }
    }
  }
  //Przy błędzie lub zakończeniu demona, zwalniamy pamięć
  if (sourcePath != NULL)
    free(sourcePath);
  if (destPath != NULL)
    free(destPath);
  //Wpisujemy do syslog informację o zakończeniu demona wraz z kodem i zamykamy proces demona
  openlog("SyncDaemon", LOG_ODELAY | LOG_PID, LOG_DAEMON);
  syslog(LOG_INFO, "end of Deamon - %d", returnCode);
  closelog();
  exit(returnCode);
}
//Funkcja aktualizująca pliki w katalogu docelowym
int updateDestFiles(const char *srcDirPath, const size_t srcDirPathLength,
                    list *filesSrc, const char *dstDirPath,
                    const size_t dstDirPathLength, list *filesDst) {
  //Ustawiamy zmienne statusowe i błedu na wyzerowane
  int returnCode = 0;
  int status = 0;
  int compare;
  char *srcFilePath = NULL;
  char *dstFilePath = NULL;
  //Rezerwujemy pamięć na zmienne przychowujące ścieżki pliku źródłowego i docelowego
  if ((srcFilePath = malloc(sizeof(char) * 4096)) == NULL)
    // w razie błędu kończymy funkcję z błędem
    return -1;
  if ((dstFilePath = malloc(sizeof(char) * 4096)) == NULL) {
    //W razie błedu czyścimy wcześniej zarezerowaną pamięć i wychodzimy z błędem
    free(srcFilePath);
    return -2;
  }
  //Przepisujemy ścieżki katalogów źródłowych i docelowych jako startowe ścieżki ich plików
  strcpy(srcFilePath, srcDirPath);
  strcpy(dstFilePath, dstDirPath);
  //Ustawiamy na obecnie przeglądane pliki pierwsze elementy list z plikami docelowymi i źródłowymi
  element *currentSrc = filesSrc->first;
  element *currentDst = filesDst->first;
  struct stat srcFile, dstFile;
  //otwieramy /var/log/syslog
  openlog("SyncDaemon", LOG_ODELAY | LOG_PID, LOG_DAEMON);
  char *srcFileName;
  char *dstFileName;
  //Dopóki jeszcze są do przejrzenia zarówno pliki w katalogu docelowym i źródłowym
  while (currentDst != NULL && currentSrc != NULL) {
    srcFileName = currentSrc->value->d_name;
    dstFileName = currentDst->value->d_name;
    //porównujemy nazwy plików w posortowanych listach
    compare = strcmp(srcFileName, dstFileName);
    //Jeśli plik źródłowy jest później w porządku leksykograficznym niż docelowy
    if (compare > 0) {
      //Przchodzimy do usunięcia pliku docelowego, tym celu wyznaczamy ścieżkę pliku
      stringAdd(dstFilePath, dstDirPathLength, dstFileName);
      //usuwamy plik i zapisujemy kod wyjściowy funkcji
      status = removeFile(dstFilePath);
      //Zapisujemy w syslog informacje o statusie usuwania
      syslog(LOG_INFO, "delete file %s -status:%d\n", dstFilePath, status);
      //Jeśli wystąpił błąd przy usuwaniu, ustawiamy kod błędu
      if (status != 0)
        returnCode = 1;
      //Przechodzimy do następnego pliku docelowego
      currentDst = currentDst->next;
    } else {
      //Wyznaczamy ścieżkę pliku źródłowego
      stringAdd(srcFilePath, srcDirPathLength, srcFileName);
      //Odczytujemy metadane pliku źródłowego, jeśli wystąpił bład i tak musimy sprawdzić dalej compare
      //aby prawidłowo poruszać się po liście
      if (stat(srcFilePath, &srcFile) == -1) {
        if (compare < 0) {
          //Zapisujemy informację, że nie możne było skopiować pliku i zapisujemy wartość errno
          syslog(LOG_INFO, "failed copying file %s to directory %s-status:%d\n",
                 srcFilePath, dstDirPath, errno);
          returnCode = 2;
        } else {
          //Zapisujemy informację, że nie było dostepu do metadanych pliku i zapisujemy wartość errno
          syslog(LOG_INFO,
                 "failed metadata check of source file %s -status:%d\n",
                 srcFilePath, errno);
          //Przechodzimy do następnego pliu docelowego
          currentDst = currentDst->next;
          returnCode = 3;
        }
        //Przechodzimy do następnego pliku źródłowego
        currentSrc = currentSrc->next;
      } else {
        //Przy poprawnym odczytaniu metadanych i plik źródłowy jest wcześniejszy
        //Jeśli plik źródłowy jest później w porządku leksykograficznym niż docelowy
        if (compare < 0) {
          //Wyznaczamy ścieżkę pliku źródłowego
          stringAdd(dstFilePath, dstDirPathLength, srcFileName);
          //Jeśli wielkośc pliku jest mniejsza niż próg dużego pliku, kopiujemy jako mały
          if (srcFile.st_size < copyThreshold)
            status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode,
                                   &srcFile.st_atim, &srcFile.st_mtim);
          else
            //Jeśli wielkośc pliku jest większa niż próg dużego pliku, kopiujemy jako duży
            status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size,
                                 srcFile.st_mode, &srcFile.st_atim,
                                 &srcFile.st_mtim);
          //Zapisujemy informację o skopiowaniu pliku wraz ze statusem
          syslog(LOG_INFO, "copying file %s to directory %s -status:%d\n",
                 srcFilePath, dstDirPath, status);
          //Jeśli wystąpił błąd to zpaisujemy kod błędu
          if (status != 0)
            returnCode = 4;
          //Przechodzimy do następnego pliku źródłowego
          currentSrc = currentSrc->next;

        } else {
          //Wyznaczamy ścieżkę pliku docelowego
          stringAdd(dstFilePath, dstDirPathLength, dstFileName);
          //Sprawdzamy metadane pliku docelowego, w przypadku błędu 
          //zapisujemy kod błędu i inforamcje w logu
          if (stat(dstFilePath, &dstFile) == -1) {
            syslog(LOG_INFO,
                   "failed metadata check of destination file %s -status:%d\n",
                   dstFilePath, errno);
            returnCode = 5;
          }//Jeśli nie wystąpił błąd to sprawdzamy czasy modyfikacji obu plików, jeśli się różnią
          //to wykonujemy kopiowanie w zależnosci od wielkosci pliku 
          else if (srcFile.st_mtim.tv_nsec != dstFile.st_mtim.tv_nsec ||
                     srcFile.st_mtim.tv_sec != dstFile.st_mtim.tv_sec) {
            if (srcFile.st_size < copyThreshold)
              status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode,
                                     &srcFile.st_atim, &srcFile.st_mtim);
            else
              status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size,
                                   srcFile.st_mode, &srcFile.st_atim,
                                   &srcFile.st_mtim);
            //Zapisujemy informacje o kopiowaniu wraz ze statusem do syslog
            syslog(LOG_INFO, "replacing file %s to directory %s -status:%d\n",
                   srcFilePath, dstDirPath, status);
            //Jeśli błąd to ustawiamy kod błędu
            if (status != 0)
              returnCode = 6;
          }//Porównujemy tryby plików jeśli są różne to próbujemy je modyfikować
          //W przypadku niepowodzenia zpaisujemy informacje w logach 
          else if (srcFile.st_mode != dstFile.st_mode) {
            if (chmod(dstFilePath, srcFile.st_mode) == -1) {
              syslog(LOG_INFO,
                     "failed copying file rights from %s to %s -status:%d\n",
                     srcFilePath, dstDirPath, errno);
              returnCode = 7;
            } else {
              status = 0;
              syslog(LOG_INFO, "copying file rights from %s to %s -status:%d\n",
                     srcFilePath, dstDirPath, status);
            }
          }
          //Przechodzimy do nastepnej pary plików
          currentDst = currentDst->next;
          currentSrc = currentSrc->next;
        }
      }
    }
  }
  //Po zakończeniu się jednej lub obu list plików
  //Jeśli zostały jakieś pliki w katalogu docelowaym to usuwamy je
  //zapisujemy informacje do logów, tak samo jak poprzednio
  while (currentDst != NULL) {
    dstFileName = currentDst->value->d_name;
    stringAdd(dstFilePath, dstDirPathLength, dstFileName);
    status = removeFile(dstFilePath);
    syslog(LOG_INFO, "deleting file %s - status %d\n", dstFilePath, status);
    if (status != 0)
      returnCode = 8;
    currentDst = currentDst->next;
  }
  //Jeśli zostały jakieś pliki w katalogu źródłowym to kopiujemy je
  //zapisujemy informacje do logów, na takich samych zasadach jak poprzednio
  while (currentSrc != NULL) {
    srcFileName = currentSrc->value->d_name;
    stringAdd(srcFilePath, srcDirPathLength, srcFileName);
    if (stat(srcFilePath, &srcFile) == -1) {
      syslog(LOG_INFO, "failed copying file %s to directory %s-status:%d\n",
             srcFilePath, dstDirPath, errno);
      returnCode = 9;
    } else {
      stringAdd(dstFilePath, dstDirPathLength, srcFileName);
      if (srcFile.st_size < copyThreshold)
        status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode,
                               &srcFile.st_atim, &srcFile.st_mtim);
      else
        status =
            copyBigFile(srcFilePath, dstFilePath, srcFile.st_size,
                        srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
      syslog(LOG_INFO, "copying file %s to directory %s -status:%d\n",
             srcFilePath, dstDirPath, status);
      if (status != 0)
        returnCode = 11;
    }
    currentSrc = currentSrc->next;
  }
  //Czyścimy pamięć, zamykamy logi i zwracamy kod zakończenia(błędu)
  free(dstFilePath);
  free(srcFilePath);
  closelog();
  return returnCode;
}
//Funkcja aktualizująca podkatalogi w katalogu docelowym
int updateDestDir(const char *srcDirPath, const size_t srcDirPathLength,
                  list *subDirsSrc, const char *dstDirPath,
                  const size_t dstDirPathLength, list *subDirsDst,
                  char *isReady) {
  //Ustawiamy zmienne statusowe i błedu na wyzerowane
  int status = 0;
  int returnCode = 0;
  int compare;
  int iterator = 0;
  //Rezerwujemy pamięć na zmienne przychowujące ścieżki katalogu źródłowego i docelowego
  char *srcSubDirPath = NULL;
  if ((srcSubDirPath = malloc(sizeof(char) * 4096)) == NULL)
  //W razie błedu wychodzimy z kodem błędu
    return -1;
  char *dstSubDirPath = NULL;
  if ((dstSubDirPath = malloc(sizeof(char) * 4069)) == NULL) {
    //W razie błedu czyścimy wcześniej zarezerowaną pamięć i wychodzimy z błędem
    free(srcSubDirPath);
    return -2;
  }
 //Przepisujemy ścieżki katalogów źródłowych i docelowych jako startowe ścieżki ich podkatalogów
  strcpy(dstSubDirPath, dstDirPath);
  strcpy(srcSubDirPath, srcDirPath);
  //Ustawiamy na obecnie przeglądane katalogi pierwsze elementy list z katalogami docelowymi i źródłowymi
  element *currentSrc = subDirsSrc->first;
  element *currentDst = subDirsDst->first;
  struct stat srcSubDir, dstSubDir;
  //Otwieramy /var/log/syslog
  openlog("SyncDaemon", LOG_ODELAY | LOG_PID, LOG_DAEMON);
  char *srcSubDirName;
  char *dstSubDirName;
  //Dopóki jeszcze są do przejrzenia zarówno podkatalogi w katalogu docelowym i źródłowym
  while (currentDst != NULL && currentSrc != NULL) {
    srcSubDirName = currentSrc->value->d_name;
    dstSubDirName = currentDst->value->d_name;
    //porównujemy nazwy podkatalogów w posortowanych listach
    compare = strcmp(srcSubDirName, dstSubDirName);
    //Jeśli podkatalog źródłowy jest później w porządku leksykograficznym niż docelowy
    if (compare > 0) {
      //Dopisujemy do ścieżki nazwę katalogu docelowego 
      size_t len = addtoSubDirName(dstSubDirPath, dstDirPathLength, dstSubDirName);
      //Usuwamy katalog docelowy rekurencyjnie
      status = removeDirRecursively(dstSubDirPath, len);
      //Zapisujemy informacje o usunięciu podkatalogu docelowego w logu wraz ze statusem 
      syslog(LOG_INFO, "delete directory %s - status %d\n", dstSubDirPath,
             status);
      //Jeśli błąd ustawiamy odpowiedni kod błedu
      if (syslog != 0)
        returnCode = 1;
      //Przechodzimy do następnego podkatalogu docelowego
      currentDst = currentDst->next;
    } else {
      //Dopisujemy do ścieżki nazwę katalogu źródłowego
      stringAdd(srcSubDirPath, srcDirPathLength, srcSubDirName);
      //Odczytujemy metadane podakatalogu, jeśli się nie udało 
      if (stat(srcSubDirPath, &srcSubDir) == -1) {
        if (compare < 0) {
          //informujemy w logu o nieudanym kopiowaniu i zaznaczamy, w isReady, że katalog nie jest gotowy do synchronizacji
          syslog(LOG_INFO, "failed copying directory %s - status %d\n",
                 dstSubDirPath, errno);
          isReady[iterator] = 0;
          //Przysuwmay zmienną do iteracji dalej
          iterator++;
          returnCode = 2;
        } else {
          //informujemy w logu o nieudanym odczytaniu metadanych
          // zakładamy, że mając takie same uprawnienia, więc w isReady, zaznaczmay że katalog jest gotowy do synchronizacji
          syslog(LOG_INFO,
                 "failed accessing metadata src directory %s - status %d\n",
                 srcSubDirPath, errno);
          isReady[iterator] = 1;
          //Przysuwmay zmienną do iteracji dalej
          iterator++;
          returnCode = 3;
          //Przechodzimy do następnego podkatalogu docelowego
          currentDst = currentDst->next;
        }
        //Przechodzimy do następnego podkatalogu źródłowego
        currentSrc = currentSrc->next;
      } else {
        //Jeśli odczytaliśmy metadane katalogu źródłowego to 
        if (compare < 0) {
          //Gdy podkatalog źródłowy jest wczesniej w porządku leksykograficznym
          //to tworzymy nowy katalog w miejscu docelowym, na wyznaczonej ścieżce
          stringAdd(dstSubDirPath, dstDirPathLength, srcSubDirName);
          status = createEmptyDir(dstSubDirPath, srcSubDir.st_mode);
          //Jeśli pojawił się błąd to zpisujemy to w logu i oznaczamy brak gotowości do synchronizacji
          if (status != 0) {
            isReady[iterator] = 0;
            iterator++;
            returnCode = 4;
            syslog(LOG_INFO, "failed creating directory %s - status %d\n",
                   dstSubDirPath, status);
          } else {
            //W przeciwnym wypadku informujemy o utworzeniu katalogu i zazczamy, że jest gotowy
            isReady[iterator] = 1;
            iterator++;
            syslog(LOG_INFO, "created directory %s - status %d\n",
                   dstSubDirPath, status);
          }
          //Przechodzimy do następnego podkatalogu źródłowego
          currentSrc = currentSrc->next;
        } else {
          //Jeśli nie uda się porównać uprawnień podkatalogów docelowego i źródłowego
          //to dalej będzie próbować synchronizacji, więc isReady ustawiamy na 1
          isReady[iterator] = 1;
          iterator++;
          //Dopisujemy nazwę podkatalogu docelowego do jego ścieźki
          stringAdd(dstSubDirPath, dstDirPathLength, dstSubDirName);
          if (stat(dstSubDirPath, &dstSubDir) == -1) {
            //Jeśli nie udało się odczytac meta danych to zapisujemy informacje o błędzie
            syslog(LOG_INFO,
                   "failed accessing metadata dst directory %s - status %d\n",
                   dstSubDirPath, errno);
            returnCode = 6;
          }//Jeśli mają inne uprawnienia
          else if (dstSubDir.st_mode != srcSubDir.st_mode) {
            if (chmod(dstSubDirPath, srcSubDir.st_mode) == -1) {
              //Jeśli nie udało się zmienić uprawnień podkatalogu docelowego to zapisujemy to w syslog
              returnCode = 7;
              syslog(
                  LOG_INFO,
                  "failed copying directory rights from %s to %s -status:%d\n",
                  srcSubDirPath, dstSubDirPath, errno);
            } else {
              //zerujemy status i zapisujemy, że udało się zmienić prawa podkatalogu
              status = 0;
              syslog(LOG_INFO,
                     "copying directory rights from %s to %s -status:%d\n",
                     srcSubDirPath, dstSubDirPath, status);
            }
          }
          //Przechodzimy do następej pary podkatalogów
          currentDst = currentDst->next;
          currentSrc = currentSrc->next;
        }
      }
    }
  }
  //Po zakończeniu się jednej lub obu list podkatalogów
  //Jeśli zostały jakieś podkatalogi w katalogu docelowaym to usuwamy je
  //zapisujemy informacje do logów, tak samo jak poprzednio
  while (currentDst != NULL) {
    dstSubDirName = currentDst->value->d_name;
    size_t len =
        addtoSubDirName(dstSubDirPath, dstDirPathLength, dstSubDirName);
    status = removeDirRecursively(dstSubDirPath, len);
    if (status != 0) {
      syslog(LOG_INFO, "failed deleting directory %s - status %d\n",
             dstSubDirPath, status);
      returnCode = 8;
    } else
      syslog(LOG_INFO, "deleting directory %s - status %d\n", dstSubDirPath,
             status);
  }
  //Jeśli zostały jakieś podkatalogi w katalogu źródłowym to kopiujemy je
  //zapisujemy informacje do logów, na takich samych zasadach jak poprzednio
  while (currentSrc != NULL) {
    srcSubDirName = currentSrc->value->d_name;
    stringAdd(srcSubDirPath, srcDirPathLength, srcSubDirName);
    if (stat(srcSubDirPath, &srcSubDir) == -1) {
      syslog(LOG_INFO, "failed accessing metadata of directory %s -status %d\n",
             srcSubDirPath, errno);
      isReady[iterator] = 0;
      iterator++;
      returnCode = 9;
    } else {
      stringAdd(dstSubDirPath, dstDirPathLength, srcSubDirName);
      status = createEmptyDir(dstSubDirPath, srcSubDir.st_mode);
      if (status != 0) {
        isReady[iterator] = 0;
        iterator++;
        returnCode = 10;
        syslog(LOG_INFO, "failed creating directory %s -status %d\n",
               dstSubDirPath, status);
      } else {
        isReady[iterator] = 1;
        iterator++;
        syslog(LOG_INFO, "created directory %s -status %d\n", dstSubDirPath,
               status);
      }
    }
    currentSrc = currentSrc->next;
  }
  //Czyścimy pamięć, zamykamy logi i zwracamy kod zakończenia(błędu)
  free(dstSubDirPath);
  free(srcSubDirPath);
  return returnCode;
}
int syncNonRecursively(const char *sourcePath, const size_t sourcePathLength,
                       const char *destinationPath,
                       const size_t destinationPathLength) {
  int ret = 0;
  DIR *dirS = opendir(sourcePath);
  if (dirS == NULL) {
    ret = -1;
    goto cleanup;
  }

  DIR *dirD = opendir(destinationPath);
  if (dirD == NULL) {
    ret = -2;
    goto cleanup;
  }

  list filesS, filesD;
  list_initialize(&filesS);
  list_initialize(&filesD);
  if (listFiles(dirS, &filesS) < 0) {
    ret = -3;
    goto cleanup;
  }
  if (listFiles(dirD, &filesD) < 0) {
    ret = -4;
    goto cleanup;
  }
  listSort(&filesS);
  listSort(&filesD);

  if (updateDestFiles(sourcePath, sourcePathLength, &filesS, destinationPath,
                      destinationPathLength, &filesD) != 0) {
    ret = -5;
    goto cleanup;
  }

cleanup:
  if (dirS != NULL)
    closedir(dirS);

  if (dirD != NULL)
    closedir(dirD);

  clear(&filesS);
  clear(&filesD);

  return ret;
}

int syncRecursively(const char *sourcePath, const size_t sourcePathLength,
                    const char *destinationPath,
                    const size_t destinationPathLength) {
  int ret = 0;
  DIR *dirS = opendir(sourcePath);
  DIR *dirD = opendir(destinationPath);
  if (dirS == NULL)
    ret = -1;
  else if (dirD == NULL)
    ret = -2;
  else {
    list filesS, subdirsS, filesD, subdirsD;
    list_initialize(&filesS);
    list_initialize(&subdirsS);
    list_initialize(&filesD);
    list_initialize(&subdirsD);

    if (listFilesAndDir(dirS, &filesS, &subdirsS) < 0)
      ret = -3;
    else if (listFilesAndDir(dirD, &filesD, &subdirsD) < 0)
      ret = -4;
    else {
      listSort(&filesS);
      listSort(&filesD);

      if (updateDestFiles(sourcePath, sourcePathLength, &filesS,
                          destinationPath, destinationPathLength,
                          &filesD) != 0) {
        ret = -5;
      }

      clear(&filesS);
      clear(&filesD);

      listSort(&subdirsS);
      listSort(&subdirsD);

      char *isReady = malloc(sizeof(char) * subdirsS.number);
      if (isReady == NULL)
        ret = -6;
      else {
        if (updateDestDir(sourcePath, sourcePathLength, &subdirsS,
                          destinationPath, destinationPathLength, &subdirsD,
                          isReady) != 0)
          ret = -7;

        clear(&subdirsD);

        char *nextSourcePath = malloc(sizeof(char) * PATH_MAX);
        char *nextDestinationPath = malloc(sizeof(char) * PATH_MAX);

        if (nextSourcePath == NULL)
          ret = -8;
        else if (nextDestinationPath == NULL)
          ret = -9;
        else {
          strcpy(nextSourcePath, sourcePath);
          strcpy(nextDestinationPath, destinationPath);

          element *curS = subdirsS.first;
          unsigned int i = 0;

          while (curS != NULL) {
            if (isReady[i++] == 1) {
              size_t nextSourcePathLength = addtoSubDirName(
                  nextSourcePath, sourcePathLength, curS->value->d_name);
              size_t nextDestinationPathLength =
                  addtoSubDirName(nextDestinationPath, destinationPathLength,
                                  curS->value->d_name);

              if (syncRecursively(nextSourcePath, nextSourcePathLength,
                                  nextDestinationPath,
                                  nextDestinationPathLength) < 0)
                ret = -10;
            }
            curS = curS->next;
          }
        }
        free(isReady);

        if (nextSourcePath != NULL)
          free(nextSourcePath);
        if (nextDestinationPath != NULL)
          free(nextDestinationPath);
      }
    }
    clear(&subdirsS);
    if (subdirsD.number != 0)
      clear(&subdirsD);
  }
  if (dirS != NULL)
    closedir(dirS);
  if (dirD != NULL)
    closedir(dirD);

  return ret;
}
