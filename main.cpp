
#include "miosix.h"
#include <cstdio>
#include <dirent.h>
#include <errno.h>

using namespace std;
using namespace miosix;

void tryReadFile() {
  iprintf("Trying to read /sd/test.txt\n");
  // Try to read the file
  FILE *fp = fopen("/sd/test.txt", "r");
  // Check file is open
  if (fp != NULL) {
    char buf[100];
    fgets(buf, 100, fp);
    iprintf("File content: %s\n", buf);
    fclose(fp);
  } else {
    perror("Error opening /sd/test.txt in readmode");
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
    iprintf("\tFound file: %s\n", ent->d_name);
  }
  closedir(dir);

  tryReadFile();

  // Open a file and write something
  iprintf("Writing to /sd/test.txt\n");
  FILE *fp = fopen("/sd/test.txt", "w");
  // Check file is open
  if (fp == NULL) {
    perror("Error opening /sd/test.txt in writemode");
    return -1;
  }
  fprintf(fp, "Hello world!\n");
  fclose(fp);

  tryReadFile();

  iprintf("Main end\n");
}
