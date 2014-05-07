// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/guest_view_manager.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/guest_view/guest_view_base.h"
#include "chrome/browser/guest_view/guest_view_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/result_codes.h"
#include "extensions/browser/extension_system.h"
#include "url/gurl.h"

using content::BrowserContext;
using content::SiteInstance;
using content::WebContents;

// A WebContents does not immediately have a RenderProcessHost. It acquires one
// on initial navigation. This observer exists until that initial navigation in
// order to grab the ID if tis RenderProcessHost so that it can register it as
// a guest.
class GuestWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit GuestWebContentsObserver(WebContents* guest_web_contents)
      : WebContentsObserver(guest_web_contents) {
  }

  virtual ~GuestWebContentsObserver() {
  }

  // WebContentsObserver:
  virtual void DidStartProvisionalLoadForFrame(
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc,
      content::RenderViewHost* render_view_host) OVERRIDE {
    GuestViewManager::FromBrowserContext(web_contents()->GetBrowserContext())->
        AddRenderProcessHostID(web_contents()->GetRenderProcessHost()->GetID());
    delete this;
  }

  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE {
    delete this;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestWebContentsObserver);
};

GuestViewManager::GuestViewManager(content::BrowserContext* context)
    : current_instance_id_(0),
      context_(context) {}

GuestViewManager::~GuestViewManager() {}

// static.
GuestViewManager* GuestViewManager::FromBrowserContext(
    BrowserContext* context) {
  GuestViewManager* guest_manager =
      static_cast<GuestViewManager*>(context->GetUserData(
          guestview::kGuestViewManagerKeyName));
  if (!guest_manager) {
    guest_manager = new GuestViewManager(context);
    context->SetUserData(guestview::kGuestViewManagerKeyName, guest_manager);
  }
  return guest_manager;
}

content::WebContents* GuestViewManager::GetGuestByInstanceIDSafely(
    int guest_instance_id,
    int embedder_render_process_id) {
  if (!CanEmbedderAccessInstanceIDMaybeKill(embedder_render_process_id,
                                            guest_instance_id)) {
    return NULL;
  }
  return GetGuestByInstanceID(guest_instance_id, embedder_render_process_id);
}

int GuestViewManager::GetNextInstanceID() {
  return ++current_instance_id_;
}

void GuestViewManager::AddGuest(int guest_instance_id,
                                WebContents* guest_web_contents) {
  DCHECK(guest_web_contents_by_instance_id_.find(guest_instance_id) ==
         guest_web_contents_by_instance_id_.end());
  guest_web_contents_by_instance_id_[guest_instance_id] = guest_web_contents;
  // This will add the RenderProcessHost ID when we get one.
  new GuestWebContentsObserver(guest_web_contents);
}

void GuestViewManager::RemoveGuest(int guest_instance_id) {
  GuestInstanceMap::iterator it =
      guest_web_contents_by_instance_id_.find(guest_instance_id);
  DCHECK(it != guest_web_contents_by_instance_id_.end());
  render_process_host_id_multiset_.erase(
      it->second->GetRenderProcessHost()->GetID());
  guest_web_contents_by_instance_id_.erase(it);
}

void GuestViewManager::MaybeGetGuestByInstanceIDOrKill(
    int guest_instance_id,
    int embedder_render_process_id,
    const GuestByInstanceIDCallback& callback) {
  if (!CanEmbedderAccessInstanceIDMaybeKill(embedder_render_process_id,
                                            guest_instance_id)) {
    // If we kill the embedder, then don't bother calling back.
    return;
  }
  content::WebContents* guest_web_contents =
      GetGuestByInstanceID(guest_instance_id, embedder_render_process_id);
  callback.Run(guest_web_contents);
}

SiteInstance* GuestViewManager::GetGuestSiteInstance(
    const GURL& guest_site) {
  for (GuestInstanceMap::const_iterator it =
       guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    if (it->second->GetSiteInstance()->GetSiteURL() == guest_site)
      return it->second->GetSiteInstance();
  }
  return NULL;
}

bool GuestViewManager::ForEachGuest(WebContents* embedder_web_contents,
                                    const GuestCallback& callback) {
  for (GuestInstanceMap::iterator it =
           guest_web_contents_by_instance_id_.begin();
       it != guest_web_contents_by_instance_id_.end(); ++it) {
    WebContents* guest = it->second;
    if (embedder_web_contents != guest->GetEmbedderWebContents())
      continue;

    if (callback.Run(guest))
      return true;
  }
  return false;
}

void GuestViewManager::AddRenderProcessHostID(int render_process_host_id) {
  render_process_host_id_multiset_.insert(render_process_host_id);
}

content::WebContents* GuestViewManager::GetGuestByInstanceID(
    int guest_instance_id,
    int embedder_render_process_id) {
  GuestInstanceMap::const_iterator it =
      guest_web_contents_by_instance_id_.find(guest_instance_id);
  if (it == guest_web_contents_by_instance_id_.end())
    return NULL;
  return it->second;
}

bool GuestViewManager::CanEmbedderAccessInstanceIDMaybeKill(
    int embedder_render_process_id,
    int guest_instance_id) {
  if (!CanEmbedderAccessInstanceID(embedder_render_process_id,
                                  guest_instance_id)) {
    // The embedder process is trying to access a guest it does not own.
    content::RecordAction(
        base::UserMetricsAction("BadMessageTerminate_BPGM"));
    base::KillProcess(
        content::RenderProcessHost::FromID(embedder_render_process_id)->
            GetHandle(),
        content::RESULT_CODE_KILLED_BAD_MESSAGE, false);
    return false;
  }
  return true;
}

bool GuestViewManager::CanEmbedderAccessInstanceID(
    int embedder_render_process_id,
    int guest_instance_id) {
  // The embedder is trying to access a guest with a negative or zero
  // instance ID.
  if (guest_instance_id <= guestview::kInstanceIDNone)
    return false;

  // The embedder is trying to access an instance ID that has not yet been
  // allocated by GuestViewManager. This could cause instance ID
  // collisions in the future, and potentially give one embedder access to a
  // guest it does not own.
  if (guest_instance_id > current_instance_id_)
    return false;

  GuestInstanceMap::const_iterator it =
      guest_web_contents_by_instance_id_.find(guest_instance_id);
  if (it == guest_web_contents_by_instance_id_.end())
    return true;

  GuestViewBase* guest_view = GuestViewBase::FromWebContents(it->second);
  if (!guest_view)
    return false;

  return CanEmbedderAccessGuest(embedder_render_process_id, guest_view);
}

bool GuestViewManager::CanEmbedderAccessGuest(int embedder_render_process_id,
                                              GuestViewBase* guest) {
  // The embedder can access the guest if it has not been attached and its
  // opener's embedder lives in the same process as the given embedder.
  if (!guest->attached()) {
    if (!guest->GetOpener())
      return false;

    return embedder_render_process_id ==
        guest->GetOpener()->GetEmbedderWebContents()->GetRenderProcessHost()->
            GetID();
  }

  return embedder_render_process_id ==
      guest->embedder_web_contents()->GetRenderProcessHost()->GetID();
}
