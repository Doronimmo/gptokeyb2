# GamePad TO KeyBoard 2

Gamepad to Keyboard/mouse/xbox360(gamepad) emulator

- Based on code by: [Kris Henriksen](https://github.com/krishenriksen/AnberPorts/tree/master/AnberPorts-Joystick) and fake Xbox code from: https://github.com/Emanem/js2xbox
- Modified to use SDL2 by: [Nikolai Wuttke](https://github.com/lethal-guitar) & [Shanti Gilbert](https://github.com/shantigilbert) for https://github.com/EmuELEC/EmuELEC
- Interactive text entry added by [Robin Duxfield](https://github.com/romadu)
- Mostly a complete rewrite by [Jacob Smith](https://github.com/kloptops)

## About

`gptokeyb2` is a fairly big rewrite of `gptokeyb`. It uses a finite state machine (FSM) and a better configuration system. The new FSM allows much more complex control schemes, allowing greater possibilities in controlling ports.

Features:
- multiple controller layouts that can be moved between.
- customizable text input engine


While `gptokeyb2` has a lot more flexibility, it is also backwards compatible with `gptokeyb`, allowing an easy drop in replacement of the older program without any extra work.




## Build

`gptokeyb2` depends on SDL2 and libevdev. On Debian, you can install them using `apt install libsdl2-dev libevdev-dev`. To build for ARM devices using cross-compilation, you can use one of the provided toolchains in `cmake/toolchains` by adding argument `-DCMAKE_TOOLCHAIN_FILE=<path to toolchain>`. 

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=[Release/Debug]
cmake --build .
strip gptokeyb2
```

## Usage

```bash
./gptokeyb2 "program" -c "controls.ini" &
./program
kill -9 $(pidof gptokeyb2)
```

### Complex Example:

```ini
[config]
deadzone_triggers = 3000
mouse_scale = 6114
mouse_delay = 16
mouse_slow_scale = 30

deadzone_mode = axial
deadzone = 1000
deadzone_scale = 7
deadzone_delay = 16

[controls]
# Clear any defaults
overlay = clear

# Scroll by this amount.
mouse_wheel_amount = 3

a = mouse_left                          # Mouse Left
b = mouse_right                         # Mouse Right
x = s                                   # Stop
y = g                                   # Guard

l1 = ctrl                               # Force attack
l2 = shift                              # Add to selection
r1 = mouse_slow                         # Slow mouse
r2 = x                                  # Scatter

back = hold_state hotkey                # back + button combos

up = 1
down = 3
left = 4
right = 2

left_analog = mouse_movement
right_analog = arrow_keys repeat

[controls:hotkey]
overlay = parent

# We get this from above.
# mouse_wheel_amount = parent

l1 = n                               # Select next unit
l2 = e                               # Select everything on screen
r1 = tab                             # Show/Hide sidebar
r2 = home                            # Center screen on selection

right_analog_left  = clear
right_analog_right = clear
right_analog_up   = mouse_wheel_up repeat
right_analog_down = mouse_wheel_down repeat

left_analog = absolute_mouse_movement
# how many pixels of deflection before we move the pointer
absolute_deadzone = 3
# default is zero but set to some other angle if you have a rotated screen
absolute_rotate = 0
# we hardcode a 1280x960 screen here so 640x480 is dead center
absolute_center_x = 640
absolute_center_y = 510
# how many pixels is maximum deflection
absolute_step = 350

[controls:analog_0]
# Controls for devices with 0 analog sticks.
overlay = controls

# Toggle to dpad_mouse
r1 = push_state dpad_mouse

up = 1
down = 3
left = 4
right = 2

[controls:dpad_mouse]
overlay = controls

# Toggle to back
r1 = pop_state

dpad = mouse_movement
```
