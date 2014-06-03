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
 * This class holds user_id and credentials (password or OAuth2 tokens). Credentials is a dictionary here.
 */
public class UserCredentials<T extends Credentials>
{

    private final AppInfo appInfo;
    private final T credentials;
    private String userId;

    public UserCredentials( AppInfo appInfo, String userId, T credentials )
    {
        this.appInfo = appInfo;
        this.userId = userId;
        this.credentials = credentials;
    }

    /**
     * Get the user credentials
     *
     * @return The credentials
     */
    public T getCredentials()
    {
        return credentials;
    }

    /**
     * Get the application informations
     *
     * @return The informations
     */
    public AppInfo getAppInfo()
    {
        return appInfo;
    }

    /**
     * Get the user identifier
     *
     * @return The user identifier
     */
    public String getUserId()
    {
        return userId;
    }

    /**
     * Set the user identifier
     *
     * @param userId The user identifier
     */
    public void setUserId( String userId )
    {
        this.userId = userId;
    }

    @Override
    public String toString()
    {
        return "UserCredentials{"
               + "appInfo=" + appInfo
               + ", userId='" + userId + '\''
               + ", credentials=" + credentials
               + '}';
    }

}
