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

#include "uthash.h"

// Log levels
#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_ERROR 2

// Function declarations
void log_message(int level, const char* message);

// Config hash table structure
typedef struct {
    char key[256];           // key string
    char value[512];         // value string
    UT_hash_handle hh;       // makes this structure hashable
} config_entry;

static config_entry *config = NULL;

// Function to add/update config entry
void config_put(const char *key, const char *value) {
    config_entry *entry;
    HASH_FIND_STR(config, key, entry);
    
    if (entry == NULL) {
        entry = malloc(sizeof(config_entry));
        strncpy(entry->key, key, sizeof(entry->key)-1);
        entry->key[sizeof(entry->key)-1] = '\0';
        HASH_ADD_STR(config, key, entry);
    }
    
    strncpy(entry->value, value, sizeof(entry->value)-1);
    entry->value[sizeof(entry->value)-1] = '\0';
}

// Function to get config value
const char* config_get(const char *key) {
    config_entry *entry;
    HASH_FIND_STR(config, key, entry);
    return entry ? entry->value : NULL;
}

// Function to load config file
void load_config() {
    FILE *fp = fopen("sdmc:/romlauncher.ini", "r");
    if (!fp) {
        log_message(LOG_ERROR, "Could not open config file");
        return;
    }

    char line[768];
    while (fgets(line, sizeof(line), fp)) {
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#' || line[0] == ';')
            continue;
            
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Find equals sign
        char *equals = strchr(line, '=');
        if (!equals) continue;
        
        // Split into key/value
        *equals = '\0';
        char *key = line;
        char *value = equals + 1;
        
        // Trim whitespace
        while (*key && isspace(*key)) key++;
        char *end = key + strlen(key) - 1;
        while (end > key && isspace(*end)) *end-- = '\0';
        
        while (*value && isspace(*value)) value++;
        end = value + strlen(value) - 1;
        while (end > value && isspace(*end)) *end-- = '\0';
        
        if (*key && *value) {
            config_put(key, value);
            char log_buf[1024];
            snprintf(log_buf, sizeof(log_buf), "Loaded config: %s = %s", key, value);
            log_message(LOG_DEBUG, log_buf);
        }
    }
    
    fclose(fp);
}

// Function to free config hash table
void free_config() {
    config_entry *current, *tmp;
    HASH_ITER(hh, config, current, tmp) {
        HASH_DEL(config, current);
        free(current);
    }
}

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
FILE* log_file = NULL;

void log_message(int level, const char* message) {
    if (log_file == NULL) return;
    
    time_t now;
    time(&now);
    char* timestamp = ctime(&now);
    timestamp[24] = '\0'; // Remove newline that ctime adds
    const char* level_str = "";
    switch(level) {
        case LOG_DEBUG: level_str = "DEBUG"; break;
        case LOG_INFO:  level_str = "INFO"; break;
        case LOG_ERROR: level_str = "ERROR"; break;
    }
    fprintf(log_file, "[%s] [%s] %s\n", timestamp, level_str, message);
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

DirContent* list_files(const char* path) {
    log_message(LOG_INFO, "Starting to list files");

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
        log_message(LOG_INFO, log_buf);
        free(content->dirs);
        free(content->files);
        free(content);
        return NULL;
    }

    snprintf(log_buf, sizeof(log_buf), "Listing contents of: %s", path);
    log_message(LOG_INFO, log_buf);

    while ((entry = readdir(dir)) != NULL) {
        if (content->dir_count >= MAX_ENTRIES || content->file_count >= MAX_ENTRIES) {
            log_message(LOG_INFO, "Maximum number of entries reached");
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
        log_message(LOG_INFO, "Directories:");
        for (int i = 0; i < content->dir_count; i++) {
            snprintf(log_buf, sizeof(log_buf), "[DIR] %s", content->dirs[i]);
            log_message(LOG_DEBUG, log_buf);
            
            // Set directory entry position
            content->dir_rects[i].x = 50;
            content->dir_rects[i].y = 50 + ((i % ENTRIES_PER_PAGE) * 40);  // 40 pixels spacing between entries
            content->dir_textures[i] = NULL;
        }
    }

    if (content->file_count > 0) {
        qsort(content->files, content->file_count, sizeof(char*), compare_strings);
        log_message(LOG_INFO, "Files:");
        for (int i = 0; i < content->file_count; i++) {
            snprintf(log_buf, sizeof(log_buf), "%s", content->files[i]);
            log_message(LOG_DEBUG, log_buf);
            
            // Set file entry position
            content->file_rects[i].x = 50;
            int virtual_index = content->dir_count + i;
            content->file_rects[i].y = 50 + ((virtual_index % ENTRIES_PER_PAGE) * 40);
            content->file_textures[i] = NULL;
        }
    }

    closedir(dir);
    return content;
}

int rand_range(int min, int max){
   return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}

void go_up_directory(DirContent* content) {
    char *last_slash = strrchr(current_path, '/');
    if (last_slash && strcmp(current_path, rom_directory) != 0) {
        *last_slash = '\0';  // Truncate at last slash to go up one level
        
        // Free existing content
        for (int i = 0; i < content->dir_count; i++) {
            free(content->dirs[i]);
            if (content->dir_textures[i]) SDL_DestroyTexture(content->dir_textures[i]);
        }
        for (int i = 0; i < content->file_count; i++) {
            free(content->files[i]);
            if (content->file_textures[i]) SDL_DestroyTexture(content->file_textures[i]);
        }
        
        // Get new directory content
        DirContent* new_content = list_files(current_path);
        if (new_content) {
            content->dirs = new_content->dirs;
            content->files = new_content->files;
            content->dir_count = new_content->dir_count;
            content->file_count = new_content->file_count;
            content->dir_textures = new_content->dir_textures;
            content->file_textures = new_content->file_textures;
            content->dir_rects = new_content->dir_rects;
            content->file_rects = new_content->file_rects;
            free(new_content);
        }
    }
}

void change_directory(DirContent* content, int selected_index) {
    if (!content || selected_index >= content->dir_count) return;
    
    snprintf(current_path, sizeof(current_path), "%s/%s", current_path, content->dirs[selected_index]);
    
    // Free existing content
    for (int i = 0; i < content->dir_count; i++) {
        free(content->dirs[i]);
        if (content->dir_textures[i]) SDL_DestroyTexture(content->dir_textures[i]);
    }
    for (int i = 0; i < content->file_count; i++) {
        free(content->files[i]);
        if (content->file_textures[i]) SDL_DestroyTexture(content->file_textures[i]);
    }
    
    // Get new directory content
    DirContent* new_content = list_files(current_path);
    if (new_content) {
        content->dirs = new_content->dirs;
        content->files = new_content->files;
        content->dir_count = new_content->dir_count;
        content->file_count = new_content->file_count;
        content->dir_textures = new_content->dir_textures;
        content->file_textures = new_content->file_textures;
        content->dir_rects = new_content->dir_rects;
        content->file_rects = new_content->file_rects;
        free(new_content);
    }
}

void set_selection(DirContent* content, SDL_Renderer *renderer, TTF_Font *font, SDL_Color *colors, int selected_index, int current_page) {
    if (!content) return;
    
    char log_buf[MAX_PATH_LEN];
    
    int start_index = current_page * ENTRIES_PER_PAGE;
    int end_index = start_index + ENTRIES_PER_PAGE;
    
    // Update directory entries
    for (int i = 0; i < content->dir_count; i++) {
        if (i < start_index || i >= end_index) {
            if (content->dir_textures[i]) {
                SDL_DestroyTexture(content->dir_textures[i]);
                content->dir_textures[i] = NULL;
            }
            continue;
        }
        snprintf(log_buf, sizeof(log_buf), "[DIR] %s", content->dirs[i]);
        if (content->dir_textures[i]) {
            SDL_DestroyTexture(content->dir_textures[i]);
        }
        content->dir_textures[i] = render_text(renderer, log_buf, font, 
            colors[i == selected_index ? 8 : 1], &content->dir_rects[i]);
    }
    
    // Update file entries
    for (int i = 0; i < content->file_count; i++) {
        int virtual_index = content->dir_count + i;
        if (virtual_index < start_index || virtual_index >= end_index) {
            if (content->file_textures[i]) {
                SDL_DestroyTexture(content->file_textures[i]);
                content->file_textures[i] = NULL;
            }
            continue;
        }
        snprintf(log_buf, sizeof(log_buf), "%s", content->files[i]);
        if (content->file_textures[i]) {
            SDL_DestroyTexture(content->file_textures[i]);
        }
        int entry_index = content->dir_count + i;
        content->file_textures[i] = render_text(renderer, log_buf, font,
            colors[entry_index == selected_index ? 8 : 0], &content->file_rects[i]);
    }
}


int main(int argc, char** argv) {
    log_file = fopen("/retrolauncher.log", "w");
    if (log_file == NULL) {
        return 1;
    }

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
        char debug_buf[256];
        snprintf(debug_buf, sizeof(debug_buf), "Found %d directories and %d files", 
                content->dir_count, content->file_count);
        log_message(LOG_INFO, debug_buf);
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
                /* char button_msg[64];
                snprintf(button_msg, sizeof(button_msg), "Button pressed: %d", event.jbutton.button);
                log_message(button_msg); */

                if (event.jbutton.button == DPAD_UP) {
                    if (selected_index > 0) {
                        selected_index--;
                        if (selected_index < (current_page * ENTRIES_PER_PAGE)) {
                            current_page--;
                        }
                        set_selection(content, renderer, font, colors, selected_index, current_page);
                    }
                }
                if (event.jbutton.button == DPAD_DOWN) {
                    if (selected_index < total_entries - 1) {
                        selected_index++;
                        if (selected_index >= ((current_page + 1) * ENTRIES_PER_PAGE)) {
                            current_page++;
                        }
                        set_selection(content, renderer, font, colors, selected_index, current_page);
                    }
                }

                if (event.jbutton.button == JOY_A) {
                    if (selected_index < content->dir_count) {
                        change_directory(content, selected_index);
                        selected_index = 0;
                        total_entries = content->dir_count + content->file_count;
                        current_page = 0;
                        set_selection(content, renderer, font, colors, selected_index, current_page);
                    } else {
                        // Calculate which file was selected (accounting for directories)
                        int file_index = selected_index - content->dir_count;
                        if (file_index >= 0 && file_index < content->file_count) {
                            // Construct full arguments with ROM path first, then core path
                            char full_arguments[MAX_PATH_LEN];
                            
                            // Check if core exists
                            FILE *core_file = fopen(SFC_CORE, "r");
                            if (core_file == NULL) {
                                char error_msg[MAX_PATH_LEN];
                                snprintf(error_msg, sizeof(error_msg), "SNES core not found at: %s", SFC_CORE);
                                log_message(LOG_ERROR, error_msg);
                            } else {
                                fclose(core_file);
                                
                                // Construct arguments in RetroArch's expected format
                                snprintf(full_arguments, sizeof(full_arguments), "%s/%s -L %s",
                                       current_path + 5, // Skip "sdmc:" prefix
                                       content->files[file_index],
                                       SFC_CORE + 5); // Skip "sdmc:" prefix from core path
                                
                                // Log launch details
                                char launch_msg[MAX_PATH_LEN * 2];
                                snprintf(launch_msg, sizeof(launch_msg), "Launching RetroArch: %s with ROM: %s", 
                                       RETROARCH_PATH, full_arguments);
                                log_message(LOG_INFO, launch_msg);

                                // Launch RetroArch with the selected ROM
                                Result rc = envSetNextLoad(RETROARCH_PATH, full_arguments);
                                if (R_SUCCEEDED(rc)) {
                                    log_message(LOG_INFO, "Successfully set next load");
                                    exit_requested = 1;
                                } else {
                                    char error_msg[MAX_PATH_LEN];
                                    snprintf(error_msg, sizeof(error_msg), "Failed to set next load, error: %x", rc);
                                    log_message(LOG_ERROR, error_msg);
                                }
                            }
                        }
                    }
                }
                
                if (event.jbutton.button == JOY_B) {
                    go_up_directory(content);
                    selected_index = 0;
                    total_entries = content->dir_count + content->file_count;
                    current_page = 0;
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

    if (log_file) {
        fclose(log_file);
    }
    return 0;
}
