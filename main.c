/* main.c - entry point for the text-based Yukon Solitaire */

#include "yukon.h"

int main(void)
{
    srand((unsigned int)time(NULL)); /* seed random so shuffles differ each run */

    GameState gs;
    game_init(&gs);

    char input[512];

    /* Main loop: show the board, read a command, run it, repeat */
    while (1) {
        display(&gs);

        if (fgets(input, sizeof(input), stdin) == NULL) break;

        /* Trim trailing whitespace and newlines */
        int length = (int)strlen(input);
        while (length > 0 &&
               (input[length - 1] == '\n' || input[length - 1] == '\r' ||
                input[length - 1] == ' '  || input[length - 1] == '\t')) {
            input[--length] = '\0';
        }

        if (length == 0) {
            strcpy(gs.last_command, "");
            strcpy(gs.last_message, "");
            continue;
        }

        strncpy(gs.last_command, input, sizeof(gs.last_command) - 1);
        gs.last_command[sizeof(gs.last_command) - 1] = '\0';

        /* QQ: exit the whole program */
        if (strcmp(input, "QQ") == 0) {
            printf("Goodbye!\n");
            break;
        }

        /* Q: quit current game, return to startup */
        if (strcmp(input, "Q") == 0) {
            cmd_q(&gs);
            continue;
        }

        /* During play, only move commands are accepted */
        if (gs.phase == PHASE_PLAY) {
            if (strstr(input, "->") != NULL) {
                cmd_move(&gs, input);
                if (check_win(&gs)) {
                    strcpy(gs.last_message, "*** YOU WIN! *** Type Q to restart or QQ to quit.");
                }
            } else {
                strcpy(gs.last_message, "In PLAY phase: use move commands (e.g. C3->C4) or Q / QQ.");
            }
            continue;
        }

        /* --- Startup commands --- */

        /* LD [filename] - load a deck */
        if (strncmp(input, "LD", 2) == 0 && (input[2] == ' ' || input[2] == '\0')) {
            char filename[256] = {0};
            if (sscanf(input + 2, " %255s", filename) == 1) cmd_ld(&gs, filename);
            else                                              cmd_ld(&gs, NULL);
            continue;
        }

        /* SW - swap the two halves of the deck */
        if (strcmp(input, "SW") == 0) {
            cmd_sw(&gs);
            continue;
        }

        /* SI [n] - interleave shuffle, optionally at position n */
        if (strncmp(input, "SI", 2) == 0 && (input[2] == ' ' || input[2] == '\0')) {
            int n = 0;
            if (sscanf(input + 2, " %d", &n) == 1) cmd_si(&gs, n, 0);
            else                                     cmd_si(&gs, 0, 1);
            continue;
        }

        /* SR - random shuffle */
        if (strcmp(input, "SR") == 0) {
            cmd_sr(&gs);
            continue;
        }

        /* SD [filename] - save deck to a file */
        if (strncmp(input, "SD", 2) == 0 && (input[2] == ' ' || input[2] == '\0')) {
            char filename[256] = {0};
            if (sscanf(input + 2, " %255s", filename) == 1) cmd_sd(&gs, filename);
            else                                              cmd_sd(&gs, NULL);
            continue;
        }

        /* P - deal the deck and start playing */
        if (strcmp(input, "P") == 0) {
            cmd_p(&gs);
            continue;
        }

        strcpy(gs.last_message, "Error: unknown command.");
    }

    /* Free all memory before exiting */
    cardlist_free_all(&gs.deck);
    game_free_columns_and_foundations(&gs);

    return 0;
}