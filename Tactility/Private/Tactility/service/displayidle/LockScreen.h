// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace tt::service::displayidle {

/** Owned by the DisplayIdle service; the lock module only fills in the LVGL widgets. */
struct LockWidgets;

/** Called when the user unlocks; the DisplayIdle service owns the state transition. */
using LockUnlockCallback = void (*)(void* context);

void lockScreenSetUnlockCallback(LockWidgets*& lock, LockUnlockCallback callback, void* context);

/** Asks the owner to unlock, e.g. from a key press. No-op without a callback. */
void lockScreenRequestUnlock(LockWidgets* lock);

/** @return true while the lock overlay is on screen */
bool lockScreenIsVisible(const LockWidgets* lock);

/**
 * Builds the lock overlay on the top layer and gives it the input group.
 * @return false when the widgets or the unlock callback are unavailable
 * @warning Caller must hold the LVGL lock
 */
bool lockScreenShow(LockWidgets*& lock);

/** Deletes the lock overlay and restores the input group. Safe when already hidden. */
void lockScreenHide(LockWidgets*& lock);

}
