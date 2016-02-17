#include <3ds.h>
#include <stdio.h>

#include "filesystem.h"
#include "loader.h"
#include "config.h"
#include "misc.h"
#include "splash.h"

#define DEFAULT_PATH ;add here;
#define DEFAULT_DELAY 2000 /* ms */
#define DEFAULT_PAYLOAD 0 /* <0 - auto, 0 - disable, >0 - enabled */
#define DEFAULT_OFFSET 0x12000

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

int main()
{
    // offset potential issues caused by homebrew that just ran
    aptOpenSession();
    APT_SetAppCpuTimeLimit(0);
    aptCloseSession();

    // set application default preferences
    application app = {
        .config = {
            .section = "DEFAULT",
            .path = DEFAULT_PATH,
            .delay = DEFAULT_DELAY,
            .payload = DEFAULT_PAYLOAD,
            .offset = DEFAULT_OFFSET,
        }
    };

    // Load default section
    app.config.section = "DEFAULT";

    return load(app);
}
