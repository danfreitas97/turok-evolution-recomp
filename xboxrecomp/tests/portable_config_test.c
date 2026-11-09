/* Run with a fake executable path and expected user directory. */
#include "../src/config/recomp_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
    char config[1024], data[1024], cache[1024];
    assert(argc == 3);
    recomp_portable_init(argv[1]);
    assert(recomp_config_user_dir(config, sizeof config));
    assert(recomp_data_dir(data, sizeof data));
    assert(recomp_cache_dir(cache, sizeof cache));
    assert(strcmp(config, argv[2]) == 0);
    assert(strcmp(data, argv[2]) == 0);
    assert(strncmp(cache, argv[2], strlen(argv[2])) == 0);
    puts("portable config: settings, saves and cache use the expected directory");
    return 0;
}
