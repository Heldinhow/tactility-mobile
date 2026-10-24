// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace tt::service::displayidle {

/** Called when the user unlocks; the DisplayIdle service owns the state transition. */
using LockUnlockCallback = void (*)(void* context);

void lockScreenSetUnlockCallback(LockUnlockCallback callback, void* context);

/** Asks the owner to unlock, e.g. from a swipe or any key press. No-op without a callback. */
void lockScreenRequestUnlock();

/** @return true while the lock overlay is on screen */
bool lockScreenIsVisible();

/**
 * Builds the lock overlay on the top layer and gives it the input group.
 * @return false when the widgets could not be created
 * @warning Caller must hold the LVGL lock
 */
bool lockScreenShow();

/** Deletes the lock overlay and restores the input group. Safe when already hidden. */
void lockScreenHide();

}
