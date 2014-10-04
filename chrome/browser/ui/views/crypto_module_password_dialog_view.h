// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CRYPTO_MODULE_PASSWORD_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CRYPTO_MODULE_PASSWORD_DIALOG_VIEW_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class Textfield;
}

namespace chrome {

class CryptoModulePasswordDialogView : public views::DialogDelegateView,
                                       public views::TextfieldController {
 public:
  CryptoModulePasswordDialogView(const std::string& slot_name,
                                 CryptoModulePasswordReason reason,
                                 const std::string& server,
                                 const CryptoModulePasswordCallback& callback);

  virtual ~CryptoModulePasswordDialogView();

 private:
  FRIEND_TEST_ALL_PREFIXES(CryptoModulePasswordDialogViewTest, TestAccept);

  // views::WidgetDelegate:
  virtual views::View* GetInitiallyFocusedView() override;
  virtual ui::ModalType GetModalType() const override;
  virtual base::string16 GetWindowTitle() const override;

  // views::DialogDelegate:
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const override;
  virtual bool Cancel() override;
  virtual bool Accept() override;

  // views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const base::string16& new_contents) override;
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const ui::KeyEvent& keystroke) override;

  // Initialize views and layout.
  void Init(const std::string& server,
            const std::string& slot_name,
            CryptoModulePasswordReason reason);

  views::Label* reason_label_;
  views::Label* password_label_;
  views::Textfield* password_entry_;

  const CryptoModulePasswordCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(CryptoModulePasswordDialogView);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_CRYPTO_MODULE_PASSWORD_DIALOG_VIEW_H_
