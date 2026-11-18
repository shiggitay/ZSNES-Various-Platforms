/* Resources for the Mac OS 9 build: the code fragment, the memory partition,
   the version and the Finder's icons and bundle. */
#include "Processes.r"
#include "CodeFragments.r"
#include "icons.r"

resource 'cfrg' (0) {
    {
        kPowerPCCFragArch, kIsCompleteCFrag, kNoVersionNum, kNoVersionNum,
        512 * 1024, kNoAppSubFolder, /* stack: path buffers are 4 KB apiece */
        kApplicationCFrag, kDataForkCFragLocator, kZeroOffset, kCFragGoesToEOF,
        "ZSNES"
    }
};

/* The emulator's memory is about 20 MB with a large ROM loaded; the rest is
   room for the GUI, the screen buffers and the Sound Manager. Raise the
   preferred size in the Finder's Get Info if a game will not load. */
resource 'SIZE' (-1) {
    reserved,
    acceptSuspendResumeEvents,
    reserved,
    canBackground,
    doesActivateOnFGSwitch,
    backgroundAndForeground,
    dontGetFrontClicks,
    ignoreChildDiedEvents,
    is32BitCompatible,
    isHighLevelEventAware,
    onlyLocalHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    reserved,
    reserved,
    reserved,
    64 * 1024 * 1024,
    40 * 1024 * 1024
};

resource 'vers' (1) {
    0x02, 0x03, release, 0x09, verUS, "2.3.9", "2.3.9, PowerPC, for Mac OS 8.6 to 9.2.2"
};

/* Any file dropped on the application goes to it: ROMs come from other
   machines with every type and creator there is. */
data 'ZSN2' (0, "Owner resource") {
    $"0B" "ZSNES 2.3.9"
};

resource 'FREF' (128) { 'APPL', 0, "" };
resource 'FREF' (129) { '****', 1, "" };

resource 'BNDL' (128) {
    'ZSN2', 0,
    {
        'ICN#', { 0, 128; 1, 129 };
        'FREF', { 0, 128; 1, 129 }
    }
};
