// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_VIRTUAL_KEYBOARD_CONTROLLER_H_
#define ASH_VIRTUAL_KEYBOARD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace ash {

// This class observes input device changes for the virtual keyboard.
class ASH_EXPORT VirtualKeyboardController
    : public ShellObserver,
      public ui::InputDeviceEventObserver {
 public:
  VirtualKeyboardController();
  ~VirtualKeyboardController() override;

  // ShellObserver:
  // TODO(rsadam@): Remove when autovirtual keyboard flag is on by default.
  virtual void OnMaximizeModeStarted() override;
  virtual void OnMaximizeModeEnded() override;

  // ui::InputDeviceObserver:
  virtual void OnTouchscreenDeviceConfigurationChanged() override;
  virtual void OnKeyboardDeviceConfigurationChanged() override;
  virtual void OnMouseDeviceConfigurationChanged() override;
  virtual void OnTouchpadDeviceConfigurationChanged() override;

  // Toggles whether the presense of an external keyboard should be ignored
  // when determining whether or not to show the on-screen keyboard.
  void ToggleIgnoreExternalKeyboard();

 private:
  // Updates the list of active input devices.
  void UpdateDevices();

  // Updates the keyboard state.
  void UpdateKeyboardEnabled();

  // Creates the keyboard if |enabled|, else destroys it.
  void SetKeyboardEnabled(bool enabled);

  // True if an external keyboard is connected.
  bool has_external_keyboard_;
  // True if an internal keyboard is connected.
  bool has_internal_keyboard_;
  // True if a touchscreen is connected.
  bool has_touchscreen_;
  // True if the presense of an external keyboard should be ignored.
  bool ignore_external_keyboard_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardController);
};

}  // namespace ash

#endif  // ASH_VIRTUAL_KEYBOARD_CONTROLLER_H_
