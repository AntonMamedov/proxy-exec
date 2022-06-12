#include "iostream"
#include "unistd.h"
char* const argv[] = {"1", "2", NULL};
char* const env[] = {NULL};
int main() {
    int a = execve("/home/amamedov/dev/university/proxy-exec/tests/test", argv, env);
  //  int a = execve("/home/amamedov/dev/university/proxy-exec/tests/build/dev_test", argv, env);
    std::cout << errno;
    return 0;
}