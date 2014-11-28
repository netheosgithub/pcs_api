/**
 * Copyright (c) 2014 Netheos (http://www.netheos.net)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <functional>

#include "gtest/gtest.h"

#include "cpprest/asyncrt_utils.h"

#include "pcs_api/storage_facade.h"
#include "pcs_api/file_byte_sink.h"
#include "pcs_api/memory_byte_source.h"
#include "pcs_api/memory_byte_sink.h"
#include "pcs_api/stdout_progress_listener.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/oauth2_storage_provider.h"
#include "pcs_api/internal/password_storage_provider.h"
#include "pcs_api/internal/logger.h"

#include "functional_base_test.h"
#include "misc_test_utils.h"
#include "fixed_buffer_byte_sink.h"
#include "bad_memory_byte_source.h"

namespace pcs_api {

typedef FunctionalTest BasicTest;

INSTANTIATE_TEST_CASE_P(
  Providers,
  BasicTest,
  ::testing::ValuesIn(g_providers_to_be_tested));

TEST_P(BasicTest, CleanupTestFolders) {
    MiscUtils::CleanupTestFolders(p_storage_);
}

TEST_P(BasicTest, TestRegisteredProviders) {
    std::vector<std::string> providers =
                                    StorageFacade::GetRegisteredProviders();
    LOG_INFO << "Registered providers in pcs_api: ";
    for (std::string pn : providers) {
        LOG_INFO << pn;
    }

    EXPECT_TRUE(std::find(providers.begin(), providers.end(), "dropbox")
                != providers.end());
    EXPECT_TRUE(std::find(providers.begin(), providers.end(), "googledrive")
                != providers.end());
    EXPECT_TRUE(std::find(providers.begin(), providers.end(), "hubic")
                != providers.end());
#if defined _WIN32 || defined _WIN64
    EXPECT_TRUE(std::find(providers.begin(), providers.end(), "cloudme")
                != providers.end());
#endif
}

TEST_P(BasicTest, TestGetUserId) {
    std::string user_id = p_storage_->GetUserId();
    LOG_INFO << "Retrieved from provider " << p_storage_->GetProviderName()
             << ": user_id = " << user_id;

    // compare with userId stored in session manager: there are several cases...
    std::shared_ptr<OAuth2StorageProvider> p_oauth2_storage =
            std::dynamic_pointer_cast<OAuth2StorageProvider>(p_storage_);
    if (p_oauth2_storage) {
        std::string expected_user_id = p_oauth2_storage->p_session_manager_->
                                                p_user_credentials_->user_id();
        EXPECT_EQ(expected_user_id, user_id);
    }

    std::shared_ptr<PasswordStorageProvider> p_password_storage =
            std::dynamic_pointer_cast<PasswordStorageProvider>(p_storage_);
    if (p_password_storage) {
        // We get a client from pool, to get credentials:
        web::http::client::http_client *p_client = p_password_storage->
                                    p_session_manager_->clients_pool_.Get();
        string_t wexpected_user_id =
                            p_client->client_config().credentials().username();
        p_password_storage->p_session_manager_->clients_pool_.Put(p_client);
        std::string expected_user_id =
                        utility::conversions::to_utf8string(wexpected_user_id);
        EXPECT_EQ(expected_user_id, user_id);
    }
}

TEST_P(BasicTest, TestDisplayQuota) {
    CQuota quota = p_storage_->GetQuota();
    LOG_INFO << "Retrieved quota for provider " << p_storage_->GetProviderName()
        << ": " << quota << " (" << quota.GetPercentUsed() << "% used)";
}

TEST_P(BasicTest, TestQuotaChangedAfterUpload) {
    WithRandomTestPath([&](CPath path) {
        CQuota quota_before = p_storage_->GetQuota();
        LOG_INFO << "Quota BEFORE upload (" << p_storage_->GetProviderName()
                << "): used=" << quota_before.bytes_used()
                << " / total=" << quota_before.bytes_allowed();

        // Upload a quite big blob:
        int file_size = 500000;
        LOG_INFO << "Uploading blob with size "
                 << file_size << " bytes to " << path;
        std::string content = MiscUtils::GenerateRandomData(file_size);
        std::shared_ptr<MemoryByteSource> p_bs =
                                std::make_shared<MemoryByteSource>(content);
        CUploadRequest upload_request(path, p_bs);
        std::shared_ptr<ProgressListener> p_pl =
                                std::make_shared<StdoutProgressListener>();
        upload_request.set_progress_listener(p_pl);
        p_storage_->Upload(upload_request);

        // Check uploaded blob exists and has correct length:
        std::shared_ptr<CFile> p_cfile = p_storage_->GetFile(path);
        ASSERT_TRUE(nullptr != p_cfile.get()) << "uploaded file exists";
        ASSERT_TRUE(p_cfile->IsBlob());
        std::shared_ptr<CBlob> p_blob =
                                    std::dynamic_pointer_cast<CBlob>(p_cfile);
        // should always succeed as we have checked before:
        ASSERT_TRUE(nullptr != p_blob.get());
        ASSERT_EQ(file_size, p_blob->length());

        LOG_INFO << "Checking quota has changed";
        CQuota quota_after = p_storage_->GetQuota();
        p_storage_->Delete(path);
        LOG_INFO << "Quota AFTER upload (" << p_storage_->GetProviderName()
                << "): used=" << quota_after.bytes_used()
                << " / total=" << quota_after.bytes_allowed();
        int64_t used_difference =   quota_after.bytes_used()
                                  - quota_before.bytes_used();
        LOG_INFO << "used bytes difference = " << used_difference
            << " (upload file size was " << file_size << ")";
        NOT_SUPPORTED_BY_PROVIDER(p_storage_,
                                  "hubic",
                                  "quota not updated in real time");
        NOT_SUPPORTED_BY_PROVIDER(p_storage_,
                                  "googledrive",
                                  "quota not updated in real time");
        EXPECT_EQ(file_size, used_difference);
    });
}

TEST_P(BasicTest, TestFileOperations) {
    WithRandomTestPath([&](CPath temp_root_path) {
        // Create a sub-folder:
        CPath sub_path = temp_root_path.Add(PCS_API_STRING_T("sub_folder"));
        LOG_INFO << "Creating sub_folder: " << sub_path;
        // True because actually created:
        EXPECT_TRUE(p_storage_->CreateFolder(sub_path));
        // False because not created:
        EXPECT_FALSE(p_storage_->CreateFolder(sub_path));

        // Check back:
        std::shared_ptr<CFile> p_sub_folder_file =
                                                p_storage_->GetFile(sub_path);
        ASSERT_TRUE(nullptr != p_sub_folder_file.get());
        EXPECT_EQ(sub_path, p_sub_folder_file->path());
        EXPECT_TRUE(p_sub_folder_file->IsFolder());
        EXPECT_FALSE(p_sub_folder_file->IsBlob());
        // Not all providers have a modif time on folders:
        if (p_sub_folder_file->modification_date() !=
                                    ::boost::posix_time::not_a_date_time) {
            EXPECT_TRUE(MiscUtils::IsDatetimeAlmostNow(
                                    p_sub_folder_file->modification_date()));
        }
        std::shared_ptr<CFolder> p_sub_folder =
                        std::dynamic_pointer_cast<CFolder>(p_sub_folder_file);

        // Upload 2 files into this sub-folder:
        CPath fpath1 = sub_path.Add(PCS_API_STRING_T("a_test_file1"));
        std::string content_file1 =
                        "This is binary cont\xE2\x82\xACnt of test file 1...";
        LOG_INFO << "Uploading blob to: " << fpath1;
        std::shared_ptr<ByteSource> p_mbs1 =
                            std::make_shared<MemoryByteSource>(content_file1);
        CUploadRequest upload_request(fpath1, p_mbs1);
        p_storage_->Upload(upload_request);

        CPath fpath2 = sub_path.Add(PCS_API_STRING_T("a_test_file2"));
        // Generate a quite big random data:
        std::string content_file2 = MiscUtils::GenerateRandomData(500000);
        LOG_INFO << "Uploading blob to: " << fpath2;
        std::shared_ptr<ByteSource> p_mbs2 =
                            std::make_shared<MemoryByteSource>(content_file2);
        upload_request = CUploadRequest(fpath2, p_mbs2);
        p_storage_->Upload(upload_request);

        // Check uploaded blobs informations:
        // we check file2 first because has just been uploaded
        // This is for modif time check
        std::shared_ptr<CFile> p_cfile = p_storage_->GetFile(fpath2);
        ASSERT_TRUE(nullptr != p_cfile.get());
        EXPECT_TRUE(p_cfile->IsBlob());
        EXPECT_FALSE(p_cfile->IsFolder());
        std::shared_ptr<CBlob> p_cblob =
                                    std::dynamic_pointer_cast<CBlob>(p_cfile);
        ASSERT_TRUE(nullptr != p_cblob.get());
        EXPECT_EQ(content_file2.length(), p_cblob->length());
        EXPECT_TRUE(MiscUtils::IsDatetimeAlmostNow(
                                                p_cblob->modification_date()));

        p_cfile = p_storage_->GetFile(fpath1);
        ASSERT_TRUE(nullptr != p_cfile.get());
        EXPECT_TRUE(p_cfile->IsBlob());
        EXPECT_FALSE(p_cfile->IsFolder());
        p_cblob = std::dynamic_pointer_cast<CBlob>(p_cfile);
        ASSERT_TRUE(nullptr != p_cblob.get());
        EXPECT_EQ(content_file1.length(), p_cblob->length());

        // Download data, and check:
        LOG_INFO << "Downloading back and checking file: "<< fpath1;
        std::shared_ptr<MemoryByteSink> p_mbsi =
                                            std::make_shared<MemoryByteSink>();
        CDownloadRequest download_request(fpath1, p_mbsi);
        p_storage_->Download(download_request);
        EXPECT_EQ(content_file1, p_mbsi->GetData());

        // Check also with different Ranges:
        LOG_INFO << "Downloading back and checking file ranges: "<< fpath1;
        {
            download_request.SetRange(5, -1);  // starting at offset 5
            std::string expected_data(content_file1.begin()+5,
                                      content_file1.end());
            p_storage_->Download(download_request);
            EXPECT_EQ(expected_data, p_mbsi->GetData());
        }
        {
            download_request.SetRange(-1, 5);  // last 5 bytes
            std::string expected_data(content_file1.end()-5,
                                      content_file1.end());
            p_storage_->Download(download_request);
            EXPECT_EQ(expected_data, p_mbsi->GetData());
        }
        {
            download_request.SetRange(2, 5);  // 5 bytes at offset 2
            std::string expected_data(content_file1.begin()+2,
                                      content_file1.begin()+2+5);
            p_storage_->Download(download_request);
            EXPECT_EQ(expected_data, p_mbsi->GetData());
        }
        LOG_INFO << "Downloading back and checking file: " << fpath2;
        download_request = CDownloadRequest(fpath2, p_mbsi);
        p_storage_->Download(download_request);
        EXPECT_EQ(content_file2, p_mbsi->GetData());

        // Check that if we upload again, blob is replaced:
        LOG_INFO << "Checking file overwrite: " << fpath2;
        content_file2 = MiscUtils::GenerateRandomData(300000);
        p_mbs2 = std::make_shared<MemoryByteSource>(content_file2);
        upload_request = CUploadRequest(fpath2, p_mbs2);
        p_storage_->Upload(upload_request);
        p_storage_->Download(download_request);
        EXPECT_EQ(content_file2, p_mbsi->GetData());

        // Check that we can replace replace existing blob with empty content:
        LOG_INFO << "Checking file overwrite with empty file: " << fpath2;
        content_file2 = std::string();  // empty
        p_mbs2 = std::make_shared<MemoryByteSource>(content_file2);
        upload_request = CUploadRequest(fpath2, p_mbs2);
        p_storage_->Upload(upload_request);
        p_storage_->Download(download_request);
        EXPECT_EQ(content_file2, p_mbsi->GetData());

        // Create a sub_sub_folder:
        CPath sub_sub_path = sub_path.Add(PCS_API_STRING_T("a_sub_sub_folder"));
        LOG_INFO << "Creating sub_sub folder: " << sub_sub_path;
        p_storage_->CreateFolder(sub_sub_path);

        LOG_INFO << "Check uploaded blobs and sub_sub_folder"
                    " all appear in folder list";
        std::shared_ptr<CFolderContent> p_folder_content =
                                        p_storage_->ListFolder(*p_sub_folder);
        ASSERT_TRUE(nullptr != p_folder_content.get());
        LOG_INFO << "sub_folder contains files: " << *p_folder_content;
        // It happened once here that hubic did not list fpath1 'a_test_file1'
        // in folder_content: only 2 files were present ?!
        EXPECT_EQ(3, p_folder_content->size());
        EXPECT_TRUE(p_folder_content->ContainsPath(fpath1));
        std::shared_ptr<CFile> p_file = p_folder_content->GetFile(fpath1);
        ASSERT_TRUE(nullptr != p_file.get());
        EXPECT_TRUE(p_file->IsBlob());
        EXPECT_FALSE(p_file->IsFolder());

        EXPECT_TRUE(p_folder_content->ContainsPath(fpath2));
        p_file = p_folder_content->GetFile(fpath2);
        ASSERT_TRUE(nullptr != p_file.get());
        EXPECT_TRUE(p_file->IsBlob());
        EXPECT_FALSE(p_file->IsFolder());

        EXPECT_TRUE(p_folder_content->ContainsPath(sub_sub_path));
        p_file = p_folder_content->GetFile(sub_sub_path);
        ASSERT_TRUE(nullptr != p_file.get());
        EXPECT_FALSE(p_file->IsBlob());
        EXPECT_TRUE(p_file->IsFolder());

        LOG_INFO << "Check that list of sub_sub folder is empty: "
                 << sub_sub_path;
        p_folder_content = p_storage_->ListFolder(sub_sub_path);
        ASSERT_TRUE(nullptr != p_folder_content.get());
        EXPECT_EQ(0, p_folder_content->size());

        LOG_INFO << "Check that listing content of a blob raises: " << fpath1;
        try {
            p_storage_->ListFolder(fpath1);
            FAIL() << "Listing a blob should raise";
        }
        catch (CInvalidFileTypeException& ex) {
            EXPECT_EQ(fpath1, ex.path());
            EXPECT_FALSE(ex.blob_expected());
        }

        LOG_INFO << "Delete file1: " << fpath1;
        EXPECT_TRUE(p_storage_->Delete(fpath1));  // We have deleted the file
        // We have not deleted anything, so false:
        EXPECT_FALSE(p_storage_->Delete(fpath1));

        LOG_INFO << "Check file1 does not appear anymore in folder: "
                 << sub_path;
        p_folder_content = p_storage_->ListFolder(sub_path);
        ASSERT_TRUE(nullptr != p_folder_content.get());
        EXPECT_FALSE(p_folder_content->ContainsPath(fpath1));
        EXPECT_EQ(nullptr, p_storage_->GetFile(fpath1).get());

        LOG_INFO << "Delete whole test folder: " << temp_root_path;
        bool ret = p_storage_->Delete(temp_root_path);
        EXPECT_TRUE(ret);  // We have deleted at least one file
        LOG_INFO << "Deleting again returns False";
        ret = p_storage_->Delete(temp_root_path);
        EXPECT_FALSE(ret);  // We have not deleted anything

        LOG_INFO << "Listing a deleted folder returns None: " << temp_root_path;
        EXPECT_EQ(nullptr, p_storage_->ListFolder(temp_root_path).get());
        EXPECT_EQ(nullptr, p_storage_->GetFile(temp_root_path).get());
    });
}

TEST_P(BasicTest, TestCreateIntermediateFolders) {
    WithRandomTestPath([&](CPath temp_root_path) {
        // We create a deep sub-folder, and check each parent has been created:
        CPath path = temp_root_path.Add(
                        PCS_API_STRING_T("sub1/sub2/sub3/sub4/sub5_folder"));
        p_storage_->CreateFolder(path);
        while (!path.IsRoot()) {
            std::shared_ptr<CFile> p_file = p_storage_->GetFile(path);
            ASSERT_TRUE(nullptr != p_file.get());
            EXPECT_TRUE(p_file->IsFolder());
            path = path.GetParent();
        }
    });
}

TEST_P(BasicTest, TestBlobContentType) {
    // Only hubiC supports content-type for now:
    NOT_SUPPORTED_BY_PROVIDER(p_storage_,
                              "dropbox",
                              "does not support content-type");
    NOT_SUPPORTED_BY_PROVIDER(p_storage_,
                              "googledrive",
                              "does not support content-type");
    NOT_SUPPORTED_BY_PROVIDER(p_storage_,
                              "cloudme",
                              "does not support content-type");

    WithRandomTestPath([&](CPath temp_root_path) {
        // some providers are sensitive to filename suffix,
        // so we do not specify any here:
        CPath path = temp_root_path.Add(PCS_API_STRING_T("uploaded_blob"));
        char data[] = "some content...";
        string_t content_type = PCS_API_STRING_T("text/plain; charset=Latin-1");
        std::shared_ptr<MemoryByteSource> p_mbs =
                                    std::make_shared<MemoryByteSource>(data);
        CUploadRequest upload_request(path, p_mbs);
        upload_request.set_content_type(content_type);
        p_storage_->Upload(upload_request);

        std::shared_ptr<CFile> p_file = p_storage_->GetFile(path);
        ASSERT_TRUE(nullptr != p_file.get());
        ASSERT_TRUE(p_file->IsBlob());
        std::shared_ptr<CBlob> p_blob =
                                    std::dynamic_pointer_cast<CBlob>(p_file);
        ASSERT_TRUE(nullptr != p_blob.get());
        EXPECT_EQ(content_type, p_blob->content_type());

        // Update file content, check content-type is updated also:
        char data2[] = "some binary content...";
        data2[4] = 0x05;
        data2[11] = (char)0xff;  // NOLINT
        content_type = PCS_API_STRING_T("application/octet-stream");
        p_mbs = std::make_shared<MemoryByteSource>(data2);
        upload_request = CUploadRequest(path, p_mbs);
        upload_request.set_content_type(content_type);
        p_storage_->Upload(upload_request);

        p_file = p_storage_->GetFile(path);
        ASSERT_TRUE(nullptr != p_file.get());
        ASSERT_TRUE(p_file->IsBlob());
        p_blob = std::dynamic_pointer_cast<CBlob>(p_file);
        ASSERT_TRUE(nullptr != p_blob.get());
        EXPECT_EQ(content_type, p_blob->content_type());
    });
}

TEST_P(BasicTest, TestDeleteSingleFolder) {
    WithRandomTestPath([&](CPath temp_root_path) {
        // Create two sub folders: a/ and ab/:
        CPath fpatha = temp_root_path.Add(PCS_API_STRING_T("a"));
        CPath fpathab = temp_root_path.Add(PCS_API_STRING_T("ab"));
        p_storage_->CreateFolder(fpatha);
        p_storage_->CreateFolder(fpathab);
        std::shared_ptr<CFile> p_file = p_storage_->GetFile(fpatha);
        ASSERT_TRUE(nullptr != p_file.get());
        ASSERT_TRUE(p_file->IsFolder());
        p_file = p_storage_->GetFile(fpathab);
        ASSERT_TRUE(nullptr != p_file.get());
        ASSERT_TRUE(p_file->IsFolder());

        CPath path = fpatha.Add(PCS_API_STRING_T("uploaded_blob.txt"));
        char data[] = "some content...";
        std::shared_ptr<ByteSource> p_bs =
                                    std::make_shared<MemoryByteSource>(data);
        CUploadRequest upload_request(path, p_bs);
        p_storage_->Upload(upload_request);

        // Now delete folder a: folder ab should not be deleted
        p_storage_->Delete(fpatha);
        p_file = p_storage_->GetFile(fpatha);
        EXPECT_EQ(nullptr, p_file.get());
        p_file = p_storage_->GetFile(fpathab);
        ASSERT_TRUE(nullptr != p_file.get());
        EXPECT_TRUE(p_file->IsFolder());

        p_storage_->Delete(temp_root_path);
    });
}

TEST_P(BasicTest, TestInvalidFileOperation) {
    WithRandomTestPath([&](CPath temp_root_path) {
        // Upload 1 file into this folder:
        CPath fpath1 = temp_root_path.Add(PCS_API_STRING_T("a_test_file1"));
        char content_file1[] =
                        "This is binary cont\xE2\x82\xACnt of test file 1...";
        std::shared_ptr<ByteSource> p_mbs =
                            std::make_shared<MemoryByteSource>(content_file1);
        CUploadRequest upload_request(fpath1, p_mbs);
        p_storage_->Upload(upload_request);
        LOG_INFO << "Created blob: " << fpath1;

        // Create a sub_folder:
        CPath sub_folder = temp_root_path.Add(PCS_API_STRING_T("sub_folder"));
        p_storage_->CreateFolder(sub_folder);

        LOG_INFO << "Check that listing content of a blob raises: " << fpath1;
        try {
            p_storage_->ListFolder(fpath1);
            FAIL() << "Listing a blob should raise";
        }
        catch (CInvalidFileTypeException& ex) {
            EXPECT_EQ(fpath1, ex.path());
            EXPECT_FALSE(ex.blob_expected());
        }

        LOG_INFO << "Check that trying to download a folder raises: "
                 << sub_folder;
        std::shared_ptr<ByteSink> p_mbsi = std::make_shared<MemoryByteSink>();
        CDownloadRequest download_request(sub_folder, p_mbsi);
        try {
            p_storage_->Download(download_request);
            FAIL() << "Downloading a folder should raise";
        }
        catch (CInvalidFileTypeException& ex) {
            EXPECT_EQ(sub_folder, ex.path());
            EXPECT_TRUE(ex.blob_expected());
        }

        LOG_INFO << "Check that we cannot create a folder over a blob: "
                 << fpath1;
        try {
            p_storage_->CreateFolder(fpath1);
            FAIL() << "Creating a folder over a blob should raise";
        }
        catch (CInvalidFileTypeException& ex) {
            EXPECT_EQ(fpath1, ex.path());
            EXPECT_FALSE(ex.blob_expected());
        }

        LOG_INFO << "Check we cannot upload over an existing folder: "
                 << sub_folder;
        try {
            upload_request = CUploadRequest(sub_folder, p_mbs);
            p_storage_->Upload(upload_request);
            FAIL() << "Uploading over a folder should raise";
        }
        catch (CInvalidFileTypeException& ex) {
            EXPECT_EQ(sub_folder, ex.path());
            EXPECT_TRUE(ex.blob_expected());
        }

        LOG_INFO << "Check that content of a never existed folder is None";
        CPath path(
            PCS_API_STRING_T("/hope i did never exist (even for tests) !"));
        std::shared_ptr<CFolderContent> p_folder_content =
                                                p_storage_->ListFolder(path);
        EXPECT_EQ(nullptr, p_folder_content.get());
        LOG_INFO << "Check that GetFile() returns None is file does not exist";
        std::shared_ptr<CFile> p_file = p_storage_->GetFile(path);
        EXPECT_EQ(nullptr, p_file.get());

        LOG_INFO << "Check that downloading a non-existing file raises";
        download_request = CDownloadRequest(path, p_mbsi);
        try {
            p_storage_->Download(download_request);
            FAIL() << "Downlad a non-existing blob should raise";
        }
        catch (CFileNotFoundException& ex) {
            LOG_DEBUG << "Expected exception: " << ex.ToString();
            EXPECT_EQ(path, ex.path());
        }
    });
}

TEST_P(BasicTest, TestCreateFolderOverBlob) {
    WithRandomTestPath([&](CPath temp_root_path) {
        // Upload 1 file into this folder:
        CPath fpath1 = temp_root_path.Add(PCS_API_STRING_T("a_test_file1"));
        char content_file[] = "This is content of test file 1...";
        std::shared_ptr<ByteSource> p_mbs =
                            std::make_shared<MemoryByteSource>(content_file);
        CUploadRequest upload_request(fpath1, p_mbs);
        p_storage_->Upload(upload_request);
        LOG_INFO << "Created blob: " << fpath1;

        try {
            CPath path = fpath1.Add(PCS_API_STRING_T("sub_folder1"));
            LOG_INFO << "Check we cannot create a folder"
                        " when remote path traverses a blob: " << path;
            p_storage_->CreateFolder(path);

            // This is known to fail on Dropbox:
            NOT_SUPPORTED_BY_PROVIDER(p_storage_,
                    "dropbox",
                    "Creating folder when path contains a blob should raise");
            FAIL() << "Creating folder when path contains a blob should raise";
        }
        catch (CInvalidFileTypeException& ex) {
            EXPECT_EQ(fpath1, ex.path());
            EXPECT_FALSE(ex.blob_expected());
        }
    });
}

TEST_P(BasicTest, TestImplicitCreateFolderOverBlob) {
    WithRandomTestPath([&](CPath temp_root_path) {
        // Upload 1 file into this folder:
        CPath fpath1 = temp_root_path.Add(PCS_API_STRING_T("a_test_file1"));
        char content_file[] = "This is content of test file 1...";
        std::shared_ptr<ByteSource> p_bs =
                            std::make_shared<MemoryByteSource>(content_file);
        CUploadRequest upload_request(fpath1, p_bs);
        p_storage_->Upload(upload_request);
        LOG_INFO << "Created blob: " << fpath1;

        // Uploading blob will implicitely create intermediate folders,
        // so will try to erase fpath1
        try {
            CPath path = fpath1.Add(PCS_API_STRING_T("sub_file1"));
            LOG_INFO << "Check we cannot upload a blob"
                        " when remote path traverses a blob: " << path;
            p_bs = std::make_shared<MemoryByteSource>(content_file);
            upload_request = CUploadRequest(path, p_bs);
            p_storage_->Upload(upload_request);

            // This is known to fail on Dropbox:
            NOT_SUPPORTED_BY_PROVIDER(p_storage_,
                    "dropbox",
                    "Creating folder when path contains a blob should raise");
            FAIL() << "Uploading when path contains a blob should raise";
        }
        catch (CInvalidFileTypeException& ex) {
            EXPECT_EQ(fpath1, ex.path());  // this is the problematic path
            EXPECT_FALSE(ex.blob_expected());
        }
    });
}

// We always use wide strings here, even on Linux
// (easier to generate/replace chars)
static utf16char GenerateRandomBlobNameChar(
                                std::shared_ptr<IStorageProvider> p_storage) {
    while (true) {
        utf16char c = (utf16char)(
                        utilities::Random() * utilities::Random() * 200) + 32;
        if (utilities::Random() < 0.02) {
            c = 0x20AC;  // euro
        }

        if (c >= 127 && c < 160) {
            continue;
        }
        if (c == '/' || c == '\\') {
            continue;
        }
        // CloudMe provider does not support quotes so we do not use them:
        if (c == '"' && p_storage->GetProviderName() == "cloudme") {
            continue;
        }
        return c;
    }
}

TEST_P(BasicTest, TestFileWithSpecialChars) {
    WithRandomTestPath([&](CPath temp_root_path) {
        CPath folder_path = temp_root_path.Add(
            PCS_API_STRING_T("hum...\u00a0',;.:\u00a0!*%&~#{[|`_ç^@ £\u20AC"));
        ASSERT_TRUE(p_storage_->CreateFolder(folder_path));
        std::shared_ptr<CFile> p_fback = p_storage_->GetFile(folder_path);
        ASSERT_TRUE(nullptr != p_fback.get());
        EXPECT_EQ(folder_path, p_fback->path());
        EXPECT_TRUE(p_fback->IsFolder());
        EXPECT_FALSE(p_fback->IsBlob());

        // Folder must appear in test root folder list:
        std::shared_ptr<CFolderContent> p_root_test_content =
                                        p_storage_->ListFolder(temp_root_path);
        ASSERT_TRUE(nullptr != p_root_test_content.get());
        ASSERT_TRUE(p_root_test_content->ContainsPath(folder_path));
        p_fback = p_root_test_content->GetFile(folder_path);
        ASSERT_TRUE(nullptr != p_fback.get());
        EXPECT_EQ(folder_path, p_fback->path());
        EXPECT_TRUE(p_fback->IsFolder());
        EXPECT_FALSE(p_fback->IsBlob());

        // Generate a random blob name
        // (ensure it does not start nor end with a space)
        utf16string blob_name;
        blob_name += 'b';
        for (int i = 0; i < 30; i++) {
            blob_name += GenerateRandomBlobNameChar(p_storage_);
        }
        blob_name += 'e';
        for (int nbBlobs = 0; nbBlobs < 20; nbBlobs++) {
            // slightly change blob name, so that we get similar
            // but different names:
            int index = static_cast<int>(
                            utilities::Random() * (blob_name.length() - 2)) + 1;
            blob_name[index] = GenerateRandomBlobNameChar(p_storage_);
            CPath blob_path = folder_path.Add(
                                utility::conversions::to_string_t(blob_name));
            LOG_INFO << "Will upload file to path: " << blob_path;

            // content depends on file name, so that we are able to detect
            // download of wrong file:
            string_t content_file_str =
                string_t(PCS_API_STRING_T("This is content of test file: '"))
                + utility::conversions::to_string_t(blob_name)
                + PCS_API_STRING_T("'")
                + utility::conversions::to_string_t(std::to_string(nbBlobs));
            std::string content_file =
                        utility::conversions::to_utf8string(content_file_str);
            std::shared_ptr<ByteSource> p_bs =
                            std::make_shared<MemoryByteSource>(content_file);
            CUploadRequest upload_request(blob_path, p_bs);
            upload_request.set_content_type(
                                PCS_API_STRING_T("text/plain ; charset=UTF-8"));
            p_storage_->Upload(upload_request);
            std::shared_ptr<CFile> p_bback = p_storage_->GetFile(blob_path);
            ASSERT_TRUE(nullptr != p_bback.get());
            EXPECT_EQ(blob_path, p_bback->path());
            EXPECT_TRUE(p_bback->IsBlob());
            EXPECT_FALSE(p_bback->IsFolder());

            // Download and check content:
            std::shared_ptr<MemoryByteSink> p_mbsi =
                                        std::make_shared<MemoryByteSink>();
            CDownloadRequest download_request(blob_path, p_mbsi);
            p_storage_->Download(download_request);
            EXPECT_EQ(content_file, p_mbsi->GetData());

            // new blob must appear in folder list:
            std::shared_ptr<CFolderContent> p_folder_content =
                                        p_storage_->ListFolder(folder_path);
            ASSERT_TRUE(nullptr != p_folder_content.get());
            EXPECT_TRUE(p_folder_content->ContainsPath(blob_path));
            std::shared_ptr<CFile> p_file =
                                        p_folder_content->GetFile(blob_path);
            ASSERT_TRUE(nullptr != p_file.get());
            EXPECT_EQ(blob_path, p_file->path());
            EXPECT_TRUE(p_file->IsBlob());
            EXPECT_FALSE(p_file->IsFolder());
        }
    });
}


class TestAbortProgressListener : public StdoutProgressListener {
 public:
    /**
    * @param nb_fails number of times an exception will be raised
    * @param offset_limit at which offset during process the exception
     *       will be raised
    * @param retriable_exception if true, a CRetriableException is thrown
     *       so request will be retried
    */
    TestAbortProgressListener(int nb_fails, int offset_limit,
                              bool retriable_exception) :
        StdoutProgressListener(false),
        nb_fails_total_(nb_fails),
        nb_fails_current_(0),
        limit_(offset_limit),
        retriable_exception_(retriable_exception) {
    }

    void Progress(std::streamsize current) override {
        StdoutProgressListener::Progress(current);
        if (current >= limit_) {
            if (nb_fails_current_ < nb_fails_total_) {
                nb_fails_current_++;
                std::ostringstream msgs;
                msgs << "Test error to make up/download fail: "
                        << nb_fails_current_ << "/" << nb_fails_total_;
                std::string msg = msgs.str();
                std::runtime_error ex(msg);
                // so that we do not erase last progress line:
                std::cout << std::endl;
                if (!retriable_exception_) {
                    LOG_DEBUG << "Raising test error: " << msg;
                    BOOST_THROW_EXCEPTION(ex);
                }
                try {
                    LOG_DEBUG << "Raising retriable test error: " << msg;
                    BOOST_THROW_EXCEPTION(ex);
                }
                catch(std::exception&) {
                    BOOST_THROW_EXCEPTION(
                                CRetriableException(std::current_exception()));
                }
            }
        }
    }

 private:
    const int nb_fails_total_;
    const int limit_;
    const bool retriable_exception_;
    int nb_fails_current_;
};

TEST_P(BasicTest, TestAbortDuringDownload) {
    WithRandomTestPath([&](CPath path) -> void {
        // If download fails, check that abort is called on progress listener
        // Upload a quite big file (for download):
        int file_size = 500000;
        LOG_INFO << "Will upload a blob for download test ("
                 << file_size << " bytes) to " << path;
        std::string content = MiscUtils::GenerateRandomData(file_size);
        std::shared_ptr<ByteSource> p_bs =
                                std::make_shared<MemoryByteSource>(content);
        CUploadRequest upload_request(path, p_bs);
        p_storage_->Upload(upload_request);

        // Now we download, asking for progress:
        LOG_INFO << "Will download this blob but fail during download...";
        boost::filesystem::path temp_path = boost::filesystem::unique_path(
                boost::filesystem::path("back_from_provider-%%%%%%"));
        std::shared_ptr<FileByteSink> p_fbsi =
                std::make_shared<FileByteSink>(temp_path,
                                             false,  // temp_name_during_write,
                                             true);  // delete_on_abort
        std::shared_ptr<TestAbortProgressListener> p_pl =
                                std::make_shared<TestAbortProgressListener>(
                                    1,  // fail once
                                    file_size / 2,  // abort at half download
                                    false);  // not retriable
        CDownloadRequest dr(path, p_fbsi);
        dr.set_progress_listener(p_pl);

        try {
            p_storage_->Download(dr);
            FAIL() << "Download should have failed !";
        }
        catch (std::exception&) {
            LOG_INFO << "Download has failed as expected: "
                      << CurrentExceptionToString();
        }

        // Check listener saw the abort:
        EXPECT_TRUE(p_pl->is_aborted());

        // check file does not exist (has been removed):
        LOG_INFO << "Check destination file does not exist: " << temp_path;
        EXPECT_FALSE(boost::filesystem::exists(temp_path));
    });
}

/**
 * Abort upload by throwing an exception from progress listener:
 * the body stream is shortened, cpprestsdk detects the unexpected content
 * length and throws.
 * HOWEVER, the thrown exception is not related to progress listener root
 * exception.
 */
TEST_P(BasicTest, TestAbortDuringUpload) {
#if !defined _WIN32 && !defined _WIN64
    // FIXME create issue in cpprestsdk:
    NOT_SUPPORTED_BY_OS("!= Windows", "Bug in cpprestsdk");
#endif
    WithRandomTestPath([&](CPath path) {
        int file_size = 500000;
        LOG_INFO << "Will upload a blob for download test ("
                 << file_size << " bytes) to " << path
                 << ", but abort during upload";
        std::string content = MiscUtils::GenerateRandomData(file_size);
        std::shared_ptr<ByteSource> p_bs =
                                std::make_shared<MemoryByteSource>(content);
        std::shared_ptr<ProgressListener> p_pl =
            std::make_shared<TestAbortProgressListener>(
                                        1,  // fail once
                                        file_size / 2,  // fail at half upload
                                        false);  // and do not retry
        CUploadRequest upload_request(path, p_bs);
        upload_request.set_progress_listener(p_pl);
        try {
            p_storage_->Upload(upload_request);
            FAIL() << "Throwing in ProgressListener should have aborted upload";
        }
        catch (const CStorageException&) {
            LOG_INFO << "Upload has failed as expected: "
                     << CurrentExceptionToString();
        }
    });
}

/**
 * FIXME for now it is not possible to abort/retry upload
 * by throwing a CRetriableException from progress listener.
 */
TEST_P(BasicTest, DISABLED_TestAbortTwiceDuringUpload) {
    WithRandomTestPath([&](CPath path) {
        int file_size = 500000;
        LOG_INFO << "Will upload a blob for download test ("
                 << file_size << " bytes) to " << path
                 << ", but fail temporarily during first two uploads";
        std::string content = MiscUtils::GenerateRandomData(file_size);
        std::shared_ptr<ByteSource> p_bs =
                                std::make_shared<MemoryByteSource>(content);
        std::shared_ptr<ProgressListener> p_pl =
                                std::make_shared<TestAbortProgressListener>(
            2,  // fail twice
            file_size / 2,  // fail at half upload
            true);  // and retry
        CUploadRequest upload_request(path, p_bs);
        upload_request.set_progress_listener(p_pl);
        p_storage_->Upload(upload_request);

        // check it has worked, really:
        std::shared_ptr<CFile> p_cfile = p_storage_->GetFile(path);
        ASSERT_TRUE(nullptr != p_cfile.get());
        ASSERT_TRUE(p_cfile->IsBlob());
        std::shared_ptr<CBlob> p_blob =
                                    std::dynamic_pointer_cast<CBlob>(p_cfile);
        ASSERT_TRUE(nullptr != p_blob.get());
        EXPECT_EQ(file_size, p_blob->length());
    });
}

/**
 * Try to download to a bad ByteSink (for example: disk full...)
 * and check for error.
 */
TEST_P(BasicTest, TestDownloadBadSink) {
    WithRandomTestPath([&](CPath path) {
        int file_size = 50000;
        LOG_INFO << "Will upload a blob for download test ("
                 << file_size << " bytes) to " << path;
        std::string content = MiscUtils::GenerateRandomData(file_size);
        std::shared_ptr<ByteSource> p_bs =
                                std::make_shared<MemoryByteSource>(content);
        CUploadRequest upload_request(path, p_bs);
        p_storage_->Upload(upload_request);

        // Now try to download to a too short ByteSink:
        // always strictly less than file_size
        int bad_sink_size = MiscUtils::Random(2048, file_size-6000);
        std::shared_ptr<ByteSink> p_bad_sink =
                        std::make_shared<FixedBufferByteSink>(bad_sink_size);
        CDownloadRequest download_request(path, p_bad_sink);
        // Sometimes we download a random range:
        std::string msg;
        msg += "file_size=" + std::to_string(file_size)
            + ", bad_sink_size=" + std::to_string(bad_sink_size);
        // if (utilities::Random() < 0.6) {
        //     int start = MiscUtils::Random(0, file_size - bad_sink_size);
        //     int length = MiscUtils::Random(bad_sink_size + 1,
        //                                    file_size - start);
        //     msg += std::string("  / Range: start=") + std::to_string(start)
        //            + ", length=" + std::to_string(length);
        //     download_request.SetRange(start, length);
        // }
        std::shared_ptr<ProgressListener> p_pl =
                            std::make_shared<StdoutProgressListener>(false);
        if (utilities::Random() < 0.5) {
            download_request.set_progress_listener(p_pl);
            msg += "  / with a progress listener";
        }
        SCOPED_TRACE(msg);
        try {
            p_storage_->Download(download_request);
            FAIL() << "Downloading to a bad sink should raise";
        }
        catch (CStorageException&) {  // expected
            LOG_DEBUG << "OK, download failed as expected with exception: "
                      << CurrentExceptionToString();
        }
    });
}

/**
 * Try to upload from a bad ByteSource and check for error.
 * Here the bad source throws at OpenStream()
 */
TEST_P(BasicTest, TestUploadBadSourceOpen) {
#if !defined _WIN32 && !defined _WIN64
    // FIXME create issue in cpprestsdk:
    NOT_SUPPORTED_BY_OS("!= Windows", "Bug in cpprestsdk");
#endif
    WithRandomTestPath([&](CPath path) {
        LOG_INFO << "Will try to upload a blob from a throwing bad source to "
                 << path;
        std::shared_ptr<ByteSource> p_bs =
                                    std::make_shared<BadMemoryByteSource>();
        CUploadRequest upload_request(path, p_bs);
        std::shared_ptr<ProgressListener> p_pl =
                                std::make_shared<StdoutProgressListener>(false);
        // Sometimes we check with a progress listener, sometimes w/o:
        if (utilities::Random() < 0.5) {
            upload_request.set_progress_listener(p_pl);
        }
        try {
            p_storage_->Upload(upload_request);
            FAIL() << "Uploading from a raising bad source should raise";
        }
        catch (CStorageException&) {  // expected
            // Note that we do not always get here the exception
            // thrown by BadMemorySource
            LOG_DEBUG << "OK, upload failed as expected with exception: "
                      << CurrentExceptionToString();
        }
    });
}

/**
* Try to upload from a bad ByteSource and check for error.
* Here the source does not provide the expected number of bytes.
*/
TEST_P(BasicTest, TestUploadBadSourceStream) {
#if !defined _WIN32 && !defined _WIN64
    NOT_SUPPORTED_BY_OS("!= Windows", "Bug in cpprestsdk");
#endif
    WithRandomTestPath([&](CPath path) {
        LOG_INFO << "Will try to upload a blob from a short bad source to "
                 << path;
        int file_size = 50000;
        // At least one byte is missing, and up to full stream data:
        int missing_bytes = MiscUtils::Random(1, file_size+1);
        std::string content = MiscUtils::GenerateRandomData(file_size);
        std::shared_ptr<ByteSource> p_bs =
                std::make_shared<BadMemoryByteSource>(content, missing_bytes);
        CUploadRequest upload_request(path, p_bs);
        std::shared_ptr<ProgressListener> p_pl =
                            std::make_shared<StdoutProgressListener>(false);
        // Sometimes we check with a progress listener, sometimes w/o:
        if (utilities::Random() < 0.5) {
            upload_request.set_progress_listener(p_pl);
        }
        try {
            p_storage_->Upload(upload_request);
            FAIL() << "Uploading from a short bad source should raise";
        }
        catch (CStorageException&) {  // expected
            LOG_DEBUG << "OK, upload failed as expected with exception: "
                      << CurrentExceptionToString();
        }
    });
}

}  // namespace pcs_api

