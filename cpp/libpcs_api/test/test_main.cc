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

#include <vector>
#include <string>

#include "boost/algorithm/string.hpp"
#include "boost/lexical_cast.hpp"

#include "pcs_api/storage_facade.h"

#include "gtest/gtest.h"

#include "functional_base_test.h"

/**
 * Global variables (read in tests)
 */
std::vector<std::string> g_providers_to_be_tested;
std::chrono::seconds g_test_duration;
unsigned int g_nb_threads;

unsigned int g_nb_ignored_tests;

void usage(char *p_progname) {  // Does not return
    std::cout << "Program usage:" << std::endl;
    std::cout << "Non gtest options are:" << std::endl;
    std::cout << "--providers p1,p2,p3    "
                 "Only test for the specified providers (default is all)."
              << std::endl;
    std::cout << "            p1,p2,p3 is "
                 "comma separated list of providers names"
              << std::endl;
    std::cout << "--test_duration N       "
                 "Number of seconds for stress tests (default is 60)."
              << std::endl;
    std::cout << "--nb_thread N           "
                 "Number of threads for stress tests (default is 4)."
              << std::endl;
    std::cout << std::endl;

    std::cout << "Generic gtest options:" << std::endl;
    const char **argv = new const char *[2];
    argv[0] = p_progname;
    argv[1] = "--help";
    int argc = 2;
    ::testing::InitGoogleTest(&argc, (char **)argv);  // NOLINT
    exit(1);
}

int main(int argc, char **argv) {
    // Handle list of providers to be tested (defaults to all)
    // This must be done before InitGoogleTest() is called below
    // by default, test all providers:
    g_providers_to_be_tested = pcs_api::StorageFacade::GetRegisteredProviders();
    g_test_duration = std::chrono::seconds(60);
    g_nb_threads = 4;

    for (int i = 1; i < argc ; ++i) {
        if (strcmp(argv[i], "--providers") == 0) {
            if (i == argc-1) {
                std::cerr << "--providers option expects a comma"
                             " separated list of providers" << std::endl;
                usage(argv[0]);
            }
            std::string providers = argv[++i];
            // split under , :
            boost::algorithm::split(g_providers_to_be_tested,
                                    providers,
                                    boost::is_any_of(","));
        } else if (strncmp(argv[i], "--providers=", 12) == 0) {
            std::string providers = std::string(argv[i] + 12);
            // split under , :
            boost::algorithm::split(g_providers_to_be_tested,
                                    providers,
                                    boost::is_any_of(","));
        } else if (strcmp(argv[i], "--test_duration") == 0) {
            if (i == argc - 1) {
                std::cerr << "--test_duration option expects an argument"
                          << std::endl;
                usage(argv[0]);
            }
            int duration_s = boost::lexical_cast<int>(argv[++i]);
            g_test_duration = std::chrono::seconds(duration_s);
        } else if (strcmp(argv[i], "--nb_thread") == 0) {
            if (i == argc - 1) {
                std::cerr << "--nb_thread option expects an argument"
                          << std::endl;
                usage(argv[0]);
            }
            g_nb_threads = boost::lexical_cast<unsigned int>(argv[++i]);
        } else if (strncmp(argv[i], "--gtest_", 8) != 0) {
            // Unknown option (may be --help)
            usage(argv[0]);
        }
    }
    std::cout << "Will run tests with the following providers:" << std::endl;
    for (std::string provider : g_providers_to_be_tested) {
        std::cout << "- " << provider << std::endl;
    }

    ::testing::InitGoogleTest(&argc, argv);

    int exit_value = RUN_ALL_TESTS();

    // Show number of ignored tests:
    if (g_nb_ignored_tests > 0) {
        std::cout << "  You have dynamically ignored "
                  << g_nb_ignored_tests << " test(s)." << std::endl;
    }
    return exit_value;
}
