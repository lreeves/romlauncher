#include "input.h"
#include "logging.h"
#include "favorites.h"
#include "config.h"

// Global variables that are defined in main.c and accessed here
int selected_index;
int total_entries;
int current_page;
int total_pages;
int menu_selection;
DirContent* content;
DirContent* favorites_content;
DirContent* history_content;
SDL_Renderer* renderer;
TTF_Font* font;
SDL_Joystick* joystick;
AppMode current_app_mode;
BrowserMode current_browser_mode;
SDL_Texture* menu_textures[MENU_OPTIONS];
SDL_Rect menu_rects[MENU_OPTIONS];
const char* menu_options[] = {"Help", "History", "Scraper", "Quit"};

// Helper function to get current content based on browser mode
DirContent* get_current_content(void) {
    if (current_app_mode == APP_MODE_BROWSER) {
        if (current_browser_mode == BROWSER_MODE_FAVORITES) {
            return favorites_content;
        } else if (current_browser_mode == BROWSER_MODE_HISTORY) {
            return history_content;
        }
    }
    return content;
}

// Helper function for up navigation
void handle_up_navigation(void) {
    if (selected_index > 0)
        selected_index--;
    else
        selected_index = total_entries - 1;
    
    current_page = selected_index / ENTRIES_PER_PAGE;
    DirContent* current_content = get_current_content();
    set_selection(current_content, renderer, font, selected_index, current_page);
    
    if (current_app_mode == APP_MODE_BROWSER && 
        current_browser_mode == BROWSER_MODE_FILES) {
        update_box_art_for_selection(content, renderer, current_path, selected_index);
        log_message(LOG_DEBUG, "Auto repeat: DPAD_UP; new selection: %d", selected_index);
    }
}

// Helper function for down navigation
void handle_down_navigation(void) {
    if (selected_index < total_entries - 1)
        selected_index++;
    else
        selected_index = 0;
    
    current_page = selected_index / ENTRIES_PER_PAGE;
    DirContent* current_content = get_current_content();
    set_selection(current_content, renderer, font, selected_index, current_page);
    
    if (current_app_mode == APP_MODE_BROWSER && 
        current_browser_mode == BROWSER_MODE_FILES) {
        update_box_art_for_selection(content, renderer, current_path, selected_index);
        log_message(LOG_DEBUG, "Auto repeat: DPAD_DOWN; new selection: %d", selected_index);
    }
}

// Helper function to handle navigation input
void handle_navigation_input(int direction) {
    if (current_app_mode == APP_MODE_BROWSER && current_browser_mode == BROWSER_MODE_FAVORITES && favorites_content) {
        // In favorites mode, skip group headers
        selected_index = find_next_rom(favorites_content, selected_index, direction);
    } else {
        // Normal directory browsing behavior
        if (direction < 0) {
            if (selected_index > 0) {
                selected_index--;
            } else {
                selected_index = total_entries - 1;
            }
        } else {
            if (selected_index < total_entries - 1) {
                selected_index++;
            } else {
                selected_index = 0;
            }
        }
    }
    
    current_page = selected_index / ENTRIES_PER_PAGE;
    DirContent* current_content = get_current_content();
    set_selection(current_content, renderer, font, selected_index, current_page);
    
    // Load box art for selected file when navigating
    if (current_app_mode == APP_MODE_BROWSER &&
        current_browser_mode == BROWSER_MODE_FILES) {
        update_box_art_for_selection(content, renderer, current_path, selected_index);
    }
}

// Helper function to handle page navigation (for shoulder buttons)
void handle_page_navigation(int direction) {
    if (direction < 0) {
        if (current_page > 0)
            current_page--;
        else
            current_page = total_pages - 1;
    } else {
        if (current_page < total_pages - 1)
            current_page++;
        else
            current_page = 0;
    }
    
    selected_index = current_page * ENTRIES_PER_PAGE;
    DirContent* current_content = get_current_content();
    set_selection(current_content, renderer, font, selected_index, current_page);
}

// Helper function to update menu selection
void update_menu_selection(int new_selection) {
    menu_selection = new_selection;
    
    // Update menu textures
    for (int i = 0; i < MENU_OPTIONS; i++) {
        if (menu_textures[i]) SDL_DestroyTexture(menu_textures[i]);
        SDL_Color color = (i == menu_selection) ? COLOR_TEXT_SELECTED : COLOR_TEXT;
        menu_textures[i] = render_text(renderer, menu_options[i], font, color, &menu_rects[i], 0);
    }
}

// Helper function to handle button repeat for navigation
void handle_button_repeat(int button, int *held_state, int *initial_delay_state, 
                         Uint32 *repeat_time, Uint32 now, void (*action)(void)) {
    if (SDL_JoystickGetButton(joystick, button)) {
        if (!(*held_state)) {
            *held_state = 1;
            *initial_delay_state = 1;  // Reset to initial delay phase
            *repeat_time = now;
            // Don't process immediately - first press is handled in JOYBUTTONDOWN event
        } else {
            // Handle repeat with initial delay
            Uint32 delay = (*initial_delay_state) ? INITIAL_DELAY_MS : REPEAT_DELAY_MS;
            
            if (now - (*repeat_time) >= delay) {
                action();
                *initial_delay_state = 0;  // Switch to repeat phase
                *repeat_time = now;
            }
        }
    } else {
        *held_state = 0;
    }
}

// Function to update box art based on current selection
void update_box_art_for_selection(DirContent* content, SDL_Renderer* renderer,
                                 const char* current_path, int selected_index) {
    if (!content || selected_index < 0) return;

    // Only load box art for files (not directories)
    if (selected_index >= content->dir_count &&
        selected_index < content->dir_count + content->file_count) {
        int file_index = selected_index - content->dir_count;
        if (file_index >= 0 && file_index < content->file_count) {
            const char* filename = content->files[file_index];
            log_message(LOG_DEBUG, "Loading box art for: %s", filename);
            load_box_art(content, renderer, current_path, filename);
        }
    }
}
