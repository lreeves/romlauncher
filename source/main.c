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
#define JOY_PLUS  10
#define JOY_MINUS 11
#define JOY_LEFT  12
#define JOY_RIGHT 14
#define JOY_LEFT_SHOULDER 6
#define JOY_RIGHT_SHOULDER 7
#define DPAD_UP    13
#define DPAD_DOWN  15

#define SCREEN_W 1280
#define SCREEN_H 720

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

const char* rom_directory = "sdmc:/roms";
char current_path[MAX_PATH_LEN];

int rand_range(int min, int max){
   return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}


int main(int argc, char** argv) {
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "/romlauncher-%ld.log", (long)time(NULL));
    log_init(log_filename);

    log_message(LOG_INFO, "Starting romlauncher");

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

    Notification notification = {0};

    SDL_Texture *switchlogo_tex = NULL, *sdllogo_tex = NULL, *helloworld_tex = NULL;
    SDL_Event event;

    SDL_Color colors[] = {
        { 128, 128, 128, 0 }, // gray
        { 255, 255, 255, 0 }, // white
        { 255, 0, 0, 0 },     // red
        { 0, 255, 0, 0 },     // green
        { 0, 0, 255, 0 },     // blue
        { 255, 255, 0, 0 },   // brown
        { 0, 255, 255, 0 },   // cyan
        { 255, 0, 255, 0 },   // purple
        { 0, 128, 255, 0 },   // bright blue
    };
    int selected_index = 0;
    int total_entries = 0;
    int current_page = 0;
    int total_pages = 0;

    srand(time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK) < 0) {
        log_message(LOG_ERROR, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    int img_flags = IMG_INIT_PNG;
    if ((IMG_Init(img_flags) & img_flags) != img_flags) {
        log_message(LOG_ERROR, "IMG_Init failed: %s", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    if (TTF_Init() < 0) {
        log_message(LOG_ERROR, "TTF_Init failed: %s", TTF_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("romlauncher",
                                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!window) {
        log_message(LOG_ERROR, "Window creation failed: %s", SDL_GetError());
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        log_message(LOG_ERROR, "Renderer creation failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

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


    SDL_JoystickEventState(SDL_ENABLE);
    SDL_JoystickOpen(0);

    // load font from romfs
    TTF_Font* font = TTF_OpenFont("data/LeroyLetteringLightBeta01.ttf", 36);

    // render text as texture
    SDL_Rect helloworld_rect = { 0, SCREEN_H - 36, 0, 0 };
    helloworld_tex = render_text(renderer, "Hello, world!", font, colors[1], &helloworld_rect);

    log_message(LOG_INFO, "About to list files");
    strncpy(current_path, rom_directory, sizeof(current_path) - 1);
    DirContent* content = list_files(current_path);
    if (content == NULL) {
        log_message(LOG_INFO, "list_files returned NULL");
    } else {
        log_message(LOG_INFO, "Got valid content");
        total_entries = content->dir_count + content->file_count;
        log_message(LOG_INFO, "Found %d directories and %d files",
                   content->dir_count, content->file_count);
        log_message(LOG_INFO, "Calculating pages: total_entries=%d, ENTRIES_PER_PAGE=%d",
                   total_entries, ENTRIES_PER_PAGE);
        total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
        set_selection(content, renderer, font, colors, selected_index, current_page);
    }

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

                if (event.jbutton.button == DPAD_UP) {
                    if (selected_index > 0) {
                        selected_index--;
                    } else {
                        selected_index = total_entries - 1;
                    }
                    current_page = selected_index / ENTRIES_PER_PAGE;
                    set_selection(current_mode == MODE_FAVORITES ? favorites_content : content,
                                renderer, font, colors, selected_index, current_page);
                }
                if (event.jbutton.button == DPAD_DOWN) {
                    if (selected_index < total_entries - 1) {
                        selected_index++;
                    } else {
                        selected_index = 0;
                    }
                    current_page = selected_index / ENTRIES_PER_PAGE;
                    set_selection(current_mode == MODE_FAVORITES ? favorites_content : content,
                                renderer, font, colors, selected_index, current_page);
                }

                if (event.jbutton.button == JOY_A) {
                    if (current_mode == MODE_BROWSER) {
                        if (selected_index < content->dir_count) {
                            change_directory(content, selected_index, current_path);
                            selected_index = 0;
                            total_entries = content->dir_count + content->file_count;
                            current_page = 0;
                            total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                            set_selection(content, renderer, font, colors, selected_index, current_page);
                        } else {
                            // Calculate which file was selected (accounting for directories)
                            int file_index = selected_index - content->dir_count;
                            if (file_index >= 0 && file_index < content->file_count) {
                                // Get file extension and look up core
                                const char* ext = get_file_extension(content->files[file_index]);
                                config_entry *core_entry;
                                HASH_FIND_STR(default_core_mappings, ext, core_entry);

                                if (!core_entry) {
                                    log_message(LOG_ERROR, "No core mapping found for extension: %s", ext);

                                    // Show notification
                                    if (notification.texture) {
                                        SDL_DestroyTexture(notification.texture);
                                    }
                                    char msg[256];
                                    snprintf(msg, sizeof(msg), "No core mapping found for .%s files", ext);
                                    notification.texture = render_text(renderer, msg, font, colors[2], &notification.rect);
                                    notification.rect.x = (SCREEN_W - notification.rect.w) / 2;
                                    notification.rect.y = SCREEN_H - notification.rect.h - 20; // 20px padding from bottom
                                    notification.active = 1;
                                    continue;
                                }

                                // Construct core path
                                char core_path[MAX_PATH_LEN];
                                int written = snprintf(core_path, sizeof(core_path), "sdmc:/retroarch/cores/%s_libretro_libnx.nro", core_entry->value);
                                if (written < 0 || (size_t)written >= sizeof(core_path)) {
                                    log_message(LOG_ERROR, "Core path construction failed (truncation or error)");
                                    continue;
                                }

                                // Check if core exists
                                FILE *core_file = fopen(core_path, "r");
                                if (core_file == NULL) {
                                    log_message(LOG_ERROR, "Core not found at: %s", core_path);
                                } else {
                                    fclose(core_file);

                                    // First, construct the full ROM/core path safely
                                    char rom_path[MAX_PATH_LEN];
                                    int written = snprintf(rom_path, sizeof(rom_path), "%s/%s", current_path + 5, content->files[file_index]);
                                    if (written < 0 || (size_t)written >= sizeof(rom_path)) {
                                        log_message(LOG_ERROR, "ROM path construction failed (truncation or error)");
                                        exit(1);
                                    }

                                    // Now, construct the full arguments string safely using the constructed path twice
                                    char full_arguments[MAX_PATH_LEN];
                                    written = snprintf(full_arguments, sizeof(full_arguments), "\"%s\" \"%s\"", rom_path, rom_path);
                                    if (written < 0 || (size_t)written >= sizeof(full_arguments)) {
                                        log_message(LOG_ERROR, "Arguments string construction failed (truncation or error)");
                                        exit(1);
                                    }

                                    // Log launch details
                                    log_message(LOG_INFO, "Launching RetroArch: %s with args: %s", core_path, full_arguments);

                                    // Launch RetroArch with the selected ROM
                                    Result rc = envSetNextLoad(core_path, full_arguments);
                                    if (R_SUCCEEDED(rc)) {
                                        log_message(LOG_INFO, "Successfully set next load");
                                        exit_requested = 1;
                                    } else {
                                        log_message(LOG_ERROR, "Failed to set next load, error: %x", rc);
                                    }
                                }
                            }
                        }
                    } else if (current_mode == MODE_FAVORITES) {
                        // In favorites mode, directly launch the selected ROM
                        if (selected_index >= 0 && selected_index < favorites_content->file_count) {
                            // Get file extension and look up core
                            const char* ext = get_file_extension(favorites_content->files[selected_index]);
                            config_entry *core_entry;
                            HASH_FIND_STR(default_core_mappings, ext, core_entry);

                            if (!core_entry) {
                                log_message(LOG_ERROR, "No core mapping found for extension: %s", ext);
                                continue;
                            }

                            // Construct core path
                            char core_path[MAX_PATH_LEN];
                            int written = snprintf(core_path, sizeof(core_path), "sdmc:/retroarch/cores/%s_libretro_libnx.nro", core_entry->value);
                            if (written < 0 || (size_t)written >= sizeof(core_path)) {
                                log_message(LOG_ERROR, "Core path construction failed (truncation or error)");
                                continue;
                            }

                            // Check if core exists
                            FILE *core_file = fopen(core_path, "r");
                            if (core_file == NULL) {
                                log_message(LOG_ERROR, "Core not found at: %s", core_path);
                            } else {
                                fclose(core_file);

                                // The favorites list stores full paths, so we can use them directly
                                const char* full_path = favorites_content->files[selected_index];

                                // Skip the "sdmc:" prefix if present
                                const char* rom_path = strncmp(full_path, "sdmc:", 5) == 0 ? full_path + 5 : full_path;

                                // Construct the arguments string
                                char full_arguments[MAX_PATH_LEN];
                                int written = snprintf(full_arguments, sizeof(full_arguments), "\"%s\" \"%s\"", rom_path, rom_path);
                                if (written < 0 || (size_t)written >= sizeof(full_arguments)) {
                                    log_message(LOG_ERROR, "Arguments string construction failed (truncation or error)");
                                    exit(1);
                                }

                                // Log launch details
                                log_message(LOG_INFO, "Launching RetroArch from favorites: %s with args: %s", core_path, full_arguments);

                                // Launch RetroArch with the selected ROM
                                Result rc = envSetNextLoad(core_path, full_arguments);
                                if (R_SUCCEEDED(rc)) {
                                    log_message(LOG_INFO, "Successfully set next load from favorites");
                                    exit_requested = 1;
                                } else {
                                    log_message(LOG_ERROR, "Failed to set next load from favorites, error: %x", rc);
                                }
                            }
                        }
                    }
                }

                if (event.jbutton.button == JOY_X) {
                    toggle_current_favorite(content, selected_index, current_path);
                    set_selection(content, renderer, font, colors, selected_index, current_page);

                    // If we're in favorites mode, refresh the list
                    if (current_mode == MODE_FAVORITES) {
                        if (favorites_content) free_dir_content(favorites_content);
                        favorites_content = list_favorites();
                        selected_index = 0;
                        current_page = 0;
                        if (favorites_content) {
                            total_entries = favorites_content->file_count;
                            total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                            set_selection(favorites_content, renderer, font, colors, selected_index, current_page);
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
                            selected_index = 0;
                            current_page = 0;
                            total_entries = favorites_content->file_count;
                            total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                            set_selection(favorites_content, renderer, font, colors, selected_index, current_page);
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
                        set_selection(content, renderer, font, colors, selected_index, current_page);
                    }
                }

                if (event.jbutton.button == JOY_B) {
                    go_up_directory(content, current_path, rom_directory);
                    selected_index = 0;
                    total_entries = content->dir_count + content->file_count;
                    current_page = 0;
                    total_pages = (total_entries + ENTRIES_PER_PAGE - 1) / ENTRIES_PER_PAGE;
                    set_selection(content, renderer, font, colors, selected_index, current_page);
                }

                if (event.jbutton.button == JOY_LEFT_SHOULDER) {
                    if (current_page > 0) {
                        current_page--;
                    } else {
                        current_page = total_pages - 1;
                    }
                    selected_index = current_page * ENTRIES_PER_PAGE;
                    set_selection(current_mode == MODE_FAVORITES ? favorites_content : content,
                                renderer, font, colors, selected_index, current_page);
                }

                if (event.jbutton.button == JOY_RIGHT_SHOULDER) {
                    if (current_page < total_pages - 1) {
                        current_page++;
                    } else {
                        current_page = 0;
                    }
                    selected_index = current_page * ENTRIES_PER_PAGE;
                    set_selection(current_mode == MODE_FAVORITES ? favorites_content : content,
                                renderer, font, colors, selected_index, current_page);
                }

                if (event.jbutton.button == JOY_PLUS) {
                    exit_requested = 1;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
        SDL_RenderClear(renderer);

        // Draw semi-transparent background if notification is active
        if (notification.active) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_RenderFillRect(renderer, NULL);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        // Render directory and file listings
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

        // Render notification if active
        if (notification.active && notification.texture) {
            SDL_RenderCopy(renderer, notification.texture, NULL, &notification.rect);
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

    if (helloworld_tex)
        SDL_DestroyTexture(helloworld_tex);

    if (notification.texture)
        SDL_DestroyTexture(notification.texture);

    // Clean up SDL systems in reverse order of initialization
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
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
