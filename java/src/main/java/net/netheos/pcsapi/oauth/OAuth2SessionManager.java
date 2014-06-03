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
package net.netheos.pcsapi.oauth;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.List;
import net.netheos.pcsapi.credentials.Credentials;
import net.netheos.pcsapi.credentials.OAuth2AppInfo;
import net.netheos.pcsapi.credentials.OAuth2Credentials;
import net.netheos.pcsapi.credentials.UserCredentials;
import net.netheos.pcsapi.credentials.UserCredentialsRepository;
import net.netheos.pcsapi.exceptions.CRetriableException;
import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.request.CResponse;
import net.netheos.pcsapi.storage.StorageBuilder;
import net.netheos.pcsapi.utils.PcsUtils;
import org.apache.http.HttpResponse;
import org.apache.http.NameValuePair;
import org.apache.http.client.HttpClient;
import org.apache.http.client.entity.UrlEncodedFormEntity;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.client.methods.HttpUriRequest;
import org.apache.http.message.BasicNameValuePair;
import org.apache.http.util.EntityUtils;
import org.json.JSONException;
import org.json.JSONObject;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * OAuth2 session manager (used by most providers).
 */
public class OAuth2SessionManager
        extends SessionManager<OAuth2Credentials>
{

    private static final Logger LOGGER = LoggerFactory.getLogger( OAuth2SessionManager.class );
    private static final String HEADER_AUTHORIZATION = "Authorization";
    private final Object refreshLock = new Object();

    private final String authorizeUrl;
    private final String accessTokenUrl;
    private final String refreshTokenUrl;
    private final boolean scopeInAuthorization;
    private final Character scopePermsSeparator;
    private final OAuth2AppInfo appInfo;
    private final UserCredentialsRepository userCredentialsRepo;
    private final HttpClient httpClient;

    public OAuth2SessionManager( String authorizeUrl,
                                 String accessTokenUrl,
                                 String refreshTokenUrl,
                                 boolean scopeInAuthorization,
                                 Character scopePermsSeparator,
                                 StorageBuilder builder )

    {
        super( builder.getUserCredentials() );

        this.httpClient = builder.getHttpClient();
        this.authorizeUrl = authorizeUrl;
        this.accessTokenUrl = accessTokenUrl;
        this.refreshTokenUrl = refreshTokenUrl;
        this.scopeInAuthorization = scopeInAuthorization;
        this.scopePermsSeparator = scopePermsSeparator;

        this.appInfo = ( OAuth2AppInfo ) builder.getAppInfo();
        this.userCredentialsRepo = builder.getUserCredentialsRepo();

        // checks if we already have user_credentials:
        if ( userCredentials != null ) {
            OAuth2Credentials credentials = userCredentials.getCredentials();
            if ( credentials.getAccessToken() == null ) {
                throw new IllegalStateException( "User credentials do not contain any access token" );
            }
        }
    }

    @Override
    public CResponse execute( HttpUriRequest request )
    {
        if ( LOGGER.isTraceEnabled() ) {
            LOGGER.trace( "{}: {}", request.getMethod(), PcsUtils.shortenUrl( request.getURI() ) );
        }

        if ( userCredentials.getCredentials().hasExpired() ) {
            refreshToken();
        }

        try {
            request.removeHeaders( HEADER_AUTHORIZATION ); // in case we are retrying
            request.addHeader( HEADER_AUTHORIZATION, "Bearer " + userCredentials.getCredentials().getAccessToken() );

            HttpResponse httpResponse = httpClient.execute( request );
            return new CResponse( request, httpResponse );

        } catch ( IOException ex ) {
            // a low level error should be retried :
            throw new CRetriableException( ex );
        }
    }

    /**
     * Refreshes access token after expiration (before sending request) thanks to the refresh token. The bew access
     * token is then stored in this manager.
     * <p/>
     * Method is synchronized so that no two threads will attempt to refresh at the same time. If a locked thread sees
     * that token has already been refreshed, no refresh is attempted either.
     * <p/>
     * Not all providers support tokens refresh (ex : CloudMe).
     */
    public void refreshToken()
            throws CStorageException
    {
        if ( refreshTokenUrl == null ) {
            throw new CStorageException( "Provider does not support token refresh" );
        }

        OAuth2Credentials currentCredentials = userCredentials.getCredentials();
        synchronized ( refreshLock ) {
            OAuth2Credentials afterLockCredentials = userCredentials.getCredentials();

            if ( afterLockCredentials.getRefreshToken() == null ) {
                throw new CStorageException( "No refresh token available" );
            }

            // TODO : better check again hasExpired() ?
            if ( !afterLockCredentials.equals( currentCredentials ) ) {
                LOGGER.debug( "Not refreshed token in this thread, already done" );
                return;
            }

            LOGGER.debug( "Refreshing token" );

            HttpResponse response;
            try {
                HttpPost post = new HttpPost( accessTokenUrl );

                List<NameValuePair> parameters = new ArrayList<NameValuePair>();
                parameters.add( new BasicNameValuePair( OAuth2.CLIENT_ID, appInfo.getAppId() ) );
                parameters.add( new BasicNameValuePair( OAuth2.CLIENT_SECRET, appInfo.getAppSecret() ) );
                parameters.add( new BasicNameValuePair( OAuth2.REFRESH_TOKEN,
                                                        afterLockCredentials.getRefreshToken() ) );
                parameters.add( new BasicNameValuePair( OAuth2.SCOPE, getScopeForAuthorization() ) );
                parameters.add( new BasicNameValuePair( OAuth2.GRANT_TYPE, OAuth2.REFRESH_TOKEN ) );
                post.setEntity( new UrlEncodedFormEntity( parameters, PcsUtils.UTF8.name() ) );

                response = httpClient.execute( post );

            } catch ( IOException ex ) {
                throw new CStorageException( "HTTP request while refreshing token has failed", ex );
            }

            // FIXME check status code here
            try {
                String data = EntityUtils.toString( response.getEntity(), PcsUtils.UTF8.name() );

                // Update credentials
                JSONObject json = new JSONObject( data );
                afterLockCredentials.update( json );

            } catch ( IOException ex ) {
                throw new CStorageException( "Can't get string from HttpResponse: " + response.toString(), ex );
            } catch ( JSONException ex ) {
                throw new CStorageException( "Error parsing the JSON response", ex );
            }

            try {
                userCredentialsRepo.save( userCredentials );
            } catch ( IOException ex ) {
                throw new CStorageException( "Can't save credentials", ex );
            }
        }
    }

    /**
     * Fetches user credentials
     *
     * @param code oauth2 OTP code
     * @return The user credentials (without userId)
     */
    UserCredentials fetchUserCredentials( String code )
    {
        HttpPost post = new HttpPost( accessTokenUrl );

        List<NameValuePair> parameters = new ArrayList<NameValuePair>();
        parameters.add( new BasicNameValuePair( OAuth2.CLIENT_ID, appInfo.getAppId() ) );
        parameters.add( new BasicNameValuePair( OAuth2.CLIENT_SECRET, appInfo.getAppSecret() ) );
        parameters.add( new BasicNameValuePair( OAuth2.CODE, code ) );
        parameters.add( new BasicNameValuePair( OAuth2.GRANT_TYPE, OAuth2.AUTHORIZATION_CODE ) );

        if ( appInfo.getRedirectUrl() != null ) {
            parameters.add( new BasicNameValuePair( OAuth2.REDIRECT_URI, appInfo.getRedirectUrl() ) );
        }

        try {
            post.setEntity( new UrlEncodedFormEntity( parameters, PcsUtils.UTF8.name() ) );
        } catch ( UnsupportedEncodingException ex ) {
            throw new CStorageException( "Can't encode parameters", ex );
        }

        HttpResponse response;

        try {
            response = httpClient.execute( post );
        } catch ( IOException e ) {
            throw new CStorageException( "HTTP request while fetching token has failed", e );
        }

        // FIXME check status code here
        final String json;

        try {
            json = EntityUtils.toString( response.getEntity(), PcsUtils.UTF8.name() );
        } catch ( IOException e ) {
            throw new CStorageException( "Can't retrieve json string in HTTP response entity", e );
        }
        LOGGER.debug( "fetchUserCredentials - json: {}", json );

        Credentials credentials = Credentials.createFromJson( json );
        userCredentials = new UserCredentials( appInfo,
                                               null, // userId is unknown yet
                                               credentials );

        return userCredentials;
    }

    /**
     * Converts scope (list of permissions) to string for building OAuth authorization URL.
     * <p/>
     * Some providers separate permissions with spaces, other with comma... Other do not support scope at all.
     *
     * @return
     */
    String getScopeForAuthorization()
    {
        if ( !scopeInAuthorization ) {
            return null;
        }

        StringBuilder sb = new StringBuilder();
        for ( String perm : appInfo.getScope() ) {
            sb.append( perm ).append( scopePermsSeparator );
        }

        return new String( sb.deleteCharAt( sb.length() - 1 ) );
    }

    UserCredentialsRepository getUserCredentialsRepository()
    {
        return userCredentialsRepo;
    }

    OAuth2AppInfo getAppInfo()
    {
        return appInfo;
    }

    String getAuthorizeUrl()
    {
        return authorizeUrl;
    }

}
