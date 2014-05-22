// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/password_generation_popup_view_cocoa.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/cocoa/autofill/password_generation_popup_view_bridge.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "grit/ui_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/text_constants.h"

using autofill::AutofillPopupView;
using autofill::PasswordGenerationPopupView;
using base::scoped_nsobject;

namespace {

NSColor* DividerColor() {
  return gfx::SkColorToCalibratedNSColor(
      PasswordGenerationPopupView::kDividerColor);
}

NSColor* HelpTextBackgroundColor() {
  return gfx::SkColorToCalibratedNSColor(
      PasswordGenerationPopupView::kExplanatoryTextBackgroundColor);
}

NSColor* HelpTextColor() {
  return gfx::SkColorToCalibratedNSColor(
      PasswordGenerationPopupView::kExplanatoryTextColor);
}

}  // namespace

@implementation PasswordGenerationPopupViewCocoa

#pragma mark Initialisers

- (id)initWithFrame:(NSRect)frame {
  NOTREACHED();
  return nil;
}

- (id)initWithController:
    (autofill::PasswordGenerationPopupController*)controller
                   frame:(NSRect)frame {
  if (self = [super initWithDelegate:controller frame:frame]) {
    controller_ = controller;
    NSFont* font = controller_->font_list().GetPrimaryFont().GetNativeFont();

    passwordField_ = [self textFieldWithText:controller_->password()
                                    withFont:font
                                       color:[self nameColor]
                                   alignment:NSLeftTextAlignment];
    [self addSubview:passwordField_];

    passwordSubtextField_ =
        [self textFieldWithText:controller_->SuggestedText()
                       withFont:font
                          color:[self subtextColor]
                      alignment:NSRightTextAlignment];
    [self addSubview:passwordSubtextField_];

    helpTextField_ = [self textFieldWithText:controller_->HelpText()
                                    withFont:font
                                       color:HelpTextColor()
                                   alignment:NSLeftTextAlignment];
    [self addSubview:helpTextField_];
  }

  return self;
}

#pragma mark NSView implementation:

- (void)drawRect:(NSRect)dirtyRect {
  // If the view is in the process of being destroyed, don't bother drawing.
  if (!controller_)
    return;

  [self drawBackgroundAndBorder];

  if (controller_->password_selected()) {
    // Draw a highlight under the suggested password.
    NSRect highlightBounds = [self passwordBounds];
    highlightBounds.origin.y +=
        PasswordGenerationPopupView::kPasswordVerticalInset;
    highlightBounds.size.height -=
        2 * PasswordGenerationPopupView::kPasswordVerticalInset;
    [[self highlightColor] set];
    [NSBezierPath fillRect:highlightBounds];
  }

  // Render the background of the help text.
  [HelpTextBackgroundColor() set];
  [NSBezierPath fillRect:[self helpBounds]];

  // Render the divider.
  [DividerColor() set];
  [NSBezierPath fillRect:[self dividerBounds]];
}

#pragma mark Public API:

- (void)updateBoundsAndRedrawPopup {
  [self positionTextField:passwordField_ inRect:[self passwordBounds]];
  [self positionTextField:passwordSubtextField_ inRect:[self passwordBounds]];
  [self positionTextField:helpTextField_ inRect:[self helpBounds]];

  [super updateBoundsAndRedrawPopup];
}

- (void)controllerDestroyed {
  controller_ = NULL;
  [super delegateDestroyed];
}

#pragma mark Private helpers:

- (NSTextField*)textFieldWithText:(const base::string16&)text
                         withFont:(NSFont*)font
                            color:(NSColor*)color
                        alignment:(NSTextAlignment)alignment {
  scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setAlignment:alignment];

  NSDictionary* textAttributes = @{
    NSFontAttributeName : font,
    NSForegroundColorAttributeName : color,
    NSParagraphStyleAttributeName : paragraphStyle
  };

  scoped_nsobject<NSAttributedString> attributedString(
      [[NSAttributedString alloc]
          initWithString:base::SysUTF16ToNSString(text)
              attributes:textAttributes]);

  NSTextField* textField =
      [[[NSTextField alloc] initWithFrame:NSZeroRect] autorelease];
  [textField setAttributedStringValue:attributedString];
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];

  return textField;
}

- (void)positionTextField:(NSTextField*)textField inRect:(NSRect)bounds {
  NSRect frame = NSInsetRect(bounds, controller_->kHorizontalPadding, 0);
  [textField setFrame:frame];

  // Center the text vertically within the bounds.
  NSSize delta = cocoa_l10n_util::WrapOrSizeToFit(textField);
  [textField setFrameOrigin:
      NSInsetRect(frame, 0, floor(-delta.height/2)).origin];
}

- (NSRect)passwordBounds {
  return NSRectFromCGRect(controller_->password_bounds().ToCGRect());
}

- (NSRect)helpBounds {
  return NSRectFromCGRect(controller_->help_bounds().ToCGRect());
}

- (NSRect)dividerBounds {
  return NSRectFromCGRect(controller_->divider_bounds().ToCGRect());
}

@end
