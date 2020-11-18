#include <stdio.h>
#include "output.h"
#include <unistd.h>

int main(int argc, char *argv[]) {

   printf("My source is: \n");
   write(STDOUT_FILENO, printsource_c, sizeof(printsource_c));

}

