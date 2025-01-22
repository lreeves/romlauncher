/* Mini SDL Demo
 * featuring SDL2 + SDL2_mixer + SDL2_image + SDL2_ttf
 * on Nintendo Switch using libnx
 *
 * Copyright 2018 carsten1ns
 *           2020 WinterMute
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
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
#define JOY_UP    13
#define JOY_RIGHT 14
#define JOY_DOWN  15

#define SCREEN_W 1280
#define SCREEN_H 720

const char* rom_directory = "sdmc:/roms";
FILE* log_file = NULL;

void log_message(const char* message) {
    if (log_file == NULL) return;
    
    time_t now;
    time(&now);
    char* timestamp = ctime(&now);
    timestamp[24] = '\0'; // Remove newline that ctime adds
    fprintf(log_file, "[%s] %s\n", timestamp, message);
    fflush(log_file);
}

SDL_Texture * render_text(SDL_Renderer *renderer, const char* text, TTF_Font *font, SDL_Color color, SDL_Rect *rect) 
{
    SDL_Surface *surface;
    SDL_Texture *texture;

    surface = TTF_RenderText_Solid(font, text, color);
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    rect->w = surface->w;
    rect->h = surface->h;

    SDL_FreeSurface(surface);

    return texture;
}

#define MAX_ENTRIES 256
#define MAX_PATH_LEN 512

typedef struct {
    char **dirs;
    char **files;
    int dir_count;
    int file_count;
    SDL_Texture **dir_textures;
    SDL_Texture **file_textures;
    SDL_Rect *dir_rects;
    SDL_Rect *file_rects;
} DirContent;

static int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

DirContent* list_files(const char* path, SDL_Renderer *renderer, TTF_Font *font, SDL_Color *colors) {
    log_message("Starting to list files");

    DIR *dir;
    struct dirent *entry;
    char log_buf[MAX_PATH_LEN];
    
    DirContent* content = malloc(sizeof(DirContent));
    if (!content) return NULL;
    
    content->dirs = calloc(MAX_ENTRIES, sizeof(char*));
    content->files = calloc(MAX_ENTRIES, sizeof(char*));
    content->dir_textures = calloc(MAX_ENTRIES, sizeof(SDL_Texture*));
    content->file_textures = calloc(MAX_ENTRIES, sizeof(SDL_Texture*));
    content->dir_rects = calloc(MAX_ENTRIES, sizeof(SDL_Rect));
    content->file_rects = calloc(MAX_ENTRIES, sizeof(SDL_Rect));
    
    if (!content->dirs || !content->files || !content->dir_textures || 
        !content->file_textures || !content->dir_rects || !content->file_rects) {
        free(content->dirs);
        free(content->files);
        free(content->dir_textures);
        free(content->file_textures);
        free(content->dir_rects);
        free(content->file_rects);
        free(content);
        return NULL;
    }
    
    content->dir_count = 0;
    content->file_count = 0;

    dir = opendir(path);
    if (dir == NULL) {
        snprintf(log_buf, sizeof(log_buf), "Failed to open directory: %s", path);
        log_message(log_buf);
        free(content->dirs);
        free(content->files);
        free(content);
        return NULL;
    }

    snprintf(log_buf, sizeof(log_buf), "Listing contents of: %s", path);
    log_message(log_buf);

    while ((entry = readdir(dir)) != NULL) {
        if (content->dir_count >= MAX_ENTRIES || content->file_count >= MAX_ENTRIES) {
            log_message("Maximum number of entries reached");
            break;
        }

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char *name = strdup(entry->d_name);
        if (name == NULL) continue;

        if (entry->d_type == DT_DIR) {
            content->dirs[content->dir_count++] = name;
        } else {
            content->files[content->file_count++] = name;
        }
    }

    if (content->dir_count > 0) {
        qsort(content->dirs, content->dir_count, sizeof(char*), compare_strings);
        log_message("Directories:");
        for (int i = 0; i < content->dir_count; i++) {
            snprintf(log_buf, sizeof(log_buf), "[DIR] %s", content->dirs[i]);
            log_message(log_buf);
            
            // Render directory entry
            content->dir_rects[i].x = 50;
            content->dir_rects[i].y = 50 + (i * 40);  // 40 pixels spacing between entries
            content->dir_textures[i] = render_text(renderer, log_buf, font, colors[1], &content->dir_rects[i]);
        }
    }

    if (content->file_count > 0) {
        qsort(content->files, content->file_count, sizeof(char*), compare_strings);
        log_message("Files:");
        for (int i = 0; i < content->file_count; i++) {
            snprintf(log_buf, sizeof(log_buf), "%s", content->files[i]);
            log_message(log_buf);
            
            // Render file entry
            content->file_rects[i].x = 50;
            content->file_rects[i].y = 50 + ((content->dir_count + i + 1) * 40);  // Continue after directories
            content->file_textures[i] = render_text(renderer, log_buf, font, colors[0], &content->file_rects[i]);
        }
    }

    closedir(dir);
    return content;
}

int rand_range(int min, int max){
   return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}


int main(int argc, char** argv) {
    log_file = fopen("/retrolauncher.log", "a");
    if (log_file == NULL) {
        return 1;
    }

    log_message("Starting romlauncher");
    log_message("Second message");

    romfsInit();
    chdir("romfs:/");

    int demo_enabled = 0;
    int exit_requested = 0;
    int trail = 0;
    int wait = 25;

    SDL_Texture *switchlogo_tex = NULL, *sdllogo_tex = NULL, *helloworld_tex = NULL;
    SDL_Rect pos = { 0, 0, 0, 0 }, sdl_pos = { 0, 0, 0, 0 };
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
    };
    int col = 0, snd = 0;

    srand(time(NULL));
    int vel_x = rand_range(1, 5);
    int vel_y = rand_range(1, 5);

    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
    Mix_Init(MIX_INIT_OGG);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow("sdl2+mixer+image+ttf demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // load logos from file
    SDL_Surface *sdllogo = IMG_Load("data/sdl.png");
    if (sdllogo) {
        sdl_pos.w = sdllogo->w;
        sdl_pos.h = sdllogo->h;
        sdllogo_tex = SDL_CreateTextureFromSurface(renderer, sdllogo);
        SDL_FreeSurface(sdllogo);
    }

    SDL_Surface *switchlogo = IMG_Load("data/switch.png");
    if (switchlogo) {
        pos.x = SCREEN_W / 2 - switchlogo->w / 2;
        pos.y = SCREEN_H / 2 - switchlogo->h / 2;
        pos.w = switchlogo->w;
        pos.h = switchlogo->h;
        switchlogo_tex = SDL_CreateTextureFromSurface(renderer, switchlogo);
        SDL_FreeSurface(switchlogo);
    }

    col = rand_range(0, 7);

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

    log_message("About to list files");
    DirContent* content = list_files(rom_directory, renderer, font, colors);
    if (content == NULL) {
        log_message("list_files returned NULL");
    } else {
        log_message("Got valid content");
        char debug_buf[256];
        snprintf(debug_buf, sizeof(debug_buf), "Found %d directories and %d files", 
                content->dir_count, content->file_count);
        log_message(debug_buf);
/*
        // Free the allocated memory
        for (int i = 0; i < content->dir_count; i++) {
            free(content->dirs[i]);
            if (content->dir_textures[i]) {
                SDL_DestroyTexture(content->dir_textures[i]);
            }
        }
        for (int i = 0; i < content->file_count; i++) {
            free(content->files[i]);
            if (content->file_textures[i]) {
                SDL_DestroyTexture(content->file_textures[i]);
            }
        }
        free(content->dirs);
        free(content->files);
        free(content->dir_textures);
        free(content->file_textures);
        free(content->dir_rects);
        free(content->file_rects);
        free(content);*/
    }

    while (!exit_requested
        && appletMainLoop()
        ) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                exit_requested = 1;

            // main event queue handler - use Switch controller inputs
            if (event.type == SDL_JOYBUTTONDOWN) {
                if (event.jbutton.button == JOY_UP)
                    if (wait > 0)
                        wait--;
                if (event.jbutton.button == JOY_DOWN)
                    if (wait < 100)
                        wait++;

                if (event.jbutton.button == JOY_PLUS)
                    exit_requested = 1;

                if (event.jbutton.button == JOY_B)
                    trail =! trail;

                if (event.jbutton.button == JOY_MINUS) {
                    demo_enabled =! demo_enabled;
                    log_message(demo_enabled ? "Demo started" : "Demo stopped");
                }
            }
        }

        if (!trail) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
            SDL_RenderClear(renderer);
        }

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

        if (demo_enabled) {
            // Animation and movement logic
            pos.y += vel_y;
            pos.x += vel_x;
            if (pos.x + pos.w > SCREEN_W) {
                pos.x = SCREEN_W - pos.w;
                vel_x = -rand_range(1, 5);
                col = rand_range(0, 4);
                snd = rand_range(0, 3);
                if (sound[snd])
                    Mix_PlayChannel(-1, sound[snd], 0);
            }
            if (pos.x < 0) {
                pos.x = 0;
                vel_x = rand_range(1, 5);
                col = rand_range(0, 4);
                snd = rand_range(0, 3);
                if (sound[snd])
                    Mix_PlayChannel(-1, sound[snd], 0);
            }
            if (pos.y + pos.h > SCREEN_H) {
                pos.y = SCREEN_H - pos.h;
                vel_y = -rand_range(1, 5);
                col = rand_range(0, 4);
                snd = rand_range(0, 3);
                if (sound[snd])
                    Mix_PlayChannel(-1, sound[snd], 0);
            }
            if (pos.y < 0) {
                pos.y = 0;
                vel_y = rand_range(1, 5);
                col = rand_range(0, 4);
                snd = rand_range(0, 3);
                if (sound[snd])
                    Mix_PlayChannel(-1, sound[snd], 0);
            }

            if (sdllogo_tex)
                SDL_RenderCopy(renderer, sdllogo_tex, NULL, &sdl_pos);
            if (switchlogo_tex) {
                SDL_SetTextureColorMod(switchlogo_tex, colors[col].r, colors[col].g, colors[col].b);
                SDL_RenderCopy(renderer, switchlogo_tex, NULL, &pos);
            }

            if (helloworld_tex)
                SDL_RenderCopy(renderer, helloworld_tex, NULL, &helloworld_rect);
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
    
    if (log_file) {
        fclose(log_file);
    }
    return 0;
}
