// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_interceptor.h"

#include "base/time/time.h"
#include "chrome/browser/devtools/devtools_network_conditions.h"
#include "chrome/browser/devtools/devtools_network_transaction.h"

namespace {

int64_t kPacketSize = 1500;

}  // namespace

DevToolsNetworkInterceptor::DevToolsNetworkInterceptor()
    : conditions_(new DevToolsNetworkConditions()),
      weak_ptr_factory_(this) {
}

DevToolsNetworkInterceptor::~DevToolsNetworkInterceptor() {
}

base::WeakPtr<DevToolsNetworkInterceptor>
DevToolsNetworkInterceptor::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DevToolsNetworkInterceptor::AddTransaction(
    DevToolsNetworkTransaction* transaction) {
  DCHECK(transactions_.find(transaction) == transactions_.end());
  transactions_.insert(transaction);
}

void DevToolsNetworkInterceptor::RemoveTransaction(
    DevToolsNetworkTransaction* transaction) {
  DCHECK(transactions_.find(transaction) != transactions_.end());
  transactions_.erase(transaction);

  if (!conditions_->IsThrottling())
    return;

  UpdateThrottles();
  throttled_transactions_.erase(std::remove(throttled_transactions_.begin(),
      throttled_transactions_.end(), transaction),
      throttled_transactions_.end());
  ArmTimer();
}

void DevToolsNetworkInterceptor::UpdateConditions(
    const scoped_refptr<DevToolsNetworkConditions> conditions) {
  DCHECK(conditions);
  if (conditions_->IsThrottling())
    UpdateThrottles();

  conditions_ = conditions;

  if (conditions->offline()) {
    timer_.Stop();
    throttled_transactions_.clear();
    Transactions old_transactions(transactions_);
    Transactions::iterator it = old_transactions.begin();
    for (;it != old_transactions.end(); ++it) {
      if (transactions_.find(*it) == transactions_.end())
        continue;
      if (!(*it)->request() || (*it)->failed())
        continue;
      if (ShouldFail(*it))
        (*it)->Fail();
    }
    return;
  }

  if (conditions->IsThrottling()) {
    DCHECK(conditions->download_throughput() != 0);
    offset_ = base::TimeTicks::Now();
    last_tick_ = 0;
    int64_t us_tick_length =
        (1000000L * kPacketSize) / conditions->download_throughput();
    DCHECK(us_tick_length != 0);
    if (us_tick_length == 0)
      us_tick_length = 1;
    tick_length_ = base::TimeDelta::FromMicroseconds(us_tick_length);
    ArmTimer();
  } else {
    timer_.Stop();
    int64_t length = throttled_transactions_.size();
    for (int64_t i = 0; i < length; ++i)
        throttled_transactions_[i]->FireThrottledCallback();
    throttled_transactions_.clear();
  }
}

void DevToolsNetworkInterceptor::UpdateThrottles() {
  int64_t last_tick = (base::TimeTicks::Now() - offset_) / tick_length_;
  int64_t ticks = last_tick - last_tick_;
  last_tick_ = last_tick;

  int64_t length = throttled_transactions_.size();
  if (!length)
    return;

  int64_t shift = ticks % length;
  for (int64_t i = 0; i < length; ++i) {
    throttled_transactions_[i]->DecreaseThrottledByteCount(
        (ticks / length) * kPacketSize + (i < shift ? kPacketSize : 0));
  }
  std::rotate(throttled_transactions_.begin(),
      throttled_transactions_.begin() + shift, throttled_transactions_.end());
}

void DevToolsNetworkInterceptor::OnTimer() {
  UpdateThrottles();

  std::vector<DevToolsNetworkTransaction*> active_transactions;
  std::vector<DevToolsNetworkTransaction*> finished_transactions;
  size_t length = throttled_transactions_.size();
  for (size_t i = 0; i < length; ++i) {
    if (throttled_transactions_[i]->throttled_byte_count() < 0)
      finished_transactions.push_back(throttled_transactions_[i]);
    else
      active_transactions.push_back(throttled_transactions_[i]);
  }
  throttled_transactions_.swap(active_transactions);

  length = finished_transactions.size();
  for (size_t i = 0; i < length; ++i)
      finished_transactions[i]->FireThrottledCallback();

  ArmTimer();
}

void DevToolsNetworkInterceptor::ArmTimer() {
  size_t length = throttled_transactions_.size();
  if (!length)
    return;
  int64_t min_ticks_left = 0x10000L;
  for (size_t i = 0; i < length; ++i) {
    int64_t packets_left = (throttled_transactions_[i]->throttled_byte_count() +
        kPacketSize - 1) / kPacketSize;
    int64_t ticks_left = (i + 1) + length * (packets_left - 1);
    if (i == 0 || ticks_left < min_ticks_left)
      min_ticks_left = ticks_left;
  }
  base::TimeTicks desired_time =
      offset_ + tick_length_ * (last_tick_ + min_ticks_left);
  timer_.Start(
      FROM_HERE,
      desired_time - base::TimeTicks::Now(),
      base::Bind(
          &DevToolsNetworkInterceptor::OnTimer,
          base::Unretained(this)));
}

void DevToolsNetworkInterceptor::ThrottleTransaction(
    DevToolsNetworkTransaction* transaction) {
  UpdateThrottles();
  throttled_transactions_.push_back(transaction);
  ArmTimer();
}

bool DevToolsNetworkInterceptor::ShouldFail(
    const DevToolsNetworkTransaction* transaction) {
  if (!conditions_->offline())
    return false;

  if (!transaction->request_initiator().empty())
    return false;

  return true;
}

bool DevToolsNetworkInterceptor::ShouldThrottle(
    const DevToolsNetworkTransaction* transaction) {
  if (!conditions_->IsThrottling())
    return false;

  if (!transaction->request_initiator().empty())
    return false;

  return true;
}
