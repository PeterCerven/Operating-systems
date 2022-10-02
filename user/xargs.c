#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int
main(int argc, char *argv[]) {
    int pid;
    char *array[50];
    char buf[512], *p;
    char letter;
    int letters;
    int currPos, startPos;
    char *s;

    for (int i = 1; i < argc; i++ ) {
        array[i - 1] = argv[i];
    }

    // for (int i = 0; i < argc - 1; i++ ) {
    //     printf("%s\n", array[i]);
    // }
    currPos = argc - 1;
    startPos = currPos;
    letters = 0;
    p = buf;
    s = buf;

    while (read(0, &letter, 1) > 0) {
        if (letter == '\n') {
            // zavolanie prikazu
            *p++ = '\0';
            array[currPos] = s;
            s = p;
            currPos++;
            letters = 0;

            pid = fork(); 
            if (pid == 0) {
                //array[currPos] = "0";
                exec(argv[1], array);
            } else {
                wait(0);
                currPos = startPos;
                //array[currPos] = "0";
            }
        } else if (letter == ' ') {
            //pridavanie slova do array
            *p++ = '\0';
            array[currPos] = s;
            s = p;
            currPos++;
            letters = 0;
        } else {
            // pridavat pismena
            *p++ = letter;
            letters++;
        }
    }

    exit(0);
}
