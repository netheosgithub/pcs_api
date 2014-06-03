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

/**
 * Represents a repository that contains the application (client) info
 */
public interface AppInfoRepository
{

    /**
     * Retrieves application information for the specified provider. The application name is not provided here because
     * it search the default application in the repository using the provider.
     *
     * @param providerName The storage provider name
     * @return appInfo The application informations
     */
    AppInfo get( String providerName );

    /**
     * Retrieves application information for the specified provider and optional application.
     * <p/>
     * If repository contains only one application for the specified provider, appName may be left unspecified.
     *
     * @param providerName The storage provider name
     * @param appName The application name (if null, the default application found in repository will be used)
     * @return appInfo The application informations
     */
    AppInfo get( String providerName, String appName );

}
