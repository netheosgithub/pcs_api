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

package net.netheos.pcsapi.credentials;

import java.io.IOException;

/**
 * Represents a repository that contains user credentials (oauth2 or login/password).
 */
public interface UserCredentialsRepository
{

    /**
     * Adds new credentials and serializes
     *
     * @param userCredentials The credentials to save
     * @throws IOException Error saving the credentials
     */
    void save( UserCredentials<?> userCredentials )
            throws IOException;

    /**
     * Retrieves user credentials for the given application and optional user id. If repository contains only one user
     * credential for the given application, userId may be left unspecified.
     *
     * @param appInfo The application informations
     * @param userId The user identifier
     * @return user credentials The credentials
     */
    UserCredentials<?> get( AppInfo appInfo, String userId );

}
