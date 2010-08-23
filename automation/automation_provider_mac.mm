// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util.h"
#include "app/l10n_util_mac.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/cocoa/tab_window_controller.h"
#include "chrome/browser/view_ids.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/test/automation/automation_messages.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "grit/generated_resources.h"

void AutomationProvider::SetWindowBounds(int handle, const gfx::Rect& bounds,
                                         bool* success) {
  *success = false;
  NSWindow* window = window_tracker_->GetResource(handle);
  if (window) {
    NSRect new_bounds = NSRectFromCGRect(bounds.ToCGRect());

    if ([[NSScreen screens] count] > 0) {
      new_bounds.origin.y =
          [[[NSScreen screens] objectAtIndex:0] frame].size.height -
          new_bounds.origin.y - new_bounds.size.height;
    }

    [window setFrame:new_bounds display:NO];
    *success = true;
  }
}

void AutomationProvider::SetWindowVisible(int handle, bool visible,
                                          bool* result) {
  *result = false;
  NSWindow* window = window_tracker_->GetResource(handle);
  if (window) {
    if (visible) {
      [window orderFront:nil];
    } else {
      [window orderOut:nil];
    }
    *result = true;
  }
}

void AutomationProvider::WindowGetViewBounds(int handle, int view_id,
                                             bool screen_coordinates,
                                             bool* success,
                                             gfx::Rect* bounds) {
  *success = false;

  // At the moment we hard code the view ID used by WebDriver and do
  // not support arbitrary view IDs.  suzhe is working on general view
  // ID support for the Mac.
  if (view_id != VIEW_ID_TAB_CONTAINER) {
    NOTIMPLEMENTED();
    return;
  }

  NSWindow* window = window_tracker_->GetResource(handle);
  if (!window)
    return;

  BrowserWindowController* controller = [window windowController];
  DCHECK([controller isKindOfClass:[BrowserWindowController class]]);
  if (![controller isKindOfClass:[BrowserWindowController class]])
    return;
  NSView* tab = [controller selectedTabView];
  if (!tab)
    return;

  NSPoint coords = NSZeroPoint;
  if (screen_coordinates) {
    coords = [window convertBaseToScreen:[tab convertPoint:NSZeroPoint
                                                    toView:nil]];
  } else {
    coords = [tab convertPoint:NSZeroPoint toView:[window contentView]];
  }
  // Flip coordinate system
  coords.y = [[window screen] frame].size.height - coords.y;
  *success = true;
}

void AutomationProvider::PrintAsync(int tab_handle) {
  NOTIMPLEMENTED();
}

void AutomationProvider::WindowSimulateDrag(int handle,
                                            std::vector<gfx::Point> drag_path,
                                            int flags,
                                            bool press_escape_en_route,
                                            IPC::Message* reply_message) {
  NOTIMPLEMENTED();
  AutomationMsg_WindowDrag::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void AutomationProvider::TerminateSession(int handle, bool* success) {
  *success = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetWindowBounds(int handle, gfx::Rect* bounds,
                                         bool* result) {
  *result = false;
  NOTIMPLEMENTED();
}

void AutomationProvider::GetWindowTitle(int handle, string16* text) {
  gfx::NativeWindow window = window_tracker_->GetResource(handle);
  NSString* title = nil;
  if ([[window delegate] isKindOfClass:[TabWindowController class]]) {
    TabWindowController* delegate =
        reinterpret_cast<TabWindowController*>([window delegate]);
    title = [delegate selectedTabTitle];
  } else {
    title = [window title];
  }
  // If we don't yet have a title, use "Untitled".
  if (![title length]) {
    text->assign(l10n_util::GetStringUTF16(
        IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED));
    return;
  }

  text->assign(base::SysNSStringToUTF16(title));
}
