#include "doctest.h"

#include <Tactility/service/displayidle/LockState.h>

using namespace tt::service::displayidle;

TEST_CASE("boot is always unlocked") {
    LockEffects effects;
    CHECK(nextLockState(LockState::LockedOff, LockEvent::Boot, effects) == LockState::Unlocked);
    CHECK(effects.backlightOn);
    CHECK_FALSE(effects.backlightOff);
    CHECK_FALSE(effects.showLock);
}

TEST_CASE("a timeout switches the screen off and hides any overlay") {
    LockEffects effects;
    CHECK(nextLockState(LockState::Unlocked, LockEvent::Timeout, effects) == LockState::LockedOff);
    CHECK(effects.backlightOff);
    CHECK(effects.hideOverlay);
    CHECK_FALSE(effects.backlightOn);
    CHECK_FALSE(effects.showLock);
}

TEST_CASE("waking a locked-off display shows the lock without unlocking") {
    LockEffects effects;
    CHECK(nextLockState(LockState::LockedOff, LockEvent::Activity, effects) == LockState::LockedOn);
    CHECK(effects.backlightOn);
    CHECK(effects.showLock);
    CHECK_FALSE(effects.resetActivity);
}

TEST_CASE("only a press that starts on the visible lock can unlock") {
    CHECK(canUnlock(LockState::LockedOn, true));
    CHECK_FALSE(canUnlock(LockState::LockedOn, false));
    CHECK_FALSE(canUnlock(LockState::LockedOff, true));
    CHECK_FALSE(canUnlock(LockState::Unlocked, true));
}

TEST_CASE("a second timeout hides the lock and keeps the state locked off") {
    LockEffects effects;
    CHECK(nextLockState(LockState::LockedOn, LockEvent::Timeout, effects) == LockState::LockedOff);
    CHECK(effects.backlightOff);
    CHECK(effects.hideLock);
    CHECK_FALSE(effects.showLock);
}

TEST_CASE("unlocking returns to the previous screen and resets activity") {
    LockEffects effects;
    CHECK(nextLockState(LockState::LockedOn, LockEvent::Unlock, effects) == LockState::Unlocked);
    CHECK(effects.backlightOn);
    CHECK(effects.hideLock);
    CHECK(effects.resetActivity);
    CHECK_FALSE(effects.backlightOff);
}

TEST_CASE("disabling the timeout while locked off wakes and unlocks") {
    LockEffects effects;
    CHECK(nextLockState(LockState::LockedOff, LockEvent::Unlock, effects) == LockState::Unlocked);
    CHECK(effects.backlightOn);
    CHECK(effects.resetActivity);
}

TEST_CASE("activity while unlocked or locked on changes nothing") {
    LockEffects unlocked_effects;
    CHECK(nextLockState(LockState::Unlocked, LockEvent::Activity, unlocked_effects) == LockState::Unlocked);
    CHECK_FALSE(unlocked_effects.backlightOn);
    CHECK_FALSE(unlocked_effects.showLock);

    LockEffects locked_effects;
    CHECK(nextLockState(LockState::LockedOn, LockEvent::Activity, locked_effects) == LockState::LockedOn);
    CHECK_FALSE(locked_effects.backlightOn);
    CHECK_FALSE(locked_effects.showLock);
}
