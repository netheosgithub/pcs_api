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

#include "boost/date_time/posix_time/posix_time_io.hpp"
#include "boost/property_tree/xml_parser.hpp"
#include "boost/property_tree/ptree.hpp"
#include "boost/foreach.hpp"
#include "boost/algorithm/string.hpp"

#include "cpprest/json.h"
#include "cpprest/uri.h"
#include "cpprest/uri_builder.h"
#include "cpprest/http_msg.h"
#include "cpprest/asyncrt_utils.h"
#include "cpprest/interopstream.h"

#include "pcs_api/internal/providers/cloudme.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/c_folder_content_builder.h"
#include "pcs_api/internal/storage_provider.h"
#include "pcs_api/internal/json_utils.h"
#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/multipart_streambuf.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

const char* CloudMe::kProviderName = "cloudme";

static const char_t *kBaseUrl = U("https://www.cloudme.com/v1");
static const char *kSoapHeader = "<SOAP-ENV:Envelope xmlns:"
                    "SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                    "SOAP-ENV:encodingStyle=\"\" "
                    "xmlns:xsi=\"http://www.w3.org/1999/XMLSchema-instance\" "
                    "xmlns:xsd=\"http://www.w3.org/1999/XMLSchema\">"
                    "<SOAP-ENV:Body>";
static const char *kSoapFooter = "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

StorageBuilder::create_provider_func CloudMe::GetCreateInstanceFunction() {
    return CloudMe::CreateInstance;
}

std::shared_ptr<IStorageProvider> CloudMe::CreateInstance(
        const StorageBuilder& builder) {
    // make_shared not usable because constructor is private
    return std::shared_ptr<IStorageProvider>(new CloudMe(builder));
}


CloudMe::CloudMe(const StorageBuilder& builder) :
    StorageProvider{builder.provider_name(),
                    std::make_shared<PasswordSessionManager>(builder,
                                              web::uri(kBaseUrl).authority()),
                    builder.retry_strategy()} {
}

void CloudMe::ThrowCStorageException(CResponse *p_response,
                                     const CPath* p_opt_path) {
    /**
    * soap errors generates http 500 Internal server errors,
    * and body looks like :
    * <code>
    * <?xml version='1.0' encoding='utf-8'?>
    * <SOAP-ENV:Envelope xmlns:SOAP-ENV='http://schemas.xmlsoap.org/soap/envelope/'
    * SOAP-ENV:encodingStyle='http://schemas.xmlsoap.org/soap/encoding/'>
    * <SOAP-ENV:Body>
    * <SOAP-ENV:Fault>
    * <faultcode>SOAP-ENV:Client</faultcode>
    * <faultstring>Not Found</faultstring>
    * <detail>
    * <error number='0' code='404' description='Not Found'>Document not found.</error>
    * </detail>
    * </SOAP-ENV:Fault>
    * </SOAP-ENV:Body>
    * </SOAP-ENV:Envelope>
    * </code>
    */
    std::string message;
    bool retriable = false;
    if (p_response->IsXmlContentType()) {
        boost::property_tree::ptree dom = p_response->AsDom();
        const boost::property_tree::ptree& fault =
              dom.get_child("SOAP-ENV:Envelope.SOAP-ENV:Body.SOAP-ENV:Fault");
        if (fault.count("faultcode") > 0) {
            const std::string& fault_code = fault.get<std::string>("faultcode");
            if (fault_code == "SOAP-ENV:Client") {
                // In case we have no details:
                message = fault.get<std::string>("faultstring");
            }
        }
        // better message if possible:
        if (fault.count("detail") > 0) {
            const boost::property_tree::ptree& detail_error =
                                               fault.get_child("detail.error");
            if (detail_error.count("<xmlattr>") > 0) {
                const std::string& code = detail_error.
                                     get<std::string>("<xmlattr>.code");
                const std::string& reason = detail_error.
                                     get<std::string>("<xmlattr>.description");
                const std::string number = detail_error.
                                     get<std::string>("<xmlattr>.number");
                message = "[" + code + " " + reason + " " + number + "] "
                          + detail_error.data();
                if (p_opt_path) {
                    message += " (" + p_opt_path->path_name_utf8() + ")";
                }
                if (code == "404") {
                    if (!p_opt_path) {  // should not happen
                        BOOST_THROW_EXCEPTION(CStorageException(
                                        "Unexpected 404 error without path"));
                    }
                    BOOST_THROW_EXCEPTION(CFileNotFoundException(message,
                                                                 *p_opt_path));
                }
            }
        }
        // These errors are not retriable,
        // as we have received a well formed response
    } else {
        // We haven't received a standard server error message ?!
        // This can happen unlikely. Usually such errors are temporary.
        std::string string_response = p_response->AsString();
        LOG_ERROR << "Unparsable server error: " << string_response;
        // Construct some message for exception:
        message = utilities::Abbreviate("Unparsable server error: "
                                                      + string_response, 200);
        if (p_response->status() >= 500) {
            retriable = true;
        }
    }

    if (!retriable) {
        p_response->ThrowCStorageException(message, p_opt_path);
    }
    try {
        p_response->ThrowCStorageException(message, p_opt_path);
    }
    catch (const std::exception&) {
        BOOST_THROW_EXCEPTION(CRetriableException(std::current_exception()));
    }
}

/**
* \brief Validate a response from CloudMe.
*
* A response is valid if server code is 2xx.
*/
void CloudMe::ValidateCloudMeResponse(CResponse *p_response,
                                      const CPath* p_opt_path) {
    LOG_DEBUG << "Validating CloudMe response: " << p_response->ToString();

    if (p_response->status() >= 300) {
        // Determining if error is retriable
        // is not possible without parsing response:
        ThrowCStorageException(p_response, p_opt_path);
    }
    // OK, response looks fine
}

/**
* \brief Validate a response from CloudMe API.
*
* A response is valid if server code is 2xx and content-type XML.
*/
void CloudMe::ValidateCloudMeApiResponse(CResponse *p_response,
                                         const CPath* p_opt_path) {
    ValidateCloudMeResponse(p_response, p_opt_path);
    // Also check content type:
    p_response->EnsureContentTypeIsXml(true);
    // OK, API response looks fine
}

/**
 * \brief An invoker that only check server status:
 * to be used for files downloads.
 *
 * @param p_path context for the request: path of remote file
 * @return a basic request invoker
 */
RequestInvoker CloudMe::GetBasicRequestInvoker(const CPath* p_path) {
    // Request execution is delegated to our session manager,
    // response validation is done here:
    return RequestInvoker(std::bind(&PasswordSessionManager::Execute,
                                    p_session_manager_.get(),
                                    std::placeholders::_1),  // do_request_func
                          std::bind(&CloudMe::ValidateCloudMeResponse,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2),  // validate_func
                          p_path);
}

/**
 * \brief An invoker that checks response content type = xml:
 * to be used by all API requests.
 *
 * @param p_opt_path (optional) context for the request: path of remote file
 * @return a request invoker specific for API requests
 */
RequestInvoker CloudMe::GetApiRequestInvoker(const CPath* p_opt_path) {
    // Request execution is delegated to our session manager,
    // response validation is done here:
    return RequestInvoker(std::bind(&PasswordSessionManager::Execute,
                                    p_session_manager_.get(),
                                    std::placeholders::_1),  // do_request_func
                          std::bind(&CloudMe::ValidateCloudMeApiResponse,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2),  // validate_func
                          p_opt_path);
}

web::http::http_request BuildSoapRequest(const std::string& action,
                                         const std::string& inner_xml) {
    web::http::http_request request(web::http::methods::POST);
    request.set_request_uri(web::uri(kBaseUrl));
    request.headers().add(U("soapaction"),
                          utility::conversions::to_string_t(action));
    request.headers().set_content_type(U("text/xml; charset=utf-8"));

    std::ostringstream soap;
    soap << kSoapHeader;
    soap << "<" << action << ">";
    soap << inner_xml;
    soap << "</" << action << ">";
    soap << kSoapFooter;
    request.set_body(soap.str());
// binary dump of request:
// {
// std::string body = soap.str();
// const char *p_str = body.c_str();
// std::cout << "POST soap body = " <<std::endl << p_str << std::endl;
// std::cout << "POST soap HEX body = " << std::endl;
// for (size_t i = 0; i<body.length(); i++) {
//     if (p_str[i] == '\r' || p_str[i] == '\n') {
//         std::cout.put(p_str[i]);
//     }
//     else {
//         char tmp[10];
//         sprintf(tmp, "%02x ", p_str[i] & 0xff);
//         std::cout << tmp;
//     }
// }
// std::cout << std::endl;
// }
    return request;
}

const boost::property_tree::ptree CloudMe::GetLogin() {
    string_t url = kBaseUrl;
    RequestInvoker ri = GetApiRequestInvoker();
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request = BuildSoapRequest("login", "");
        p_response = ri.Invoke(request);
    });
    return p_response->AsDom();
}

std::string CloudMe::GetRootId() {
    if (root_id_.empty()) {
        boost::property_tree::ptree dom = GetLogin();
        root_id_ = dom.get<std::string>(
                "SOAP-ENV:Envelope.SOAP-ENV:Body.xcr:loginResponse.home");
    }
    return root_id_;
}

std::string CloudMe::GetUserId() {
    boost::property_tree::ptree dom = GetLogin();
    std::string user_id = dom.get<std::string>(
            "SOAP-ENV:Envelope.SOAP-ENV:Body.xcr:loginResponse.username");
    return user_id;
}

CQuota CloudMe::GetQuota() {
    boost::property_tree::ptree dom = GetLogin();
    const boost::property_tree::ptree& drive = dom.get_child(
            "SOAP-ENV:Envelope.SOAP-ENV:Body.xcr:loginResponse.drives.drive");
    int64_t used = drive.get<int64_t>("currentSize");
    int64_t limit = drive.get<int64_t>("quotaLimit");
    return CQuota(used, limit);
}

std::unique_ptr<CloudMe::CMFolder> CloudMe::LoadFoldersStructure() {
    std::unique_ptr<CMFolder> root_folder(new CMFolder(GetRootId()));

    RequestInvoker ri = GetApiRequestInvoker();
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request = BuildSoapRequest(
                "getFolderXML",
                "<folder id='" + root_folder->id() + "'/>");
        p_response = ri.Invoke(request);
    });
    boost::property_tree::ptree dom = p_response->AsDom();

    const boost::property_tree::ptree& root_element = dom.get_child(
          "SOAP-ENV:Envelope.SOAP-ENV:Body.xcr:getFolderXMLResponse.fs:folder");
    ScanFolderLevel(root_element, root_folder.get());

    return root_folder;
}

/**
 * Recursive method that parses folders XML and builds CMFolder structure.
 *
 * @param element the xml structure to parse
 * @param p_cm_folder the object to fill (add children)
 */
void CloudMe::ScanFolderLevel(const boost::property_tree::ptree& element,
                              CloudMe::CMFolder *p_cm_folder) {
    BOOST_FOREACH(auto v, element) {
        if (v.first != "fs:folder") {
            continue;
        }

        CMFolder* p_child_folder = p_cm_folder->AddChild(
                v.second.get<std::string>("<xmlattr>.id"),
                v.second.get<std::string>("<xmlattr>.name"));
        ScanFolderLevel(v.second, p_child_folder);
    }
}

/**
 * \brief Get a blob according to the parent folder id of the folder.
 *
 * @param cm_folder parent folder
 * @param base_name base name of the file to find
 * @return the CloudMe blob, or null if not found
 */
std::unique_ptr<CloudMe::CMBlob> CloudMe::GetBlobByName(
        const CloudMe::CMFolder* p_cm_folder,
        const std::string& base_name) {
    std::ostringstream inner_xml;
    inner_xml << "<folder id='" << p_cm_folder->id() << "'/>"
        << "<query>"
        << "\"" << utilities::EscapeXml(base_name) << "\""
        << "</query>"
        << "<count>1</count>";

    CPath folder_path = p_cm_folder->GetPath();
    RequestInvoker ri = GetApiRequestInvoker(&folder_path);
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request = BuildSoapRequest("queryFolder",
                                                           inner_xml.str());
        p_response = ri.Invoke(request);;
    });

    boost::property_tree::ptree dom = p_response->AsDom();
    boost::property_tree::ptree& feed_element = dom.get_child(
          "SOAP-ENV:Envelope.SOAP-ENV:Body.xcr:queryFolderResponse.atom:feed");
    if (feed_element.count("atom:entry") == 0) {
        return std::unique_ptr<CloudMe::CMBlob>();  // no file: empty pointer
    }
    boost::property_tree::ptree& xml_entry = feed_element.
            get_child("atom:entry");
    return CMBlob::BuildFromXmlElement(*p_cm_folder, xml_entry);
}

/**
* Lists all blobs (ie files that are not folders) in a given cloudme folder.
*
* @param cmFolder CloudMe folder to be listed
* @return list of all blobs
*/
std::vector<std::unique_ptr<CloudMe::CMBlob>> CloudMe::ListBlobs(
        const CloudMe::CMFolder& cm_folder) {
    CPath folder_path = cm_folder.GetPath();
    RequestInvoker ri = GetApiRequestInvoker(&folder_path);
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request = BuildSoapRequest(
                "queryFolder", "<folder id='" + cm_folder.id() + "'/>");
        p_response = ri.Invoke(request);
    });

    boost::property_tree::ptree dom = p_response->AsDom();
    boost::property_tree::ptree& feed_element = dom.get_child(
          "SOAP-ENV:Envelope.SOAP-ENV:Body.xcr:queryFolderResponse.atom:feed");
    std::vector<std::unique_ptr<CMBlob>> cm_blobs;
    BOOST_FOREACH(const auto& entry, feed_element) {
        if (entry.first != "atom:entry") {
            continue;
        }
        cm_blobs.push_back(CMBlob::BuildFromXmlElement(cm_folder,
                                                       entry.second));
    }

    return cm_blobs;
}



std::shared_ptr<CFolderContent> CloudMe::ListRootFolder() {
    return ListFolder(CPath(U("/")));
}

/**
 * There are 3 main steps to list a folder :
 * 1 - generate the tree view of CloudMe storage
 * 2 - list all the subfolders
 * 3 - list all the blobs
 */
std::shared_ptr<CFolderContent> CloudMe::ListFolder(const CPath& path) {
    // 1
    std::unique_ptr<CMFolder> cm_root = LoadFoldersStructure();
    const CMFolder* p_cm_folder = cm_root->GetFolder(path);

    if (p_cm_folder == nullptr) {
        // Folder does not exists; may be a blob though
        const CMFolder* p_cm_parent_folder =
            cm_root->GetFolder(path.GetParent());
        if (p_cm_parent_folder != nullptr) {
            std::unique_ptr<CMBlob> p_cm_blob =
                GetBlobByName(p_cm_parent_folder,
                      utility::conversions::to_utf8string(path.GetBaseName()));
            if (p_cm_blob) {  // A blob exists: error !
                BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, false));
            }
        }
        return std::shared_ptr<CFolderContent>();  // empty pointer
    }

    CFolderContentBuilder cfcb;

    // 2 - Adding folders
    for (const auto& f : p_cm_folder->children()) {
        const CMFolder& child_folder = f.second;
        std::shared_ptr<CFile> p_file = child_folder.ToCFolder();
        cfcb.Add(p_file->path(), p_file);
    }

    // 3 - Adding blobs
    std::vector<std::unique_ptr<CMBlob>> cm_blobs = ListBlobs(*p_cm_folder);
    for (const std::unique_ptr<CMBlob> & cm_blob : cm_blobs) {
        std::shared_ptr<CBlob> cblob = cm_blob->ToCBlob();
        cfcb.Add(cblob->path(), cblob);
    }

    return cfcb.BuildFolderContent();
}

std::shared_ptr<CFolderContent> CloudMe::ListFolder(const CFolder& folder) {
    return ListFolder(folder.path());
}

bool CloudMe::CreateFolder(const CPath& path) {
    if (path.IsRoot()) {
        return false;
    }

    std::unique_ptr<CMFolder> cm_root = LoadFoldersStructure();
    const CMFolder *p_cm_folder = cm_root->GetFolder(path);
    if (p_cm_folder) {
        // folder already exists
        return false;
    }

    CreateIntermediateFolders(cm_root.get(), path);
    return true;
}

CloudMe::CMFolder* CloudMe::CreateIntermediateFolders(
        CloudMe::CMFolder* p_cm_root, const CPath& cpath) {
    std::vector<string_t> base_names = cpath.Split();

    CMFolder *p_current_folder = p_cm_root;
    CMFolder* p_child_folder = nullptr;
    bool first_folder_creation = true;

    for (string_t base_name : base_names) {
        std::string base_name_utf8 =
                                utility::conversions::to_utf8string(base_name);
        p_child_folder = p_current_folder->GetChildByName(base_name_utf8);
        if (!p_child_folder) {
            // Intermediate folder does not exist: has to be created

            if (first_folder_creation) {
                // This is the first intermediate folder to create:
                // let's check that there no blob exists with that name
                std::unique_ptr<CMBlob> p_cm_blob =
                        GetBlobByName(p_current_folder, base_name_utf8);
                if (p_cm_blob) {
                    BOOST_THROW_EXCEPTION(
                        CInvalidFileTypeException(p_cm_blob->GetPath(), false));
                }
                first_folder_creation = false;
            }

            p_child_folder = RawCreateFolder(p_current_folder, base_name_utf8);
        }

        p_current_folder = p_child_folder;
    }

    return p_child_folder;
}

/**
* Issue request to create a folder with given parent folder and name.
* <p/>
* No checks are performed before request.
*
* @param p_cm_parent_folder pointer to parent of folder to create
* @param base_name base name of the folder to be created
* @return a pointer to the created CloudMe folder (added to parent folder
* children collection)
*/
CloudMe::CMFolder* CloudMe::RawCreateFolder(CMFolder *p_cm_parent_folder,
                                            const std::string& base_name) {
    std::ostringstream inner_xml;
    inner_xml << "<folder id='" << p_cm_parent_folder->id() << "'/>";
    inner_xml << "<childFolder>"
              << utilities::EscapeXml(base_name)
              << "</childFolder>";
    RequestInvoker ri = GetApiRequestInvoker();
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request = BuildSoapRequest("newFolder",
                                                           inner_xml.str());
        p_response = ri.Invoke(request);
    });

    boost::property_tree::ptree dom = p_response->AsDom();
    std::string new_folder_id = dom.get<std::string>(
          "SOAP-ENV:Envelope.SOAP-ENV:Body.xcr:newFolderResponse.newFolderId");

    return p_cm_parent_folder->AddChild(new_folder_id, base_name);
}

bool CloudMe::Delete(const CPath& path) {
    if (path.IsRoot()) {
        BOOST_THROW_EXCEPTION(CStorageException("Can't delete root folder"));
    }

    std::unique_ptr<CMFolder> p_cm_root = LoadFoldersStructure();
    CMFolder* p_cm_parent_folder = p_cm_root->GetFolder(path.GetParent());
    if (!p_cm_parent_folder) {
        // parent folder of given path does exist => path does not exist
        return false;
    }

    CMFolder* p_cm_folder = p_cm_parent_folder->GetChildByName(
                      utility::conversions::to_utf8string(path.GetBaseName()));
    if (p_cm_folder) {
        // We have to delete a folder
        std::ostringstream inner_xml;
        inner_xml << "<folder id='" << p_cm_parent_folder->id() << "'/>";
        inner_xml << "<childFolder id='" << p_cm_folder->id() << "'/>";
        RequestInvoker ri = GetApiRequestInvoker(&path);
        std::shared_ptr<CResponse> p_response;
        p_retry_strategy_->InvokeRetry([&] {
            web::http::http_request request = BuildSoapRequest("deleteFolder",
                                                               inner_xml.str());
            p_response = ri.Invoke(request);
        });
        boost::property_tree::ptree dom = p_response->AsDom();
        std::string result = dom.get<std::string>(
                "SOAP-ENV:Envelope.SOAP-ENV:Body.xcr:deleteFolderResponse");
        return boost::iequals(boost::algorithm::trim_copy(result), "ok");

    } else {
        // It's not a folder, it should be a blob...
        std::unique_ptr<CMBlob> p_cm_blob = GetBlobByName(p_cm_parent_folder,
                      utility::conversions::to_utf8string(path.GetBaseName()));
        if (!p_cm_blob) {
            // The blob does not exist... nothing to do
            return false;
        }
        std::ostringstream inner_xml;
        inner_xml << "<folder id='" << p_cm_parent_folder->id() << "'/>";
        inner_xml << "<document id='" << p_cm_blob->id() << "'/>";
        RequestInvoker ri = GetApiRequestInvoker(&path);
        std::shared_ptr<CResponse> p_response;
        p_retry_strategy_->InvokeRetry([&] {
            web::http::http_request request = BuildSoapRequest("deleteDocument",
                                                               inner_xml.str());
            p_response = ri.Invoke(request);
        });
        boost::property_tree::ptree dom = p_response->AsDom();
        std::string result = dom.get<std::string>(
                "SOAP-ENV:Envelope.SOAP-ENV:Body.xcr:deleteDocumentResponse");

        return boost::iequals(boost::algorithm::trim_copy(result), "ok");
    }
}

const string_t CloudMe::BuildRestUrl(const string_t& method_path) const {
    return string_t(kBaseUrl) + U('/') + method_path + U('/');
}

std::shared_ptr<CFile> CloudMe::GetFile(const CPath& path) {
    std::unique_ptr<CMFolder> cm_root = LoadFoldersStructure();
    CMFolder* p_cm_parent_folder = cm_root->GetFolder(path.GetParent());

    if (!p_cm_parent_folder) {
        // parent folder of given path does exist => path does not exist
        return std::shared_ptr<CFile>();  // empty pointer
    }

    const CMFolder* p_cm_folder = p_cm_parent_folder->GetChildByName(
                     utility::conversions::to_utf8string(path.GetBaseName()));
    if (p_cm_folder) {
        // the path corresponds to a folder
        return std::make_shared<CFolder>(path,
                                         boost::posix_time::not_a_date_time);
    } else {
        // It's not a folder, it should be a blob...
        std::unique_ptr<CMBlob> p_cm_blob = GetBlobByName(p_cm_parent_folder,
                      utility::conversions::to_utf8string(path.GetBaseName()));
        if (!p_cm_blob) {
            // No blob exists at this path
            return std::shared_ptr<CFile>();  // empty pointer
        }
        return p_cm_blob->ToCBlob();
    }
}

void CloudMe::Download(const CDownloadRequest& download_request) {
    const CPath& path = download_request.path();
    const std::string base_name_utf8 =
        utility::conversions::to_utf8string(path.GetBaseName());

    std::unique_ptr<CMFolder> p_cm_root = LoadFoldersStructure();
    CMFolder* p_cm_parent_folder = p_cm_root->GetFolder(path.GetParent());
    if (!p_cm_parent_folder) {
        // parent folder of given path does exist => file does not exist
        BOOST_THROW_EXCEPTION(CFileNotFoundException("This file does not exist",
                                                     path));
    }

    CMFolder* p_cm_folder = p_cm_parent_folder->GetChildByName(base_name_utf8);
    if (p_cm_folder) {
        // the path corresponds to a folder
        BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, true));
    }

    // It's not a folder, it should be a blob...
    std::unique_ptr<CMBlob> p_cm_blob = GetBlobByName(p_cm_parent_folder,
                                                      base_name_utf8);
    if (!p_cm_blob) {
        BOOST_THROW_EXCEPTION(CFileNotFoundException(
                "Can't download this file, it does not exist", path));
    }

    string_t url = BuildRestUrl(U("documents"))
                + utility::conversions::to_string_t(p_cm_parent_folder->id())
                + U('/')
                + utility::conversions::to_string_t(p_cm_blob->id()) + U("/1");
    web::uri uri(url);
    RequestInvoker ri = GetBasicRequestInvoker(&path);
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::GET);
        request.set_request_uri(uri);
        for (std::pair<string_t, string_t> header :
                download_request.GetHttpHeaders()) {
            request.headers().add(header.first, header.second);
        }
        p_response = ri.Invoke(request);
        std::shared_ptr<ByteSink> p_byte_sink = download_request.GetByteSink();
        p_response->DownloadDataToSink(p_byte_sink.get());
    });
}

void CloudMe::Upload(const CUploadRequest& upload_request) {
    const CPath& path = upload_request.path();
    std::string base_name_utf8 =
            utility::conversions::to_utf8string(path.GetBaseName());

    std::unique_ptr<CMFolder> p_cm_root = LoadFoldersStructure();
    CMFolder* p_cm_parent_folder = p_cm_root->GetFolder(path.GetParent());
    if (!p_cm_parent_folder) {
        // parent folder of given path does not exist =>
        // folders needs to be created
        p_cm_parent_folder = CreateIntermediateFolders(p_cm_root.get(),
                                                       path.GetParent());
    } else {
        // parent folder already exists:
        // check if a folder exists with same name
        CMFolder* p_cm_folder = p_cm_parent_folder->
                                    GetChildByName(base_name_utf8);
        if (p_cm_folder) {
            // The CPath corresponds to an existing folder,
            // upload is not possible
            BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, true));
        }
    }

    string_t url = BuildRestUrl(U("documents"))
            + utility::conversions::to_string_t(p_cm_parent_folder->id());
    web::uri uri(url);
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::POST);
        request.set_request_uri(uri);

        std::shared_ptr<ByteSource> p_source = upload_request.GetByteSource();
        MultipartStreamer mp_streamer;
        MultipartStreamer::Part file_part("bin", p_source.get());
        // filename is not escaped: this is how CloudMe works...
        // (quotes are not allowed in filenames)
        file_part.AddHeader("Content-Disposition",
            "form-data; name=\"bin\"; filename=\"" + base_name_utf8 + "\"");
        // useless, content type is ignored:
        file_part.AddHeader("Content-Type",
            utility::conversions::to_utf8string(upload_request.content_type()));
        mp_streamer.AddPart(file_part);

        // Prepare stream and adapt to cpprest:
        MultipartStreambuf mp_streambuf(&mp_streamer);
        // stream itself will not be used, only its streambuf:
        std::istream mp_istream(&mp_streambuf);
        // Adapt std::istream to asynchronous concurrency istream:
        concurrency::streams::stdio_istream<uint8_t> is_wrapper(mp_istream);

        request.set_body(is_wrapper,
            mp_streamer.ContentLength(),
            mp_streamer.ContentType());

        RequestInvoker ri = GetBasicRequestInvoker(&path);
        std::shared_ptr<CResponse> p_response = ri.Invoke(request);
        // We are not interested in response body
    });
}



// ============ Utility classes below

CloudMe::CMFolder::CMFolder(const std::string& id) :
    CMFolder(nullptr, id, "") {
}

CloudMe::CMFolder::CMFolder(const CMFolder *p_parent,
                            const std::string& id,
                            const std::string& name) :
    p_parent_{p_parent}, id_(id), name_(name) {
}

const std::shared_ptr<CFolder> CloudMe::CMFolder::ToCFolder() const {
    return std::make_shared<CFolder>(GetPath(),
                                     boost::posix_time::not_a_date_time);
}

const CPath CloudMe::CMFolder::GetPath() const {
    if (!p_parent_) {
        return CPath(U("/"));
    }

    const CMFolder *p_current_folder = this;
    std::string path;
    while (p_current_folder->p_parent_) {
        path.insert(0, p_current_folder->name());
        path.insert(0, "/");

        p_current_folder = p_current_folder->p_parent_;
    }

    return CPath(utility::conversions::to_string_t(path));
}

CloudMe::CMFolder* CloudMe::CMFolder::AddChild(const std::string& id,
                                               const std::string& name) {
    auto p = children_.insert(std::map<std::string, CMFolder>::value_type(
                                name,
                                CMFolder(this, id, name)));
    CloudMe::CMFolder* ret = &(*p.first).second;
    return ret;
}


CloudMe::CMFolder* CloudMe::CMFolder::GetFolder(const CPath& path) {
    if (path.IsRoot()) {
        return this;
    }
    std::vector<string_t> base_names = path.Split();

    CMFolder *p_current_folder = this;
    CMFolder *p_sub_folder = nullptr;

    for (string_t base_name : base_names) {
        p_sub_folder = p_current_folder->
                GetChildByName(utility::conversions::to_utf8string(base_name));
        if (!p_sub_folder) {
            return nullptr;
        }
        p_current_folder = p_sub_folder;
    }

    return p_current_folder;
}

CloudMe::CMFolder* CloudMe::CMFolder::GetChildByName(const std::string& name) {
    auto it = children_.find(name);
    if (it != children_.end()) {
        return &it->second;
    }
    return nullptr;
}


std::unique_ptr<CloudMe::CMBlob> CloudMe::CMBlob::BuildFromXmlElement(
        const CMFolder& folder,
        const boost::property_tree::ptree& xml_entry) {
    std::string name = xml_entry.get<std::string>("atom:title");
    std::string id = xml_entry.get<std::string>("dc:document");
    // example: 2013-12-06T11:26:38Z
    std::string updated_str = xml_entry.get<std::string>("atom:updated");
    std::locale loc(std::locale::classic(),
                 new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%S"));
    std::istringstream is(updated_str);
    is.imbue(loc);
    ::boost::posix_time::ptime modified;
    is >> modified;

    boost::property_tree::ptree link_element = xml_entry.get_child("atom:link");
    int64_t length = link_element.get<int64_t>("<xmlattr>.length");
    std::string content_type = link_element.get<std::string>("<xmlattr>.type");

    return std::unique_ptr<CMBlob>(new CMBlob(folder, id, name, length,
                                              modified, content_type));
}

CloudMe::CMBlob::CMBlob(const CMFolder& folder,
        const std::string& id,
        const std::string& name,
        int64_t length,
        const ::boost::posix_time::ptime& updated,
        const std::string& content_type) :
    folder_{folder},
    id_(id),
    name_(name),
    length_{length},
    updated_{updated},
    content_type_(content_type) {
}

const CPath CloudMe::CMBlob::GetPath() const {
    return folder_.GetPath().Add(utility::conversions::to_string_t(name_));
}

const std::shared_ptr<CBlob> CloudMe::CMBlob::ToCBlob() const {
    std::shared_ptr<CBlob> p_blob = std::make_shared<CBlob>(
                            GetPath(),
                            length_,
                            utility::conversions::to_string_t(content_type_),
                            updated_);
    return p_blob;
}


}  // namespace pcs_api
