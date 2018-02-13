/**
 * Copyright Soramitsu Co., Ltd. 2018 All Rights Reserved.
 * http://soramitsu.co.jp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <rapidjson/document.h>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <pqxx/pqxx>

#include "common/files.hpp"
#include "main/iroha_conf_loader.hpp"

using namespace boost::process;
using namespace std::chrono_literals;

class IrohadTest : public testing::Test {
 public:
  virtual void SetUp() {
    timeout = 1s;
    setPaths();
    setParams();
    auto config = parse_iroha_config(path_config_.string());
    blockstore_path_ = config[config_members::BlockStorePath].GetString();
    pgopts_ = config[config_members::PgOpt].GetString();
  }
  virtual void TearDown() {
    iroha::remove_all(blockstore_path_);
    dropPostgres();
  }

 private:
  void setPaths() {
    path_irohad_ = boost::filesystem::path(PATHIROHAD);
    irohad_executable = path_irohad_ / "irohad";
    path_example_ = path_irohad_.parent_path().parent_path() / "example";
    path_config_ = path_example_ / "config.sample";
    path_genesis_ = path_example_ / "genesis.block";
    path_keypair_ = path_example_ / "node0";
  }

  void setParams() {
    params = " --config " + path_config_.string() + " --genesis_block "
        + path_genesis_.string() + " --keypair_name " + path_keypair_.string();
  }

  void dropPostgres() {
    auto connection = std::make_shared<pqxx::lazyconnection>(pgopts_);
    try {
      connection->activate();
    } catch (const pqxx::broken_connection &e) {
      FAIL() << "Connection to PostgreSQL broken: " << e.what();
    }

    const auto drop = R"(
DROP TABLE IF EXISTS account_has_signatory;
DROP TABLE IF EXISTS account_has_asset;
DROP TABLE IF EXISTS role_has_permissions;
DROP TABLE IF EXISTS account_has_roles;
DROP TABLE IF EXISTS account_has_grantable_permissions;
DROP TABLE IF EXISTS account;
DROP TABLE IF EXISTS asset;
DROP TABLE IF EXISTS domain;
DROP TABLE IF EXISTS signatory;
DROP TABLE IF EXISTS peer;
DROP TABLE IF EXISTS role;
DROP TABLE IF EXISTS height_by_hash;
DROP TABLE IF EXISTS height_by_account_set;
DROP TABLE IF EXISTS index_by_creator_height;
DROP TABLE IF EXISTS index_by_id_height_asset;
)";

    pqxx::work txn(*connection);
    txn.exec(drop);
    txn.commit();
    connection->disconnect();
  }

 public:
  boost::filesystem::path irohad_executable;
  std::string params;
  std::chrono::milliseconds timeout = 1s;

 private:
  boost::filesystem::path path_irohad_;
  boost::filesystem::path path_example_;
  boost::filesystem::path path_config_;
  boost::filesystem::path path_genesis_;
  boost::filesystem::path path_keypair_;
  std::string pgopts_;
  std::string blockstore_path_;
};

/*
 * @given path to irohad executable
 * @when run irohad without any parameters
 * @then irohad should not start
 */
TEST_F(IrohadTest, RunIrohadWithoutArgs) {
  child c(irohad_executable);
  ASSERT_TRUE(c.wait_for(timeout));
}

/*
 * @given path to irohad executable and paths to files irohad is needed to be
 * run (config, genesis block, keypair)
 * @when run irohad with all parameters it needs to operate as a full node
 * @then irohad should be started and running until timeout expired
 */
TEST_F(IrohadTest, RunIrohad) {
  child c(irohad_executable.string() + params);
  std::this_thread::sleep_for(timeout);
  ASSERT_TRUE(c.running());
  c.terminate();
}