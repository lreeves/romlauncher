#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include "logging.h"
#include "config.h"
#include "browser.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <switch.h>

// some switch buttons
#define JOY_A     0
#define JOY_B     1
#define JOY_X     2
#define JOY_Y     3
#define MODE_BROWSER 0
#define MODE_FAVORITES 1
#define MODE_MENU 2
#define MODE_SCRAPING 3

// Menu options
#define MENU_HELP 0
#define MENU_SCRAPER 1
#define MENU_QUIT 2
#define MENU_OPTIONS 3  // Total number of menu options
#define JOY_PLUS  10
#define JOY_MINUS 11
#define JOY_LEFT  12
#define JOY_RIGHT 14
#define JOY_LEFT_SHOULDER 6
#define JOY_RIGHT_SHOULDER 7
#define DPAD_UP    13
#define DPAD_DOWN  15

#define ROMLAUNCHER_DATA_DIRECTORY "sdmc:/romlauncher/"
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

#define MAX_PATH_LEN 512

// Helper function to get file extension
const char* get_file_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

// Launch RetroArch with the given ROM path
int launch_retroarch(const char* rom_path) {
    log_message(LOG_INFO, "Launch request for ROM: %s", rom_path);

    // Get file extension and look up core
    const char* ext = get_file_extension(rom_path);
    log_message(LOG_INFO, "ROM extension: %s", ext);

    config_entry *core_entry;
    HASH_FIND_STR(default_core_mappings, ext, core_entry);

    if (!core_entry) {
        log_message(LOG_ERROR, "No core mapping found for extension: %s", ext);
        return 0;
    }

    log_message(LOG_INFO, "Found core mapping: %s -> %s", ext, core_entry->value);

    // Construct core path
    char core_path[MAX_PATH_LEN];
    int written = snprintf(core_path, sizeof(core_path), "sdmc:/retroarch/cores/%s_libretro_libnx.nro", core_entry->value);
    if (written < 0 || (size_t)written >= sizeof(core_path)) {
        log_message(LOG_ERROR, "Core path construction failed (truncation or error)");
        return 0;
    }

    // Check if core exists
    FILE *core_file = fopen(core_path, "r");
    if (core_file == NULL) {
        log_message(LOG_ERROR, "Core not found at: %s", core_path);
        return 0;
    }
    fclose(core_file);

    // Skip the "sdmc:" prefix if present for the arguments
    const char* rom_arg = strncmp(rom_path, "sdmc:", 5) == 0 ? rom_path + 5 : rom_path;

    // Construct the arguments string
    char full_arguments[MAX_PATH_LEN];
    written = snprintf(full_arguments, sizeof(full_arguments), "%s \"%s\"", core_path, rom_arg);
    if (written < 0 || (size_t)written >= sizeof(full_arguments)) {
        log_message(LOG_ERROR, "Arguments string construction failed (truncation or error)");
        return 0;
    }

    // Log launch details
    log_message(LOG_INFO, "Launching RetroArch: %s with args: %s", core_path, full_arguments);

    // Launch RetroArch with the selected ROM
    Result rc = envSetNextLoad(core_path, full_arguments);
    if (R_SUCCEEDED(rc)) {
        log_message(LOG_INFO, "Successfully set next load");
        return 1;
    } else {
        log_message(LOG_ERROR, "Failed to set next load, error: %x", rc);
        return 0;
    }
}

const char* rom_directory = ROM_PATH;
char current_path[MAX_PATH_LEN];

int rand_range(int min, int max){
   return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}


int main(int argc, char** argv) {
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

    // Load config and favorites
    load_config();
    load_favorites();
    init_default_core_mappings();
    log_message(LOG_INFO, "Config and favorites loaded");

    romfsInit();
    chdir("romfs:/");

    log_message(LOG_DEBUG, "romFS initialized");

    int exit_requested = 0;
    int wait = 25;
    int current_mode = MODE_BROWSER;
    DirContent* favorites_content = NULL;
    char saved_path[MAX_PATH_LEN];
    int menu_selection = 0;
    SDL_Texture* menu_textures[MENU_OPTIONS] = {NULL};
    SDL_Rect menu_rects[MENU_OPTIONS];
    const char* menu_options[] = {"Help", "Scraper", "Quit"};
    
    // Scraping mode message
    SDL_Texture* scraping_message = NULL;
    SDL_Rect scraping_rect;

    Notification notification = {0};

    SDL_Texture *switchlogo_tex = NULL, *sdllogo_tex = NULL;
    SDL_Event event;

    int selected_index = 0;
    int total_entries = 0;
    int current_page = 0;
    int total_pages = 0;

    srand(time(NULL));

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        log_message(LOG_ERROR, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    log_message(LOG_DEBUG, "SDL_Init completed");

    int img_flags = IMG_INIT_PNG;
    if ((IMG_Init(img_flags) & img_flags) != img_flags) {
        log_message(LOG_ERROR, "IMG_Init failed: %s", IMG_GetError());
        SDL_Quit();
        return 1;
    }
    log_message(LOG_DEBUG, "IMG_Init completed");

    if (TTF_Init() < 0) {
        log_message(LOG_ERROR, "TTF_Init failed: %s", TTF_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
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

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        log_message(LOG_ERROR, "Renderer creation failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    log_message(LOG_DEBUG, "SDL renderer created");

    // load logos from file
    SDL_Surface *sdllogo = IMG_Load("data/sdl.png");
    if (sdllogo) {
        sdllogo_tex = SDL_CreateTextureFromSurface(renderer, sdllogo);
        SDL_FreeSurface(sdllogo);
    }

    SDL_Surface *switchlogo = IMG_Load("data/switch.png");
    if (switchlogo) {
        switchlogo_tex = SDL_CreateTextureFromSurface(renderer, switchlogo);
        SDL_FreeSurface(switchlogo);
    }

    log_message(LOG_DEBUG, "SDL surfaces initialized");

    SDL_JoystickEventState(SDL_ENABLE);
    SDL_Joystick* joystick = SDL_JoystickOpen(0);

    log_message(LOG_DEBUG, "SDL joystick initialized");

    // load fonts from romfs
    TTF_Font* font = TTF_OpenFont("data/Raleway-Regular.ttf", 32);
    TTF_Font* small_font = TTF_OpenFont("data/Raleway-Regular.ttf", 16);

    log_message(LOG_INFO, "About to list files");
    strncpy(current_path, rom_directory, sizeof(current_path) - 1);
    DirContent* content = list_files(current_path);
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

    while (!exit_requested
        && appletMainLoop()
        ) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                exit_requested = 1;

            // main event queue handler - use Switch controller inputs
            if (event.type == SDL_JOYBUTTONDOWN) {
                log_message(LOG_DEBUG, "Button pressed: %d", event.jbutton.button);

                // If notification is active, any button dismisses it
                if (notification.active) {
                    notification.active = 0;
                    continue;
                }

                if (event.jbutton.button == DPAD_UP || event.jbutton.button == DPAD_DOWN) {
                    if (current_mode == MODE_FAVORITES && favorites_content) {
                        // In favorites mode, skip group headers
                        int direction = (event.jbutton.button == DPAD_UP) ? -1 : 1;
                        selected_index = find_next_rom(favorites_content, selected_index, direction);
                    } else {
                        // Normal directory browsing behavior
                        if (event.jbutton.button == DPAD_UP) {
                            if (selected_index > 0) {
                                selected_index--;
                            } else {
                                selected_index = total_entries - 1;
                            }
                        } else { // DPAD_DOWN
                            if (selected_index < total_entries - 1) {
                                selected_index++;
                            } else {
                                selected_index = 0;
                            }
                        }
                    }
                    current_page = selected_index / ENTRIES_PER_PAGE;
                    set_selection(current_mode == MODE_FAVORITES ? favorites_content : content,
                                renderer, font, selected_index, current_page);
                }

                if (event.jbutton.button == JOY_A) {
                    log_message(LOG_DEBUG, "A button pressed in mode: %s", 
                              current_mode == MODE_BROWSER ? "BROWSER" : "FAVORITES");
                    
                    if (current_mode == MODE_BROWSER) {
                        if (selected_index < content->dir_count) {
                            change_directory(content, selected_index, current_path);
                            selected_index = 0;
                            total_entries = content->dir_count + content->file_count;
                            current_page = 0;
                            total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                            set_selection(content, renderer, font, selected_index, current_page);
                        } else {
                            // Calculate which file was selected (accounting for directories)
                            int file_index = selected_index - content->dir_count;
                            if (file_index >= 0 && file_index < content->file_count) {
                                // First, construct the full ROM path safely
                                char rom_path[MAX_PATH_LEN];
                                int written = snprintf(rom_path, sizeof(rom_path), "%s/%s", current_path + 5, content->files[file_index]);
                                if (written < 0 || (size_t)written >= sizeof(rom_path)) {
                                    log_message(LOG_ERROR, "ROM path construction failed (truncation or error)");
                                    continue;
                                }

                                // Try to launch RetroArch
                                if (launch_retroarch(rom_path)) {
                                    exit_requested = 1;
                                } else {
                                    // Show error notification
                                    if (notification.texture) {
                                        SDL_DestroyTexture(notification.texture);
                                    }
                                    notification.texture = render_text(renderer, "Error launching emulator", font, COLOR_TEXT_ERROR, &notification.rect);
                                    notification.rect.x = (SCREEN_W - notification.rect.w) / 2;
                                    notification.rect.y = SCREEN_H - notification.rect.h - 20; // 20px padding from bottom
                                    notification.active = 1;
                                }
                            }
                        }
                    } else if (current_mode == MODE_FAVORITES) {
                        log_message(LOG_DEBUG, "Favorites mode: selected_index=%d, file_count=%d", 
                                  selected_index, favorites_content ? favorites_content->file_count : -1);
                        
                        // In favorites mode, directly launch the selected ROM
                        if (selected_index >= 0 && favorites_content && selected_index < favorites_content->file_count) {
                            // Skip if this is a group header
                            if (!is_group_header(favorites_content->files[selected_index])) {
                                log_message(LOG_DEBUG, "Selected favorite is not a group header");
                                // Find the actual path from the groups structure
                                FavoriteGroup* group = favorites_content->groups;
                                int count = 0;
                                
                                while (group) {
                                    count++; // Skip the group header
                                    if (selected_index < count + group->entry_count) {
                                        // Found the right group, now find the entry
                                        FavoriteEntry* entry = group->entries;
                                        // Reverse through the linked list since entries were added in reverse
                                        for (int i = 0; i < (group->entry_count - 1) - (selected_index - count); i++) {
                                            entry = entry->next;
                                        }
                                        if (entry && entry->path) {
                                            // Ensure path starts with "sdmc:"
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
                                                // Show error notification
                                                if (notification.texture) {
                                                    SDL_DestroyTexture(notification.texture);
                                                }
                                                notification.texture = render_text(renderer, "Error launching emulator", font, COLOR_TEXT_ERROR, &notification.rect);
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
                    }
                }

                if (event.jbutton.button == JOY_X) {
                    toggle_current_favorite(content, selected_index, current_path);
                    set_selection(content, renderer, font, selected_index, current_page);

                    // If we're in favorites mode, refresh the list
                    if (current_mode == MODE_FAVORITES) {
                        if (favorites_content) free_dir_content(favorites_content);
                        favorites_content = list_favorites();
                        selected_index = 0;
                        current_page = 0;
                        if (favorites_content) {
                            total_entries = favorites_content->file_count;
                            total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                            set_selection(favorites_content, renderer, font, selected_index, current_page);
                        }
                    }
                }

                if (event.jbutton.button == JOY_Y) {
                    if (current_mode == MODE_BROWSER) {
                        // Switch to favorites mode
                        strncpy(saved_path, current_path, MAX_PATH_LEN-1);
                        saved_path[MAX_PATH_LEN-1] = '\0';
                        favorites_content = list_favorites();
                        if (favorites_content) {
                            current_mode = MODE_FAVORITES;
                            // Find first non-header entry
                            selected_index = find_next_rom(favorites_content, -1, 1);
                            current_page = selected_index / ENTRIES_PER_PAGE;
                            total_entries = favorites_content->file_count;
                            total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                            set_selection(favorites_content, renderer, font, selected_index, current_page);
                        }
                    } else {
                        // Switch back to browser mode
                        if (favorites_content) free_dir_content(favorites_content);
                        favorites_content = NULL;
                        current_mode = MODE_BROWSER;
                        strncpy(current_path, saved_path, MAX_PATH_LEN-1);
                        current_path[MAX_PATH_LEN-1] = '\0';
                        selected_index = 0;
                        current_page = 0;
                        set_selection(content, renderer, font, selected_index, current_page);
                    }
                }

                if (event.jbutton.button == JOY_B) {
                    if (current_mode == MODE_SCRAPING) {
                        current_mode = MODE_BROWSER;
                        if (scraping_message) {
                            SDL_DestroyTexture(scraping_message);
                            scraping_message = NULL;
                        }
                    } else {
                        go_up_directory(content, current_path, rom_directory);
                        selected_index = 0;
                        total_entries = content->dir_count + content->file_count;
                        current_page = 0;
                        total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                        set_selection(content, renderer, font, selected_index, current_page);
                    }
                }

                if (event.jbutton.button == JOY_LEFT_SHOULDER) {
                    if (current_page > 0) {
                        current_page--;
                    } else {
                        current_page = total_pages - 1;
                    }
                    selected_index = current_page * ENTRIES_PER_PAGE;
                    set_selection(current_mode == MODE_FAVORITES ? favorites_content : content,
                                renderer, font, selected_index, current_page);
                }

                if (event.jbutton.button == JOY_RIGHT_SHOULDER) {
                    if (current_page < total_pages - 1) {
                        current_page++;
                    } else {
                        current_page = 0;
                    }
                    selected_index = current_page * ENTRIES_PER_PAGE;
                    set_selection(current_mode == MODE_FAVORITES ? favorites_content : content,
                                renderer, font, selected_index, current_page);
                }

                if (event.jbutton.button == JOY_MINUS) {
                    if (current_mode != MODE_MENU) {
                        current_mode = MODE_MENU;
                        menu_selection = 0;
                        
                        // Create menu textures
                        for (int i = 0; i < MENU_OPTIONS; i++) {
                            if (menu_textures[i]) {
                                SDL_DestroyTexture(menu_textures[i]);
                            }
                            SDL_Color color = (i == menu_selection) ? COLOR_TEXT_SELECTED : COLOR_TEXT;
                            menu_textures[i] = render_text(renderer, menu_options[i], font, color, &menu_rects[i]);
                            menu_rects[i].x = (SCREEN_W - menu_rects[i].w) / 2;
                            menu_rects[i].y = SCREEN_H/3 + i * 60;
                        }
                    } else {
                        current_mode = MODE_BROWSER;
                        // Cleanup menu textures
                        for (int i = 0; i < MENU_OPTIONS; i++) {
                            if (menu_textures[i]) {
                                SDL_DestroyTexture(menu_textures[i]);
                                menu_textures[i] = NULL;
                            }
                        }
                    }
                }

                if (current_mode == MODE_MENU) {
                    if (event.jbutton.button == DPAD_UP) {
                        if (menu_selection > 0) menu_selection--;
                        else menu_selection = MENU_OPTIONS - 1;
                        
                        // Update menu textures
                        for (int i = 0; i < MENU_OPTIONS; i++) {
                            if (menu_textures[i]) SDL_DestroyTexture(menu_textures[i]);
                            SDL_Color color = (i == menu_selection) ? COLOR_TEXT_SELECTED : COLOR_TEXT;
                            menu_textures[i] = render_text(renderer, menu_options[i], font, color, &menu_rects[i]);
                        }
                    }
                    else if (event.jbutton.button == DPAD_DOWN) {
                        if (menu_selection < MENU_OPTIONS - 1) menu_selection++;
                        else menu_selection = 0;
                        
                        // Update menu textures
                        for (int i = 0; i < MENU_OPTIONS; i++) {
                            if (menu_textures[i]) SDL_DestroyTexture(menu_textures[i]);
                            SDL_Color color = (i == menu_selection) ? COLOR_TEXT_SELECTED : COLOR_TEXT;
                            menu_textures[i] = render_text(renderer, menu_options[i], font, color, &menu_rects[i]);
                        }
                    }
                    else if (event.jbutton.button == JOY_A) {
                        switch (menu_selection) {
                            case MENU_HELP:
                                // Help functionality to be implemented
                                break;
                            case MENU_SCRAPER:
                                current_mode = MODE_SCRAPING;
                                if (scraping_message) SDL_DestroyTexture(scraping_message);
                                scraping_message = render_text(renderer, "Press B to stop", font, COLOR_TEXT, &scraping_rect);
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
        if (current_mode == MODE_SCRAPING && scraping_message) {
            SDL_RenderCopy(renderer, scraping_message, NULL, &scraping_rect);
        } else if (current_mode == MODE_MENU) {
            // Render menu options
            for (int i = 0; i < MENU_OPTIONS; i++) {
                if (menu_textures[i]) {
                    SDL_RenderCopy(renderer, menu_textures[i], NULL, &menu_rects[i]);
                }
            }
        } else {
            DirContent* current_content = (current_mode == MODE_FAVORITES) ? favorites_content : content;
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
        SDL_Rect status_bar = {0, SCREEN_H - STATUS_BAR_HEIGHT, SCREEN_W, STATUS_BAR_HEIGHT};
        SDL_RenderFillRect(renderer, &status_bar);

        // Render status text
        SDL_Color status_color = COLOR_STATUS_TEXT;
        SDL_Rect status_rect;
        SDL_Texture* status_text = render_text(renderer, 
            "- MENU    + QUIT    X FAVORITES    Y ADD FAVORITE",
            small_font, status_color, &status_rect);
        if (status_text) {
            status_rect.x = (SCREEN_W - status_rect.w) / 2;
            status_rect.y = SCREEN_H - STATUS_BAR_HEIGHT + (STATUS_BAR_HEIGHT - status_rect.h) / 2;
            SDL_RenderCopy(renderer, status_text, NULL, &status_rect);
            SDL_DestroyTexture(status_text);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(wait);
    }

    // Free favorites content if it exists
    if (favorites_content) {
        free_dir_content(favorites_content);
    }

    // clean up your textures when you are done with them
    if (sdllogo_tex)
        SDL_DestroyTexture(sdllogo_tex);

    if (switchlogo_tex)
        SDL_DestroyTexture(switchlogo_tex);

    if (notification.texture)
        SDL_DestroyTexture(notification.texture);
    
    if (scraping_message)
        SDL_DestroyTexture(scraping_message);
        
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
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    romfsExit();

    // Free config, favorites, and core mappings hash tables
    free_config();
    free_favorites();
    free_default_core_mappings();

    log_message(LOG_INFO, "Finished cleanup - all done!");
    log_close();

    return 0;
}
