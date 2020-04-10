#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> // for strcmp()

int main() {
  printf("sneaky_process pid = %d\n", getpid());
  system("sudo insmod sneaky_mod.ko");

  char* line = NULL;
  size_t size;
  int ctn = 1;
  while (ctn) {
    getline(&line, &size, stdin);
    printf("%s", line);
    if (strcmp(line, "q") == 0) {
      ctn = 0;
    } else {
      system(line);
    }
  }
  
  system("sudo rmmod sneaky_mod");
  
  return EXIT_SUCCESS;
}
