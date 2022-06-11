#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
struct program_argument_arg {
    int argc;
    char** argv;
};

char* argv[3] = {"arg1", "arg2", "arg3"};

int main() {
    struct program_argument_arg pga = {
            .argc = 3,
            .argv = argv
    };

    int fd = open("/dev/pec_device", O_RDWR);
    char *errorbuf = strerror(errno);
    printf("%s\n", errorbuf);
    printf("%d\n", fd);
    int ret = ioctl(fd, 1, &pga);
    printf("%d\n", ret);
    return 0;
}

