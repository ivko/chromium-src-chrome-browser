// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/wildcard_login_checker.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "net/url_request/url_request_context_getter.h"

namespace policy {

namespace {

// Presence of this key in the userinfo response indicates whether the user is
// on a hosted domain.
const char kHostedDomainKey[] = "hd";

// UMA histogram names.
const char kUMADelayPolicyTokenFetch[] =
    "Enterprise.WildcardLoginCheck.DelayPolicyTokenFetch";
const char kUMADelayUserInfoFetch[] =
    "Enterprise.WildcardLoginCheck.DelayUserInfoFetch";
const char kUMADelayTotal[] =
    "Enterprise.WildcardLoginCheck.DelayTotal";

}  // namespace

WildcardLoginChecker::WildcardLoginChecker() {}

WildcardLoginChecker::~WildcardLoginChecker() {}

void WildcardLoginChecker::Start(
    scoped_refptr<net::URLRequestContextGetter> signin_context,
    const StatusCallback& callback) {
  CHECK(!token_fetcher_);
  CHECK(!user_info_fetcher_);

  start_timestamp_ = base::Time::Now();

  callback_ = callback;
  token_fetcher_.reset(new PolicyOAuth2TokenFetcher(
      signin_context,
      g_browser_process->system_request_context(),
      base::Bind(&WildcardLoginChecker::OnPolicyTokenFetched,
                 base::Unretained(this))));
  token_fetcher_->Start();
}

void WildcardLoginChecker::StartWithAccessToken(
    const std::string& access_token,
    const StatusCallback& callback) {
  CHECK(!token_fetcher_);
  CHECK(!user_info_fetcher_);
  callback_ = callback;

  StartUserInfoFetcher(access_token);
}

void WildcardLoginChecker::OnGetUserInfoSuccess(
    const base::DictionaryValue* response) {
  if (!start_timestamp_.is_null()) {
    base::Time now = base::Time::Now();
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMADelayUserInfoFetch,
                               now - token_available_timestamp_);
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMADelayTotal,
                               now - start_timestamp_);
  }

  OnCheckCompleted(response->HasKey(kHostedDomainKey));
}

void WildcardLoginChecker::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "Failed to fetch user info " << error.ToString();
  OnCheckCompleted(false);
}

void WildcardLoginChecker::OnPolicyTokenFetched(
    const std::string& access_token,
    const GoogleServiceAuthError& error) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Failed to fetch policy token " << error.ToString();
    OnCheckCompleted(false);
    return;
  }

  if (!start_timestamp_.is_null()) {
    token_available_timestamp_ = base::Time::Now();
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMADelayPolicyTokenFetch,
                               token_available_timestamp_ - start_timestamp_);
  }

  token_fetcher_.reset();
  StartUserInfoFetcher(access_token);
}

void WildcardLoginChecker::StartUserInfoFetcher(
    const std::string& access_token) {
  user_info_fetcher_.reset(
      new UserInfoFetcher(this, g_browser_process->system_request_context()));
  user_info_fetcher_->Start(access_token);
}

void WildcardLoginChecker::OnCheckCompleted(bool result) {
  if (!callback_.is_null())
    callback_.Run(result);
}

}  // namespace policy
