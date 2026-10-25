// SPDX-License-Identifier: Apache-2.0
#include "sdl_input.h"
#include "sdl_display.h"

#include <tactility/drivers/keyboard.h>

#include <lvgl/lvgl.h>
#include <lvgl/draw/lv_snapshot.h>

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <mutex>

// Development aid, see Tactility/Source/lvgl/SimulatorDevNavigation.cpp
extern void simulatorDevAction(int index);
extern void simulatorDevActionList(const char* list);

namespace {

constexpr size_t KEY_QUEUE_CAPACITY = 32;

// Written by sdl_input_pump() on the real main thread, read by sdl_input_get_pointer_state()/
// sdl_input_pop_key()/sdl_input_has_queued_key() on the lvgl task.
std::mutex state_mutex;

// The position in window coordinates, and the size of the window then
SdlPointerState pointer_state = { 0, 0, false };
int window_width = 0;
int window_height = 0;

uint32_t key_queue[KEY_QUEUE_CAPACITY];
size_t key_queue_head = 0;
size_t key_queue_count = 0;

bool text_input_started = false;

void push_key(uint32_t key) {
    if (key == 0 || key_queue_count >= KEY_QUEUE_CAPACITY) {
        return;
    }
    key_queue[(key_queue_head + key_queue_count) % KEY_QUEUE_CAPACITY] = key;
    key_queue_count++;
}

// Mirrors LVGL's own lv_sdl_keyboard.c keycode_to_ctrl_key(): every key returns a real Unicode
// codepoint per KeyboardKeyData::key's contract - never an LV_KEY_* constant, even for keys with
// no ordinary character of their own (arrows, Tab/Shift+Tab as focus nav, Home/End), which use the
// CodePoint enum's standard Unicode symbols for the concept. lvgl-module's keyboard.cpp translates
// all of these back to LVGL's own sentinels (CODEPOINT_ESCAPE/BACKSPACE/DELETE already equal their
// LV_KEY_* counterpart numerically, so no translation is needed for those). Printable characters
// arrive separately via SDL_TEXTINPUT.
// The window can be freely resized (see sdl_display.cpp's SDL_WINDOW_RESIZABLE/
// SDL_RenderSetLogicalSize()), so raw SDL mouse coordinates are in window-pixel space, not the
// resolution LVGL renders at. The view also changes shape when the display rotates. The raw
// position is kept with the window size, and sdl_input_get_pointer_state() maps it onto the
// current view, so a rotation takes effect right away even when the mouse doesn't move.
// LVGL rotates pointer positions from the display's native orientation to the rotated view itself,
// so the position in the view is turned back to a native one at the end.
// The view is scaled to fit the window and centered, like SDL_RenderSetLogicalSize() does.
void set_pointer_position(int32_t window_x, int32_t window_y) {
    pointer_state.x = window_x;
    pointer_state.y = window_y;
    SDL_Renderer* renderer = sdl_display_get_renderer();
    SDL_Window* window = renderer != nullptr ? SDL_RenderGetWindow(renderer) : nullptr;
    if (window != nullptr) {
        SDL_GetWindowSize(window, &window_width, &window_height);
    }
}

void map_to_view(int32_t* x, int32_t* y) {
    int view_width, view_height;
    if (window_width <= 0 || window_height <= 0 || !sdl_display_get_view_size(&view_width, &view_height) || view_width <= 0 || view_height <= 0) {
        return;
    }
    const float scale = std::min(static_cast<float>(window_width) / view_width, static_cast<float>(window_height) / view_height);
    const float offset_x = (window_width - view_width * scale) / 2.0f;
    const float offset_y = (window_height - view_height * scale) / 2.0f;
    *x = std::clamp(static_cast<int32_t>((*x - offset_x) / scale), 0, view_width - 1);
    *y = std::clamp(static_cast<int32_t>((*y - offset_y) / scale), 0, view_height - 1);
    sdl_display_view_to_native(x, y);
}

uint32_t keycode_to_key(SDL_Keycode sdl_key, bool shift) {
    switch (sdl_key) {
        case SDLK_RIGHT: return CODEPOINT_ARROW_RIGHT;
        case SDLK_LEFT: return CODEPOINT_ARROW_LEFT;
        case SDLK_UP: return CODEPOINT_ARROW_UP;
        case SDLK_DOWN: return CODEPOINT_ARROW_DOWN;
        case SDLK_ESCAPE: return CODEPOINT_ESCAPE;
        case SDLK_BACKSPACE: return CODEPOINT_BACKSPACE;
        case SDLK_DELETE: return CODEPOINT_DELETE;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: return CODEPOINT_ENTER;
        case SDLK_TAB: return CODEPOINT_TAB;
        case SDLK_HOME: return CODEPOINT_HOME;
        case SDLK_END: return CODEPOINT_END;
        default: return 0;
    }
}

/**
 * Development aid for the screen comparisons: F12 saves the active screen as a PPM (RGB888).
 * TACTILITY_SIM_SCREENSHOT overrides the path; the default is next to the simulator's working
 * directory. A PPM keeps this free of an encoder dependency; the verification step converts it.
 */
/** Takes the snapshot; the caller runs on the LVGL task or holds the LVGL lock. */
void take_screenshot_now() {
    // The lock lives on the top layer; overlays in the app layer are part of the active screen
    lv_obj_t* target = lv_layer_top();
    if (lv_obj_get_child_count(target) == 0) {
        target = lv_screen_active();
    }
    lv_draw_buf_t* snapshot = lv_snapshot_take(target, LV_COLOR_FORMAT_RGB888);
    if (snapshot != nullptr) {
        const char* path = getenv("TACTILITY_SIM_SCREENSHOT");
        if (path == nullptr) {
            path = "sim-screenshot.ppm";
        }
        FILE* file = fopen(path, "wb");
        if (file != nullptr) {
            fprintf(file, "P6\n%u %u\n255\n", static_cast<unsigned>(snapshot->header.w), static_cast<unsigned>(snapshot->header.h));
            const auto* data = static_cast<const uint8_t*>(snapshot->data);
            for (uint32_t y = 0; y < snapshot->header.h; y++) {
                const uint8_t* row = data + y * snapshot->header.stride;
                for (uint32_t x = 0; x < snapshot->header.w; x++) {
                    // LVGL's RGB888 buffer is BGR; PPM is RGB
                    const uint8_t pixel[3] = { row[x * 3 + 2], row[x * 3 + 1], row[x * 3] };
                    fwrite(pixel, 1, 3, file);
                }
            }
            fclose(file);
        }
        lv_draw_buf_destroy(snapshot);
    }
}

void take_screenshot() {
    lvgl_lock();
    take_screenshot_now();
    lvgl_unlock();
}

}

/** Development aid for SimulatorDevNavigation: called on the LVGL task. */
void simulator_take_screenshot_now() {
    take_screenshot_now();
}

void sdl_input_pump() {
    // exit() must run with state_mutex unlocked: it never returns, so a lock_guard held across it
    // would never release the mutex, hanging any other thread that later calls into this file's
    // other functions (all of which lock state_mutex) while exit() tears the process down.
    bool quit_requested = false;

    {
        std::lock_guard<std::mutex> lock(state_mutex);

        if (!text_input_started) {
            SDL_StartTextInput();
            text_input_started = true;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_MOUSEMOTION:
                    set_pointer_position(event.motion.x, event.motion.y);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        // event.button.x/y can be stale immediately after a window resize (an
                        // SDL/X11 event-queue quirk - confirmed by comparing against a live
                        // SDL_GetWindowSize() at the same instant). SDL_GetMouseState() queries the
                        // OS for the current pointer position directly, sidestepping that entirely.
                        int live_x, live_y;
                        SDL_GetMouseState(&live_x, &live_y);
                        set_pointer_position(live_x, live_y);
                        pointer_state.pressed = true;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        pointer_state.pressed = false;
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_F12) {
                        take_screenshot();
                    } else if (event.key.keysym.sym >= SDLK_F1 && event.key.keysym.sym <= SDLK_F9) {
                        simulatorDevAction(static_cast<int>(event.key.keysym.sym - SDLK_F1) + 1);
                    } else {
                        push_key(keycode_to_key(event.key.keysym.sym, (event.key.keysym.mod & KMOD_SHIFT) != 0));
                    }
                    break;
                case SDL_TEXTINPUT:
                    // ASCII only (first byte of event.text.text) - sufficient for a simulator keyboard.
                    push_key(static_cast<uint8_t>(event.text.text[0]));
                    break;
                case SDL_WINDOWEVENT:
                    // Resizing doesn't change what LVGL last rendered, only how large it should
                    // appear - re-present the existing frame at the new scale immediately, rather
                    // than leaving stale-looking content on screen until the next LVGL-driven flush.
                    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        window_width = event.window.data1;
                        window_height = event.window.data2;
                        sdl_display_present_now();
                    }
                    break;
                case SDL_QUIT:
                    quit_requested = true;
                    break;
                default:
                    break;
            }
        }
    }

    if (quit_requested) {
        exit(0);
    }

    // Development aid: open a screen from TACTILITY_SIM_ACTION (1-9, see simulatorDevAction)
    static bool dev_action_done = false;
    if (!dev_action_done) {
        const char* action = getenv("TACTILITY_SIM_ACTION");
        if (action != nullptr && SDL_GetTicks() >= 2500) {
            simulatorDevActionList(action);
            dev_action_done = true;
        }
    }

    // Development aid: capture once after a delay without needing a window or key interaction.
    // With TACTILITY_SIM_ACTION the action schedules the capture on the LVGL task instead, because
    // taking the LVGL lock from this thread while it is contended aborts the POSIX port.
    static bool auto_capture_done = false;
    if (!auto_capture_done && getenv("TACTILITY_SIM_ACTION") == nullptr) {
        const char* delay = getenv("TACTILITY_SIM_SCREENSHOT_DELAY_MS");
        if (delay != nullptr && SDL_GetTicks() >= static_cast<uint32_t>(atoi(delay))) {
            take_screenshot();
            auto_capture_done = true;
        }
    }
}

void sdl_input_get_pointer_state(SdlPointerState* out_state) {
    std::lock_guard<std::mutex> lock(state_mutex);
    *out_state = pointer_state;
    map_to_view(&out_state->x, &out_state->y);
}

bool sdl_input_pop_key(uint32_t* out_key) {
    std::lock_guard<std::mutex> lock(state_mutex);
    if (key_queue_count == 0) {
        return false;
    }
    *out_key = key_queue[key_queue_head];
    key_queue_head = (key_queue_head + 1) % KEY_QUEUE_CAPACITY;
    key_queue_count--;
    return true;
}

bool sdl_input_has_queued_key() {
    std::lock_guard<std::mutex> lock(state_mutex);
    return key_queue_count > 0;
}
