/* Toolbox declarations the Multiversal Interfaces (Retro68's free headers)
   leave out. Values and layouts from Inside Macintosh; the functions are in
   the InterfaceLib and AppearanceLib import libraries. */
#ifndef ZSNES_MACEXTRA_H
#define ZSNES_MACEXTRA_H

#include <MacTypes.h>
#include <Dialogs.h>

#pragma pack(push, 2)

typedef struct UnsignedWide {
    UInt32 hi;
    UInt32 lo;
} UnsignedWide;

typedef SInt16 AlertType;

typedef struct AlertStdAlertParamRec {
    Boolean movable;
    Boolean helpButton;
    ModalFilterUPP filterProc;
    ConstStringPtr defaultText;
    ConstStringPtr cancelText;
    ConstStringPtr otherText;
    SInt16 defaultButton;
    SInt16 cancelButton;
    UInt16 position;
} AlertStdAlertParamRec;

#pragma pack(pop)

enum {
    fsRtParID = 1,
    fsRtDirID = 2,
    kIsInvisible = 0x4000,
    kMiddleC = 60,
    initMono = 0x0080,
    initStereo = 0x00C0,
    suspendResumeMessage = 1,
    resumeFlag = 1,
    kAlertNoteAlert = 1,
    kAlertStdAlertOKButton = 1,
    kWindowDefaultPosition = 0,
    zoomDocProc = 8
};

pascal void Microseconds(UnsignedWide* micro);
pascal OSErr HGetVol(StringPtr volName, short* vRefNum, long* dirID);
pascal OSErr StandardAlert(AlertType type, ConstStr255Param error, ConstStr255Param explanation,
    AlertStdAlertParamRec const* param, SInt16* itemHit);

#endif
