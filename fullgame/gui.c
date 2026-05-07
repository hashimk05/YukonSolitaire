/* gui.c - graphical version of Yukon Solitaire using SDL3 */

#include "yukon.h"
#include <SDL3/SDL.h>

/* Window and card layout sizes in pixels */
#define WIN_W        1180
#define WIN_H         820
#define CARD_W         82
#define CARD_H        116
#define COL_GAP        18
#define CARD_GAP_Y     28   /* vertical overlap between cards in a column */
#define TOP_Y         110   /* y position of the first card row           */
#define LEFT_X         32   /* x position of column C1                    */
#define FOUNDATION_X  840
#define FOUNDATION_Y  110
#define BUTTON_Y       18
#define BUTTON_H       34
#define MSG_Y         740

/* A clickable toolbar button */
typedef struct {
    SDL_FRect rect;
    char      label[32];
    char      command[64]; /* command string sent when this button is clicked */
} Button;

/* Tracks what the player clicked first (waiting for a second click to complete the move) */
typedef enum { SEL_NONE, SEL_COLUMN, SEL_FOUNDATION } SelectionType;

typedef struct {
    SelectionType type;
    int  index;
    char rank;
    char suit;
    int  is_mid_column; /* 1 if the clicked card is not at the bottom of the column */
} Selection;

/* Returns 1 if point (x, y) is inside rectangle r */
static int point_in_rect(float x, float y, SDL_FRect r)
{
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

static SDL_Color make_color(Uint8 r, Uint8 g, Uint8 b) { SDL_Color c = {r, g, b, 255}; return c; }

static void set_render_color(SDL_Renderer *renderer, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

/* Draws text at position (x, y) with the given color */
static void draw_text(SDL_Renderer *renderer, float x, float y, const char *text, SDL_Color color)
{
    set_render_color(renderer, color);
    SDL_RenderDebugText(renderer, x, y, text);
}

/* Draws a filled rectangle */
static void draw_filled_rect(SDL_Renderer *renderer, SDL_FRect rect, SDL_Color color)
{
    set_render_color(renderer, color);
    SDL_RenderFillRect(renderer, &rect);
}

/* Draws the outline of a rectangle */
static void draw_rect_outline(SDL_Renderer *renderer, SDL_FRect rect, SDL_Color color)
{
    set_render_color(renderer, color);
    SDL_RenderRect(renderer, &rect);
}

/* Returns 1 if the suit is red (Diamonds or Hearts) */
static int suit_is_red(char suit)
{
    return suit == 'D' || suit == 'H';
}

/* Draws a single toolbar button */
static void draw_button(SDL_Renderer *renderer, Button *button)
{
    draw_filled_rect(renderer, button->rect, make_color(225, 225, 225));
    draw_rect_outline(renderer, button->rect, make_color(40, 40, 40));
    draw_text(renderer, button->rect.x + 9, button->rect.y + 11,
              button->label, make_color(20, 20, 20));
}

/* Draws one card on screen. Shows green slot if NULL, blue back if face-down,
   white card face if face-up. Gold border if selected. */
static void draw_card(SDL_Renderer *renderer, SDL_FRect rect, Card *card, int is_selected)
{
    if (card == NULL) {
        draw_filled_rect(renderer, rect, make_color(34, 110, 74));
        draw_rect_outline(renderer, rect, make_color(190, 210, 190));
        return;
    }

    if (!card->is_face_up) {
        draw_filled_rect(renderer, rect, make_color(50, 85, 170));
        draw_rect_outline(renderer, rect, make_color(245, 245, 245));
        draw_text(renderer, rect.x + 30, rect.y + 48, "[]", make_color(245, 245, 245));
        return;
    }

    draw_filled_rect(renderer, rect, make_color(248, 248, 238));
    SDL_Color border_color = is_selected ? make_color(255, 215, 0) : make_color(20, 20, 20);
    draw_rect_outline(renderer, rect, border_color);

    if (is_selected) {
        /* Draw a second inner border to make the highlight more visible */
        SDL_FRect inner_rect = {rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4};
        draw_rect_outline(renderer, inner_rect, make_color(255, 215, 0));
    }

    char card_text[4] = {card->rank, card->suit, '\0', '\0'};
    SDL_Color text_color = suit_is_red(card->suit) ? make_color(190, 20, 20) : make_color(15, 15, 15);
    draw_text(renderer, rect.x + 8,  rect.y + 8,  card_text, text_color);
    draw_text(renderer, rect.x + 32, rect.y + 50, card_text, text_color);
}

/* Saves the command string into the game state so it can be displayed */
static void save_last_command(GameState *gs, const char *command)
{
    strncpy(gs->last_command, command, sizeof(gs->last_command) - 1);
    gs->last_command[sizeof(gs->last_command) - 1] = '\0';
}

/* Parses and runs one command string. Returns 0 if the program should exit. */
static int execute_command(GameState *gs, const char *raw_input)
{
    char command[512];
    strncpy(command, raw_input, sizeof(command) - 1);
    command[sizeof(command) - 1] = '\0';

    /* Trim trailing whitespace */
    int length = (int)strlen(command);
    while (length > 0 &&
           (command[length-1] == '\n' || command[length-1] == '\r' ||
            command[length-1] == ' '  || command[length-1] == '\t')) {
        command[--length] = '\0';
    }
    if (length == 0) return 1;

    save_last_command(gs, command);

    if (strcmp(command, "QQ") == 0) return 0;

    if (strcmp(command, "Q") == 0) {
        cmd_q(gs);
        return 1;
    }

    if (gs->phase == PHASE_PLAY) {
        if (strstr(command, "->") != NULL) {
            cmd_move(gs, command);
            if (check_win(gs)) {
                strcpy(gs->last_message, "*** YOU WIN! *** Type Q to restart or QQ to quit.");
            }
        } else {
            strcpy(gs->last_message, "In PLAY phase. Use mouse or type a move (e.g. C3->C4).");
        }
        return 1;
    }

    /* Startup commands */
    if (strncmp(command, "LD", 2) == 0 && (command[2] == ' ' || command[2] == '\0')) {
        char filename[256] = {0};
        if (sscanf(command + 2, " %255s", filename) == 1) cmd_ld(gs, filename);
        else                                                cmd_ld(gs, NULL);
        return 1;
    }
    if (strcmp(command, "SW") == 0) { cmd_sw(gs); return 1; }
    if (strncmp(command, "SI", 2) == 0 && (command[2] == ' ' || command[2] == '\0')) {
        int n = 0;
        if (sscanf(command + 2, " %d", &n) == 1) cmd_si(gs, n, 0);
        else                                       cmd_si(gs, 0, 1);
        return 1;
    }
    if (strcmp(command, "SR") == 0) { cmd_sr(gs); return 1; }
    if (strncmp(command, "SD", 2) == 0 && (command[2] == ' ' || command[2] == '\0')) {
        char filename[256] = {0};
        if (sscanf(command + 2, " %255s", filename) == 1) cmd_sd(gs, filename);
        else                                                cmd_sd(gs, NULL);
        return 1;
    }
    if (strcmp(command, "P") == 0) { cmd_p(gs); return 1; }

    strcpy(gs->last_message, "Error: unknown command.");
    return 1;
}

/* Checks if (mx, my) is over a face-up card in the given column.
   Fills in selection and returns 1 if a card was hit, 0 otherwise. */
static int click_hit_column(GameState *gs, int col, float mx, float my, Selection *selection)
{
    float col_x = LEFT_X + col * (CARD_W + COL_GAP);

    if (mx < col_x || mx > col_x + CARD_W) return 0;

    /* Collect all cards in this column into an array for easy indexing */
    int   card_count = 0;
    Card *cards[52];
    Card *card = gs->columns[col].head;
    while (card != NULL) {
        cards[card_count++] = card;
        card = card->next;
    }

    if (card_count == 0) {
        /* Clicking an empty column slot selects the column */
        SDL_FRect empty_slot = {col_x, TOP_Y, CARD_W, CARD_H};
        if (point_in_rect(mx, my, empty_slot)) {
            selection->type          = SEL_COLUMN;
            selection->index         = col;
            selection->is_mid_column = 0;
            selection->rank          = '\0';
            selection->suit          = '\0';
            return 1;
        }
        return 0;
    }

    /* Check from the bottom card upward so the topmost visible card wins */
    for (int row = card_count - 1; row >= 0; row--) {
        SDL_FRect card_rect = {col_x, TOP_Y + row * CARD_GAP_Y, CARD_W, CARD_H};
        if (point_in_rect(mx, my, card_rect)) {
            if (!cards[row]->is_face_up) return 0; /* can't select a hidden card */
            selection->type          = SEL_COLUMN;
            selection->index         = col;
            selection->rank          = cards[row]->rank;
            selection->suit          = cards[row]->suit;
            selection->is_mid_column = (row != card_count - 1);
            return 1;
        }
    }
    return 0;
}

/* Returns 1 if (mx, my) is over one of the four foundations */
static int click_hit_foundation(float mx, float my, Selection *selection)
{
    for (int i = 0; i < 4; i++) {
        SDL_FRect foundation_rect = {
            FOUNDATION_X,
            FOUNDATION_Y + i * (CARD_H + 20),
            CARD_W, CARD_H
        };
        if (point_in_rect(mx, my, foundation_rect)) {
            selection->type          = SEL_FOUNDATION;
            selection->index         = i;
            selection->is_mid_column = 0;
            selection->rank          = '\0';
            selection->suit          = '\0';
            return 1;
        }
    }
    return 0;
}

/* Builds a move string (e.g. "C3:KH->F1") from two selections */
static void build_move_string(const Selection *source, const Selection *destination,
                               char *output, size_t output_size)
{
    char source_str[24], destination_str[16];

    if (source->type == SEL_FOUNDATION) {
        snprintf(source_str, sizeof(source_str), "F%d", source->index + 1);
    } else if (source->is_mid_column) {
        snprintf(source_str, sizeof(source_str), "C%d:%c%c",
                 source->index + 1, source->rank, source->suit);
    } else {
        snprintf(source_str, sizeof(source_str), "C%d", source->index + 1);
    }

    if (destination->type == SEL_FOUNDATION) {
        snprintf(destination_str, sizeof(destination_str), "F%d", destination->index + 1);
    } else {
        snprintf(destination_str, sizeof(destination_str), "C%d", destination->index + 1);
    }

    snprintf(output, output_size, "%s->%s", source_str, destination_str);
}

/* Draws the deck preview during startup */
static void draw_startup_preview(SDL_Renderer *renderer, GameState *gs)
{
    draw_text(renderer, LEFT_X, 72,
              "Startup: use buttons or type LD, SW, SI, SR, SD, P",
              make_color(245, 245, 245));

    int   column_card_count[7] = {0};
    Card *column_cards[7][12]  = {{0}};
    Card *current = gs->deck.head;

    for (int i = 0; i < 52 && current != NULL; i++, current = current->next) {
        int col = COL_MAP[i];
        column_cards[col][column_card_count[col]++] = current;
    }

    int face_down_per_column[7] = {0, 1, 2, 3, 4, 5, 6};

    for (int col = 0; col < 7; col++) {
        char column_label[4];
        snprintf(column_label, sizeof(column_label), "C%d", col + 1);
        draw_text(renderer, LEFT_X + col * (CARD_W + COL_GAP) + 28, 92,
                  column_label, make_color(245, 245, 245));

        int rows = gs->deck_is_loaded ? column_card_count[col] : COL_SIZES[col];

        for (int row = 0; row < rows; row++) {
            SDL_FRect card_rect = {
                LEFT_X + col * (CARD_W + COL_GAP),
                TOP_Y  + row * CARD_GAP_Y,
                CARD_W, CARD_H
            };
            Card  temp_card;
            Card *card_ptr = NULL;

            if (gs->deck_is_loaded && column_cards[col][row] != NULL) {
                temp_card             = *column_cards[col][row];
                temp_card.is_face_up  = (row >= face_down_per_column[col]) ? 1 : 0;
                card_ptr              = &temp_card;
            }
            draw_card(renderer, card_rect, card_ptr, 0);
        }
    }
}

/* Draws the columns during an active game */
static void draw_play_area(SDL_Renderer *renderer, GameState *gs, Selection current_selection)
{
    draw_text(renderer, LEFT_X, 72,
              "Play: click source card, then click destination",
              make_color(245, 245, 245));

    for (int col = 0; col < 7; col++) {
        char column_label[4];
        snprintf(column_label, sizeof(column_label), "C%d", col + 1);
        draw_text(renderer, LEFT_X + col * (CARD_W + COL_GAP) + 28, 92,
                  column_label, make_color(245, 245, 245));

        float col_x = LEFT_X + col * (CARD_W + COL_GAP);

        if (gs->columns[col].head == NULL) {
            SDL_FRect empty_slot = {col_x, TOP_Y, CARD_W, CARD_H};
            int is_selected = (current_selection.type == SEL_COLUMN &&
                               current_selection.index == col);
            draw_card(renderer, empty_slot, NULL, is_selected);
            continue;
        }

        int  row  = 0;
        Card *card = gs->columns[col].head;
        while (card != NULL) {
            SDL_FRect card_rect = {col_x, TOP_Y + row * CARD_GAP_Y, CARD_W, CARD_H};
            int is_selected = (current_selection.type == SEL_COLUMN &&
                               current_selection.index == col &&
                               current_selection.rank  == card->rank &&
                               current_selection.suit  == card->suit);
            draw_card(renderer, card_rect, card, is_selected);
            card = card->next;
            row++;
        }
    }
}

/* Draws the four foundation piles */
static void draw_foundations(SDL_Renderer *renderer, GameState *gs, Selection current_selection)
{
    draw_text(renderer, FOUNDATION_X, 84, "Foundations", make_color(245, 245, 245));

    for (int i = 0; i < 4; i++) {
        SDL_FRect foundation_rect = {
            FOUNDATION_X,
            FOUNDATION_Y + i * (CARD_H + 20),
            CARD_W, CARD_H
        };
        int is_selected = (current_selection.type == SEL_FOUNDATION &&
                           current_selection.index == i);
        draw_card(renderer, foundation_rect, gs->foundations[i].tail, is_selected);

        char label[4];
        snprintf(label, sizeof(label), "F%d", i + 1);
        draw_text(renderer, FOUNDATION_X + CARD_W + 10, foundation_rect.y + 45,
                  label, make_color(245, 245, 245));
    }
}

/* Draws the entire screen each frame */
static void draw_frame(SDL_Renderer *renderer, GameState *gs,
                       Button *buttons, int button_count,
                       const char *typed_input, Selection current_selection)
{
    /* Green felt background */
    draw_filled_rect(renderer, (SDL_FRect){0, 0, WIN_W, WIN_H}, make_color(20, 115, 75));

    for (int i = 0; i < button_count; i++) {
        draw_button(renderer, &buttons[i]);
    }

    if (gs->phase == PHASE_STARTUP) draw_startup_preview(renderer, gs);
    else                            draw_play_area(renderer, gs, current_selection);

    draw_foundations(renderer, gs, current_selection);

    /* Phase label in the top-right corner */
    char phase_label[64];
    snprintf(phase_label, sizeof(phase_label), "Phase: %s",
             gs->phase == PHASE_PLAY ? "PLAY" : "STARTUP");
    draw_text(renderer, 990, 28, phase_label, make_color(245, 245, 245));

    /* Last command and message at the bottom */
    char last_command_line[320];
    snprintf(last_command_line, sizeof(last_command_line), "Last command: %s", gs->last_command);
    draw_text(renderer, LEFT_X, MSG_Y,      last_command_line, make_color(245, 245, 245));
    draw_text(renderer, LEFT_X, MSG_Y + 22, gs->last_message,  make_color(245, 245, 245));

    /* Text input box */
    SDL_FRect input_box = {LEFT_X, MSG_Y + 48, 760, 34};
    draw_filled_rect(renderer, input_box, make_color(245, 245, 245));
    draw_rect_outline(renderer, input_box, make_color(20, 20, 20));

    char input_display[560];
    snprintf(input_display, sizeof(input_display), "> %s", typed_input);
    draw_text(renderer, input_box.x + 8, input_box.y + 11,
              input_display, make_color(20, 20, 20));
}

/* Program entry point for the GUI version */
int main(void)
{
    srand((unsigned int)time(NULL));

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Yukon Solitaire", WIN_W, WIN_H, SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_StartTextInput(window);

    GameState gs;
    game_init(&gs);

    /* Toolbar buttons: label shown on screen, command sent when clicked */
    Button buttons[] = {
        {{ 32, BUTTON_Y, 72, BUTTON_H}, "LD",    "LD"  },
        {{112, BUTTON_Y, 72, BUTTON_H}, "SW",    "SW"  },
        {{192, BUTTON_Y, 72, BUTTON_H}, "SI",    "SI"  },
        {{272, BUTTON_Y, 72, BUTTON_H}, "SR",    "SR"  },
        {{352, BUTTON_Y, 72, BUTTON_H}, "SD",    "SD"  },
        {{432, BUTTON_Y, 72, BUTTON_H}, "PLAY",  "P"   },
        {{512, BUTTON_Y, 72, BUTTON_H}, "RESET", "Q"   },
        {{592, BUTTON_Y, 72, BUTTON_H}, "QUIT",  "QQ"  },
    };
    int button_count = (int)(sizeof(buttons) / sizeof(buttons[0]));

    char      typed_input[512]  = "";
    Selection current_selection = {SEL_NONE, -1, '\0', '\0', 0};
    int       running           = 1;

    /* Event loop: handle input and redraw every frame */
    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {

            if (event.type == SDL_EVENT_QUIT) {
                running = 0;
            }
            /* Player typed a character into the input box */
            else if (event.type == SDL_EVENT_TEXT_INPUT) {
                if (strlen(typed_input) + strlen(event.text.text) < sizeof(typed_input) - 1) {
                    strcat(typed_input, event.text.text);
                }
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_BACKSPACE) {
                    /* Delete the last typed character */
                    size_t n = strlen(typed_input);
                    if (n > 0) typed_input[n - 1] = '\0';
                } else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER) {
                    /* Submit the typed command */
                    running = execute_command(&gs, typed_input);
                    typed_input[0]          = '\0';
                    current_selection.type  = SEL_NONE;
                } else if (event.key.key == SDLK_ESCAPE) {
                    current_selection.type = SEL_NONE;
                }
            }
            /* Mouse click */
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                     event.button.button == SDL_BUTTON_LEFT) {
                float mx       = event.button.x;
                float my       = event.button.y;
                int   handled  = 0;

                /* Check toolbar buttons first */
                for (int i = 0; i < button_count; i++) {
                    if (point_in_rect(mx, my, buttons[i].rect)) {
                        running = execute_command(&gs, buttons[i].command);
                        current_selection.type = SEL_NONE;
                        handled = 1;
                        break;
                    }
                }
                if (handled) continue;

                /* Two-click moves during play: first click = source, second = destination */
                if (gs.phase == PHASE_PLAY) {
                    Selection clicked = {SEL_NONE, -1, '\0', '\0', 0};

                    if (!click_hit_foundation(mx, my, &clicked)) {
                        for (int col = 0; col < 7; col++) {
                            if (click_hit_column(&gs, col, mx, my, &clicked)) break;
                        }
                    }

                    if (clicked.type != SEL_NONE) {
                        if (current_selection.type == SEL_NONE) {
                            current_selection = clicked; /* remember the source */
                        } else {
                            /* We have both source and destination: execute the move */
                            char move_string[64];
                            build_move_string(&current_selection, &clicked,
                                              move_string, sizeof(move_string));
                            running = execute_command(&gs, move_string);
                            current_selection.type = SEL_NONE;
                        }
                    } else {
                        current_selection.type = SEL_NONE; /* clicked empty space: cancel */
                    }
                }
            }
        }

        draw_frame(renderer, &gs, buttons, button_count, typed_input, current_selection);
        SDL_RenderPresent(renderer);
        SDL_Delay(16); /* ~60 fps */
    }

    /* Free memory and shut down SDL before exiting */
    cardlist_free_all(&gs.deck);
    game_free_columns_and_foundations(&gs);
    SDL_StopTextInput(window);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}