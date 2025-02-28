#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include "logging.h"
#include "config.h"
#include "browser.h"
#include "favorites.h"
#include "history.h"
#include "launch.h"
#include "input.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#ifndef ROMLAUNCHER_BUILD_LINUX
#include <switch.h>
#endif
#define MAX_PATH_LEN 512


// Define current_path as a global variable so it can be accessed from other files
char current_path[MAX_PATH_LEN];


#define SCREEN_W 1280
#define SCREEN_H 720
#define STATUS_BAR_HEIGHT 30

// Status bar colors - inverted from main colors
#define COLOR_STATUS_BAR (SDL_Color){220, 220, 220, 255}
#define COLOR_STATUS_TEXT (SDL_Color){20, 20, 20, 255}

typedef struct {
    char message[256];
    SDL_Texture* texture;
    SDL_Rect rect;
    int active;
} Notification;


#ifdef ROMLAUNCHER_BUILD_LINUX
int appletMainLoop() {
    return 1;
}
#endif

int main(int argc __attribute__((unused)), char** argv __attribute__((unused))) {
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "%sromlauncher-%ld.log", ROMLAUNCHER_DATA_DIRECTORY, (long)time(NULL));
    log_init(log_filename);

    log_message(LOG_INFO, "Starting romlauncher");

    // Create data and media directories if they don't exist
    struct stat st = {0};
    if (stat(ROMLAUNCHER_DATA_DIRECTORY, &st) == -1) {
        mkdir(ROMLAUNCHER_DATA_DIRECTORY, 0755);
        log_message(LOG_INFO, "Created data directory: %s", ROMLAUNCHER_DATA_DIRECTORY);
    }

    if (stat(ROMLAUNCHER_MEDIA_DIRECTORY, &st) == -1) {
        mkdir(ROMLAUNCHER_MEDIA_DIRECTORY, 0755);
        log_message(LOG_INFO, "Created media directory: %s", ROMLAUNCHER_MEDIA_DIRECTORY);
    }

    // Load config, favorites, and history
    load_config();
    load_favorites();
    load_history();
    init_default_core_mappings();
    init_system_names_mappings();
    log_message(LOG_INFO, "Config, favorites, and history loaded");


    int exit_requested = 0;
    int wait = 25;
    current_app_mode = APP_MODE_BROWSER;
    current_browser_mode = BROWSER_MODE_FILES;
    favorites_content = NULL;
    history_content = NULL;
    char saved_path[MAX_PATH_LEN];
    menu_selection = 0;
    for (int i = 0; i < MENU_OPTIONS; i++) {
        menu_textures[i] = NULL;
    }

    // Scraping mode message
    SDL_Texture* scraping_message = NULL;
    SDL_Rect scraping_rect;

    Notification notification = {0};

    SDL_Event event;

    selected_index = 0;
    total_entries = 0;
    current_page = 0;
    total_pages = 0;

    srand(time(NULL));

    // Initialize SDL subsystems with proper error handling
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        log_message(LOG_ERROR, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    log_message(LOG_DEBUG, "SDL_Init completed");

    // Set initialization flags
    int sdl_initialized = 1;
    int img_initialized = 0;
    int ttf_initialized = 0;
    int romfs_initialized __attribute__((unused)) = 0;

#ifndef ROMLAUNCHER_BUILD_LINUX
    // Initialize romfs
    Result rc = romfsInit();
    if (R_FAILED(rc)) {
        log_message(LOG_ERROR, "romfsInit failed: %08X", rc);
        goto cleanup;
    }
    romfs_initialized = 1;
    chdir("romfs:/");
    log_message(LOG_DEBUG, "romFS initialized");
#endif

    // Initialize SDL_image
    int img_flags = IMG_INIT_PNG;
    if ((IMG_Init(img_flags) & img_flags) != img_flags) {
        log_message(LOG_ERROR, "IMG_Init failed: %s", IMG_GetError());
        goto cleanup;
    }
    img_initialized = 1;
    log_message(LOG_DEBUG, "IMG_Init completed");

    // Initialize SDL_ttf
    if (TTF_Init() < 0) {
        log_message(LOG_ERROR, "TTF_Init failed: %s", TTF_GetError());
        goto cleanup;
    }
    ttf_initialized = 1;
    log_message(LOG_DEBUG, "TTF_Init completed");

    SDL_Window* window = SDL_CreateWindow(NULL,
                                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!window) {
        log_message(LOG_ERROR, "Window creation failed: %s", SDL_GetError());
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    log_message(LOG_DEBUG, "SDL window created");

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        log_message(LOG_ERROR, "Renderer creation failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    log_message(LOG_DEBUG, "SDL renderer created");

    // Initialize joystick with better cross-platform support
    SDL_JoystickEventState(SDL_ENABLE);
    joystick = NULL;
    
    // Count available joysticks
    int num_joysticks = SDL_NumJoysticks();
    log_message(LOG_INFO, "Found %d joystick(s)", num_joysticks);
    
    // Try to open the first available joystick
    for (int i = 0; i < num_joysticks; i++) {
        log_message(LOG_INFO, "Joystick %d: %s", i, SDL_JoystickNameForIndex(i));
        joystick = SDL_JoystickOpen(i);
        if (joystick) {
            log_message(LOG_INFO, "Using joystick: %s with %d buttons, %d axes", 
                SDL_JoystickName(joystick),
                SDL_JoystickNumButtons(joystick),
                SDL_JoystickNumAxes(joystick));
            break;
        }
    }
    
    if (!joystick) {
        log_message(LOG_ERROR, "Failed to open any joystick: %s", SDL_GetError());
        // Continue anyway, as this isn't fatal
    } else {
        log_message(LOG_DEBUG, "SDL joystick initialized");
    }

    Uint32 dpadUpRepeatTime = 0;
    Uint32 dpadDownRepeatTime = 0;
    int dpadUpHeld = 0;
    int dpadUpInitialDelay = 1;  // Flag to track if we're in initial delay phase
    int dpadDownHeld = 0;
    int dpadDownInitialDelay = 1;  // Flag to track if we're in initial delay phase
    Uint32 leftShoulderRepeatTime = 0;
    Uint32 rightShoulderRepeatTime = 0;
    int leftShoulderHeld = 0;
    int rightShoulderHeld = 0;

#ifdef ROMLAUNCHER_BUILD_LINUX
    // load fonts from romfs
    font = TTF_OpenFont(ROMLAUNCHER_DATA_DIRECTORY "data/Raleway-Regular.ttf", 32);
    TTF_Font* small_font = TTF_OpenFont(ROMLAUNCHER_DATA_DIRECTORY "data/Raleway-Regular.ttf", 16);
#else
    font = TTF_OpenFont("data/Raleway-Regular.ttf", 32);
    TTF_Font* small_font = TTF_OpenFont("data/Raleway-Regular.ttf", 16);
#endif

    if((!font) || (!small_font)) {
        log_message(LOG_ERROR, "Couldn't load fonts");
        exit(1);
    }

    log_message(LOG_INFO, "About to list files");
    strncpy(current_path, ROM_DIRECTORY, sizeof(current_path) - 1);
    content = list_files(current_path);
    if (content == NULL) {
        log_message(LOG_INFO, "list_files returned NULL");
    } else {
        log_message(LOG_DEBUG, "Got valid content");
        total_entries = content->dir_count + content->file_count;
        log_message(LOG_DEBUG, "Found %d directories and %d files",
                   content->dir_count, content->file_count);
        log_message(LOG_DEBUG, "Calculating pages: total_entries=%d, ENTRIES_PER_PAGE=%d",
                   total_entries, ENTRIES_PER_PAGE);
        total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;

        log_message(LOG_DEBUG, "Calling set_selection");
        set_selection(content, renderer, font, selected_index, current_page);
    }

    log_message(LOG_DEBUG, "Starting main loop");

    // Initialize status bar elements
    SDL_Rect status_bar = {0, SCREEN_H - STATUS_BAR_HEIGHT, SCREEN_W, STATUS_BAR_HEIGHT};
    SDL_Color status_color = COLOR_STATUS_TEXT;
    SDL_Rect status_rect;
    SDL_Texture* status_text = render_text(renderer,
        "- MENU    + QUIT    X BROWSE/FAVES/HISTORY    Y TOGGLE FAVORITE",
        small_font, status_color, &status_rect, 0);
    if (status_text) {
        status_rect.x = (SCREEN_W - status_rect.w) / 2;
        status_rect.y = SCREEN_H - STATUS_BAR_HEIGHT + (STATUS_BAR_HEIGHT - status_rect.h) / 2;
    }

    while (!exit_requested
        && appletMainLoop()
        ) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                exit_requested = 1;

            // main event queue handler - handle multiple controller input types
            if (event.type == SDL_JOYBUTTONDOWN) {
                log_message(LOG_DEBUG, "Button pressed: %d", event.jbutton.button);

                // If notification is active, any button dismisses it
                if (notification.active) {
                    notification.active = 0;
                    continue;
                }

                if (event.jbutton.button == DPAD_UP || event.jbutton.button == DPAD_DOWN) {
                    int direction = (event.jbutton.button == DPAD_UP) ? -1 : 1;
                    handle_navigation_input(direction);
                }

                if (event.jbutton.button == JOY_A) {
                    switch (current_app_mode) {
                        case APP_MODE_BROWSER:
                            if (current_browser_mode == BROWSER_MODE_FILES) {
                                log_message(LOG_DEBUG, "A button pressed in mode: BROWSER/FILES");
                                if (selected_index < content->dir_count) {
                                    change_directory(content, selected_index, current_path);
                                    selected_index = 0;
                                    total_entries = content->dir_count + content->file_count;
                                    current_page = 0;
                                    total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                                    set_selection(content, renderer, font, selected_index, current_page);
                                } else {
                                    int file_index = selected_index - content->dir_count;
                                    if (file_index >= 0 && file_index < content->file_count) {
                                        char rom_path[MAX_PATH_LEN];
                                        int written = snprintf(rom_path, sizeof(rom_path), "%s/%s", current_path + 5, content->files[file_index]);
                                        if (written < 0 || (size_t)written >= sizeof(rom_path)) {
                                            log_message(LOG_ERROR, "ROM path construction failed (truncation or error)");
                                            continue;
                                        }
                                        if (launch_retroarch(rom_path)) {
                                            exit_requested = 1;
                                        } else {
                                            if (notification.texture) {
                                                SDL_DestroyTexture(notification.texture);
                                            }
                                            notification.texture = render_text(renderer, "Error launching emulator", font, COLOR_TEXT_ERROR, &notification.rect, 0);
                                            notification.rect.x = (SCREEN_W - notification.rect.w) / 2;
                                            notification.rect.y = SCREEN_H - notification.rect.h - 20;
                                            notification.active = 1;
                                        }
                                    }
                                }
                            } else if (current_browser_mode == BROWSER_MODE_FAVORITES) {
                                log_message(LOG_DEBUG, "Favorites mode: selected_index=%d, file_count=%d",
                                            selected_index, favorites_content ? favorites_content->file_count : -1);
                                if (selected_index >= 0 && favorites_content && selected_index < favorites_content->file_count) {
                                    if (!is_group_header(favorites_content->files[selected_index])) {
                                        log_message(LOG_DEBUG, "Selected favorite is not a group header");
                                        FavoriteGroup* group = favorites_content->groups;
                                        int count = 0;
                                        while (group) {
                                            count++; // Skip the group header
                                            if (selected_index < count + group->entry_count) {
                                                FavoriteEntry* entry = group->entries;
                                                for (int i = 0; i < (group->entry_count - 1) - (selected_index - count); i++) {
                                                    entry = entry->next;
                                                }
                                                if (entry && entry->path) {
                                                    char launch_path[MAX_PATH_LEN];
                                                    if (strncmp(entry->path, "sdmc:", 5) != 0) {
                                                        snprintf(launch_path, sizeof(launch_path), "sdmc:%s", entry->path);
                                                    } else {
                                                        strncpy(launch_path, entry->path, sizeof(launch_path)-1);
                                                        launch_path[sizeof(launch_path)-1] = '\0';
                                                    }
                                                    log_message(LOG_INFO, "Attempting to launch favorite: %s", launch_path);
                                                    if (launch_retroarch(launch_path)) {
                                                        exit_requested = 1;
                                                    } else {
                                                        if (notification.texture) {
                                                            SDL_DestroyTexture(notification.texture);
                                                        }
                                                        notification.texture = render_text(renderer, "Error launching emulator", font, COLOR_TEXT_ERROR, &notification.rect, 0);
                                                        notification.rect.x = (SCREEN_W - notification.rect.w) / 2;
                                                        notification.rect.y = SCREEN_H - notification.rect.h - 20;
                                                        notification.active = 1;
                                                    }
                                                } else {
                                                    log_message(LOG_ERROR, "Invalid favorite entry or path");
                                                }
                                                break;
                                            }
                                            count += group->entry_count;
                                            group = group->next;
                                        }
                                    }
                                }
                            } else if (current_browser_mode == BROWSER_MODE_HISTORY) {
                                log_message(LOG_DEBUG, "History mode: selected_index=%d, file_count=%d",
                                            selected_index, history_content ? history_content->file_count : -1);
                                if (selected_index >= 0 && history_content && selected_index < history_content->file_count) {
                                    const char* rom_path = get_history_rom_path(selected_index);
                                    if (rom_path) {
                                        log_message(LOG_INFO, "Attempting to launch history entry: %s", rom_path);
                                        if (launch_retroarch(rom_path)) {
                                            exit_requested = 1;
                                        } else {
                                            if (notification.texture) {
                                                SDL_DestroyTexture(notification.texture);
                                            }
                                            notification.texture = render_text(renderer, "Error launching emulator", font, COLOR_TEXT_ERROR, &notification.rect, 0);
                                            notification.rect.x = (SCREEN_W - notification.rect.w) / 2;
                                            notification.rect.y = SCREEN_H - notification.rect.h - 20;
                                            notification.active = 1;
                                        }
                                    } else {
                                        log_message(LOG_ERROR, "Invalid history entry path");
                                    }
                                }
                            }
                            break;
                        case APP_MODE_MENU:
                            // Menu mode is handled elsewhere
                            break;
                        case APP_MODE_SCRAPING:
                            // Scraping mode is handled elsewhere
                            break;
                        default:
                            // Do nothing for unhandled modes
                            break;
                    }
                }

                if (event.jbutton.button == JOY_X) {
                    // Cycle through browser modes: FILES -> FAVORITES -> HISTORY -> FILES
                    if (current_app_mode == APP_MODE_BROWSER) {
                        strncpy(saved_path, current_path, MAX_PATH_LEN-1);
                        saved_path[MAX_PATH_LEN-1] = '\0';

                        switch (current_browser_mode) {
                            case BROWSER_MODE_FILES:
                                log_message(LOG_INFO, "Switching to favorites");
                                current_browser_mode = BROWSER_MODE_FAVORITES;

                                // Switch to favorites mode
                                if (favorites_content) free_dir_content(favorites_content);
                                favorites_content = list_favorites();
                                if (favorites_content) {
                                    selected_index = find_next_rom(favorites_content, -1, 1);
                                    current_page = selected_index / ENTRIES_PER_PAGE;
                                    total_entries = favorites_content->file_count;
                                    total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                                    set_selection(favorites_content, renderer, font, selected_index, current_page);
                                }
                                break;

                            case BROWSER_MODE_FAVORITES:
                                // Switch to history mode
                                log_message(LOG_INFO, "Switching to history");
                                current_browser_mode = BROWSER_MODE_HISTORY;

                                if (favorites_content) free_dir_content(favorites_content);
                                favorites_content = NULL;

                                if (history_content) free_dir_content(history_content);
                                history_content = list_history();
                                if (history_content) {
                                    selected_index = 0;
                                    current_page = 0;
                                    total_entries = history_content->file_count;
                                    total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                                    set_selection(history_content, renderer, font, selected_index, current_page);
                                }
                                break;

                            case BROWSER_MODE_HISTORY:
                                log_message(LOG_INFO, "Switching to files");
                                current_browser_mode = BROWSER_MODE_FILES;

                                // Switch back to files mode
                                if (history_content) free_dir_content(history_content);
                                history_content = NULL;

                                strncpy(current_path, saved_path, MAX_PATH_LEN-1);
                                current_path[MAX_PATH_LEN-1] = '\0';
                                selected_index = 0;
                                current_page = 0;
                                set_selection(content, renderer, font, selected_index, current_page);
                                break;
                        }
                    }

                }

                if (event.jbutton.button == JOY_Y) {
                    // Y button now toggles favorites in any mode
                    toggle_current_favorite(content, selected_index, current_path);

                    // If we're in favorites mode, refresh the list
                    if (current_app_mode == APP_MODE_BROWSER && current_browser_mode == BROWSER_MODE_FAVORITES) {
                        if (favorites_content) free_dir_content(favorites_content);
                        favorites_content = list_favorites();
                        selected_index = 0;
                        current_page = 0;
                        if (favorites_content) {
                            total_entries = favorites_content->file_count;
                            total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                            set_selection(favorites_content, renderer, font, selected_index, current_page);
                        }
                    } else {
                        set_selection(content, renderer, font, selected_index, current_page);

                        // Load box art for selected file
                        update_box_art_for_selection(content, renderer, current_path, selected_index);
                    }
                }

                if (event.jbutton.button == JOY_B) {
                    switch (current_app_mode) {
                        case APP_MODE_SCRAPING:
                            current_app_mode = APP_MODE_BROWSER;
                            current_browser_mode = BROWSER_MODE_FILES;
                            if (scraping_message) {
                                SDL_DestroyTexture(scraping_message);
                                scraping_message = NULL;
                            }
                            break;
                        case APP_MODE_BROWSER:
                            if (current_browser_mode == BROWSER_MODE_FAVORITES) {
                                if (favorites_content) free_dir_content(favorites_content);
                                favorites_content = NULL;
                                current_browser_mode = BROWSER_MODE_FILES;
                                strncpy(current_path, saved_path, MAX_PATH_LEN-1);
                                current_path[MAX_PATH_LEN-1] = '\0';
                                selected_index = 0;
                                current_page = 0;
                                set_selection(content, renderer, font, selected_index, current_page);
                            } else if (current_browser_mode == BROWSER_MODE_HISTORY) {
                                if (history_content) free_dir_content(history_content);
                                history_content = NULL;
                                current_browser_mode = BROWSER_MODE_FILES;
                                strncpy(current_path, saved_path, MAX_PATH_LEN-1);
                                current_path[MAX_PATH_LEN-1] = '\0';
                                selected_index = 0;
                                current_page = 0;
                                set_selection(content, renderer, font, selected_index, current_page);
                            } else {
                                go_up_directory(content, current_path, ROM_DIRECTORY);
                                selected_index = 0;
                                total_entries = content->dir_count + content->file_count;
                                current_page = 0;
                                total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                                set_selection(content, renderer, font, selected_index, current_page);
                            }
                            break;
                        default:
                            break;
                    }
                }

                if (event.jbutton.button == JOY_LEFT_SHOULDER) {
                    handle_page_navigation(-1);
                }

                if (event.jbutton.button == JOY_RIGHT_SHOULDER) {
                    handle_page_navigation(1);
                }

                if (event.jbutton.button == JOY_MINUS) {
                    if (current_app_mode != APP_MODE_MENU) {
                        current_app_mode = APP_MODE_MENU;
                        menu_selection = 0;

                        // Create menu textures
                        update_menu_selection(0);
                        
                        // Position menu items
                        for (int i = 0; i < MENU_OPTIONS; i++) {
                            menu_rects[i].x = (SCREEN_W - menu_rects[i].w) / 2;
                            menu_rects[i].y = SCREEN_H/3 + i * 60;
                        }
                    } else {
                        current_app_mode = APP_MODE_BROWSER;
                        // Cleanup menu textures
                        for (int i = 0; i < MENU_OPTIONS; i++) {
                            if (menu_textures[i]) {
                                SDL_DestroyTexture(menu_textures[i]);
                                menu_textures[i] = NULL;
                            }
                        }
                    }
                }

                if (current_app_mode == APP_MODE_MENU) {
                    if (event.jbutton.button == DPAD_UP) {
                        int new_selection = (menu_selection > 0) ? menu_selection - 1 : MENU_OPTIONS - 1;
                        update_menu_selection(new_selection);
                    }
                    else if (event.jbutton.button == DPAD_DOWN) {
                        int new_selection = (menu_selection < MENU_OPTIONS - 1) ? menu_selection + 1 : 0;
                        update_menu_selection(new_selection);
                    }
                    else if (event.jbutton.button == JOY_A) {
                        switch (menu_selection) {
                            case MENU_HELP:
                                // Help functionality to be implemented
                                break;
                            case MENU_HISTORY:
                                // Switch to history mode
                                strncpy(saved_path, current_path, MAX_PATH_LEN-1);
                                saved_path[MAX_PATH_LEN-1] = '\0';
                                history_content = list_history();
                                if (history_content) {
                                    selected_index = 0;
                                    current_page = 0;
                                    total_entries = history_content->file_count;
                                    total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                                    log_message(LOG_INFO, "Switching to history mode with %d entries", total_entries);
                                    set_selection(history_content, renderer, font, selected_index, current_page);
                                }
                                break;
                            case MENU_SCRAPER:
                                current_app_mode = APP_MODE_SCRAPING;
                                if (scraping_message) SDL_DestroyTexture(scraping_message);
                                scraping_message = render_text(renderer, "Press B to stop", font, COLOR_TEXT, &scraping_rect, 0);
                                scraping_rect.x = (SCREEN_W - scraping_rect.w) / 2;
                                scraping_rect.y = (SCREEN_H - scraping_rect.h) / 2;
                                break;
                            case MENU_QUIT:
                                exit_requested = 1;
                                break;
                        }
                    }
                }

                if (event.jbutton.button == JOY_PLUS) {
                    exit_requested = 1;
                }
            }
            // Handle hat events (D-pad on many controllers)
            if (event.type == SDL_JOYHATMOTION) {
                log_message(LOG_DEBUG, "Hat motion: %d", event.jhat.value);
                
                if (notification.active) {
                    notification.active = 0;
                    continue;
                }
                
                if (event.jhat.value & SDL_HAT_UP) {
                    handle_navigation_input(-1);
                }
                else if (event.jhat.value & SDL_HAT_DOWN) {
                    handle_navigation_input(1);
                }
            }
            // Handle axis events (analog sticks and sometimes D-pad)
            else if (event.type == SDL_JOYAXISMOTION) {
                // Only process significant movements (avoid drift)
                if (abs(event.jaxis.value) > 16000) {
                    log_message(LOG_DEBUG, "Axis %d motion: %d", event.jaxis.axis, event.jaxis.value);
                    
                    if (notification.active) {
                        notification.active = 0;
                        continue;
                    }
                    
                    // Vertical axis (typically 1 or 3 for left/right stick)
                    if (event.jaxis.axis == 1 || event.jaxis.axis == 3) {
                        if (event.jaxis.value < -16000) {
                            // Up direction
                            handle_navigation_input(-1);
                        } 
                        else if (event.jaxis.value > 16000) {
                            // Down direction
                            handle_navigation_input(1);
                        }
                    }
                }
            }
        }

        
        // Process button repeats
        {
            Uint32 now = SDL_GetTicks();
            
            // Up button repeat
            handle_button_repeat(DPAD_UP, &dpadUpHeld, &dpadUpInitialDelay, &dpadUpRepeatTime, now, 
                                 handle_up_navigation);
            
            // Down button repeat
            handle_button_repeat(DPAD_DOWN, &dpadDownHeld, &dpadDownInitialDelay, &dpadDownRepeatTime, now,
                                 handle_down_navigation);
            
            // Left shoulder button repeat
            if (SDL_JoystickGetButton(joystick, JOY_LEFT_SHOULDER)) {
                if (!leftShoulderHeld) {
                    leftShoulderHeld = 1;
                    leftShoulderRepeatTime = now;
                } else if (now - leftShoulderRepeatTime >= 50) {
                    handle_page_navigation(-1);
                    leftShoulderRepeatTime = now;
                }
            } else {
                leftShoulderHeld = 0;
            }
            
            // Right shoulder button repeat
            if (SDL_JoystickGetButton(joystick, JOY_RIGHT_SHOULDER)) {
                if (!rightShoulderHeld) {
                    rightShoulderHeld = 1;
                    rightShoulderRepeatTime = now;
                } else if (now - rightShoulderRepeatTime >= 50) {
                    handle_page_navigation(1);
                    rightShoulderRepeatTime = now;
                }
            } else {
                rightShoulderHeld = 0;
            }
        }

        SDL_SetRenderDrawColor(renderer,
            COLOR_BACKGROUND.r,
            COLOR_BACKGROUND.g,
            COLOR_BACKGROUND.b,
            COLOR_BACKGROUND.a);
        SDL_RenderClear(renderer);

        // Draw semi-transparent background if notification is active
        if (notification.active) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_RenderFillRect(renderer, NULL);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        // Render directory and file listings, menu, or scraping message
        if (current_app_mode == APP_MODE_SCRAPING && scraping_message) {
            SDL_RenderCopy(renderer, scraping_message, NULL, &scraping_rect);
        } else if (current_app_mode == APP_MODE_MENU) {
            // Render menu options
            for (int i = 0; i < MENU_OPTIONS; i++) {
                if (menu_textures[i]) {
                    SDL_RenderCopy(renderer, menu_textures[i], NULL, &menu_rects[i]);
                }
            }
        } else {
            DirContent* current_content = get_current_content();
            if (current_content) {
                for (int i = 0; i < current_content->dir_count; i++) {
                    if (current_content->dir_textures[i]) {
                        SDL_RenderCopy(renderer, current_content->dir_textures[i], NULL, &current_content->dir_rects[i]);
                    }
                }
                for (int i = 0; i < current_content->file_count; i++) {
                    if (current_content->file_textures[i]) {
                        SDL_RenderCopy(renderer, current_content->file_textures[i], NULL, &current_content->file_rects[i]);
                    }
                }
            }
        }

        // Render box art if available
        if (current_app_mode != APP_MODE_MENU && current_app_mode != APP_MODE_SCRAPING) {
            DirContent* current_content = get_current_content();
            if (current_content && current_content->box_art_texture) {
                SDL_RenderCopy(renderer, current_content->box_art_texture, NULL, &current_content->box_art_rect);
            }
        }

        // Render notification if active
        if (notification.active && notification.texture) {
            SDL_RenderCopy(renderer, notification.texture, NULL, &notification.rect);
        }

        // Render status bar
        SDL_SetRenderDrawColor(renderer,
            COLOR_STATUS_BAR.r,
            COLOR_STATUS_BAR.g,
            COLOR_STATUS_BAR.b,
            COLOR_STATUS_BAR.a);
        SDL_RenderFillRect(renderer, &status_bar);

        // Render status text
        if (status_text) {
            SDL_RenderCopy(renderer, status_text, NULL, &status_rect);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(wait);
    }

    // Free favorites and history content if they exist
    if (favorites_content) {
        free_dir_content(favorites_content);
    }

    if (history_content) {
        free_dir_content(history_content);
    }

cleanup:
    // Clean up all textures
    if (notification.texture)
        SDL_DestroyTexture(notification.texture);
    if (scraping_message)
        SDL_DestroyTexture(scraping_message);
    if (status_text)
        SDL_DestroyTexture(status_text);

    // Clean up menu textures
    for (int i = 0; i < MENU_OPTIONS; i++) {
        if (menu_textures[i])
            SDL_DestroyTexture(menu_textures[i]);
    }

    // Close joystick if it was opened
    if (joystick)
        SDL_JoystickClose(joystick);

    // Clean up SDL systems in reverse order of initialization
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    if (font)
        TTF_CloseFont(font);
    if (small_font)
        TTF_CloseFont(small_font);

    // Quit SDL subsystems in reverse order
    if (ttf_initialized)
        TTF_Quit();
    if (img_initialized)
        IMG_Quit();
    if (sdl_initialized)
        SDL_Quit();
#ifndef ROMLAUNCHER_BUILD_LINUX
    if (romfs_initialized)
        romfsExit();
#endif

    // Free config, favorites, history, and hash tables
    free_config();
    free_favorites();
    free_history();
    free_default_core_mappings();
    free_system_names_mappings();

    log_message(LOG_INFO, "Finished cleanup - all done!");
    log_close();

    return 0;
}
