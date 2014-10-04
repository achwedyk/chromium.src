// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_
#define CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"

class MockTabModalConfirmDialogDelegate : public TabModalConfirmDialogDelegate {
 public:
  class Delegate {
   public:
    virtual void OnAccepted() = 0;
    virtual void OnCanceled() = 0;
    virtual void OnClosed() = 0;
   protected:
    virtual ~Delegate() {}
  };

  MockTabModalConfirmDialogDelegate(content::WebContents* web_contents,
                                    Delegate* delegate);
  virtual ~MockTabModalConfirmDialogDelegate();

  virtual base::string16 GetTitle() override;
  virtual base::string16 GetDialogMessage() override;

  virtual void OnAccepted() override;
  virtual void OnCanceled() override;
  virtual void OnClosed() override;

 private:
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockTabModalConfirmDialogDelegate);
};

class TabModalConfirmDialogTest
    : public InProcessBrowserTest,
      public MockTabModalConfirmDialogDelegate::Delegate {
 public:
  TabModalConfirmDialogTest();

  virtual void SetUpOnMainThread() override;
  virtual void TearDownOnMainThread() override;

  // MockTabModalConfirmDialogDelegate::Delegate:
  virtual void OnAccepted() override;
  virtual void OnCanceled() override;
  virtual void OnClosed() override;

 protected:
  // Owned by |dialog_|.
  MockTabModalConfirmDialogDelegate* delegate_;

  // Deletes itself.
  TabModalConfirmDialog* dialog_;

  int accepted_count_;
  int canceled_count_;
  int closed_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogTest);
};

#endif  // CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_BROWSERTEST_H_
