#include <3ds.h>
#include <stdio.h>

#include "string.h"
#include "filesystem.h"
#include "loader.h"
#include "misc.h"
#include "splash.h"

#define DEFAULT_PATH NULL
#define DEFAULT_DELAY 2000 /* ms */
#define DEFAULT_PAYLOAD -1 /* <0 - auto, 0 - disable, >0 - enabled */
#define DEFAULT_OFFSET 0x12000
#define DEFAULT_SECTION "GLOBAL"
#define DEFAULT_SPLASH 3 /* 0 - disabled, 1 - splash screen, 2 - entry info, 3 - both */
#define DEFAULT_SPLASH_IMAGE NULL
#define INI_FILE "/boot_config.ini"

typedef struct
{
    char* path;
} loadappFormat;

// handled in main
// doing it in main is preferred because execution ends in launching another 3dsx
void __appInit()
{
    srvInit();
    aptInit();
    gfxInitDefault();
    initFilesystem();
    openSDArchive();
    hidInit();
}

// same
void __appExit()
{
    hidExit();
    gfxExit();
    closeSDArchive();
    exitFilesystem();
    aptExit();
    srvExit();
}

void removeChar(char *str, char garbage) {
    char *src, *dst;
    for (src = dst = str; *src != '\0'; src++) {
        *dst = *src;
        if (*dst != garbage) dst++;
    }
    *dst = '\0';
}

int handleLoadapp(void *user, const char *section, const char *name, const char *value)
{
	loadappFormat *pconfig = (loadappFormat *) user;
	#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
	if (MATCH("", "path")) {
		pconfig->path = strdup(value);
	}
	return 1;
}

int main()
{
    // offset potential issues caused by homebrew that just ran
    aptOpenSession();
    APT_SetAppCpuTimeLimit(0);
    aptCloseSession();
	
	char *pathToOpen;
	
	if(access("loadapp.ini", F_OK) != -1) {
		loadappFormat config;
		ini_parse("/loadapp.ini", handleLoadapp, &config);
		pathToOpen = config.path;
		remove("loadapp.ini");
		
		// set application default preferences
		application app = {
			.config = {
				.section = "DEFAULT",
				.path = pathToOpen,
				.delay = DEFAULT_DELAY,
				.payload = DEFAULT_PAYLOAD,
				.offset = DEFAULT_OFFSET,
			}
		};

		// Load default section
		app.config.section = "DEFAULT";
		
		return load(app);
	} else { // run original code
		// set application default preferences
		application app = {
			.config = {
				.section = DEFAULT_SECTION,
				.path = DEFAULT_PATH,
				.delay = DEFAULT_DELAY,
				.payload = DEFAULT_PAYLOAD,
				.offset = DEFAULT_OFFSET,
				.splash = DEFAULT_SPLASH,
				.splash_image = DEFAULT_SPLASH_IMAGE,
			}
		};

		// override default options with [GLOBAL] section
		// don't need to check error for now
		ini_parse(INI_FILE, handler, &app.config);

		// get pressed user key, convert to string to pass to ini_parse
		hidScanInput();
		u32 key = hidKeysDown();
		switch (key) {
			// using X-macros to generate each switch-case rules
			// https://en.wikibooks.org/wiki/C_Programming/Preprocessor#X-Macros
			#define KEY(k) \
			case KEY_##k: \
				app.config.section = "KEY_"#k; \
				break;
			#include "keys.def"
			default:
				app.config.section = "DEFAULT";
				break;
		}

		// load config from section [KEY_*], check error for issues
		int config_err = ini_parse(INI_FILE, handler, &app.config);
		switch (config_err) {
			case 0:
				// section not found, try to load [DEFAULT] section
				if (!app.config.path) {
					app.config.section = "DEFAULT";
					// don't need to check error again
					ini_parse(INI_FILE, handler, &app.config);
					if (!app.config.path)
						panic("Section [DEFAULT] not found or \"path\" not set.");
				} else if (app.config.path[0] != '/') {
					panic("In section [%s], path %s is invalid.",
							app.config.section, app.config.path);
				} else if (!file_exists(app.config.path)) {
					panic("In section [%s], file %s not found.",
							app.config.section, app.config.path);
				}
				break;
			case -1:
				panic("Config file %s not found.", INI_FILE);
				break;
			case -2:
				// should not happen, however better be safe than sorry
				panic("Config file %s too big.", INI_FILE);
				break;
			default:
				panic("Error found in config file %s on line %d.",
						INI_FILE, config_err);
				break;
		}

		if (app.config.splash >= 2) {
			// print entry information in bottom screen
			consoleInit(GFX_BOTTOM, NULL);
			printf("Booting entry [%s]:\n"
				   "\t* path = %s\n"
				   "\t* delay = %llu\n"
				   "\t* payload = %d\n"
				   "\t* offset = 0x%lx\n",
				   app.config.section, app.config.path, app.config.delay,
				   app.config.payload, app.config.offset);
		}

		if (app.config.splash == 1 || app.config.splash >= 3) {
			// try to load user set splash image
			if (app.config.splash_image) {
				splash_image(app.config.splash_image);
			} else {
				splash_ascii();
			}
		}

		return load(app);
	}
}
