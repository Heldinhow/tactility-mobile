#pragma once

namespace tt::service::displayidle {

/** The three states of the Lock screensaver type. */
enum class LockState {
    /** Normal operation; the screen may be on or off according to the timeout. */
    Unlocked,
    /** The timeout fired: the backlight is off and no lock widgets exist. */
    LockedOff,
    /** The screen was woken while locked: the lock overlay is visible. */
    LockedOn,
};

enum class LockEvent {
    /** The service starts; there is no lock at boot. */
    Boot,
    /** The display timeout fired. */
    Timeout,
    /** The display saw a wake-up touch while the backlight was off. */
    Activity,
    /** The user unlocked, or the timeout was disabled while locked. */
    Unlock,
};

/** What the caller must apply after a transition. All fields default to "no change". */
struct LockEffects {
    bool backlightOn = false;
    bool backlightOff = false;
    bool showLock = false;
    bool hideLock = false;
    bool hideOverlay = false;
    bool resetActivity = false;
};

/** Applies one event to the lock state and reports the effects to apply. */
inline LockState nextLockState(LockState state, LockEvent event, LockEffects& effects) {
    if (event == LockEvent::Boot) {
        effects.backlightOn = true;
        return LockState::Unlocked;
    }

    switch (state) {
        case LockState::Unlocked:
            if (event == LockEvent::Timeout) {
                effects.backlightOff = true;
                effects.hideOverlay = true;
                effects.hideLock = true;
                return LockState::LockedOff;
            }
            return state;
        case LockState::LockedOff:
            if (event == LockEvent::Activity) {
                effects.backlightOn = true;
                effects.showLock = true;
                return LockState::LockedOn;
            }
            if (event == LockEvent::Unlock) {
                // The timeout was disabled while the screen was off
                effects.backlightOn = true;
                effects.resetActivity = true;
                return LockState::Unlocked;
            }
            return state;
        case LockState::LockedOn:
            if (event == LockEvent::Timeout) {
                effects.backlightOff = true;
                effects.hideLock = true;
                return LockState::LockedOff;
            }
            if (event == LockEvent::Unlock) {
                effects.backlightOn = true;
                effects.hideLock = true;
                effects.resetActivity = true;
                return LockState::Unlocked;
            }
            return state;
    }
    return state;
}

/**
 * @return true only when the lock is visible and the current press started on it. A press that
 * began while the screen was still off, or before the lock appeared, never unlocks.
 */
inline bool canUnlock(LockState state, bool press_started_on_lock) {
    return state == LockState::LockedOn && press_started_on_lock;
}

}
