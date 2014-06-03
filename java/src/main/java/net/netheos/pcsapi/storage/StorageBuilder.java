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

package net.netheos.pcsapi.storage;

import net.netheos.pcsapi.credentials.AppInfo;
import net.netheos.pcsapi.credentials.AppInfoRepository;
import net.netheos.pcsapi.credentials.UserCredentials;
import net.netheos.pcsapi.credentials.UserCredentialsRepository;
import net.netheos.pcsapi.models.RetryStrategy;

import org.apache.http.client.HttpClient;

import java.lang.reflect.Constructor;
import net.netheos.pcsapi.utils.PcsUtils;
import org.apache.http.impl.conn.PoolingClientConnectionManager;
import org.apache.http.params.BasicHttpParams;
import org.apache.http.params.HttpConnectionParams;
import org.apache.http.params.HttpParams;

/**
 * Class that facilitates the construction of a storage provider
 */
public class StorageBuilder
{

    private final String providerName;
    private final Class<?> providerClass;
    private AppInfoRepository appInfoRepo;
    private String appName;
    private UserCredentialsRepository userCredentialsRepo;
    private String userId;
    private boolean forBootstrapping;
    private RetryStrategy retryStrategy;
    private HttpClient httpClient = createDefaultClient();

    StorageBuilder( String providerName, Class<?> providerClass )
    {
        this.providerName = providerName;
        this.providerClass = providerClass;
        this.forBootstrapping = false;
        this.retryStrategy = new RetryStrategy( 5, 1000 );
    }

    /**
     * Set the app informations repository. This setter must always be called during the build
     *
     * @param appInfoRepo The repository to use
     * @param appName The application name
     * @return The builder
     */
    public StorageBuilder setAppInfoRepository( AppInfoRepository appInfoRepo, String appName )
    {
        this.appInfoRepo = appInfoRepo;
        this.appName = appName;

        return this;
    }

    /**
     * Set the user credentials repository. This setter must always be called during the build
     *
     * @param userCredentialsRepo The repository
     * @param userId The user identifier (may be null if the identifier is unknown)
     * @return The builder
     */
    public StorageBuilder setUserCredentialsRepository( UserCredentialsRepository userCredentialsRepo, String userId )
    {
        this.userCredentialsRepo = userCredentialsRepo;
        this.userId = userId;

        return this;
    }

    /**
     * OAuth bootstrap is not obvious : storage must be instantiated _without_ users credentials (for retrieving userId
     * thanks to accessToken). As this use case is unlikely (instantiating a storage without user credentials), this
     * method indicates the specificity : no 'missing users credentials' error will be raised.
     *
     * @param forBootstrapping true if it is the first start of the API
     * @return The builder
     */
    public StorageBuilder setForBootstrapping( boolean forBootstrapping )
    {
        this.forBootstrapping = forBootstrapping;

        return this;
    }

    /**
     * Builds a provider-specific storage implementation, by passing this builder in constructor. Each implementation
     * gets its required information from builder.
     *
     * @return The storage builder
     */
    public IStorageProvider build()
    {
        if ( appInfoRepo == null ) {
            throw new IllegalStateException( "Undefined application information repository" );
        }
        if ( userCredentialsRepo == null ) {
            throw new IllegalStateException( "Undefined user credentials repository" );
        }

        try {
            Constructor providerConstructor = providerClass.getConstructor( StorageBuilder.class );
            StorageProvider providerInstance = ( StorageProvider ) providerConstructor.newInstance( this );
            return providerInstance;

        } catch ( Exception ex ) {
            throw new UnsupportedOperationException( "Error instantiating the provider " + providerClass.getSimpleName(), ex );
        }
    }

    public AppInfo getAppInfo()
    {
        return appInfoRepo.get( providerName, appName );
    }

    public UserCredentialsRepository getUserCredentialsRepo()
    {
        return userCredentialsRepo;
    }

    /**
     *
     * @return user credentials, or null if this object is for bootstrapping.
     */
    public UserCredentials<?> getUserCredentials()
    {
        if ( !forBootstrapping ) {
            // Usual case : retrieve user credentials
            // Note that userId may be undefined, repository should handle this :
            return userCredentialsRepo.get( getAppInfo(), userId );
        } else {
            // Special case for getting initial oauth tokens :
            // we'll instantiate without any user_credentials
            return null;
        }
    }

    public HttpClient getHttpClient()
    {
        return httpClient;
    }

    /**
     * Sets a HTTP client that extends org.apache.http.client.HttpClient. Allows users to use a custom HTTP client with
     * custom parameters.
     *
     * @param httpClient The http client to use for issuing requests. This client settings may be modified by pcsapi,
     * thus it is recommended to use a dedicated client for each provider.
     * @return The builder
     */
    public StorageBuilder setHttpClient( HttpClient httpClient )
    {
        PcsUtils.releaseHttpClient( this.httpClient );
        this.httpClient = httpClient;
        return this;
    }

    public RetryStrategy getRetryStrategy()
    {
        return retryStrategy;
    }

    /**
     * Sets the retry requests strategy to be used by storage. If not set, a default retry strategy is used : retry 3
     * times with initial 1 second initial delay
     *
     * @param retryStrategy The retry strategy to use
     * @return The storage builder
     */
    public StorageBuilder setRetryStrategy( RetryStrategy retryStrategy )
    {
        this.retryStrategy = retryStrategy;

        return this;
    }

    private static HttpClient createDefaultClient()
    {
        if ( PcsUtils.ANDROID ) {
            return android.net.http.AndroidHttpClient.newInstance( "pcsapi" );

        } else {
            HttpParams params = new BasicHttpParams();
            HttpConnectionParams.setConnectionTimeout( params, 15000 ); // 15 sec
            HttpConnectionParams.setSoTimeout( params, 60000 ); // 1 min
            return new org.apache.http.impl.client.DefaultHttpClient( new PoolingClientConnectionManager(), params );
        }
    }

}
