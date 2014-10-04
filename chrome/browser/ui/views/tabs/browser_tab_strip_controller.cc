// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"

#include "base/auto_reset.h"
#include "base/prefs/pref_service.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/autocomplete_match.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/webplugininfo.h"
#include "ipc/ipc_message.h"
#include "net/base/filename_util.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

using base::UserMetricsAction;
using content::WebContents;

namespace {

TabRendererData::NetworkState TabContentsNetworkState(
    WebContents* contents) {
  if (!contents || !contents->IsLoadingToDifferentDocument())
    return TabRendererData::NETWORK_STATE_NONE;
  if (contents->IsWaitingForResponse())
    return TabRendererData::NETWORK_STATE_WAITING;
  return TabRendererData::NETWORK_STATE_LOADING;
}

bool DetermineTabStripLayoutStacked(
    PrefService* prefs,
    chrome::HostDesktopType host_desktop_type,
    bool* adjust_layout) {
  *adjust_layout = false;
  // For ash, always allow entering stacked mode.
  if (host_desktop_type != chrome::HOST_DESKTOP_TYPE_ASH)
    return false;
  *adjust_layout = true;
  return prefs->GetBoolean(prefs::kTabStripStackedLayout);
}

// Get the MIME type of the file pointed to by the url, based on the file's
// extension. Must be called on a thread that allows IO.
std::string FindURLMimeType(const GURL& url) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::FilePath full_path;
  net::FileURLToFilePath(url, &full_path);

  // Get the MIME type based on the filename.
  std::string mime_type;
  net::GetMimeTypeFromFile(full_path, &mime_type);

  return mime_type;
}

}  // namespace

class BrowserTabStripController::TabContextMenuContents
    : public ui::SimpleMenuModel::Delegate {
 public:
  TabContextMenuContents(Tab* tab,
                         BrowserTabStripController* controller)
      : tab_(tab),
        controller_(controller),
        last_command_(TabStripModel::CommandFirst) {
    model_.reset(new TabMenuModel(
        this, controller->model_,
        controller->tabstrip_->GetModelIndexOfTab(tab)));
    menu_runner_.reset(new views::MenuRunner(
        model_.get(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU));
  }

  virtual ~TabContextMenuContents() {
    if (controller_)
      controller_->tabstrip_->StopAllHighlighting();
  }

  void Cancel() {
    controller_ = NULL;
  }

  void RunMenuAt(const gfx::Point& point, ui::MenuSourceType source_type) {
    if (menu_runner_->RunMenuAt(tab_->GetWidget(),
                                NULL,
                                gfx::Rect(point, gfx::Size()),
                                views::MENU_ANCHOR_TOPLEFT,
                                source_type) ==
        views::MenuRunner::MENU_DELETED) {
      return;
    }
  }

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const override {
    return false;
  }
  virtual bool IsCommandIdEnabled(int command_id) const override {
    return controller_->IsCommandEnabledForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_);
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) override {
    int browser_cmd;
    return TabStripModel::ContextMenuCommandToBrowserCommand(command_id,
                                                             &browser_cmd) ?
        controller_->tabstrip_->GetWidget()->GetAccelerator(browser_cmd,
                                                            accelerator) :
        false;
  }
  virtual void CommandIdHighlighted(int command_id) override {
    controller_->StopHighlightTabsForCommand(last_command_, tab_);
    last_command_ = static_cast<TabStripModel::ContextMenuCommand>(command_id);
    controller_->StartHighlightTabsForCommand(last_command_, tab_);
  }
  virtual void ExecuteCommand(int command_id, int event_flags) override {
    // Executing the command destroys |this|, and can also end up destroying
    // |controller_|. So stop the highlights before executing the command.
    controller_->tabstrip_->StopAllHighlighting();
    controller_->ExecuteCommandForTab(
        static_cast<TabStripModel::ContextMenuCommand>(command_id),
        tab_);
  }

  virtual void MenuClosed(ui::SimpleMenuModel* /*source*/) override {
    if (controller_)
      controller_->tabstrip_->StopAllHighlighting();
  }

 private:
  scoped_ptr<TabMenuModel> model_;
  scoped_ptr<views::MenuRunner> menu_runner_;

  // The tab we're showing a menu for.
  Tab* tab_;

  // A pointer back to our hosting controller, for command state information.
  BrowserTabStripController* controller_;

  // The last command that was selected, so that we can start/stop highlighting
  // appropriately as the user moves through the menu.
  TabStripModel::ContextMenuCommand last_command_;

  DISALLOW_COPY_AND_ASSIGN(TabContextMenuContents);
};

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, public:

BrowserTabStripController::BrowserTabStripController(Browser* browser,
                                                     TabStripModel* model)
    : model_(model),
      tabstrip_(NULL),
      browser_(browser),
      hover_tab_selector_(model),
      weak_ptr_factory_(this) {
  model_->AddObserver(this);

  local_pref_registrar_.Init(g_browser_process->local_state());
  local_pref_registrar_.Add(
      prefs::kTabStripStackedLayout,
      base::Bind(&BrowserTabStripController::UpdateStackedLayout,
                 base::Unretained(this)));
}

BrowserTabStripController::~BrowserTabStripController() {
  // When we get here the TabStrip is being deleted. We need to explicitly
  // cancel the menu, otherwise it may try to invoke something on the tabstrip
  // from its destructor.
  if (context_menu_contents_.get())
    context_menu_contents_->Cancel();

  model_->RemoveObserver(this);
}

void BrowserTabStripController::InitFromModel(TabStrip* tabstrip) {
  tabstrip_ = tabstrip;

  UpdateStackedLayout();

  // Walk the model, calling our insertion observer method for each item within
  // it.
  for (int i = 0; i < model_->count(); ++i)
    AddTab(model_->GetWebContentsAt(i), i, model_->active_index() == i);
}

bool BrowserTabStripController::IsCommandEnabledForTab(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) const {
  int model_index = tabstrip_->GetModelIndexOfTab(tab);
  return model_->ContainsIndex(model_index) ?
      model_->IsContextMenuCommandEnabled(model_index, command_id) : false;
}

void BrowserTabStripController::ExecuteCommandForTab(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) {
  int model_index = tabstrip_->GetModelIndexOfTab(tab);
  if (model_->ContainsIndex(model_index))
    model_->ExecuteContextMenuCommand(model_index, command_id);
}

bool BrowserTabStripController::IsTabPinned(Tab* tab) const {
  return IsTabPinned(tabstrip_->GetModelIndexOfTab(tab));
}

const ui::ListSelectionModel& BrowserTabStripController::GetSelectionModel() {
  return model_->selection_model();
}

int BrowserTabStripController::GetCount() const {
  return model_->count();
}

bool BrowserTabStripController::IsValidIndex(int index) const {
  return model_->ContainsIndex(index);
}

bool BrowserTabStripController::IsActiveTab(int model_index) const {
  return model_->active_index() == model_index;
}

int BrowserTabStripController::GetActiveIndex() const {
  return model_->active_index();
}

bool BrowserTabStripController::IsTabSelected(int model_index) const {
  return model_->IsTabSelected(model_index);
}

bool BrowserTabStripController::IsTabPinned(int model_index) const {
  return model_->ContainsIndex(model_index) && model_->IsTabPinned(model_index);
}

bool BrowserTabStripController::IsNewTabPage(int model_index) const {
  if (!model_->ContainsIndex(model_index))
    return false;

  const WebContents* contents = model_->GetWebContentsAt(model_index);
  return contents && (contents->GetURL() == GURL(chrome::kChromeUINewTabURL) ||
      chrome::IsInstantNTP(contents));
}

void BrowserTabStripController::SelectTab(int model_index) {
  model_->ActivateTabAt(model_index, true);
}

void BrowserTabStripController::ExtendSelectionTo(int model_index) {
  model_->ExtendSelectionTo(model_index);
}

void BrowserTabStripController::ToggleSelected(int model_index) {
  model_->ToggleSelectionAt(model_index);
}

void BrowserTabStripController::AddSelectionFromAnchorTo(int model_index) {
  model_->AddSelectionFromAnchorTo(model_index);
}

void BrowserTabStripController::CloseTab(int model_index,
                                         CloseTabSource source) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  tabstrip_->PrepareForCloseAt(model_index, source);
  model_->CloseWebContentsAt(model_index,
                             TabStripModel::CLOSE_USER_GESTURE |
                             TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
}

void BrowserTabStripController::ToggleTabAudioMute(int model_index) {
  content::WebContents* const contents = model_->GetWebContentsAt(model_index);
  chrome::SetTabAudioMuted(contents, !chrome::IsTabAudioMuted(contents));
}

void BrowserTabStripController::ShowContextMenuForTab(
    Tab* tab,
    const gfx::Point& p,
    ui::MenuSourceType source_type) {
  context_menu_contents_.reset(new TabContextMenuContents(tab, this));
  context_menu_contents_->RunMenuAt(p, source_type);
}

void BrowserTabStripController::UpdateLoadingAnimations() {
  // Don't use the model count here as it's possible for this to be invoked
  // before we've applied an update from the model (Browser::TabInsertedAt may
  // be processed before us and invokes this).
  for (int i = 0, tab_count = tabstrip_->tab_count(); i < tab_count; ++i) {
    if (model_->ContainsIndex(i)) {
      Tab* tab = tabstrip_->tab_at(i);
      WebContents* contents = model_->GetWebContentsAt(i);
      tab->UpdateLoadingAnimation(TabContentsNetworkState(contents));
    }
  }
}

int BrowserTabStripController::HasAvailableDragActions() const {
  return model_->delegate()->GetDragActions();
}

void BrowserTabStripController::OnDropIndexUpdate(int index,
                                                  bool drop_before) {
  // Perform a delayed tab transition if hovering directly over a tab.
  // Otherwise, cancel the pending one.
  if (index != -1 && !drop_before) {
    hover_tab_selector_.StartTabTransition(index);
  } else {
    hover_tab_selector_.CancelTabTransition();
  }
}

void BrowserTabStripController::PerformDrop(bool drop_before,
                                            int index,
                                            const GURL& url) {
  chrome::NavigateParams params(browser_, url, ui::PAGE_TRANSITION_LINK);
  params.tabstrip_index = index;

  if (drop_before) {
    content::RecordAction(UserMetricsAction("Tab_DropURLBetweenTabs"));
    params.disposition = NEW_FOREGROUND_TAB;
  } else {
    content::RecordAction(UserMetricsAction("Tab_DropURLOnTab"));
    params.disposition = CURRENT_TAB;
    params.source_contents = model_->GetWebContentsAt(index);
  }
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);
}

bool BrowserTabStripController::IsCompatibleWith(TabStrip* other) const {
  Profile* other_profile =
      static_cast<BrowserTabStripController*>(other->controller())->profile();
  return other_profile == profile();
}

void BrowserTabStripController::CreateNewTab() {
  model_->delegate()->AddTabAt(GURL(), -1, true);
}

void BrowserTabStripController::CreateNewTabWithLocation(
    const base::string16& location) {
  // Use autocomplete to clean up the text, going so far as to turn it into
  // a search query if necessary.
  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(profile())->Classify(
      location, false, false, metrics::OmniboxEventProto::BLANK, &match, NULL);
  if (match.destination_url.is_valid())
    model_->delegate()->AddTabAt(match.destination_url, -1, true);
}

bool BrowserTabStripController::IsIncognito() {
  return browser_->profile()->IsOffTheRecord();
}

void BrowserTabStripController::StackedLayoutMaybeChanged() {
  bool adjust_layout = false;
  bool stacked_layout =
      DetermineTabStripLayoutStacked(g_browser_process->local_state(),
                                     browser_->host_desktop_type(),
                                     &adjust_layout);
  if (!adjust_layout || stacked_layout == tabstrip_->stacked_layout())
    return;

  g_browser_process->local_state()->SetBoolean(prefs::kTabStripStackedLayout,
                                               tabstrip_->stacked_layout());
}

void BrowserTabStripController::OnStartedDraggingTabs() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (browser_view && !immersive_reveal_lock_.get()) {
    // The top-of-window views should be revealed while the user is dragging
    // tabs in immersive fullscreen. The top-of-window views may not be already
    // revealed if the user is attempting to attach a tab to a tabstrip
    // belonging to an immersive fullscreen window.
    immersive_reveal_lock_.reset(
        browser_view->immersive_mode_controller()->GetRevealedLock(
            ImmersiveModeController::ANIMATE_REVEAL_NO));
  }
}

void BrowserTabStripController::OnStoppedDraggingTabs() {
  immersive_reveal_lock_.reset();
}

void BrowserTabStripController::CheckFileSupported(const GURL& url) {
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&FindURLMimeType, url),
      base::Bind(&BrowserTabStripController::OnFindURLMimeTypeCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 url));
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripController, TabStripModelObserver implementation:

void BrowserTabStripController::TabInsertedAt(WebContents* contents,
                                              int model_index,
                                              bool is_active) {
  DCHECK(contents);
  DCHECK(model_->ContainsIndex(model_index));
  AddTab(contents, model_index, is_active);
}

void BrowserTabStripController::TabDetachedAt(WebContents* contents,
                                              int model_index) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  tabstrip_->RemoveTabAt(model_index);
}

void BrowserTabStripController::TabSelectionChanged(
    TabStripModel* tab_strip_model,
    const ui::ListSelectionModel& old_model) {
  tabstrip_->SetSelection(old_model, model_->selection_model());
}

void BrowserTabStripController::TabMoved(WebContents* contents,
                                         int from_model_index,
                                         int to_model_index) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  // Pass in the TabRendererData as the pinned state may have changed.
  TabRendererData data;
  SetTabRendererDataFromModel(contents, to_model_index, &data, EXISTING_TAB);
  tabstrip_->MoveTab(from_model_index, to_model_index, data);
}

void BrowserTabStripController::TabChangedAt(WebContents* contents,
                                             int model_index,
                                             TabChangeType change_type) {
  if (change_type == TITLE_NOT_LOADING) {
    tabstrip_->TabTitleChangedNotLoading(model_index);
    // We'll receive another notification of the change asynchronously.
    return;
  }

  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::TabReplacedAt(TabStripModel* tab_strip_model,
                                              WebContents* old_contents,
                                              WebContents* new_contents,
                                              int model_index) {
  SetTabDataAt(new_contents, model_index);
}

void BrowserTabStripController::TabPinnedStateChanged(WebContents* contents,
                                                      int model_index) {
  // Currently none of the renderers render pinned state differently.
}

void BrowserTabStripController::TabMiniStateChanged(WebContents* contents,
                                                    int model_index) {
  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::TabBlockedStateChanged(WebContents* contents,
                                                       int model_index) {
  SetTabDataAt(contents, model_index);
}

void BrowserTabStripController::SetTabRendererDataFromModel(
    WebContents* contents,
    int model_index,
    TabRendererData* data,
    TabStatus tab_status) {
  FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(contents);

  data->favicon = favicon_tab_helper->GetFavicon().AsImageSkia();
  data->network_state = TabContentsNetworkState(contents);
  data->title = contents->GetTitle();
  data->url = contents->GetURL();
  data->loading = contents->IsLoading();
  data->crashed_status = contents->GetCrashedStatus();
  data->incognito = contents->GetBrowserContext()->IsOffTheRecord();
  data->mini = model_->IsMiniTab(model_index);
  data->show_icon = data->mini || favicon_tab_helper->ShouldDisplayFavicon();
  data->blocked = model_->IsTabBlocked(model_index);
  data->app = extensions::TabHelper::FromWebContents(contents)->is_app();
  data->media_state = chrome::GetTabMediaStateForContents(contents);
}

void BrowserTabStripController::SetTabDataAt(content::WebContents* web_contents,
                                             int model_index) {
  TabRendererData data;
  SetTabRendererDataFromModel(web_contents, model_index, &data, EXISTING_TAB);
  tabstrip_->SetTabData(model_index, data);
}

void BrowserTabStripController::StartHighlightTabsForCommand(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) {
  if (command_id == TabStripModel::CommandCloseOtherTabs ||
      command_id == TabStripModel::CommandCloseTabsToRight) {
    int model_index = tabstrip_->GetModelIndexOfTab(tab);
    if (IsValidIndex(model_index)) {
      std::vector<int> indices =
          model_->GetIndicesClosedByCommand(model_index, command_id);
      for (std::vector<int>::const_iterator i(indices.begin());
           i != indices.end(); ++i) {
        tabstrip_->StartHighlight(*i);
      }
    }
  }
}

void BrowserTabStripController::StopHighlightTabsForCommand(
    TabStripModel::ContextMenuCommand command_id,
    Tab* tab) {
  if (command_id == TabStripModel::CommandCloseTabsToRight ||
      command_id == TabStripModel::CommandCloseOtherTabs) {
    // Just tell all Tabs to stop pulsing - it's safe.
    tabstrip_->StopAllHighlighting();
  }
}

void BrowserTabStripController::AddTab(WebContents* contents,
                                       int index,
                                       bool is_active) {
  // Cancel any pending tab transition.
  hover_tab_selector_.CancelTabTransition();

  TabRendererData data;
  SetTabRendererDataFromModel(contents, index, &data, NEW_TAB);
  tabstrip_->AddTabAt(index, data, is_active);
}

void BrowserTabStripController::UpdateStackedLayout() {
  bool adjust_layout = false;
  bool stacked_layout =
      DetermineTabStripLayoutStacked(g_browser_process->local_state(),
                                     browser_->host_desktop_type(),
                                     &adjust_layout);
  tabstrip_->set_adjust_layout(adjust_layout);
  tabstrip_->SetStackedLayout(stacked_layout);
}

void BrowserTabStripController::OnFindURLMimeTypeCompleted(
    const GURL& url,
    const std::string& mime_type) {
  // Check whether the mime type, if given, is known to be supported or whether
  // there is a plugin that supports the mime type (e.g. PDF).
  // TODO(bauerb): This possibly uses stale information, but it's guaranteed not
  // to do disk access.
  content::WebPluginInfo plugin;
  tabstrip_->FileSupported(
      url,
      mime_type.empty() ||
      net::IsSupportedMimeType(mime_type) ||
      content::PluginService::GetInstance()->GetPluginInfo(
          -1,                // process ID
          MSG_ROUTING_NONE,  // routing ID
          model_->profile()->GetResourceContext(),
          url, GURL(), mime_type, false,
          NULL, &plugin, NULL));
}
