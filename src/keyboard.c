/* Copyright (c) 2021-2024
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
#
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
#
* You should have received a copy of the GNU General Public
* License along with this program; if not, write to the
* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
* Boston, MA 02110-1301 USA
#
* Authored by: Kris Henriksen <krishenriksen.work@gmail.com>
#
* AnberPorts-Keyboard-Mouse
* 
* Part of the code is from from https://github.com/krishenriksen/AnberPorts/blob/master/AnberPorts-Keyboard-Mouse/main.c (mostly the fake keyboard)
* Fake Xbox code from: https://github.com/Emanem/js2xbox
* 
* Modified (badly) by: Shanti Gilbert for EmuELEC
* Modified further by: Nikolai Wuttke for EmuELEC (Added support for SDL and the SDLGameControllerdb.txt)
* Modified further by: Jacob Smith
* 
* Any help improving this code would be greatly appreciated! 
* 
* DONE: Xbox360 mode: Fix triggers so that they report from 0 to 255 like real Xbox triggers
*       Xbox360 mode: Figure out why the axis are not correctly labeled?  SDL_CONTROLLER_AXIS_RIGHTX / SDL_CONTROLLER_AXIS_RIGHTY / SDL_CONTROLLER_AXIS_TRIGGERLEFT / SDL_CONTROLLER_AXIS_TRIGGERRIGHT
*       Keyboard mode: Add a config file option to load mappings from.
*       add L2/R2 triggers
* 
*/

#include "gptokeyb2.h"
#include <stdio.h>
#include <string.h>

// set up absolute mouse movement ioctl incantations
/* static void setup_abs(int fd, int type, int min, int max, int res) */
/* { */
/*     struct uinput_abs_setup abs = { */
/*         .code = type, */
/*         .absinfo = { */
/*             .minimum = min, */
/*             .maximum = max, */
/*             .resolution = res */
/*         } */
/*     }; */

/*     if (-1 == ioctl(fd, UI_ABS_SETUP, &abs)) { */
/*         fprintf(stderr, "Error in ioctl UI_ABS_SETUP\n"); */
/*         exit(255); */
/*     } */
/* } */

void setupFakeAbsoluteMouseDevice()
{
    struct uinput_user_dev device;
    
    memset(&device, 0, sizeof(device));
    strncpy(device.name, "Fake Absolute Mouse", UINPUT_MAX_NAME_SIZE);
    device.id.vendor = 0x1235;  /* sample vendor */
    device.id.product = 0x5678; /* sample product */
    device.id.version = 1;
    device.id.bustype = BUS_USB;

    int fd = abs_uinp_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0)
    {
        fprintf(stderr, "Unable to open /dev/uinput\n");
        exit(255);
    }

    // more magical incantations
    ioctl(fd, UI_SET_EVBIT, EV_SYN);
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);

    // Fake mouse for absolute positioning
    ioctl(fd, UI_SET_EVBIT, EV_ABS);

    // magical incantations to the absolute pointer gods.
    // These with/height/etc are all arbitrary
    device.absmin[ABS_X] = 0;
    device.absmax[ABS_X] = 1280;
    device.absfuzz[ABS_X] = 4;
    device.absflat[ABS_X] = 8;

    device.absmin[ABS_Y] = 0;
    device.absmax[ABS_Y] = 1024;
    device.absfuzz[ABS_Y] = 4;
    device.absflat[ABS_Y] = 8;

    // Create input device into input sub-system.  UI_DEV_SETUP is too new for arkos
    // kernel so we just write it to the fd
    
    //if (-1 == ioctl(fd, UI_DEV_SETUP, device)) {
    if (-1 == write(fd, &device, sizeof(device))) {
        fprintf(stderr, "Unable to setup abs mouse device structure: %s\n", strerror(errno));
        exit(255);
    }

    if (ioctl(fd, UI_DEV_CREATE)) {
        printf("Unable to create abs mouse UINPUT device.\n");
        exit(-1);
    }
}

void setupFakeKeyboardMouseDevice()
{
    struct uinput_user_dev device;
    
    memset(&device, 0, sizeof(device));
    strncpy(device.name, "Fake Keyboard Mouse", UINPUT_MAX_NAME_SIZE);
    device.id.vendor = 0x1234;  /* sample vendor */
    device.id.product = 0x5678; /* sample product */
    device.id.version = 1;
    device.id.bustype = BUS_USB;

    int fd = kb_uinp_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0)
    {
        printf("Unable to open /dev/uinput\n");
        exit(255);
    }

    for (int i = 0; i < 256; i++)
    {
        ioctl(fd, UI_SET_KEYBIT, i);
    }

    if (
        // Keys or Buttons
        ioctl(fd, UI_SET_EVBIT, EV_SYN) ||
        ioctl(fd, UI_SET_EVBIT, EV_KEY) ||

        // Fake mouse for both absolute and relative positioning
        ioctl(fd, UI_SET_EVBIT, EV_REL) ||
        ioctl(fd, UI_SET_RELBIT, REL_X) ||
        ioctl(fd, UI_SET_RELBIT, REL_Y) ||
        ioctl(fd, UI_SET_KEYBIT, BTN_LEFT) ||
        ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT) ||
        // FUCKING SCHROLL WHEEL
        ioctl(fd, UI_SET_RELBIT, REL_WHEEL)
        ) {
        fprintf(stderr, "One of the keyboard/mouse ioctls failed: %s\n", strerror(errno));
        exit(255);
    }

    //Create input device into input sub-system
    //if (-1 == ioctl(fd, UI_DEV_SETUP, device)) {
    if (-1 == write(fd, &device, sizeof(device))) {
        fprintf(stderr, "Unable to setup mouse/keyboard device structure: %s\n", strerror(errno));
        exit(255);
    }

    fsync(fd);

    int ret = ioctl(fd, UI_DEV_CREATE);
    if (-1 == ret) {
        fprintf(stderr, "Unable to create keyboard UINPUT device: %d - %s\n", ret, strerror(errno));
        exit(255);
    }
}


void handleEventBtnFakeKeyboardMouseDevice(const SDL_Event *event, bool pressed)
{
    //config mode (i.e. not textinputinteractive_mode_active)
    switch (event->cbutton.button)
    {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        update_button(GBTN_DPAD_LEFT, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        update_button(GBTN_DPAD_UP, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        update_button(GBTN_DPAD_RIGHT, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        update_button(GBTN_DPAD_DOWN, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_A:
        update_button(GBTN_A, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_B:
        update_button(GBTN_B, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_X:
        update_button(GBTN_X, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_Y:
        update_button(GBTN_Y, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        update_button(GBTN_L1, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        update_button(GBTN_R1, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
        update_button(GBTN_L3, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        update_button(GBTN_R3, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_GUIDE:
        update_button(GBTN_GUIDE, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_BACK: // aka select
        update_button(GBTN_BACK, pressed);
        break;

    case SDL_CONTROLLER_BUTTON_START:
        update_button(GBTN_START, pressed);
        break;
    } //switch
}


#define _ANALOG_AXIS_ZERO(ANALOG_VALUE, DEADZONE) (abs(ANALOG_VALUE) < DEADZONE)
#define _ANALOG_AXIS_POS(ANALOG_VALUE, DEADZONE)  (!_ANALOG_AXIS_ZERO(ANALOG_VALUE, DEADZONE) && ((ANALOG_VALUE) > DEADZONE))
#define _ANALOG_AXIS_NEG(ANALOG_VALUE, DEADZONE)  (!_ANALOG_AXIS_ZERO(ANALOG_VALUE, DEADZONE) && ((ANALOG_VALUE) < DEADZONE))

void handleEventAxisFakeKeyboardMouseDevice(const SDL_Event *event)
{
    bool left_axis_movement = false;
    bool right_axis_movement = false;
    bool l2_movement = false;
    bool r2_movement = false;

    switch (event->caxis.axis)
    {
    case SDL_CONTROLLER_AXIS_LEFTX:
        current_state.current_left_analog_x = event->caxis.value;
        left_axis_movement = true;
        break;

    case SDL_CONTROLLER_AXIS_LEFTY:
        current_state.current_left_analog_y = event->caxis.value;
        left_axis_movement = true;
        break;

    case SDL_CONTROLLER_AXIS_RIGHTX:
        current_state.current_right_analog_x = event->caxis.value;
        right_axis_movement = true;
        break;

    case SDL_CONTROLLER_AXIS_RIGHTY:
        current_state.current_right_analog_y = event->caxis.value;
        right_axis_movement = true;
        break;

    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        current_state.current_l2 = event->caxis.value;
        l2_movement = true;
        break;

    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        current_state.current_r2 = event->caxis.value;
        r2_movement = true;
        break;
    } // switch (event->caxis.axis)

    // fake mouse
    if (current_left_analog_as_mouse && left_axis_movement)
    {
        deadzone_mouse_calc(
            &current_state.mouse_relative_x, &current_state.mouse_relative_y,
            current_state.current_left_analog_x, current_state.current_left_analog_y);

        // GPTK2_DEBUG("fake mouse %d %d\n", current_state.mouse_x, current_state.mouse_y);

    }
    else if (current_right_analog_as_mouse && right_axis_movement)
    {
        deadzone_mouse_calc(
            &current_state.mouse_relative_x, &current_state.mouse_relative_y,
            current_state.current_right_analog_x, current_state.current_right_analog_y);

        // GPTK2_DEBUG("fake mouse %d %d\n", current_state.mouse_x, current_state.mouse_y);
    }
    else if (current_left_analog_as_absolute_mouse && left_axis_movement)
    {
        current_state.mouse_absolute_x = current_state.current_left_analog_x;
        current_state.mouse_absolute_y = current_state.current_left_analog_y;

        GPTK2_DEBUG("fake absolute mouse %d %d\n", current_state.mouse_absolute_x, current_state.mouse_absolute_y);
    }
    else if (current_right_analog_as_absolute_mouse && right_axis_movement)
    {
        current_state.mouse_absolute_x = current_state.current_right_analog_x;
        current_state.mouse_absolute_y = current_state.current_right_analog_y;

        GPTK2_DEBUG("fake absolute mouse %d %d\n", current_state.mouse_absolute_x, current_state.mouse_absolute_y);
    }
    else
    {
        if (left_axis_movement)
        {
            update_button(GBTN_LEFT_ANALOG_UP,    _ANALOG_AXIS_NEG(current_state.current_left_analog_y, current_state.deadzone_y));
            update_button(GBTN_LEFT_ANALOG_DOWN,  _ANALOG_AXIS_POS(current_state.current_left_analog_y, current_state.deadzone_y));
            update_button(GBTN_LEFT_ANALOG_LEFT,  _ANALOG_AXIS_NEG(current_state.current_left_analog_x, current_state.deadzone_x));
            update_button(GBTN_LEFT_ANALOG_RIGHT, _ANALOG_AXIS_POS(current_state.current_left_analog_x, current_state.deadzone_x));
        }
        if (right_axis_movement)
        {
            update_button(GBTN_RIGHT_ANALOG_UP,    _ANALOG_AXIS_NEG(current_state.current_right_analog_y, current_state.deadzone_y));
            update_button(GBTN_RIGHT_ANALOG_DOWN,  _ANALOG_AXIS_POS(current_state.current_right_analog_y, current_state.deadzone_y));
            update_button(GBTN_RIGHT_ANALOG_LEFT,  _ANALOG_AXIS_NEG(current_state.current_right_analog_x, current_state.deadzone_x));
            update_button(GBTN_RIGHT_ANALOG_RIGHT, _ANALOG_AXIS_POS(current_state.current_right_analog_x, current_state.deadzone_x));
        }
    } // Analogs trigger keys 

    if (l2_movement)
        update_button(GBTN_L2, current_state.current_l2 > current_state.deadzone_triggers);

    if (r2_movement)
        update_button(GBTN_R2, current_state.current_r2 > current_state.deadzone_triggers);
}
