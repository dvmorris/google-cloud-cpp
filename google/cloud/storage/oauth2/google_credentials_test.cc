// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/storage/internal/compute_engine_util.h"
#include "google/cloud/storage/oauth2/anonymous_credentials.h"
#include "google/cloud/storage/oauth2/authorized_user_credentials.h"
#include "google/cloud/storage/oauth2/compute_engine_credentials.h"
#include "google/cloud/storage/oauth2/google_application_default_credentials_file.h"
#include "google/cloud/storage/oauth2/service_account_credentials.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/environment_variable_restore.h"
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
namespace {

using ::google::cloud::internal::SetEnv;
using ::google::cloud::internal::UnsetEnv;
using ::google::cloud::storage::internal::GceCheckOverrideEnvVar;
using ::google::cloud::testing_util::EnvironmentVariableRestore;
using ::testing::HasSubstr;

class GoogleCredentialsTest : public ::testing::Test {
 public:
  GoogleCredentialsTest()
      : home_env_var_(GoogleAdcHomeEnvVar()),
        adc_env_var_(GoogleAdcEnvVar()),
        gcloud_path_override_env_var_(GoogleGcloudAdcFileEnvVar()),
        gce_check_override_env_var_(GceCheckOverrideEnvVar()) {}

 protected:
  void SetUp() override {
    home_env_var_.SetUp();
    adc_env_var_.SetUp();
    gcloud_path_override_env_var_.SetUp();
    gce_check_override_env_var_.SetUp();
  }

  void TearDown() override {
    gce_check_override_env_var_.TearDown();
    gcloud_path_override_env_var_.TearDown();
    adc_env_var_.TearDown();
    home_env_var_.TearDown();
  }

 protected:
  EnvironmentVariableRestore home_env_var_;
  EnvironmentVariableRestore adc_env_var_;
  EnvironmentVariableRestore gcloud_path_override_env_var_;
  EnvironmentVariableRestore gce_check_override_env_var_;
};

std::string const AUTHORIZED_USER_CRED_FILENAME = "authorized-user.json";
std::string const AUTHORIZED_USER_CRED_CONTENTS = R"""({
  "client_id": "test-invalid-test-invalid.apps.googleusercontent.com",
  "client_secret": "invalid-invalid-invalid",
  "refresh_token": "1/test-test-test",
  "type": "authorized_user"
})""";

void SetupAuthorizedUserCredentialsFileForTest(std::string const& filename) {
  std::ofstream os(filename);
  os << AUTHORIZED_USER_CRED_CONTENTS;
  os.close();
}

/**
 * @test Verify `GoogleDefaultCredentials()` loads authorized user credentials.
 *
 * This test only verifies the right type of object is created, the unit tests
 * for `AuthorizedUserCredentials` already check that once loaded the class
 * works correctly. Testing here would be redundant. Furthermore, calling
 * `AuthorizationHeader()` initiates the key verification workflow, that
 * requires valid keys and contacting Google's production servers, and would
 * make this an integration test.
 */
TEST_F(GoogleCredentialsTest, LoadValidAuthorizedUserCredentialsViaEnvVar) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupAuthorizedUserCredentialsFileForTest(filename);

  // Test that the authorized user credentials are loaded as the default when
  // specified via the well known environment variable.
  SetEnv(GoogleAdcEnvVar(), filename.c_str());
  auto creds = GoogleDefaultCredentials();
  ASSERT_STATUS_OK(creds);
  // Need to create a temporary for the pointer because clang-tidy warns about
  // using expressions with (potential) side-effects inside typeid().
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(AuthorizedUserCredentials<>));
}

TEST_F(GoogleCredentialsTest, LoadValidAuthorizedUserCredentialsViaGcloudFile) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupAuthorizedUserCredentialsFileForTest(filename);
  // Test that the authorized user credentials are loaded as the default when
  // stored in the the well known gcloud ADC file path.
  UnsetEnv(GoogleAdcEnvVar());
  SetEnv(GoogleGcloudAdcFileEnvVar(), filename.c_str());
  auto creds = GoogleDefaultCredentials();
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(AuthorizedUserCredentials<>));
}

TEST_F(GoogleCredentialsTest, LoadValidAuthorizedUserCredentialsFromFilename) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupAuthorizedUserCredentialsFileForTest(filename);
  auto creds = CreateAuthorizedUserCredentialsFromJsonFilePath(filename);
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(AuthorizedUserCredentials<>));
}

TEST_F(GoogleCredentialsTest, LoadValidAuthorizedUserCredentialsFromContents) {
  // Test that the authorized user credentials are loaded from a string
  // representing JSON contents.
  auto creds = CreateAuthorizedUserCredentialsFromJsonContents(
      AUTHORIZED_USER_CRED_CONTENTS);
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(AuthorizedUserCredentials<>));
}

/**
 * @test Verify `GoogleDefaultCredentials()` loads service account credentials.
 *
 * This test only verifies the right type of object is created, the unit tests
 * for `ServiceAccountCredentials` already check that once loaded the class
 * works correctly. Testing here would be redundant. Furthermore, calling
 * `AuthorizationHeader()` initiates the key verification workflow, that
 * requires valid keys and contacting Google's production servers, and would
 * make this an integration test.
 */

std::string const SERVICE_ACCOUNT_CRED_FILENAME = "service-account.json";
std::string const SERVICE_ACCOUNT_CRED_CONTENTS = R"""({
    "type": "service_account",
    "project_id": "foo-project",
    "private_key_id": "a1a111aa1111a11a11a11aa111a111a1a1111111",
    "private_key": "-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCltiF2oP3KJJ+S\ntTc1McylY+TuAi3AdohX7mmqIjd8a3eBYDHs7FlnUrFC4CRijCr0rUqYfg2pmk4a\n6TaKbQRAhWDJ7XD931g7EBvCtd8+JQBNWVKnP9ByJUaO0hWVniM50KTsWtyX3up/\nfS0W2R8Cyx4yvasE8QHH8gnNGtr94iiORDC7De2BwHi/iU8FxMVJAIyDLNfyk0hN\neheYKfIDBgJV2v6VaCOGWaZyEuD0FJ6wFeLybFBwibrLIBE5Y/StCrZoVZ5LocFP\nT4o8kT7bU6yonudSCyNMedYmqHj/iF8B2UN1WrYx8zvoDqZk0nxIglmEYKn/6U7U\ngyETGcW9AgMBAAECggEAC231vmkpwA7JG9UYbviVmSW79UecsLzsOAZnbtbn1VLT\nPg7sup7tprD/LXHoyIxK7S/jqINvPU65iuUhgCg3Rhz8+UiBhd0pCH/arlIdiPuD\n2xHpX8RIxAq6pGCsoPJ0kwkHSw8UTnxPV8ZCPSRyHV71oQHQgSl/WjNhRi6PQroB\nSqc/pS1m09cTwyKQIopBBVayRzmI2BtBxyhQp9I8t5b7PYkEZDQlbdq0j5Xipoov\n9EW0+Zvkh1FGNig8IJ9Wp+SZi3rd7KLpkyKPY7BK/g0nXBkDxn019cET0SdJOHQG\nDiHiv4yTRsDCHZhtEbAMKZEpku4WxtQ+JjR31l8ueQKBgQDkO2oC8gi6vQDcx/CX\nZ23x2ZUyar6i0BQ8eJFAEN+IiUapEeCVazuxJSt4RjYfwSa/p117jdZGEWD0GxMC\n+iAXlc5LlrrWs4MWUc0AHTgXna28/vii3ltcsI0AjWMqaybhBTTNbMFa2/fV2OX2\nUimuFyBWbzVc3Zb9KAG4Y7OmJQKBgQC5324IjXPq5oH8UWZTdJPuO2cgRsvKmR/r\n9zl4loRjkS7FiOMfzAgUiXfH9XCnvwXMqJpuMw2PEUjUT+OyWjJONEK4qGFJkbN5\n3ykc7p5V7iPPc7Zxj4mFvJ1xjkcj+i5LY8Me+gL5mGIrJ2j8hbuv7f+PWIauyjnp\nNx/0GVFRuQKBgGNT4D1L7LSokPmFIpYh811wHliE0Fa3TDdNGZnSPhaD9/aYyy78\nLkxYKuT7WY7UVvLN+gdNoVV5NsLGDa4cAV+CWPfYr5PFKGXMT/Wewcy1WOmJ5des\nAgMC6zq0TdYmMBN6WpKUpEnQtbmh3eMnuvADLJWxbH3wCkg+4xDGg2bpAoGAYRNk\nMGtQQzqoYNNSkfus1xuHPMA8508Z8O9pwKU795R3zQs1NAInpjI1sOVrNPD7Ymwc\nW7mmNzZbxycCUL/yzg1VW4P1a6sBBYGbw1SMtWxun4ZbnuvMc2CTCh+43/1l+FHe\nMmt46kq/2rH2jwx5feTbOE6P6PINVNRJh/9BDWECgYEAsCWcH9D3cI/QDeLG1ao7\nrE2NcknP8N783edM07Z/zxWsIsXhBPY3gjHVz2LDl+QHgPWhGML62M0ja/6SsJW3\nYvLLIc82V7eqcVJTZtaFkuht68qu/Jn1ezbzJMJ4YXDYo1+KFi+2CAGR06QILb+I\nlUtj+/nH3HDQjM4ltYfTPUg=\n-----END PRIVATE KEY-----\n",
    "client_email": "foo-email@foo-project.iam.gserviceaccount.com",
    "client_id": "100000000000000000001",
    "auth_uri": "https://accounts.google.com/o/oauth2/auth",
    "token_uri": "https://accounts.google.com/o/oauth2/token",
    "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
    "client_x509_cert_url": "https://www.googleapis.com/robot/v1/metadata/x509/foo-email%40foo-project.iam.gserviceaccount.com"
})""";

void SetupServiceAccountCredentialsFileForTest(std::string const& filename) {
  std::ofstream os(filename);
  os << SERVICE_ACCOUNT_CRED_CONTENTS;
  os.close();
}

TEST_F(GoogleCredentialsTest, LoadValidServiceAccountCredentialsViaEnvVar) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded as the default when
  // specified via the well known environment variable.
  SetEnv(GoogleAdcEnvVar(), filename.c_str());
  auto creds = GoogleDefaultCredentials();
  ASSERT_STATUS_OK(creds);
  // Need to create a temporary for the pointer because clang-tidy warns about
  // using expressions with (potential) side-effects inside typeid().
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest, LoadValidServiceAccountCredentialsViaGcloudFile) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded as the default when
  // stored in the the well known gcloud ADC file path.
  UnsetEnv(GoogleAdcEnvVar());
  SetEnv(GoogleGcloudAdcFileEnvVar(), filename.c_str());
  auto creds = GoogleDefaultCredentials();
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest, LoadValidServiceAccountCredentialsFromFilename) {
  std::string filename = ::testing::TempDir() + SERVICE_ACCOUNT_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded from a file.
  auto creds = CreateServiceAccountCredentialsFromJsonFilePath(filename);
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest,
       LoadValidServiceAccountCredentialsFromFilenameWithOptionalArgs) {
  std::string filename = ::testing::TempDir() + SERVICE_ACCOUNT_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded from a file.
  auto creds = CreateServiceAccountCredentialsFromJsonFilePath(
      filename, {{"https://www.googleapis.com/auth/devstorage.full_control"}},
      "user@foo.bar");
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest,
       LoadValidServiceAccountCredentialsFromDefaultPathsViaEnvVar) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded as the default when
  // specified via the well known environment variable.
  SetEnv(GoogleAdcEnvVar(), filename.c_str());
  auto creds = CreateServiceAccountCredentialsFromDefaultPaths();
  ASSERT_STATUS_OK(creds);
  // Need to create a temporary for the pointer because clang-tidy warns about
  // using expressions with (potential) side-effects inside typeid().
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest,
       LoadValidServiceAccountCredentialsFromDefaultPathsViaGcloudFile) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded as the default when
  // stored in the the well known gcloud ADC file path.
  UnsetEnv(GoogleAdcEnvVar());
  SetEnv(GoogleGcloudAdcFileEnvVar(), filename.c_str());
  auto creds = CreateServiceAccountCredentialsFromDefaultPaths();
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest,
       LoadValidServiceAccountCredentialsFromDefaultPathsWithOptionalArgs) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupServiceAccountCredentialsFileForTest(filename);

  // Test that the service account credentials are loaded as the default when
  // specified via the well known environment variable.
  SetEnv(GoogleAdcEnvVar(), filename.c_str());
  auto creds = CreateServiceAccountCredentialsFromDefaultPaths(
      {{"https://www.googleapis.com/auth/devstorage.full_control"}},
      "user@foo.bar");
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(
    GoogleCredentialsTest,
    DoNotLoadAuthorizedUserCredentialsFromCreateServiceAccountCredentialsFromDefaultPaths) {
  std::string filename = ::testing::TempDir() + AUTHORIZED_USER_CRED_FILENAME;
  SetupAuthorizedUserCredentialsFileForTest(filename);

  // Test that the authorized user credentials are loaded as the default when
  // specified via the well known environment variable.
  SetEnv(GoogleAdcEnvVar(), filename.c_str());
  auto creds = CreateServiceAccountCredentialsFromDefaultPaths();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_THAT(creds.status().message(),
              HasSubstr("Unsupported credential type"));
}

TEST_F(GoogleCredentialsTest, LoadValidServiceAccountCredentialsFromContents) {
  // Test that the service account credentials are loaded from a string
  // representing JSON contents.
  auto creds = CreateServiceAccountCredentialsFromJsonContents(
      SERVICE_ACCOUNT_CRED_CONTENTS,
      {{"https://www.googleapis.com/auth/devstorage.full_control"}},
      "user@foo.bar");
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ServiceAccountCredentials<>));
}

TEST_F(GoogleCredentialsTest, LoadComputeEngineCredentialsFromADCFlow) {
  // Make sure other higher-precedence credentials (ADC env var, gcloud ADC from
  // well-known path) aren't loaded.
  UnsetEnv(GoogleAdcEnvVar());
  SetEnv(GoogleGcloudAdcFileEnvVar(), "");
  // If the ADC flow thinks we're on a GCE instance, it should return
  // ComputeEngineCredentials.
  SetEnv(GceCheckOverrideEnvVar(), "1");

  auto creds = GoogleDefaultCredentials();
  ASSERT_STATUS_OK(creds);
  auto* ptr = creds->get();
  EXPECT_EQ(typeid(*ptr), typeid(ComputeEngineCredentials<>));
}

TEST_F(GoogleCredentialsTest, CreateComputeEngineCredentialsWithDefaultEmail) {
  auto credentials = CreateComputeEngineCredentials();
  auto* ptr = credentials.get();
  EXPECT_EQ(typeid(*ptr), typeid(ComputeEngineCredentials<>));
  EXPECT_EQ(
      std::string("default"),
      dynamic_cast<ComputeEngineCredentials<>*>(ptr)->service_account_email());
}

TEST_F(GoogleCredentialsTest, CreateComputeEngineCredentialsWithExplicitEmail) {
  auto credentials = CreateComputeEngineCredentials("foo@bar.baz");
  auto* ptr = credentials.get();
  EXPECT_EQ(typeid(*ptr), typeid(ComputeEngineCredentials<>));
  EXPECT_EQ(
      std::string("foo@bar.baz"),
      dynamic_cast<ComputeEngineCredentials<>*>(ptr)->service_account_email());
}

TEST_F(GoogleCredentialsTest, LoadUnknownTypeCredentials) {
  std::string filename = ::testing::TempDir() + "unknown-type-credentials.json";
  std::ofstream os(filename);
  std::string contents_str = R"""({
  "type": "unknown_type"
})""";
  os << contents_str;
  os.close();
  SetEnv(GoogleAdcEnvVar(), filename.c_str());

  auto creds = GoogleDefaultCredentials();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_THAT(creds.status().message(),
              HasSubstr("Unsupported credential type"));
  EXPECT_THAT(creds.status().message(), HasSubstr(filename));
}

TEST_F(GoogleCredentialsTest, LoadInvalidCredentials) {
  std::string filename = ::testing::TempDir() + "invalid-credentials.json";
  std::ofstream os(filename);
  std::string contents_str = R"""( not-a-json-object-string )""";
  os << contents_str;
  os.close();
  SetEnv(GoogleAdcEnvVar(), filename.c_str());

  auto creds = GoogleDefaultCredentials();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_EQ(StatusCode::kInvalidArgument, creds.status().code());
  EXPECT_THAT(creds.status().message(),
              HasSubstr("credentials file " + filename));
}

TEST_F(GoogleCredentialsTest, MissingCredentialsViaEnvVar) {
  char const filename[] = "missing-credentials.json";
  SetEnv(GoogleAdcEnvVar(), filename);

  auto creds = GoogleDefaultCredentials();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_THAT(creds.status().message(),
              HasSubstr("Cannot open credentials file"));
  EXPECT_THAT(creds.status().message(), HasSubstr(filename));
}

TEST_F(GoogleCredentialsTest, MissingCredentialsViaGcloudFilePath) {
  char const filename[] = "missing-credentials.json";

  // Make sure other credentials (ADC env var, implicit environment-based creds)
  // aren't found either.
  UnsetEnv(GoogleAdcEnvVar());
  SetEnv(GceCheckOverrideEnvVar(), "0");
  // The method to create default credentials should see that no file exists at
  // this path, then continue trying to load the other credential types,
  // eventually finding no valid credentials and hitting a runtime error.
  SetEnv(GoogleGcloudAdcFileEnvVar(), filename);

  auto creds = GoogleDefaultCredentials();
  ASSERT_FALSE(creds) << "status=" << creds.status();
  EXPECT_THAT(creds.status().message(),
              HasSubstr("Could not automatically determine"));
}

}  // namespace
}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
