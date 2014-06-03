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

import java.util.List;

/**
 * This class holds application information for OAuth2 providers (web application workflow).
 */
public class OAuth2AppInfo
        implements AppInfo
{

    private final String providerName;
    private final String appName;
    private final String appId;
    private final String appSecret;
    private final List<String> scope;
    private final String redirectUrl;

    /**
     * Constructor for OAuth clients
     *
     * @param providerName lower case name of provider
     * @param appName Name of application, as registered on provider.
     * @param appId If OAuth provider, application ID (may be null)
     * @param appSecret If OAuth provider, application secret (may be null)
     * @param scope If OAuth provider, list of permissions asked by application (may be null)
     * @param redirectUrl If OAuth provider, application URL (for web application workflow) (may be null)
     */
    public OAuth2AppInfo( String providerName, String appName, String appId, String appSecret, List<String> scope, String redirectUrl )
    {
        this.providerName = providerName;
        this.appName = appName;
        this.appId = appId;
        this.appSecret = appSecret;
        this.scope = scope;
        this.redirectUrl = redirectUrl;
    }

    public String getAppId()
    {
        return appId;
    }

    public String getAppSecret()
    {
        return appSecret;
    }

    public String getRedirectUrl()
    {
        return redirectUrl;
    }

    public List<String> getScope()
    {
        return scope;
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
        return "OAuth2AppInfo{"
               + "providerName='" + providerName + '\''
               + ", appName='" + appName + '\''
               + ", appId='" + appId + '\''
               + ", scope=" + scope
               + ", redirectUrl='" + redirectUrl + "'}";
    }

}
