
#include "miosix.h"
#include <cstdio>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include <vector>

using namespace std;
using namespace miosix;

void tryReadFile(int fileNumber) {
  char filename[20];
  snprintf(filename, sizeof(filename), "/sd/test%d.txt", fileNumber);
  iprintf("Trying to read %s\n", filename);
  // Try to read the file
  FILE *fp = fopen(filename, "r");
  // Check file is open
  if (fp != NULL) {
    char buf[100];
    fgets(buf, 100, fp);
    iprintf("'%s' content: %s\n", filename, buf);
    fclose(fp);
  } else {
    perror(filename);
  }
}

void tryWriteFile(int fileNumber) {
  char filename[20];
  snprintf(filename, sizeof(filename), "/sd/test%d.txt", fileNumber);
  iprintf("Trying to write %s\n", filename);
  // Try to write the file
  FILE *fp = fopen(filename, "w");
  // Check file is open
  if (fp != NULL) {
    fprintf(fp, "Hello world %d!", fileNumber);
    fclose(fp);
  } else {
    perror(filename);
  }
}

int main() {
  iprintf("Main start\n");

  // Try to opena non-existing directory

  DIR *dir0 = opendir("/fcguyidsa");
  if (dir0 == NULL) {
    perror("Couldn't open /fcguyidsa directory, AS EXPECTED");
  } else {
    iprintf("Opened /fcguyidsa directory???????");
    closedir(dir0);
  }

  // List all the files in the root directory
  DIR *dir = opendir("/sd");
  if (dir == NULL) {
    perror("Error opening /sd directory");
    return -1;
  }
  struct dirent *ent;
  iprintf("Files in /sd:\n");
  while ((ent = readdir(dir)) != NULL) {
    char filename[200];
    snprintf(filename, sizeof(filename), "/sd/%s", ent->d_name);
    iprintf("\tFound file: '%s'\n", filename);
    int foundFileFd = open(filename, O_RDONLY);
    iprintf("\t\tFile descriptor: %d\n", foundFileFd);
    if (foundFileFd < 0) {
      perror("Error opening file");
    } else {
      struct stat pstat;
      if (fstat(foundFileFd, &pstat) == 0) {
        iprintf("\t\tFile size: %lld\n", pstat.st_size);
      } else {
        perror("Error getting file size");
      }
      close(foundFileFd);
    }
  }
  closedir(dir);

  tryReadFile(11);
  tryWriteFile(11);
  tryReadFile(11);

  iprintf("--- Begin write test on multithreading ---\n");
  vector<thread> threads;
  for (int i = 0; i < MAX_OPEN_FILES - 3; i++) {
    threads.emplace_back(thread(tryWriteFile, i));
  }

  for (auto &t : threads) {
    t.join();
  }

  iprintf("--- End write test on multithreading--- \n");

  iprintf("--- Begin read test on multithreading ---\n");
  threads.clear();

  for (int i = 0; i < MAX_OPEN_FILES - 3; i++) {
    threads.emplace_back(thread(tryReadFile, i));
  }

  for (auto &t : threads) {
    t.join();
  }

  iprintf("--- End read test on multithreading ---\n");

  iprintf("--- Begin mixed read/write test on multithreading ---\n");
  threads.clear();
  for (int i = 0; i < (MAX_OPEN_FILES - 3) / 2; i++) {
    threads.emplace_back(thread(tryReadFile, i));
    threads.emplace_back(thread(tryWriteFile, i));
  }

  for (auto &t : threads) {
    t.join();
  }

  iprintf("--- End mixed read/write test on multithreading ---\n");

  iprintf("Main end\n");
}
