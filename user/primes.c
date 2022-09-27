#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{

int NUMBERS_SIZE = 34, pipePrime[2], nextPipe[2], isPrime, primeNum, pid;

pipe(pipePrime);
int readFromParent = pipePrime[0];

if ((pid = fork()) == -1) {
    fprintf(2, "forking went wrong");
}
if (pid > 0) {
    for (int i = 2; i < NUMBERS_SIZE + 2; i++) {
        write(pipePrime[1], &i, sizeof(int));
    }
    close(pipePrime[1]);
    close(readFromParent);
    wait(0);

} else if (pid == 0) {
    close(pipePrime[1]);

    while (read(readFromParent, &primeNum, sizeof(int)) > 0) {
        fprintf(1, "prime %d\n", primeNum);
        if (pipe(nextPipe) == -1) {
            fprintf(2, "piping went wrong");
        }
        if ((pid = fork()) == -1) {
            fprintf(2, "forking went wrong");
        }
        if (pid == 0) {
            close(readFromParent);
            readFromParent = nextPipe[0];
            close(nextPipe[1]);
        } else {
            close(nextPipe[0]);

            while(read(readFromParent, &isPrime, sizeof(int)) > 0) {
                if (isPrime % primeNum != 0) {
                    write(nextPipe[1], &isPrime, sizeof(int));
                }
            }
            close(nextPipe[1]);
            
        }
    }
}
wait(0);
exit(0);
}
