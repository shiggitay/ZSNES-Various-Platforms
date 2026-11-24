/* ios.h - what the iOS build adds to the SDL3 frontend (unix/ios.c). */
#ifndef ZSNES_IOS_H
#define ZSNES_IOS_H

#include <SDL3/SDL.h>

/* Before SDL starts: settings and saves in the app's sandbox, ROMs in its
   Documents folder (the Files app's "On My iPhone > ZSNES"), the audio
   session and how touches reach the app. */
void ios_setup(void);

/* Where the picture goes on the screen, in the renderer's output pixels: 4:3,
   as tall as the screen, centred, with the controls in the margins. */
void ios_picture_rect(SDL_Renderer* r, SDL_FRect* dst);

/* The on-screen controller, over the frame just drawn. */
void ios_draw_overlay(SDL_Renderer* r);

/* A finger event. Fingers on the controller press its buttons; the rest
   point at the menus: then this returns 1, with the position in the picture
   (0 to 1 across and down) and *phase 1 for down, 0 for up, -1 for a move. */
int ios_touch(SDL_Event const* e, float* fx, float* fy, int* phase);

/* A game controller (Switch Pro, Xbox, PlayStation, MFi) as player 1 with
   no setting up; Config > Input can still bind it as well. */
void ios_gamepad_button(SDL_JoystickID which, int button, int down);
void ios_gamepad_axis(int axis, int value);

/* In the menus, the left stick moves the pointer: how far since the last
   call, as a fraction of the picture's width and height. 0 when it is not
   moving it. */
int ios_pointer_motion(float* dx, float* dy);

/* The app is going to the background, where iOS may end it unasked. */
void ios_background(void);

#endif
