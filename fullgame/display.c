/* display.c - prints the game board to the terminal */

#include "yukon.h"

/* Shows the board during startup (before the game starts) */
static void display_startup(GameState *gs)
{
    printf("C1\tC2\tC3\tC4\tC5\tC6\tC7\n");

    if (!gs->deck_is_loaded) {
        /* No deck loaded yet: show empty slots */
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 7; col++) {
                if (col > 0) printf("\t");
                printf("  ");
            }
            int fd = row / 2;
            if (row % 2 == 0 && fd < 4) printf("\tF%d:[]", fd + 1);
            printf("\n");
        }
        return;
    }

    /* Deck is loaded: build a 2D grid showing where each card would be dealt */
    Card *vcols[7][12];
    int   col_counts[7];
    for (int c = 0; c < 7; c++) {
        col_counts[c] = 0;
        for (int r = 0; r < 12; r++) vcols[c][r] = NULL;
    }

    Card *current = gs->deck.head;
    for (int i = 0; i < 52 && current != NULL; i++, current = current->next) {
        int col = COL_MAP[i];
        vcols[col][col_counts[col]++] = current;
    }

    int max_rows = 7;
    for (int c = 0; c < 7; c++) {
        if (COL_SIZES[c] > max_rows) max_rows = COL_SIZES[c];
    }

    int face_down_count[7] = {0, 1, 2, 3, 4, 5, 6};

    for (int row = 0; row < max_rows; row++) {
        for (int col = 0; col < 7; col++) {
            if (col > 0) printf("\t");
            if (row < col_counts[col]) {
                Card *card = vcols[col][row];
                if (row >= face_down_count[col]) printf("%c%c", card->rank, card->suit);
                else                              printf("[]");
            } else {
                printf("  ");
            }
        }
        int fd_idx = row / 2;
        if (row % 2 == 0 && fd_idx < 4) printf("\tF%d:[]", fd_idx + 1);
        printf("\n");
    }
}

/* Shows the board during an active game */
static void display_play(GameState *gs)
{
    printf("C1\tC2\tC3\tC4\tC5\tC6\tC7\n");

    /* Build a 2D array for easy row-by-row printing */
    int   col_size[7];
    Card *col_cards[7][52];
    for (int c = 0; c < 7; c++) {
        col_size[c] = 0;
        for (Card *card = gs->columns[c].head; card != NULL; card = card->next) {
            col_cards[c][col_size[c]++] = card;
        }
    }

    int max_rows = 7;
    for (int c = 0; c < 7; c++) {
        if (col_size[c] > max_rows) max_rows = col_size[c];
    }

    for (int row = 0; row < max_rows; row++) {
        for (int col = 0; col < 7; col++) {
            if (col > 0) printf("\t");
            if (row < col_size[col]) {
                Card *card = col_cards[col][row];
                if (card->is_face_up) printf("%c%c", card->rank, card->suit);
                else               printf("[]");
            } else {
                printf("  ");
            }
        }
        int fd_idx = row / 2;
        if (row % 2 == 0 && fd_idx < 4) {
            printf("\tF%d:", fd_idx + 1);
            Card *top = gs->foundations[fd_idx].tail;
            if (top != NULL) printf("%c%c", top->rank, top->suit);
            else             printf("[]");
        }
        printf("\n");
    }
}

/* Main display function: picks startup or play view and prints the prompt */
void display(GameState *gs)
{
    if (gs->phase == PHASE_STARTUP) display_startup(gs);
    else                            display_play(gs);

    printf("\nLAST Command: %s\nMessage: %s\nINPUT > ",
           gs->last_command, gs->last_message);
    fflush(stdout);
}
