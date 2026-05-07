#ifndef YUKON_H
#define YUKON_H

/* Shared types and function declarations used by every .c file */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* A single playing card.
   Cards are chained together: [AH] -> [2H] -> [3H] -> NULL */
typedef struct Card {
    char rank;          /* 'A' '2'..'9' 'T' 'J' 'Q' 'K'        */
    char suit;          /* 'C' 'D' 'H' 'S'                      */
    int  is_face_up;    /* 1 = visible to player, 0 = hidden     */
    struct Card *next;  /* next card in the chain, NULL if last  */
} Card;

/* A linked list of cards with a head, tail, and count */
typedef struct {
    Card *head;   /* first card (NULL if empty) */
    Card *tail;   /* last card  (NULL if empty) */
    int   count;  /* number of cards in the list */
} CardList;

/* The two phases of the game */
typedef enum {
    PHASE_STARTUP,  /* before the game starts */
    PHASE_PLAY      /* actively playing       */
} GamePhase;

/* Everything about the current game */
typedef struct {
    CardList  deck;
    CardList  columns[7];         /* 7 tableau columns C1..C7               */
    CardList  foundations[4];     /* 4 foundation piles F1..F4              */
    char      foundation_suit[4]; /* suit assigned to each foundation pile  */
    GamePhase phase;
    int       deck_is_loaded;
    char      last_command[256];
    char      last_message[512];
} GameState;

/* Number of cards dealt to each column, and which column each dealt card goes to */
extern const int COL_SIZES[7];
extern const int COL_MAP[52];

/* --- card.c --- */
Card *card_create(char rank, char suit, int is_face_up);
int   rank_to_value(char rank);
int   is_valid_rank(char rank);
int   is_valid_suit(char suit);
void  cardlist_init(CardList *list);
void  cardlist_add_to_tail(CardList *list, Card *card);
void  cardlist_add_to_head(CardList *list, Card *card);
Card *cardlist_remove_tail(CardList *list);
Card *cardlist_remove_head(CardList *list);
void  cardlist_free_all(CardList *list);
Card *cardlist_find(CardList *list, char rank, char suit);
int   cardlist_split_at(CardList *source, Card *split_card, CardList *destination);
void  cardlist_append(CardList *destination, CardList *source);

/* --- game.c --- */
void game_init(GameState *gs);
void game_free_columns_and_foundations(GameState *gs);
int  cmd_ld(GameState *gs, const char *filename);
int  cmd_sw(GameState *gs);
int  cmd_si(GameState *gs, int split_position, int use_random);
int  cmd_sr(GameState *gs);
int  cmd_sd(GameState *gs, const char *filename);
int  cmd_p(GameState *gs);
int  cmd_q(GameState *gs);
int  cmd_move(GameState *gs, const char *move_str);
int  check_win(GameState *gs);

/* --- display.c --- */
void display(GameState *gs);

#endif /* YUKON_H */