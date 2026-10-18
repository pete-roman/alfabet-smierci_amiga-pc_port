#include "app_paths.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static char g_save_dir[1024];

int app_paths_init(void)
{
#if defined(ALF_NATIVE_BUILD)
    char *path = SDL_GetPrefPath("Pete Roman", "Alfabet Smierci");
    if (!path) {
        fprintf(stderr, "SDL_GetPrefPath: %s\n", SDL_GetError());
        return 0;
    }
    SDL_strlcpy(g_save_dir, path, sizeof g_save_dir);
    SDL_free(path);
#else
    const char *base = SDL_GetBasePath();
    SDL_snprintf(g_save_dir, sizeof g_save_dir, "%ssaves/", base ? base : "./");
    char dir[1024];
    SDL_snprintf(dir, sizeof dir, "%ssaves", base ? base : "./");
    SDL_CreateDirectory(dir);
#endif
    return 1;
}

void app_save_path(char *buf, size_t size, const char *filename)
{
    SDL_snprintf(buf, size, "%s%s", g_save_dir, filename);
}
