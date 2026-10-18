#ifndef ALF_APP_PATHS_H
#define ALF_APP_PATHS_H

#include <stddef.h>

/* Called on the main thread after SDL_Init and before starting the game thread. */
int app_paths_init(void);
void app_save_path(char *buf, size_t size, const char *filename);

#endif
