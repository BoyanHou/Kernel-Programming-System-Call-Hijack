#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> // for strcmp()

void execv_line() {
  char* line = NULL;
  size_t size;
  int ctn = 1;
  while (ctn) {
    getline(&line, &size, stdin);
    if (strcmp(line, "q\n") == 0) {
      ctn = 0;
    } else {
      system(line);
    }
  }
}

int main() {
  printf("sneaky_process pid = %d\n", getpid());  // print pid
  system("sudo insmod sneaky_mod.ko");  // load mod

  execv_line(); // execute terminal commands
  
  system("sudo rmmod sneaky_mod");     // un-load mod
  
  return EXIT_SUCCESS;
}
