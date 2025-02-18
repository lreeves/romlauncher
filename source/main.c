#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include "logging.h"
#include "config.h"
#include "browser.h"
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <switch.h>



// some switch buttons
#define JOY_A     0
#define JOY_B     1
#define JOY_X     2
#define JOY_Y     3
#define JOY_PLUS  10
#define JOY_MINUS 11
#define JOY_LEFT  12
#define JOY_RIGHT 14
#define DPAD_UP    13
#define DPAD_DOWN  15

#define SCREEN_W 1280
#define SCREEN_H 720

#define MAX_ENTRIES 256
#define ENTRIES_PER_PAGE 15  // How many entries fit on one page
#define MAX_PATH_LEN 512
#define RETROARCH_PATH "/switch/retroarch_switch.nro"
#define SFC_CORE "sdmc:/retroarch/cores/snes9x_libretro_libnx.nro"

const char* rom_directory = "sdmc:/roms";
char current_path[MAX_PATH_LEN];

int rand_range(int min, int max){
   return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}


int main(int argc, char** argv) {
    log_init("/retrolauncher.log");

    log_message(LOG_INFO, "Starting romlauncher");

    // Load config file
    load_config();
    log_message(LOG_INFO, "Config loaded");

    romfsInit();
    chdir("romfs:/");

    int exit_requested = 0;
    int wait = 25;

    SDL_Texture *switchlogo_tex = NULL, *sdllogo_tex = NULL, *helloworld_tex = NULL;
    Mix_Music *music = NULL;
    Mix_Chunk *sound[4] = { NULL };
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
    int snd = 0;
    int selected_index = 0;
    int total_entries = 0;
    int current_page = 0;
    int total_pages = 0;

    srand(time(NULL));

    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
    Mix_Init(MIX_INIT_OGG);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow("sdl2+mixer+image+ttf demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

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


    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_JoystickOpen(0);

    // load font from romfs
    TTF_Font* font = TTF_OpenFont("data/LeroyLetteringLightBeta01.ttf", 36);

    // render text as texture
    SDL_Rect helloworld_rect = { 0, SCREEN_H - 36, 0, 0 };
    helloworld_tex = render_text(renderer, "Hello, world!", font, colors[1], &helloworld_rect);

    SDL_InitSubSystem(SDL_INIT_AUDIO);
    Mix_AllocateChannels(5);
    Mix_OpenAudio(48000, AUDIO_S16, 2, 4096);

    // load music and sounds from files
    music = Mix_LoadMUS("data/background.ogg");
    sound[0] = Mix_LoadWAV("data/pop1.wav");
    sound[1] = Mix_LoadWAV("data/pop2.wav");
    sound[2] = Mix_LoadWAV("data/pop3.wav");
    sound[3] = Mix_LoadWAV("data/pop4.wav");

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

                if (event.jbutton.button == DPAD_UP) {
                    if (selected_index > 0) {
                        selected_index--;
                    } else {
                        selected_index = total_entries - 1;
                    }
                    current_page = selected_index / ENTRIES_PER_PAGE;
                    set_selection(content, renderer, font, colors, selected_index, current_page);
                }
                if (event.jbutton.button == DPAD_DOWN) {
                    if (selected_index < total_entries - 1) {
                        selected_index++;
                    } else {
                        selected_index = 0;
                    }
                    current_page = selected_index / ENTRIES_PER_PAGE;
                    set_selection(content, renderer, font, colors, selected_index, current_page);
                }

                if (event.jbutton.button == JOY_A) {
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
                            // Check if core exists
                            FILE *core_file = fopen(SFC_CORE, "r");
                            if (core_file == NULL) {
                                log_message(LOG_ERROR, "SNES core not found at: %s", SFC_CORE);
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
                                log_message(LOG_INFO, "Launching RetroArch: %s with args: %s", RETROARCH_PATH, full_arguments);

                                // Launch RetroArch with the selected ROM
                                Result rc = envSetNextLoad("/retroarch/cores/snes9x_libretro_libnx.nro", full_arguments);
                                if (R_SUCCEEDED(rc)) {
                                    log_message(LOG_INFO, "Successfully set next load");
                                    exit_requested = 1;
                                } else {
                                    log_message(LOG_ERROR, "Failed to set next load, error: %x", rc);
                                }
                            }
                        }
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

                if (event.jbutton.button == 6) {
                    log_message(LOG_INFO, "Left shoulder");
                    if (current_page > 0) {
                        current_page--;
                    } else {
                        current_page = total_pages - 1;
                    }
                    log_message(LOG_INFO, "Left shoulder - moving to page %d", current_page);
                    selected_index = current_page * ENTRIES_PER_PAGE;
                    set_selection(content, renderer, font, colors, selected_index, current_page);
                }

                if (event.jbutton.button == 7) {
                    log_message(LOG_INFO, "Right shoulder");
                    log_message(LOG_INFO, "Current page: %d, Total pages: %d", current_page, total_pages);
                    if (current_page < total_pages - 1) {
                        current_page++;
                    } else {
                        current_page = 0;
                    }
                    log_message(LOG_INFO, "Right shoulder - moving to page %d", current_page);
                    selected_index = current_page * ENTRIES_PER_PAGE;
                    set_selection(content, renderer, font, colors, selected_index, current_page);
                }

                if (event.jbutton.button == JOY_PLUS) {
                    exit_requested = 1;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
        SDL_RenderClear(renderer);

        // Render directory and file listings
        if (content) {
            for (int i = 0; i < content->dir_count; i++) {
                if (content->dir_textures[i]) {
                    SDL_RenderCopy(renderer, content->dir_textures[i], NULL, &content->dir_rects[i]);
                }
            }
            for (int i = 0; i < content->file_count; i++) {
                if (content->file_textures[i]) {
                    SDL_RenderCopy(renderer, content->file_textures[i], NULL, &content->file_rects[i]);
                }
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(wait);
    }

    // clean up your textures when you are done with them
    if (sdllogo_tex)
        SDL_DestroyTexture(sdllogo_tex);

    if (switchlogo_tex)
        SDL_DestroyTexture(switchlogo_tex);

    if (helloworld_tex)
        SDL_DestroyTexture(helloworld_tex);

    // stop sounds and free loaded data
    Mix_HaltChannel(-1);
    Mix_FreeMusic(music);
    for (snd = 0; snd < 4; snd++)
        if (sound[snd])
            Mix_FreeChunk(sound[snd]);

    IMG_Quit();
    Mix_CloseAudio();
    TTF_Quit();
    Mix_Quit();
    SDL_Quit();
    romfsExit();

    // Free config hash table
    free_config();

    log_close();
    return 0;
}
