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

import net.netheos.pcsapi.utils.PcsUtils;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONTokener;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This class is a simple reader of applications information from a plain text file with format:
 * <pre>
 * providerName1.appName1 = jsonobject1
 * providerName1.appName2 = jsonobject2
 * </pre>
 */
public class AppInfoFileRepository
        implements AppInfoRepository
{

    private final File file;
    private final Map<String, AppInfo> appInfoMap = new HashMap<String, AppInfo>();

    public AppInfoFileRepository( File file )
            throws IOException
    {
        this.file = file;

        BufferedReader reader = null;
        try {
            reader = new BufferedReader( new InputStreamReader( new FileInputStream( file ), PcsUtils.UTF8 ) );

            String line;
            while ( ( line = reader.readLine() ) != null ) { // while loop begins here
                line = line.trim();

                if ( line.startsWith( "#" ) || line.length() == 0 ) {
                    continue;
                }

                String[] appInfoArray = line.split( "=", 2 );

                // Key
                String[] appInfoKeyArray = appInfoArray[0].trim().split( "\\." );
                String providerName = appInfoKeyArray[0];
                String appName = appInfoKeyArray[1];

                String appInfoValue = appInfoArray[1].trim();
                JSONObject jsonObj = ( JSONObject ) new JSONTokener( appInfoValue ).nextValue();
                String appId = jsonObj.optString( "appId", null );

                AppInfo appInfo;
                if ( appId != null ) {
                    // OAuth
                    JSONArray jsonScope = jsonObj.optJSONArray( "scope" );
                    List<String> scopeList = new ArrayList<String>();

                    for ( int i = 0; i < jsonScope.length(); i++ ) {
                        scopeList.add( jsonScope.get( i ).toString() );
                    }
                    String appSecret = jsonObj.getString( "appSecret" );
                    String redirectUrl = jsonObj.optString( "redirectUrl", null );
                    appInfo = new OAuth2AppInfo( providerName, appName, appId, appSecret, scopeList, redirectUrl );

                } else {
                    // Login / Password
                    appInfo = new PasswordAppInfo( providerName, appName );
                }

                appInfoMap.put( getAppKey( providerName, appName ), appInfo );
            }

        } finally {
            PcsUtils.closeQuietly( reader );
        }
    }

    private String getAppKey( String providerName, String appName )
    {
        return providerName + "." + appName;
    }

    /**
     * Retrieves application information for the specified provider.
     *
     * @param providerName
     * @return appInfo
     */
    @Override
    public AppInfo get( String providerName )
    {
        return get( providerName, null );
    }

    @Override
    public AppInfo get( String providerName, String appName )
    {
        AppInfo appInfo = null;

        if ( appName != null ) {
            final String key = getAppKey( providerName, appName );
            appInfo = appInfoMap.get( key );

            if ( appInfo == null ) {
                throw new IllegalStateException( String.format( "No application found for provider '%s' and name '%s'", providerName, appName ) );
            }

            return appInfo;

        } else {
            // loop over all appInfo

            for ( String key : appInfoMap.keySet() ) {
                if ( key.startsWith( providerName + "." ) ) {

                    if ( appInfo != null ) {
                        throw new IllegalStateException( "Several applications found for provider:" + providerName );
                    }

                    appInfo = appInfoMap.get( key );
                }
            }
        }

        if ( appInfo == null ) {
            throw new IllegalStateException( "No application found for provider: " + providerName );
        }

        return appInfo;
    }

    @Override
    public String toString()
    {
        return "AppInfoFileRepo{"
               + "file='" + file + '\''
               + ", appInfoMap=" + appInfoMap
               + '}';
    }

}
