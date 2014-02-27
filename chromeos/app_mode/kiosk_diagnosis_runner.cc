// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_diagnosis_runner.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace chromeos {

class KioskDiagnosisRunner::Factory : public BrowserContextKeyedServiceFactory {
 public:
  static KioskDiagnosisRunner* GetForProfile(Profile* profile) {
    return static_cast<KioskDiagnosisRunner*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static Factory* GetInstance() {
    return Singleton<Factory>::get();
  }

 private:
  friend struct DefaultSingletonTraits<Factory>;

  Factory()
      : BrowserContextKeyedServiceFactory(
            "KioskDiagnosisRunner",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(extensions::ExtensionsBrowserClient::Get()
                  ->GetExtensionSystemFactory());
    DependsOn(extensions::FeedbackPrivateAPI::GetFactoryInstance());
  }

  virtual ~Factory() {}

  // BrowserContextKeyedServiceFactory overrides:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE {
    Profile* profile = static_cast<Profile*>(context);
    return new KioskDiagnosisRunner(profile);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Factory);
};

// static
void KioskDiagnosisRunner::Run(Profile* profile,
                               const std::string& app_id) {
  KioskDiagnosisRunner::Factory::GetForProfile(profile)->Start(app_id);
}

KioskDiagnosisRunner::KioskDiagnosisRunner(Profile* profile)
    : profile_(profile),
      weak_factory_(this) {}

KioskDiagnosisRunner::~KioskDiagnosisRunner() {}

void KioskDiagnosisRunner::Start(const std::string& app_id) {
  app_id_ = app_id;

  // Schedules system logs to be collected after 1 minute.
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&KioskDiagnosisRunner::StartSystemLogCollection,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMinutes(1));
}

void KioskDiagnosisRunner::StartSystemLogCollection() {
  extensions::FeedbackService* service =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()
          ->GetForProfile(profile_)
          ->GetService();
  DCHECK(service);

  service->GetSystemInformation(
      base::Bind(&KioskDiagnosisRunner::SendSysLogFeedback,
                 weak_factory_.GetWeakPtr()));
}

void KioskDiagnosisRunner::SendSysLogFeedback(
    const extensions::SystemInformationList& sys_info) {
  scoped_refptr<FeedbackData> feedback_data(new FeedbackData());

  feedback_data->set_profile(profile_);
  feedback_data->set_description(base::StringPrintf(
      "Autogenerated feedback:\nAppId: %s\n(uniquifier:%s)",
      app_id_.c_str(),
      base::Int64ToString(base::Time::Now().ToInternalValue()).c_str()));

  scoped_ptr<FeedbackData::SystemLogsMap> sys_logs(
      new FeedbackData::SystemLogsMap);
  for (extensions::SystemInformationList::const_iterator it = sys_info.begin();
       it != sys_info.end(); ++it) {
    (*sys_logs.get())[it->get()->key] = it->get()->value;
  }
  feedback_data->SetAndCompressSystemInfo(sys_logs.Pass());

  extensions::FeedbackService* service =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()
          ->GetForProfile(profile_)
          ->GetService();
  DCHECK(service);
  service->SendFeedback(profile_,
                        feedback_data,
                        base::Bind(&KioskDiagnosisRunner::OnFeedbackSent,
                                   weak_factory_.GetWeakPtr()));
}

void KioskDiagnosisRunner::OnFeedbackSent(bool) {
  // Do nothing.
}

}  // namespace chromeos
