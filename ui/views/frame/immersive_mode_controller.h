// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_H_

#include "base/compiler_specific.h"

class BookmarkBarView;
class FullscreenController;

namespace gfx {
class Rect;
class Size;
}

namespace views {
class View;
class Widget;
}

// Base class for a lock which keeps the top-of-window views revealed for the
// duration of its lifetime. See ImmersiveModeController::GetRevealedLock() for
// more details.
class ImmersiveRevealedLock {
 public:
  virtual ~ImmersiveRevealedLock() {}
};

// Controller for an "immersive mode" similar to MacOS presentation mode where
// the top-of-window views are hidden until the mouse hits the top of the
// screen. The tab strip is optionally painted with miniature "tab indicator"
// rectangles.
// Currently, immersive mode is only available for Chrome OS.
class ImmersiveModeController {
 public:
  enum AnimateReveal {
    ANIMATE_REVEAL_YES,
    ANIMATE_REVEAL_NO
  };

  class Delegate {
   public:
    // Returns the bookmark bar, or NULL if the window does not support one.
    virtual BookmarkBarView* GetBookmarkBar() = 0;

    // Returns the browser's FullscreenController.
    virtual FullscreenController* GetFullscreenController() = 0;

    // Notifies the delegate that fullscreen has been entered or exited.
    virtual void FullscreenStateChanged() = 0;

    // Requests that the tab strip be painted in a short, "light bar" style.
    virtual void SetImmersiveStyle(bool immersive) = 0;

   protected:
    virtual ~Delegate() {}
  };

  virtual ~ImmersiveModeController() {}

  // Must initialize after browser view has a Widget and native window.
  virtual void Init(Delegate* delegate,
                    views::Widget* widget,
                    views::View* top_container) = 0;

  // Enables or disables immersive mode.
  virtual void SetEnabled(bool enabled) = 0;
  virtual bool IsEnabled() const = 0;

  // True if the miniature "tab indicators" should be hidden in the main browser
  // view when immersive mode is enabled.
  virtual bool ShouldHideTabIndicators() const = 0;

  // True when the top views are hidden due to immersive mode.
  virtual bool ShouldHideTopViews() const = 0;

  // True when the top views are fully or partially visible.
  virtual bool IsRevealed() const = 0;

  // Returns the top container's vertical offset relative to its parent. When
  // revealing or closing the top-of-window views, part of the top container is
  // offscreen.
  // This method takes in the top container's size because it is called as part
  // of computing the new bounds for the top container in
  // BrowserViewLayout::UpdateTopContainerBounds().
  virtual int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const = 0;

  // Returns a lock which will keep the top-of-window views revealed for its
  // lifetime. Several locks can be obtained. When all of the locks are
  // destroyed, if immersive mode is enabled and there is nothing else keeping
  // the top-of-window views revealed, the top-of-window views will be closed.
  // This method always returns a valid lock regardless of whether immersive
  // mode is enabled. The lock's lifetime can span immersive mode being
  // enabled / disabled.
  // If acquiring the lock causes a reveal, the top-of-window views will animate
  // according to |animate_reveal|.
  // The caller takes ownership of the returned lock.
  virtual ImmersiveRevealedLock* GetRevealedLock(
      AnimateReveal animate_reveal) WARN_UNUSED_RESULT = 0;

  // Anchor |widget| to the top-of-window views. This repositions |widget| such
  // that it stays |y_offset| below the top-of-window views when the
  // top-of-window views are animating (top-of-window views reveal / unreveal)
  // or the top container's bounds change (eg the bookmark bar is shown).
  // If the top-of-window views are revealed (or become revealed), |widget|
  // will keep the top-of-window views revealed till either |widget| is hidden
  // or UnanchorWidgetFromTopContainer() is called.
  // It is legal for a widget to be anchored when immersive fullscreen is
  // disabled, however it will have no effect till immersive fullscreen is
  // enabled.
  virtual void AnchorWidgetToTopContainer(views::Widget* widget,
                                          int y_offset) = 0;

  // Stops managing |widget|'s y position.
  // Closes the top-of-window views if no locks or other anchored widgets are
  // keeping the top-of-window views revealed.
  virtual void UnanchorWidgetFromTopContainer(views::Widget* widget) = 0;

  // Called by the TopContainerView to indicate that its bounds have changed.
  virtual void OnTopContainerBoundsChanged() = 0;

  // Called by the find bar to indicate that its visible bounds have changed.
  // |new_visible_bounds_in_screen| should be empty if the find bar is not
  // visible.
  virtual void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds_in_screen) = 0;
};

namespace chrome {

// Implemented in immersive_mode_controller_factory.cc.
ImmersiveModeController* CreateImmersiveModeController();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_H_
