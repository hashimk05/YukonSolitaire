/* game.c - handles all game commands and move validation */

#include "yukon.h"

static const char DEFAULT_RANKS[] = "A23456789TJQK";
static const char DEFAULT_SUITS[] = "CDHS";

/* Sets up a blank game state before any cards are loaded */
void game_init(GameState *gs)
{
    cardlist_init(&gs->deck);

    for (int i = 0; i < 7; i++) {
        cardlist_init(&gs->columns[i]);
    }
    for (int i = 0; i < 4; i++) {
        cardlist_init(&gs->foundations[i]);
        gs->foundation_suit[i] = '\0';
    }

    gs->phase          = PHASE_STARTUP;
    gs->deck_is_loaded = 0;
    strcpy(gs->last_command, "none");
    strcpy(gs->last_message, "Welcome to Yukon Solitaire!");
}

/* Frees the columns and foundations but keeps the deck in memory */
void game_free_columns_and_foundations(GameState *gs)
{
    for (int i = 0; i < 7; i++) {
        cardlist_free_all(&gs->columns[i]);
    }
    for (int i = 0; i < 4; i++) {
        cardlist_free_all(&gs->foundations[i]);
        gs->foundation_suit[i] = '\0';
    }
}

/* Converts a suit character to an array index: C=0, D=1, H=2, S=3 */
static int suit_to_index(char suit)
{
    if (suit == 'C') return 0;
    if (suit == 'D') return 1;
    if (suit == 'H') return 2;
    return 3;
}

/* Loads a deck from a file, or builds the default 52-card deck if filename is NULL */
int cmd_ld(GameState *gs, const char *filename)
{
    cardlist_free_all(&gs->deck);

    /* No filename given: build the standard unshuffled deck */
    if (filename == NULL) {
        for (int s = 0; s < 4; s++) {
            for (int r = 0; r < 13; r++) {
                Card *new_card = card_create(DEFAULT_RANKS[r], DEFAULT_SUITS[s], 1);
                cardlist_add_to_tail(&gs->deck, new_card);
            }
        }
        gs->deck_is_loaded = 1;
        strcpy(gs->last_message, "OK");
        return 1;
    }

    /* Filename given: try to open it */
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        snprintf(gs->last_message, sizeof(gs->last_message),
                 "Error: cannot open file '%s'.", filename);
        return 0;
    }

    int seen[4][13] = {{0}}; /* tracks which cards we have already read */
    int card_count  = 0;
    int line_number = 0;
    char line[64];

    /* Use a temporary list so we only update the deck on full success */
    CardList temp_deck;
    cardlist_init(&temp_deck);

    while (fgets(line, sizeof(line), file)) {
        line_number++;

        /* Skip leading spaces */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        /* Skip blank lines */
        if (*p == '\n' || *p == '\r' || *p == '\0') continue;

        char rank = p[0];
        char suit = (char)toupper((unsigned char)p[1]);

        if (!is_valid_rank(rank) || !is_valid_suit(suit)) {
            snprintf(gs->last_message, sizeof(gs->last_message),
                     "Error: invalid card '%c%c' on line %d.", rank, suit, line_number);
            cardlist_free_all(&temp_deck);
            fclose(file);
            return 0;
        }

        int suit_index = suit_to_index(suit);
        int rank_index = rank_to_value(rank) - 1;

        if (seen[suit_index][rank_index]) {
            snprintf(gs->last_message, sizeof(gs->last_message),
                     "Error: duplicate card '%c%c' on line %d.", rank, suit, line_number);
            cardlist_free_all(&temp_deck);
            fclose(file);
            return 0;
        }
        seen[suit_index][rank_index] = 1;

        cardlist_add_to_tail(&temp_deck, card_create(rank, suit, 1));
        card_count++;
    }
    fclose(file);

    if (card_count != 52) {
        snprintf(gs->last_message, sizeof(gs->last_message),
                 "Error: deck must have 52 cards, but found %d.", card_count);
        cardlist_free_all(&temp_deck);
        return 0;
    }

    gs->deck          = temp_deck;
    gs->deck_is_loaded = 1;
    strcpy(gs->last_message, "OK");
    return 1;
}

/* Swaps the top half and bottom half of the deck */
int cmd_sw(GameState *gs)
{
    if (!gs->deck_is_loaded) {
        strcpy(gs->last_message, "Error: no deck loaded.");
        return 0;
    }

    int total_cards   = gs->deck.count;
    int half          = total_cards / 2;

    /* Walk to the last card of the first half */
    Card *end_of_first_half = gs->deck.head;
    for (int i = 0; i < half - 1; i++) {
        end_of_first_half = end_of_first_half->next;
    }

    /* Split off the second half */
    CardList second_half;
    cardlist_init(&second_half);
    second_half.head  = end_of_first_half->next;
    second_half.tail  = gs->deck.tail;
    second_half.count = total_cards - half;

    end_of_first_half->next = NULL;
    gs->deck.tail           = end_of_first_half;
    gs->deck.count          = half;

    /* Put the first half at the end of the second half */
    cardlist_append(&second_half, &gs->deck);
    gs->deck = second_half;

    strcpy(gs->last_message, "OK");
    return 1;
}

/* Riffle shuffle: splits the deck in two and interleaves them.
   If use_random is 1, the split position is chosen randomly. */
int cmd_si(GameState *gs, int split_position, int use_random)
{
    if (!gs->deck_is_loaded) {
        strcpy(gs->last_message, "Error: no deck loaded.");
        return 0;
    }

    int total_cards = gs->deck.count;

    if (total_cards < 2) {
        strcpy(gs->last_message, "Error: deck too small to shuffle.");
        return 0;
    }

    if (use_random) {
        split_position = (rand() % (total_cards - 1)) + 1;
    }

    if (split_position < 1 || split_position >= total_cards) {
        strcpy(gs->last_message, "Error: split must be between 1 and 51.");
        return 0;
    }

    /* Walk to the last card of pile 1 */
    Card *end_of_pile1 = gs->deck.head;
    for (int i = 1; i < split_position; i++) {
        end_of_pile1 = end_of_pile1->next;
    }

    /* pile2 gets the rest */
    CardList pile2;
    cardlist_init(&pile2);
    pile2.head  = end_of_pile1->next;
    pile2.tail  = gs->deck.tail;
    pile2.count = total_cards - split_position;

    end_of_pile1->next  = NULL;
    gs->deck.tail       = end_of_pile1;
    gs->deck.count      = split_position;

    /* Interleave: alternate taking one card from pile1 and one from pile2 */
    CardList result;
    cardlist_init(&result);

    while (gs->deck.head != NULL || pile2.head != NULL) {
        if (gs->deck.head != NULL) {
            cardlist_add_to_tail(&result, cardlist_remove_head(&gs->deck));
        }
        if (pile2.head != NULL) {
            cardlist_add_to_tail(&result, cardlist_remove_head(&pile2));
        }
    }

    gs->deck = result;
    strcpy(gs->last_message, "OK");
    return 1;
}

/* Randomly shuffles the deck by inserting each card at a random position */
int cmd_sr(GameState *gs)
{
    if (!gs->deck_is_loaded) {
        strcpy(gs->last_message, "Error: no deck loaded.");
        return 0;
    }

    CardList shuffled;
    cardlist_init(&shuffled);

    while (gs->deck.head != NULL) {
        Card *card           = cardlist_remove_head(&gs->deck);
        int   insert_at      = (shuffled.count > 0) ? rand() % (shuffled.count + 1) : 0;

        if (insert_at == 0) {
            cardlist_add_to_head(&shuffled, card);
        } else if (insert_at == shuffled.count) {
            cardlist_add_to_tail(&shuffled, card);
        } else {
            /* Walk to the card just before the insert position */
            Card *card_before = shuffled.head;
            for (int i = 0; i < insert_at - 1; i++) {
                card_before = card_before->next;
            }

            /* Insert card between card_before and card_before->next */
            card->next        = card_before->next;
            card_before->next = card;
            shuffled.count++;

            if (card->next == NULL) {
                shuffled.tail = card;
            }
        }
    }

    gs->deck = shuffled;
    strcpy(gs->last_message, "OK");
    return 1;
}

/* Saves the current deck to a file, one card per line (e.g. "5D") */
int cmd_sd(GameState *gs, const char *filename)
{
    if (!gs->deck_is_loaded) {
        strcpy(gs->last_message, "Error: no deck loaded.");
        return 0;
    }

    const char *output_filename = (filename != NULL) ? filename : "cards.txt";

    FILE *file = fopen(output_filename, "w");
    if (file == NULL) {
        snprintf(gs->last_message, sizeof(gs->last_message),
                 "Error: cannot write to '%s'.", output_filename);
        return 0;
    }

    Card *current = gs->deck.head;
    while (current != NULL) {
        fprintf(file, "%c%c\n", current->rank, current->suit);
        current = current->next;
    }

    fclose(file);
    strcpy(gs->last_message, "OK");
    return 1;
}

/* Deals the deck into 7 columns and starts the game */
int cmd_p(GameState *gs)
{
    if (!gs->deck_is_loaded) {
        strcpy(gs->last_message, "Error: no deck loaded.");
        return 0;
    }

    game_free_columns_and_foundations(gs);

    /* Column i gets i face-down cards at the top before the face-up ones start */
    int face_down_per_column[7] = {0, 1, 2, 3, 4, 5, 6};
    int cards_placed[7]         = {0};

    Card *current = gs->deck.head;

    for (int i = 0; i < 52 && current != NULL; i++, current = current->next) {
        int col        = COL_MAP[i];
        int is_face_up = (cards_placed[col] >= face_down_per_column[col]) ? 1 : 0;

        Card *new_card = card_create(current->rank, current->suit, is_face_up);
        cardlist_add_to_tail(&gs->columns[col], new_card);

        cards_placed[col]++;
    }

    gs->phase = PHASE_PLAY;
    strcpy(gs->last_message, "OK");
    return 1;
}

/* Quits the current game and returns to the startup phase */
int cmd_q(GameState *gs)
{
    game_free_columns_and_foundations(gs);
    gs->phase = PHASE_STARTUP;
    strcpy(gs->last_message, "OK");
    return 1;
}

/* ---------------------------------------------------------------
   MOVE PARSING
   A move string looks like "C3->C5" or "C2:KH->F1".
   We parse each side into a MoveLocation struct.
--------------------------------------------------------------- */
typedef struct {
    int  is_foundation;  /* 1 = foundation (F1-F4), 0 = column (C1-C7) */
    int  index;          /* 0-based index                               */
    int  has_named_card; /* 1 if a specific card was named (e.g. C3:KH) */
    char named_rank;
    char named_suit;
} MoveLocation;

/* Parses a location string like "F2", "C4", or "C4:KH" into a MoveLocation.
   Returns 1 on success, 0 if the string is invalid. */
static int parse_move_location(const char *str, MoveLocation *location)
{
    location->has_named_card = 0;
    location->named_rank     = '\0';
    location->named_suit     = '\0';

    if (str[0] == 'F' || str[0] == 'f') {
        if (!isdigit((unsigned char)str[1])) return 0;
        location->is_foundation = 1;
        location->index         = str[1] - '1'; /* '1'->0, '2'->1, etc. */
        if (location->index < 0 || location->index > 3) return 0;
        return 1;
    }

    if (str[0] == 'C' || str[0] == 'c') {
        if (!isdigit((unsigned char)str[1])) return 0;
        location->is_foundation = 0;
        location->index         = str[1] - '1';
        if (location->index < 0 || location->index > 6) return 0;

        if (str[2] == ':') {
            /* A specific card was named, e.g. "C3:KH" */
            location->named_rank     = (char)toupper((unsigned char)str[3]);
            location->named_suit     = (char)toupper((unsigned char)str[4]);
            if (!is_valid_rank(location->named_rank)) return 0;
            if (!is_valid_suit(location->named_suit)) return 0;
            location->has_named_card = 1;
        }
        return 1;
    }

    return 0;
}

/* Parses and executes a move string like "C3->C5" or "C2:KH->F1" */
int cmd_move(GameState *gs, const char *move_str)
{
    if (gs->phase != PHASE_PLAY) {
        strcpy(gs->last_message, "Error: not in play phase.");
        return 0;
    }

    /* Find the "->" separator */
    const char *arrow = strstr(move_str, "->");
    if (arrow == NULL) {
        strcpy(gs->last_message, "Error: invalid move syntax (missing ->).");
        return 0;
    }

    /* Copy the source and destination strings separately */
    char source_str[16]      = {0};
    char destination_str[16] = {0};
    int  source_length       = (int)(arrow - move_str);

    if (source_length <= 0 || source_length >= 16) {
        strcpy(gs->last_message, "Error: invalid move syntax.");
        return 0;
    }

    strncpy(source_str,      move_str,    source_length);
    strncpy(destination_str, arrow + 2,   15);

    /* Trim trailing whitespace from both strings */
    for (int i = (int)strlen(destination_str) - 1; i >= 0; i--) {
        if (destination_str[i] == '\n' || destination_str[i] == '\r' || destination_str[i] == ' ')
            destination_str[i] = '\0';
        else break;
    }
    for (int i = (int)strlen(source_str) - 1; i >= 0; i--) {
        if (source_str[i] == '\n' || source_str[i] == '\r' || source_str[i] == ' ')
            source_str[i] = '\0';
        else break;
    }

    MoveLocation source, destination;

    if (!parse_move_location(source_str, &source)) {
        strcpy(gs->last_message, "Error: invalid source.");
        return 0;
    }
    if (!parse_move_location(destination_str, &destination)) {
        strcpy(gs->last_message, "Error: invalid destination.");
        return 0;
    }

    /* --- Foundation -> Column --- */
    if (source.is_foundation && !destination.is_foundation) {
        CardList *from_foundation = &gs->foundations[source.index];
        CardList *to_column       = &gs->columns[destination.index];

        if (from_foundation->tail == NULL) {
            strcpy(gs->last_message, "Error: foundation is empty.");
            return 0;
        }

        Card *card_to_move = from_foundation->tail;

        if (to_column->tail != NULL) {
            int rank_difference = rank_to_value(to_column->tail->rank) - rank_to_value(card_to_move->rank);
            if (rank_difference != 1) {
                strcpy(gs->last_message, "Error: rank mismatch.");
                return 0;
            }
            if (to_column->tail->suit == card_to_move->suit) {
                strcpy(gs->last_message, "Error: cannot place card of same suit.");
                return 0;
            }
        }

        Card *moved_card      = cardlist_remove_tail(from_foundation);
        moved_card->is_face_up = 1;
        cardlist_add_to_tail(to_column, moved_card);
        strcpy(gs->last_message, "OK");
        return 1;
    }

    /* --- Column -> Foundation --- */
    if (!source.is_foundation && destination.is_foundation) {
        if (source.has_named_card) {
            strcpy(gs->last_message, "Error: can only move the bottom card to a foundation.");
            return 0;
        }

        CardList *from_column  = &gs->columns[source.index];
        CardList *to_foundation = &gs->foundations[destination.index];

        if (from_column->tail == NULL) {
            strcpy(gs->last_message, "Error: source column is empty.");
            return 0;
        }

        Card *card_to_move = from_column->tail;

        if (!card_to_move->is_face_up) {
            strcpy(gs->last_message, "Error: card is face-down.");
            return 0;
        }

        if (to_foundation->tail == NULL) {
            /* First card on this foundation must be an Ace */
            if (rank_to_value(card_to_move->rank) != 1) {
                strcpy(gs->last_message, "Error: foundation must start with an Ace.");
                return 0;
            }
            if (gs->foundation_suit[destination.index] == '\0') {
                gs->foundation_suit[destination.index] = card_to_move->suit;
            } else if (gs->foundation_suit[destination.index] != card_to_move->suit) {
                strcpy(gs->last_message, "Error: wrong suit for this foundation.");
                return 0;
            }
        } else {
            /* Foundation not empty: must be same suit, one rank higher */
            if (card_to_move->suit != gs->foundation_suit[destination.index]) {
                strcpy(gs->last_message, "Error: wrong suit for this foundation.");
                return 0;
            }
            int expected_rank = rank_to_value(to_foundation->tail->rank) + 1;
            if (rank_to_value(card_to_move->rank) != expected_rank) {
                strcpy(gs->last_message, "Error: card must be one rank higher than the top of the foundation.");
                return 0;
            }
        }

        Card *moved_card = cardlist_remove_tail(from_column);
        cardlist_add_to_tail(to_foundation, moved_card);

        /* Flip the newly exposed card in the source column if it is face-down */
        if (from_column->tail != NULL && !from_column->tail->is_face_up) {
            from_column->tail->is_face_up = 1;
        }

        strcpy(gs->last_message, "OK");
        return 1;
    }

    /* --- Column -> Column --- */
    if (!source.is_foundation && !destination.is_foundation) {
        CardList *from_column = &gs->columns[source.index];
        CardList *to_column   = &gs->columns[destination.index];

        if (from_column->tail == NULL) {
            strcpy(gs->last_message, "Error: source column is empty.");
            return 0;
        }

        Card *card_to_move = NULL;

        if (source.has_named_card) {
            /* Player named a specific card to move from */
            card_to_move = cardlist_find(from_column, source.named_rank, source.named_suit);
            if (card_to_move == NULL) {
                strcpy(gs->last_message, "Error: card not found in that column.");
                return 0;
            }
            if (!card_to_move->is_face_up) {
                strcpy(gs->last_message, "Error: that card is face-down.");
                return 0;
            }
        } else {
            /* No specific card named: move from the bottom of the column */
            card_to_move = from_column->tail;
            if (!card_to_move->is_face_up) {
                strcpy(gs->last_message, "Error: card is face-down.");
                return 0;
            }
        }

        if (to_column->tail == NULL) {
            /* Only a King may be placed on an empty column */
            if (rank_to_value(card_to_move->rank) != 13) {
                strcpy(gs->last_message, "Error: only a King can be placed on an empty column.");
                return 0;
            }
        } else {
            int rank_difference = rank_to_value(to_column->tail->rank) - rank_to_value(card_to_move->rank);
            if (rank_difference != 1) {
                strcpy(gs->last_message, "Error: rank mismatch.");
                return 0;
            }
            if (to_column->tail->suit == card_to_move->suit) {
                strcpy(gs->last_message, "Error: cannot place a card of the same suit.");
                return 0;
            }
        }

        /* Split the source column at card_to_move and append the stack to the destination */
        CardList moving_stack;
        cardlist_init(&moving_stack);

        if (!cardlist_split_at(from_column, card_to_move, &moving_stack)) {
            strcpy(gs->last_message, "Error: internal split failed.");
            return 0;
        }

        cardlist_append(to_column, &moving_stack);

        /* Flip the newly exposed card in the source column if it is face-down */
        if (from_column->tail != NULL && !from_column->tail->is_face_up) {
            from_column->tail->is_face_up = 1;
        }

        strcpy(gs->last_message, "OK");
        return 1;
    }

    strcpy(gs->last_message, "Error: cannot move between two foundations.");
    return 0;
}

/* Returns 1 if all 4 foundations end with a King (the player has won) */
int check_win(GameState *gs)
{
    for (int i = 0; i < 4; i++) {
        if (gs->foundations[i].tail == NULL) return 0;
        if (rank_to_value(gs->foundations[i].tail->rank) != 13) return 0;
    }
    return 1;
}