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
 * Implementation for login/password authenticated users : in this case application name can be set to 'login'.
 */
public class PasswordAppInfo
        implements AppInfo
{

    private final String providerName;
    private final String appName;

    /**
     * Constructor for login/password clients
     *
     * @param providerName lower case name of provider
     * @param appName Name of application, as registered on provider.
     */
    public PasswordAppInfo( String providerName, String appName )
    {
        this.providerName = providerName;
        this.appName = appName;
    }

    @Override
    public String getProviderName()
    {
        return providerName;
    }

    @Override
    public String getAppName()
    {
        return appName;
    }

    @Override
    public String toString()
    {
        return "PasswordAppInfo{"
               + "providerName='" + providerName + '\''
               + ", appName='" + appName + "'}";
    }

}
