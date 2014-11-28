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


#include <memory>
#include <algorithm>
#include <exception>
#include <deque>


#include "boost/log/core.hpp"
#include "boost/log/trivial.hpp"
#include "boost/log/expressions.hpp"
#include "boost/log/utility/setup/from_settings.hpp"
#include "boost/log/utility/setup/from_stream.hpp"
#include "boost/log/utility/setup/common_attributes.hpp"
#include "boost/log/utility/setup/console.hpp"
#include "boost/program_options/cmdline.hpp"
#include "boost/program_options/options_description.hpp"
#include "boost/program_options/parsers.hpp"
#include "boost/program_options/variables_map.hpp"
namespace po = boost::program_options;

#include "pcs_api/model.h"
#include "pcs_api/storage_facade.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/memory_byte_sink.h"
#include "pcs_api/memory_byte_source.h"
#include "pcs_api/file_byte_sink.h"
#include "pcs_api/stdout_progress_listener.h"
#include "pcs_api/app_info_file_repository.h"
#include "pcs_api/user_credentials_file_repository.h"
#include "pcs_api/oauth2_bootstrapper.h"


using namespace pcs_api;


static po::variables_map ParseCliOptions(int argc, char*argv[]);
static void InitLogging(int verbose_level);
static std::list<std::shared_ptr<CFile>> FilterFilesByType(
                                                const CFolderContent& content,
                                                bool keep_blobs);



int main(int argc, char *argv[]) {
    po::variables_map cli_options = ParseCliOptions(argc, argv);
    std::cout << "Current working dir = " << boost::filesystem::current_path()
              << std::endl;
    try {
        int verbose_level = 0;
        if (cli_options.count("verbose") > 0) {
            verbose_level = cli_options["verbose"].as<int>();
        }
        InitLogging(verbose_level);

        std::string provider_name = "dropbox";
        if (cli_options.count("provider_name") > 0) {
            provider_name = cli_options["provider_name"].as<std::string>();
        }
        bool bootstrapping = cli_options.count("bootstrap") > 0;

        const char *p_pcs_api_repo_dir = getenv("PCS_API_REPOSITORY_DIR");
        if (p_pcs_api_repo_dir == NULL) {
            p_pcs_api_repo_dir = "../../repositories";
        }
        boost::filesystem::path pcs_api_repo_path(p_pcs_api_repo_dir);
        std::shared_ptr<AppInfoRepository> p_appRepo =
               std::make_shared<AppInfoFileRepository>(
                                    pcs_api_repo_path / "app_info_data.txt");
        std::shared_ptr<UserCredentialsRepository> p_userRepo =
               std::make_shared<UserCredentialsFileRepository>(
                            pcs_api_repo_path / "user_credentials_data.txt");

        std::shared_ptr<IStorageProvider> p_storage =
            StorageFacade::ForProvider(provider_name)
                .app_info_repository(p_appRepo, "")
                .user_credentials_repository(p_userRepo, "")
                .for_bootstrapping(bootstrapping)
                .Build();
        std::cout << "Instantiated storage for provider: "
                  << p_storage->GetProviderName() << std::endl;

        if (bootstrapping) {
            OAuth2Bootstrapper bootstrapper(p_storage);
            std::string authorize_url = bootstrapper.GetAuthorizeBrowserUrl();
            std::cout << "Please go to this URL with your browser: "
                      << std::endl << authorize_url << std::endl;
            std::cout << "and copy/paste code or redirect URL"
                         " after authorization: " << std::endl;
            std::string code_or_url;
            std::cin >> code_or_url;
            bootstrapper.GetUserCredentials(code_or_url);
        }

        std::cout << "UserId: " << p_storage->GetUserId() << std::endl;
        std::cout << "Quota: " << p_storage->GetQuota() << std::endl;

        // Recursively list all regular files (blobs in folders)
        std::deque<CPath> folders_to_process;  // in order to avoid recursion
        folders_to_process.push_back(CPath(PCS_API_STRING_T("/")));
        std::shared_ptr<CBlob> largest_blob;
        while (!folders_to_process.empty()) {
            CPath f = folders_to_process.front();
            folders_to_process.pop_front();

            // List folder
            std::cout << "Content of folder: " << f.path_name_utf8()
                      << std::endl;
            std::shared_ptr<CFolderContent> content = p_storage->ListFolder(f);
            if (!content) {
                std::cout << "ERROR: no content for folder "
                          << f.path_name_utf8()
                          << " (deleted in background ?)" << std::endl;
                continue;
            }
            std::list<std::shared_ptr<CFile>> blobs =
                                            FilterFilesByType(*content, true);
            std::list<std::shared_ptr<CFile>> folders =
                                            FilterFilesByType(*content, false);

            // Print blobs of this folder
            for (std::shared_ptr<CFile> file : blobs) {
                std::shared_ptr<CBlob> blob =
                                        std::dynamic_pointer_cast<CBlob>(file);
                std::cout << "  " << *blob << std::endl;
                if (!largest_blob || blob->length() > largest_blob->length()) {
                    largest_blob = blob;
                }
            }
            for (std::shared_ptr<CFile> folder : folders) {
                std::cout << "  " << *folder << std::endl;
                // Add folder path to queue
                folders_to_process.push_back(folder->path());
            }
            std::cout << std::endl;
        }

        // Create a new folder
        CPath fpath(PCS_API_STRING_T("/pcs_api_new_folder"));
        std::cout << "Creating a folder: " << fpath << std::endl;
        p_storage->CreateFolder(fpath);

        // Upload a local file in this folder
        std::cout << "Uploading some data into this new folder..." << std::endl;
        CPath bpath = fpath.Add(PCS_API_STRING_T("pcs_api_new_file"));
        std::string file_content = "this is file content...";
        CUploadRequest upload_request(bpath,
                            std::make_shared<MemoryByteSource>(file_content));
        upload_request.set_content_type(PCS_API_STRING_T("text/plain"));
        p_storage->Upload(upload_request);

        // Download back the file
        std::cout << "Upload done. Downloading and checking content..."
                  << std::endl;
        std::shared_ptr<MemoryByteSink> mbs =
                                            std::make_shared<MemoryByteSink>();
        CDownloadRequest download_request(bpath, mbs);
        p_storage->Download(download_request);
        if (file_content == mbs->GetData()) {
            std::cout << "OK, data match." << std::endl;
        } else {
            std::cerr << "ERROR: Downloaded data is different from source !"
                      << std::endl;
        }

        // Delete remote folder
        std::cout << "Deleting folder " << fpath << std::endl;
        p_storage->Delete(fpath);

        // Example : we'll download a range of largest blob
        if (largest_blob) {
            int64_t range_start = largest_blob->length() / 2;
            int64_t range_length = std::min(largest_blob->length()/2,
                                            (int64_t)1000000);
            std::shared_ptr<FileByteSink> bs =
                            std::make_shared<FileByteSink>("dest_file.txt");
            std::cout << "Will download range from largest blob: "
                      << *largest_blob
                      << " to file: " << bs->path() << std::endl;

            CDownloadRequest dr(largest_blob->path(), bs);
            dr.set_progress_listener(
                                    std::make_shared<StdoutProgressListener>());
            dr.SetRange(range_start, range_length);
            p_storage->Download(dr);
            std::cout << "Download successful, created local file: "
                      << bs->path()
                      << " with size: " << boost::filesystem::file_size(bs->path())
                      << " bytes." << std::endl;
        }
    } catch (std::exception&) {
        std::cerr << "ERROR: catch exception: "
                  << CurrentExceptionToString() << std::endl;
    } catch (...) {
        std::cout << "in catch (...) ?!" << std::endl;
        throw;
    }

    // only to reduce valgrind warnings about false leaks:
    std::cout.imbue(std::locale());

    //system("pause");
    return 0;
}

static void usage(const po::options_description& desc) {
    std::cout << "Usage: sample [provider_name] [options]" << std::endl;
    std::cout << "Available providers:" << std::endl;
    for (std::string p : StorageFacade::GetRegisteredProviders()) {
        std::cout << "  - " << p << std::endl;
    }
    std::cout << "Default provider_name is dropbox" << std::endl;
    std::cout << desc << std::endl;
    exit(1);
}

static po::variables_map ParseCliOptions(int argc, char*argv[]) {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("app_name,a", po::value<std::string>(),
            "define application name (only required if several exists)")
        ("user_id,u", po::value<std::string>(),
            "define user id (only required if several exists)")
        ("bootstrap,b",
            "Do an OAuth2 code authorization workflow before using provider")
        ("verbose,v", po::value<int>()->implicit_value(1),
            "Set library verbose level"
            " (-2=ERROR, -1=WARN, 0=INFO, 1=DEBUG, 2=TRACE)")
    ;  // NOLINT
    po::options_description hidden;
    hidden.add_options()
        ("provider_name", po::value<std::string>()->implicit_value("dropbox"),
            "Provider name")
    ;  // NOLINT
    po::positional_options_description p;
    p.add("provider_name", -1);  // first positional param is provider name

    po::variables_map variables_map;
    try {
        po::options_description all;
        all.add(desc).add(hidden);
        po::store(po::command_line_parser(argc, argv)
                    .options(all)
                    .positional(p)
                    .run(), variables_map);
        po::notify(variables_map);
    }
    catch (std::exception& e) {
        std::cerr << "Invocation error: " << e.what() << std::endl;
        usage(desc);
    }
    if (variables_map.count("help")) {
        usage(desc);
    }

    return variables_map;
}

/**
 * Setup core logging filter level, based on program options.
 *
 * -v increase log level, whereas -q diminishes it. Default is INFO.
 *
 * @param argc
 * @param argv
 */
static void InitLogging(int verbose_level) {
    // must be done first:
    boost::log::register_simple_formatter_factory<
                    boost::log::trivial::severity_level, char >("Severity");

    std::ifstream settings("logging.conf");
    if (settings.is_open()) {
        boost::log::init_from_stream(settings);
    } else {
        std::cout << "InitLogging() could not open logging.conf file:"
                     " will use default log settings" << std::endl;
        boost::log::settings setts;
        setts["Core"]["DisableLogging"] = false;
        setts["Sinks.Console"]["Destination"] = "Console";
        // setts["Sinks.Console"]["AutoFlush"] = true;
        // setts["Sinks.Console"]["Format"] = "[%TimeStamp%] [%Severity%]"
        //                                    " :: (%Scope%) :: %Message%";
        setts["Sinks.Console"]["Format"] = "[%TimeStamp%] [%Severity%]"
                                           " :: %Message%";
        boost::log::init_from_settings(setts);
    }

    auto min_level = boost::log::trivial::info;
    switch (verbose_level) {
        case -2:
            min_level = boost::log::trivial::error;
            break;
        case -1:
            min_level = boost::log::trivial::warning;
            break;
        case 0:
            min_level = boost::log::trivial::info;
            break;
        case 1:
            min_level = boost::log::trivial::debug;
            break;
        case 2:
            min_level = boost::log::trivial::trace;
            break;
    }
    boost::log::core::get()->set_filter(
                                boost::log::trivial::severity >= min_level);

    boost::log::core::get()->add_global_attribute(
                            "Scope", boost::log::attributes::named_scope());
    boost::log::add_common_attributes();
}

/**
 * Filter a list of files by type (blob or folder)
 */
static std::list<std::shared_ptr<CFile>> FilterFilesByType(
                            const CFolderContent& content, bool keep_blobs) {
    std::list<std::shared_ptr<CFile>> ret;
    for (auto it = content.cbegin(); it != content.cend() ; ++it) {
        auto file = it->second;
        if (   (file->IsBlob() && keep_blobs)
            || (file->IsFolder() && !keep_blobs)) {
            ret.push_back(file);
        }
    }
    return ret;
}

