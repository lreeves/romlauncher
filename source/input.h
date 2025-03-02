#ifndef INPUT_H
#define INPUT_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "browser.h"

// Button definitions
#define JOY_A     0
#define JOY_B     1
#define JOY_X     2
#define JOY_Y     3
#define JOY_PLUS  10
#define JOY_MINUS 11
#define JOY_LEFT  12
#define JOY_RIGHT 14
#define JOY_LEFT_SHOULDER 6
#define JOY_RIGHT_SHOULDER 7
#define DPAD_UP    13
#define DPAD_DOWN  15

// Menu options
#define MENU_OPTIONS 4  // Total number of menu options
#define MENU_HELP 0
#define MENU_HISTORY 1
#define MENU_SCRAPER 2
#define MENU_QUIT 3

// Timing constants for button repeat behavior
#define INITIAL_DELAY_MS 300  // Initial delay before auto-repeat starts
#define REPEAT_DELAY_MS 50    // Delay between repeats after initial delay

typedef enum {
    APP_MODE_BROWSER,
    APP_MODE_MENU,
    APP_MODE_SCRAPING,
} AppMode;

typedef enum {
    BROWSER_MODE_FILES,
    BROWSER_MODE_FAVORITES,
    BROWSER_MODE_HISTORY
} BrowserMode;

// Function declarations
void handle_up_navigation(const char* current_path);
void handle_down_navigation(const char* current_path);
void handle_page_navigation(int direction, const char* current_path);
void handle_navigation_input(int direction, const char* current_path);
void update_menu_selection(int new_selection, const char* current_path);
void handle_button_repeat(int button, int *held_state, int *initial_delay_state,
                         Uint32 *repeat_time, Uint32 now, void (*action_fn)(const char*), const char* action_param);
DirContent* get_current_content(void);
void update_box_art_for_selection(DirContent* content, const char* current_path, int selected_index);

// External variables that need to be accessed
extern int selected_index;
extern int total_entries;
extern int current_page;
extern int total_pages;
extern int menu_selection;
extern DirContent* content;
extern DirContent* favorites_content;
extern DirContent* history_content;
extern SDL_Renderer* renderer;
extern TTF_Font* font;
extern SDL_Joystick* joystick;
extern AppMode current_app_mode;
extern BrowserMode current_browser_mode;
extern SDL_Texture* menu_textures[];
extern SDL_Rect menu_rects[];
extern const char* menu_options[];

#endif // INPUT_H
