// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_COUNTING_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_COUNTING_POLICY_H_

#include <string>

#include "base/containers/hash_tables.h"
#include "chrome/browser/extensions/activity_log/activity_database.h"
#include "chrome/browser/extensions/activity_log/activity_log_policy.h"
#include "chrome/browser/extensions/activity_log/database_string_table.h"

namespace extensions {

// A policy for logging the stream of actions, but without arguments.
class CountingPolicy : public ActivityLogDatabasePolicy {
 public:
  explicit CountingPolicy(Profile* profile);
  virtual ~CountingPolicy();

  virtual void ProcessAction(scoped_refptr<Action> action) OVERRIDE;

  virtual void ReadFilteredData(
      const std::string& extension_id,
      const Action::ActionType type,
      const std::string& api_name,
      const std::string& page_url,
      const std::string& arg_url,
      const int days_ago,
      const base::Callback
          <void(scoped_ptr<Action::ActionVector>)>& callback) OVERRIDE;

  virtual void Close() OVERRIDE;

  // Gets or sets the amount of time that old records are kept in the database.
  const base::TimeDelta& retention_time() const { return retention_time_; }
  void set_retention_time(const base::TimeDelta& delta) {
    retention_time_ = delta;
  }

  // Clean the URL data stored for this policy.
  virtual void RemoveURLs(const std::vector<GURL>&) OVERRIDE;

  // Delete everything in the database.
  virtual void DeleteDatabase() OVERRIDE;

  // The main database table, and the name for a read-only view that
  // decompresses string values for easier parsing.
  static const char* kTableName;
  static const char* kReadViewName;

 protected:
  // The ActivityDatabase::Delegate interface.  These are always called from
  // the database thread.
  virtual bool InitDatabase(sql::Connection* db) OVERRIDE;
  virtual bool FlushDatabase(sql::Connection* db) OVERRIDE;
  virtual void OnDatabaseFailure() OVERRIDE;
  virtual void OnDatabaseClose() OVERRIDE;

 private:
  // A type used to track pending writes to the database.  The key is an action
  // to write; the value is the amount by which the count field should be
  // incremented in the database.
  typedef std::map<scoped_refptr<Action>, int, ActionComparatorExcludingTime>
      ActionQueue;

  // Adds an Action to those to be written out; this is an internal method used
  // by ProcessAction and is called on the database thread.
  void QueueAction(scoped_refptr<Action> action);

  // Internal method to read data from the database; called on the database
  // thread.
  scoped_ptr<Action::ActionVector> DoReadFilteredData(
      const std::string& extension_id,
      const Action::ActionType type,
      const std::string& api_name,
      const std::string& page_url,
      const std::string& arg_url,
      const int days_ago);

  // The implementation of RemoveURLs; this must only run on the database
  // thread.
  void DoRemoveURLs(const std::vector<GURL>& restrict_urls);

  // The implementation of DeleteDatabase; called on the database thread.
  void DoDeleteDatabase();

  // Cleans old records from the activity log database.
  bool CleanOlderThan(sql::Connection* db, const base::Time& cutoff);

  // Cleans unused interned strings from the database.  This should be run
  // after deleting rows from the main log table to clean out stale values.
  bool CleanStringTables(sql::Connection* db);

  // API calls for which complete arguments should be logged.
  std::set<std::string> api_arg_whitelist_;

  // Tables for mapping strings to integers for shrinking database storage
  // requirements.  URLs are kept in a separate table from other strings to
  // make history clearing simpler.
  DatabaseStringTable string_table_;
  DatabaseStringTable url_table_;

  // Tracks any pending updates to be written to the database, if write
  // batching is turned on.  Should only be accessed from the database thread.
  ActionQueue queued_actions_;

  // All queued actions must fall on the same day, so that we do not
  // accidentally aggregate actions that should be kept separate.
  // queued_actions_date_ is the date (timestamp at local midnight) of all the
  // actions in queued_actions_.
  base::Time queued_actions_date_;

  // The amount of time old activity log records should be kept in the
  // database.  This time is subtracted from the current time, rounded down to
  // midnight, and rows older than this are deleted from the database when
  // cleaning runs.
  base::TimeDelta retention_time_;

  // The time at which old activity log records were last cleaned out of the
  // database (only tracked for this browser session).  Old records are deleted
  // on the first database flush, and then every 12 hours subsequently.
  base::Time last_database_cleaning_time_;

  friend class CountingPolicyTest;
  FRIEND_TEST_ALL_PREFIXES(CountingPolicyTest, EarlyFlush);
  FRIEND_TEST_ALL_PREFIXES(CountingPolicyTest, MergingAndExpiring);
  FRIEND_TEST_ALL_PREFIXES(CountingPolicyTest, StringTableCleaning);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_COUNTING_POLICY_H_
