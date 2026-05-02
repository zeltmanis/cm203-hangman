/*
 * Companion to the main hangman exploit.
 *
 * The main game overflows gs.name into gs.max_wrong — but both fields
 * live inside the same GameState struct, so AddressSanitizer treats
 * the whole struct as one allocation and DOES NOT flag the overflow.
 *
 * This program demonstrates the case ASan does catch: a stack-local
 * buffer with redzones around it. Same bug pattern (unsafe strcpy of
 * an attacker-controlled string), different memory layout — ASan fires
 * because the overflow crosses an allocation boundary.
 */

#include <stdio.h>
#include <string.h>

static void greet(const char *name) {
    char banner[16];           /* stack-local — ASan puts redzones around it */
    strcpy(banner, name);      /* unsafe: no length check */
    printf("=== welcome, %s ===\n", banner);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <name>\n", argv[0]);
        return 1;
    }
    greet(argv[1]);
    return 0;
}
