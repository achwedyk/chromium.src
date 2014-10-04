// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_UI_DEFAULT_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_UI_DEFAULT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "extensions/common/extension.h"

class Browser;
class Profile;

namespace extensions {

class ExtensionErrorUIDefault : public ExtensionErrorUI {
 public:
  explicit ExtensionErrorUIDefault(ExtensionErrorUI::Delegate* delegate);
  virtual ~ExtensionErrorUIDefault();

  // ExtensionErrorUI implementation:
  virtual bool ShowErrorInBubbleView() override;
  virtual void ShowExtensions() override;
  virtual void Close() override;

 private:
  class ExtensionGlobalError : public GlobalErrorWithStandardBubble {
   public:
    explicit ExtensionGlobalError(ExtensionErrorUIDefault* error_ui);

   private:
    // GlobalError methods.
    virtual bool HasMenuItem() override;
    virtual int MenuItemCommandID() override;
    virtual base::string16 MenuItemLabel() override;
    virtual void ExecuteMenuItem(Browser* browser) override;
    virtual base::string16 GetBubbleViewTitle() override;
    virtual std::vector<base::string16> GetBubbleViewMessages() override;
    virtual base::string16 GetBubbleViewAcceptButtonLabel() override;
    virtual base::string16 GetBubbleViewCancelButtonLabel() override;
    virtual void OnBubbleViewDidClose(Browser* browser) override;
    virtual void BubbleViewAcceptButtonPressed(Browser* browser) override;
    virtual void BubbleViewCancelButtonPressed(Browser* browser) override;

    // The ExtensionErrorUIDefault who owns us.
    ExtensionErrorUIDefault* error_ui_;

    DISALLOW_COPY_AND_ASSIGN(ExtensionGlobalError);
  };

  // The profile associated with this error.
  Profile* profile_;

  // The browser the bubble view was shown into.
  Browser* browser_;

  scoped_ptr<ExtensionGlobalError> global_error_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionErrorUIDefault);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_UI_DEFAULT_H_
