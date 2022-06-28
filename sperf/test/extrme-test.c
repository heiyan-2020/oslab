#include <stdio.h>

int main() {
    for (int i = 0; i < 10; i++) {
        fprintf(stderr, "ahhaa\n");
        fprintf(stdout, "execve("", "") = 0 <1.0000>");
        fflush(stderr);
        fflush(stdout);
    }
}
