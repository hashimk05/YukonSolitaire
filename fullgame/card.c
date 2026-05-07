/* card.c - creates cards and manages linked lists of cards */

#include "yukon.h"

/* How many cards each column gets when dealing */
const int COL_SIZES[7] = {1, 6, 7, 8, 9, 10, 11};

/* Which column (0-based) the i-th dealt card goes to */
const int COL_MAP[52] = {
    0, 1, 2, 3, 4, 5, 6,  /* row 0: one card to every column   */
    1, 2, 3, 4, 5, 6,      /* row 1: C1 is done, skip it        */
    1, 2, 3, 4, 5, 6,
    1, 2, 3, 4, 5, 6,
    1, 2, 3, 4, 5, 6,
    1, 2, 3, 4, 5, 6,
    2, 3, 4, 5, 6,          /* row 6: C2 is done, skip it        */
    3, 4, 5, 6,             /* row 7: C3 is done                 */
    4, 5, 6,                /* row 8: C4 is done                 */
    5, 6,                   /* row 9: C5 is done                 */
    6                       /* row 10: only C7 left              */
};

/* Allocates a new card in memory and fills in its values */
Card *card_create(char rank, char suit, int is_face_up)
{
    Card *new_card = malloc(sizeof(Card));

    if (new_card == NULL) {
        fprintf(stderr, "Error: out of memory!\n");
        exit(EXIT_FAILURE);
    }

    new_card->rank       = rank;
    new_card->suit       = suit;
    new_card->is_face_up = is_face_up;
    new_card->next       = NULL;

    return new_card;
}

/* Returns the numeric value of a rank: A=1, 2-9, T=10, J=11, Q=12, K=13.
   Returns -1 for an invalid rank. */
int rank_to_value(char rank)
{
    const char *all_ranks = "A23456789TJQK";

    for (int i = 0; i < 13; i++) {
        if (all_ranks[i] == rank) {
            return i + 1;
        }
    }

    return -1;
}

/* Returns 1 if rank is a valid rank character, 0 otherwise */
int is_valid_rank(char rank)
{
    return rank_to_value(rank) != -1;
}

/* Returns 1 if suit is C, D, H, or S — 0 otherwise */
int is_valid_suit(char suit)
{
    return suit == 'C' || suit == 'D' || suit == 'H' || suit == 'S';
}

/* Resets a CardList to empty. Call this before using any new CardList. */
void cardlist_init(CardList *list)
{
    list->head  = NULL;
    list->tail  = NULL;
    list->count = 0;
}

/* Adds a card to the END of the list */
void cardlist_add_to_tail(CardList *list, Card *card)
{
    card->next = NULL;

    if (list->tail == NULL) {
        /* List was empty: this card is both head and tail */
        list->head = card;
    } else {
        /* Link the old tail to the new card */
        list->tail->next = card;
    }

    list->tail = card;
    list->count++;
}

/* Adds a card to the FRONT of the list */
void cardlist_add_to_head(CardList *list, Card *card)
{
    card->next = list->head;

    if (list->head == NULL) {
        /* List was empty: this card is also the tail */
        list->tail = card;
    }

    list->head = card;
    list->count++;
}

/* Removes and returns the LAST card. Returns NULL if the list is empty. */
Card *cardlist_remove_tail(CardList *list)
{
    if (list->head == NULL) {
        return NULL;
    }

    if (list->head == list->tail) {
        /* Only one card in the list */
        Card *only_card = list->head;
        list->head  = NULL;
        list->tail  = NULL;
        list->count = 0;
        return only_card;
    }

    /* Walk to the card just before the tail */
    Card *second_to_last = list->head;
    while (second_to_last->next != list->tail) {
        second_to_last = second_to_last->next;
    }

    Card *removed_card       = list->tail;
    second_to_last->next     = NULL;
    list->tail               = second_to_last;
    list->count--;
    removed_card->next       = NULL;

    return removed_card;
}

/* Removes and returns the FIRST card. Returns NULL if the list is empty. */
Card *cardlist_remove_head(CardList *list)
{
    if (list->head == NULL) {
        return NULL;
    }

    Card *removed_card = list->head;
    list->head         = removed_card->next;

    if (list->head == NULL) {
        /* We just removed the only card */
        list->tail = NULL;
    }

    removed_card->next = NULL;
    list->count--;

    return removed_card;
}

/* Frees every card in the list and resets it to empty */
void cardlist_free_all(CardList *list)
{
    Card *current = list->head;

    while (current != NULL) {
        Card *next_card = current->next; /* save next BEFORE freeing current */
        free(current);
        current = next_card;
    }

    cardlist_init(list);
}

/* Searches for a card by rank and suit. Returns the card or NULL if not found. */
Card *cardlist_find(CardList *list, char rank, char suit)
{
    Card *current = list->head;

    while (current != NULL) {
        if (current->rank == rank && current->suit == suit) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/* Splits the list at split_card: everything from split_card to the end
   moves into destination. source is shortened.
   Returns 1 on success, 0 if split_card was not found. */
int cardlist_split_at(CardList *source, Card *split_card, CardList *destination)
{
    cardlist_init(destination);

    if (split_card == NULL || source->head == NULL) {
        return 0;
    }

    /* Special case: split at the very first card */
    if (source->head == split_card) {
        *destination = *source;
        cardlist_init(source);
        return 1;
    }

    /* Walk through looking for the card just before split_card */
    Card *card_before_split = source->head;
    int   cards_before      = 1;

    while (card_before_split->next != NULL &&
           card_before_split->next != split_card) {
        card_before_split = card_before_split->next;
        cards_before++;
    }

    if (card_before_split->next == NULL) {
        return 0; /* split_card was not in the list */
    }

    /* Set up destination starting at split_card */
    destination->head  = split_card;
    destination->tail  = source->tail;
    destination->count = source->count - cards_before;

    /* Cut source short just before split_card */
    card_before_split->next = NULL;
    source->tail            = card_before_split;
    source->count           = cards_before;

    return 1;
}

/* Moves all cards from source onto the end of destination, then clears source */
void cardlist_append(CardList *destination, CardList *source)
{
    if (source->head == NULL) {
        return;
    }

    if (destination->head == NULL) {
        /* Destination is empty: just take everything from source */
        *destination = *source;
    } else {
        /* Link destination's tail to source's head */
        destination->tail->next  = source->head;
        destination->tail        = source->tail;
        destination->count      += source->count;
    }

    cardlist_init(source);
}