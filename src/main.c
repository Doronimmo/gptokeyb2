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
#include <linux/uinput.h>
#include <stdbool.h>

#define MAX_PROCESS_NAME 64

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

// ioctls prevent these from being on the same fd
int xbox_uinp_fd = 0; // fake xbox controller
int kb_uinp_fd = 0;  // fake relative mouse and keyboard
int abs_uinp_fd = 0; // fake absolute position mouse

bool xbox360_mode=false;
bool config_mode=false;

bool want_pc_quit = false;
bool want_kill = false;
bool want_sudo = false;

char user_config_file[MAX_PATH];

char game_prefix[MAX_PROCESS_NAME] = "";
char kill_process_name[MAX_PROCESS_NAME] = "";


gptokeyb_config *default_config=NULL;



int main(int argc, char* argv[])
{
    bool do_dump_config = false;

    string_init();
    state_init();
    config_init();
    input_init();

    xbox_uinp_fd = 0;
    kb_uinp_fd = 0;
    abs_uinp_fd = 0;

    char* env_home = SDL_getenv("HOME");
    if (env_home)
    {
        snprintf(user_config_file, MAX_PATH, "%s/.config/gptokeyb2.ini", env_home);
    }
    else
    {
        strncpy(user_config_file, "~/.config/gptokeyb2.ini", MAX_PATH);
    }

    // Add hotkey environment variable if available
    char* env_hotkey = SDL_getenv("HOTKEY");
    if (env_hotkey)
    {
        const button_match *button = find_button(env_hotkey);

        if (button != NULL)
        {
            printf("set hotkey as %s\n", env_hotkey);
            set_hotkey(button->gbtn);
        }
    }

    // Add pc alt+f4 exit environment variable if available
    char* env_pckill_mode = SDL_getenv("PCKILLMODE");
    if (env_pckill_mode)
    {
        if (strcmp(env_pckill_mode, "Y") == 0)
        {
            printf("Using pc quit mode.\n");
            want_pc_quit = true;
        }
    }

    char* pkill_mode = SDL_getenv("NO_PKILL");
    if (pkill_mode)
    {
        want_kill = true;
    }

    int opt;
    char default_control[MAX_CONTROL_NAME] = "";

    // Fix some old gptokeyb settings.
    for (int k=0; k < argc; k++)
    {
        if (strcmp(argv[k], "-sudokill") == 0)
        {   // This needs to be "-s"
            argv[k][2] = '\0';
        }
    }

    while ((opt = getopt(argc, argv, "vk1g:hdxp:c:ZXPH:s:")) != -1)
    {
        switch (opt)
        {
        case 'k':
        case '1':
            // do nothing.
            break;

        case 'v':
            printf("gptokeyb2 %s\n", GPTK2_VERSION);
            return 0;
            break;

        case 's':
            if (!want_sudo)
            {
                printf("Using sudo mode.\n");
                want_sudo = true;
            }
            break;

        case 'X':
            if (!want_kill)
            {
                printf("Using kill mode.\n");
                want_kill = true;
            }
            break;

        case 'Z':
            if (want_kill)
            {
                printf("Using pkill mode.\n");
                want_kill = false;
            }
            break;

        case 'P':
            if (!want_pc_quit)
            {
                printf("Using pc quit mode.\n");
                want_pc_quit = true;
            }
            break;

        case 'H':
            {
                const button_match *button = find_button(optarg);

                if (button != NULL)
                {
                    printf("set hotkey as %s\n", optarg);
                    set_hotkey(button->gbtn);
                }
                else
                {
                    printf("unable to set hotkey as %s, unknown hotkey\n", optarg);
                }
            }
            break;

        case 'd':
            do_dump_config = true;
            break;

        case 'g':
            strncpy(game_prefix, optarg, MAX_PROCESS_NAME - 1);
            break;

        case 'c':
            config_load(optarg, false);
            config_mode = true;
            xbox360_mode = false;
            break;

        case 'x':
            config_mode = false;
            xbox360_mode = true;
            break;

        case 'p':
            strncpy(default_control, optarg, MAX_CONTROL_NAME-1);
            printf("using control %s\n", default_control);
            break;

        case '?':
        case 'h':
            if (opt == '?')
            {
                if (optopt == 'c')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);

                fprintf(stderr, "\n");
            }

            fprintf(stderr, "Usage: %s <program> [-dPXZ] [-H hotkey] [-c <config.ini>] [-p control_mode]\n",
                argv[0]);
            fprintf(stderr, "\n");
            fprintf(stderr, "Args:\n");
            fprintf(stderr, "  -P                  - pc quit mode (sends alt + f4 to quit program)\n");
            fprintf(stderr, "  -X                  - uses kill to quit the program\n");
            fprintf(stderr, "  -Z                  - uses pkill to quit the program\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "  -g  \"game_prefix\"   - game prefix used to allow per-game config.\n");
            fprintf(stderr, "  -x                  - xbox360 mode.\n");
            fprintf(stderr, "  -c  \"config.ini\"    - config file to load.\n");
            fprintf(stderr, "  -p  \"control\"       - what control mode to start in.\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "  -d                  - dump config parsed.\n");
            fprintf(stderr, "  -v                  - print version and quit.");
            fprintf(stderr, "\n");
            return 1;
            break;

        default:
            config_quit();
            state_quit();
            input_quit();
            string_quit();
            exit(EXIT_FAILURE);
            break;
        }
    }

    for (int index=optind, i=0; index < argc; index++, i++)
    {
        if (i == 0)
        {
            strncpy(kill_process_name, argv[index], MAX_PROCESS_NAME - 1);

            if (strlen(game_prefix) == 0 && strlen(kill_process_name) > 0)
            {
                size_t kill_process_name_len = strlen(kill_process_name);

                // strip these suffixes
                char *suffixes[] = {
                    ".x86_64", ".x86", ".aarch64", ".arm64", ".armhf", ".arm", ".32", ".64"};
                bool changed = false;

                for (size_t j=0; j < (sizeof(suffixes) / sizeof(suffixes[0])); j++)
                {
                    if (strcaseendswith(kill_process_name, suffixes[j]))
                    {
                        strncpy(game_prefix, kill_process_name, kill_process_name_len - strlen(suffixes[j]));
                        changed = true;
                        break;
                    }
                }

                if (!changed)
                    strncpy(game_prefix, kill_process_name, MAX_PROCESS_NAME);
            }
        }
        else
        {
            // GPTK2_DEBUG("Extra option: %s\n", argv[index]);
        }
    }

    if (config_mode)
    {
        if (!do_dump_config && access(user_config_file, F_OK) == 0)
        // if (access(user_config_file, F_OK) == 0)
        {
            printf("Loading '%s'\n", user_config_file);

            if (config_load(user_config_file, true))
            {
                config_quit();
                state_quit();
                input_quit();
                string_quit();
                return 1;
            }
        }

        if (strlen(default_control_name) > 0)
        {
            default_config = config_find(default_control_name);

            if (default_config == NULL)
            {
                fprintf(stderr, "Unable to find control '%s'\n", default_control_name);
            }
        }

        if (default_config == NULL && strlen(default_control) > 0)
        {
            default_config = config_find(default_control);

            if (default_config == NULL)
            {
                fprintf(stderr, "Unable to find control '%s'\n", default_control);
            }
        }

        if (default_config == NULL)
        {
            default_config = root_config;
        }

        config_stack[0] = default_config;
    }

    config_finalise();
    state_change_update();

    if (do_dump_config)
    {
        config_dump();
        config_quit();
        state_quit();
        input_quit();
        string_quit();
        return 0;
    }

    if (strlen(kill_process_name) > 0)
        printf("Watching '%s'\n", kill_process_name);

    if (strlen(game_prefix) > 0)
        printf("Game prefix '%s'\n", game_prefix);

    // SDL initialization and main loop
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0)
    {
        fprintf(stderr, "SDL_Init() failed: %s\n", SDL_GetError());
        return -1;
    }

    // Create fake input devices
    if (config_mode || xbox360_mode)
    {
        // fake keyboard and mouse for any key input (and maybe mouse input)
        setupFakeKeyboardMouseDevice();

        if (xbox360_mode)
        {
            // seperately setup the fake xbox controller
            printf("Running in Fake Xbox 360 Mode\n");
            setupFakeXbox360Device();

            // disable the fake mouse overlay configs
            config_overlay_clear(root_config);
        }
        else
        {
            // or setup the absolute position mouse just in case
            printf("Running in Fake Keyboard mode\n");
            setupFakeAbsoluteMouseDevice();
        }

    }

    const char* db_file = SDL_getenv("SDL_GAMECONTROLLERCONFIG_FILE");
    if (db_file)
    {
        SDL_GameControllerAddMappingsFromFile(db_file);
    }

    SDL_Event event;
    int mouse_x=0;
    int mouse_y=0;
    bool mouse_moved=false;
    vector2d mouse_move;
    float slow_scale = (100.0 / (float)(current_state.mouse_slow_scale));

    while (current_state.running)
    {
        while (current_state.running && SDL_PollEvent(&event))
        {
            handleInputEvent(&event);
        }

        mouse_x=0;
        mouse_y=0;
        mouse_moved = false;

        state_update();

        if (current_state.mouse_relative_x != 0 ||
            current_state.mouse_relative_y != 0 ||
            current_dpad_as_mouse)
        {
            mouse_x = current_state.mouse_relative_x;
            mouse_y = current_state.mouse_relative_y;

            if (current_dpad_as_mouse > 0)
            {
                vector2d_clear(&mouse_move);

                mouse_move.x -= (is_pressed(GBTN_DPAD_LEFT ) ? 1.0f : 0.0f);
                mouse_move.x += (is_pressed(GBTN_DPAD_RIGHT) ? 1.0f : 0.0f);
                mouse_move.y -= (is_pressed(GBTN_DPAD_UP   ) ? 1.0f : 0.0f);
                mouse_move.y += (is_pressed(GBTN_DPAD_DOWN ) ? 1.0f : 0.0f);

                if (current_state.dpad_mouse_normalize)
                    vector2d_normalize(&mouse_move);

                mouse_x += (int)(mouse_move.x * current_state.dpad_mouse_step);
                mouse_y += (int)(mouse_move.y * current_state.dpad_mouse_step);
            }

            if (current_state.mouse_slow)
            {
                mouse_x = (int)((float)(mouse_x) / slow_scale);
                mouse_y = (int)((float)(mouse_y) / slow_scale);
            }

            emitRelativeMouseMotion(mouse_x, mouse_y);

            if (mouse_x != 0 || mouse_y != 0) {
                mouse_moved=true;
                GPTK2_DEBUG("relative mouse move %d %d\n", mouse_x, mouse_y);
            }
        }

        if (current_state.mouse_absolute_x != 0 || current_state.mouse_absolute_y != 0)
        {
            if (current_state.absolute_rotate == 90) {
                mouse_x = current_state.absolute_center_x + (current_state.absolute_step * -current_state.mouse_absolute_y / INT16_MAX);
                mouse_y = current_state.absolute_center_y + (current_state.absolute_step * current_state.mouse_absolute_x / INT16_MAX);
            }
            else if (current_state.absolute_rotate == 180) { 
                mouse_x = current_state.absolute_center_x + (current_state.absolute_step * -current_state.mouse_absolute_x / INT16_MAX);
                mouse_y = current_state.absolute_center_y + (current_state.absolute_step * -current_state.mouse_absolute_y / INT16_MAX);
            }
            else if (current_state.absolute_rotate == 270) {
                mouse_x = current_state.absolute_center_x + (current_state.absolute_step * current_state.mouse_absolute_y / INT16_MAX);
                mouse_y = current_state.absolute_center_y + (current_state.absolute_step * -current_state.mouse_absolute_x / INT16_MAX);
            }
            else {
                mouse_x = current_state.absolute_center_x + (current_state.absolute_step * current_state.mouse_absolute_x / INT16_MAX);
                mouse_y = current_state.absolute_center_y + (current_state.absolute_step * current_state.mouse_absolute_y / INT16_MAX);
            }
            
            if (abs(mouse_x - current_state.absolute_center_x) > current_state.absolute_deadzone ||
                abs(mouse_y - current_state.absolute_center_y) > current_state.absolute_deadzone) {
                
                emitAbsoluteMouseMotion(mouse_x, mouse_y);
                mouse_moved=true;
            }
        }

        if (mouse_moved) {
            // sleep.
            // TODO: FIX ME
            SDL_Delay(current_state.mouse_delay);
        }
        else {
            // GPTK2_DEBUG("-- WAIT FOR EVENT --\n");
            if (!SDL_WaitEvent(&event))
            {
                printf("SDL_WaitEvent() failed: %s\n", SDL_GetError());
                return -1;
            }

            handleInputEvent(&event);
        }
    }

    SDL_Quit();

    /*
     * Give userspace some time to read the events before we destroy the
     * device with UI_DEV_DESTROY.
     */
    sleep(1);

    /* Clean up */
    if (kb_uinp_fd) {
        ioctl(kb_uinp_fd, UI_DEV_DESTROY);
        close(kb_uinp_fd);
    }
    if (xbox_uinp_fd) {
        ioctl(xbox_uinp_fd, UI_DEV_DESTROY);
        close(xbox_uinp_fd);
    }
    if (abs_uinp_fd) {
        ioctl(abs_uinp_fd, UI_DEV_DESTROY);
        close(abs_uinp_fd);
    }

    config_quit();
    state_quit();
    input_quit();
    string_quit();

    return 0;
}
