// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/test/test_server.h"

using std::wstring;

namespace {

const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");

}  // namespace

class LoginPromptTest : public UITest {
 protected:
  LoginPromptTest()
      : username_basic_(L"basicuser"),
        username_digest_(L"digestuser"),
        password_(L"secret"),
        password_bad_(L"denyme"),
        test_server_(net::TestServer::TYPE_HTTP, FilePath(kDocRoot)) {
  }

  void AppendTab(const GURL& url) {
    scoped_refptr<BrowserProxy> window_proxy(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(window_proxy.get());
    ASSERT_TRUE(window_proxy->AppendTab(url));
  }

 protected:
  wstring username_basic_;
  wstring username_digest_;
  wstring password_;
  wstring password_bad_;

  net::TestServer test_server_;
};

wstring ExpectedTitleFromAuth(const wstring& username,
                              const wstring& password) {
  // The TestServer sets the title to username/password on successful login.
  return username + L"/" + password;
}

// Test that "Basic" HTTP authentication works.
TEST_F(LoginPromptTest, TestBasicAuth) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-basic")));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_FALSE(tab->SetAuth(username_basic_, password_bad_));
  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->CancelAuth());
  EXPECT_EQ(L"Denied: wrong password", GetActiveTabTitle());

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-basic")));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->SetAuth(username_basic_, password_));
  EXPECT_EQ(ExpectedTitleFromAuth(username_basic_, password_),
            GetActiveTabTitle());
}

// Test that "Digest" HTTP authentication works.
TEST_F(LoginPromptTest, TestDigestAuth) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-digest")));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_FALSE(tab->SetAuth(username_digest_, password_bad_));
  EXPECT_TRUE(tab->CancelAuth());
  EXPECT_EQ(L"Denied: wrong password", GetActiveTabTitle());

  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            tab->NavigateToURL(test_server_.GetURL("auth-digest")));

  EXPECT_TRUE(tab->NeedsAuth());
  EXPECT_TRUE(tab->SetAuth(username_digest_, password_));
  EXPECT_EQ(ExpectedTitleFromAuth(username_digest_, password_),
            GetActiveTabTitle());
}

// Test that logging in on 2 tabs at once works.
TEST_F(LoginPromptTest, TestTwoAuths) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> basic_tab(GetActiveTab());
  ASSERT_TRUE(basic_tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            basic_tab->NavigateToURL(test_server_.GetURL("auth-basic")));

  AppendTab(GURL(chrome::kAboutBlankURL));
  scoped_refptr<TabProxy> digest_tab(GetActiveTab());
  ASSERT_TRUE(digest_tab.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            digest_tab->NavigateToURL(test_server_.GetURL("auth-digest")));

  EXPECT_TRUE(basic_tab->NeedsAuth());
  EXPECT_TRUE(basic_tab->SetAuth(username_basic_, password_));
  EXPECT_TRUE(digest_tab->NeedsAuth());
  EXPECT_TRUE(digest_tab->SetAuth(username_digest_, password_));

  wstring title;
  EXPECT_TRUE(basic_tab->GetTabTitle(&title));
  EXPECT_EQ(ExpectedTitleFromAuth(username_basic_, password_), title);

  EXPECT_TRUE(digest_tab->GetTabTitle(&title));
  EXPECT_EQ(ExpectedTitleFromAuth(username_digest_, password_), title);
}

// If multiple tabs are looking for the same auth, the user should only have to
// enter it once.
TEST_F(LoginPromptTest, SupplyRedundantAuths) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> basic_tab1(GetActiveTab());
  ASSERT_TRUE(basic_tab1.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            basic_tab1->NavigateToURL(test_server_.GetURL("auth-basic/1")));
  EXPECT_TRUE(basic_tab1->NeedsAuth());

  AppendTab(GURL(chrome::kAboutBlankURL));
  scoped_refptr<TabProxy> basic_tab2(GetActiveTab());
  ASSERT_TRUE(basic_tab2.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            basic_tab2->NavigateToURL(test_server_.GetURL("auth-basic/2")));
  EXPECT_TRUE(basic_tab2->NeedsAuth());

  // Set the auth in only one of the tabs (but wait for the other to load).
  int64 last_navigation_time;
  ASSERT_TRUE(basic_tab2->GetLastNavigationTime(&last_navigation_time));
  EXPECT_TRUE(basic_tab1->SetAuth(username_basic_, password_));
  EXPECT_TRUE(basic_tab2->WaitForNavigation(last_navigation_time));

  // Now both tabs have loaded.
  wstring title1;
  EXPECT_TRUE(basic_tab1->GetTabTitle(&title1));
  EXPECT_EQ(ExpectedTitleFromAuth(username_basic_, password_), title1);
  wstring title2;
  EXPECT_TRUE(basic_tab2->GetTabTitle(&title2));
  EXPECT_EQ(ExpectedTitleFromAuth(username_basic_, password_), title2);
}

// If multiple tabs are looking for the same auth, and one is cancelled, the
// other should be cancelled as well.
TEST_F(LoginPromptTest, CancelRedundantAuths) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<TabProxy> basic_tab1(GetActiveTab());
  ASSERT_TRUE(basic_tab1.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            basic_tab1->NavigateToURL(test_server_.GetURL("auth-basic/1")));
  EXPECT_TRUE(basic_tab1->NeedsAuth());

  AppendTab(GURL(chrome::kAboutBlankURL));
  scoped_refptr<TabProxy> basic_tab2(GetActiveTab());
  ASSERT_TRUE(basic_tab2.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
            basic_tab2->NavigateToURL(test_server_.GetURL("auth-basic/2")));
  EXPECT_TRUE(basic_tab2->NeedsAuth());

  // Cancel the auth in only one of the tabs (but wait for the other to load).
  int64 last_navigation_time;
  ASSERT_TRUE(basic_tab2->GetLastNavigationTime(&last_navigation_time));
  EXPECT_TRUE(basic_tab1->CancelAuth());
  EXPECT_TRUE(basic_tab2->WaitForNavigation(last_navigation_time));

  // Now both tabs have been denied.
  wstring title1;
  EXPECT_TRUE(basic_tab1->GetTabTitle(&title1));
  EXPECT_EQ(L"Denied: no auth", title1);
  wstring title2;
  EXPECT_TRUE(basic_tab2->GetTabTitle(&title2));
  EXPECT_EQ(L"Denied: no auth", title2);
}
