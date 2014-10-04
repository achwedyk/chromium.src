// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace views {
class MessageBoxView;
class Widget;
}

// Displays a tab-modal dialog, i.e. a dialog that will block the current page
// but still allow the user to switch to a different page.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class TabModalConfirmDialogViews : public TabModalConfirmDialog,
                                   public views::DialogDelegate,
                                   public views::LinkListener {
 public:
  TabModalConfirmDialogViews(TabModalConfirmDialogDelegate* delegate,
                             content::WebContents* web_contents);

  // views::DialogDelegate:
  virtual base::string16 GetWindowTitle() const override;
  virtual base::string16 GetDialogButtonLabel(
      ui::DialogButton button) const override;
  virtual bool Cancel() override;
  virtual bool Accept() override;
  virtual bool Close() override;

  // views::WidgetDelegate:
  virtual views::View* GetContentsView() override;
  virtual views::Widget* GetWidget() override;
  virtual const views::Widget* GetWidget() const override;
  virtual void DeleteDelegate() override;
  virtual ui::ModalType GetModalType() const override;

 private:
  virtual ~TabModalConfirmDialogViews();

  // TabModalConfirmDialog:
  virtual void AcceptTabModalDialog() override;
  virtual void CancelTabModalDialog() override;

  // TabModalConfirmDialogCloseDelegate:
  virtual void CloseDialog() override;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) override;

  scoped_ptr<TabModalConfirmDialogDelegate> delegate_;

  // The message box view whose commands we handle.
  views::MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_MODAL_CONFIRM_DIALOG_VIEWS_H_
