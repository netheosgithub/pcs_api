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

import net.netheos.pcsapi.credentials.UserCredentials;
import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.storage.IStorageProvider;
import net.netheos.pcsapi.storage.StorageProvider;
import net.netheos.pcsapi.utils.PcsUtils;
import net.netheos.pcsapi.utils.URIBuilder;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;

import net.netheos.pcsapi.credentials.OAuth2AppInfo;
import net.netheos.pcsapi.utils.URIUtil;

/**
 * Utility class to retrieve initial token and populate a UserCredentialsRepository.
 */
public class OAuth2Bootstrapper
{

    private static final Logger LOGGER = LoggerFactory.getLogger( OAuth2Bootstrapper.class );
    private final StorageProvider storageProvider;
    private final OAuth2SessionManager sessionManager;
    private String state;

    public OAuth2Bootstrapper( IStorageProvider storageProvider )
    {
        this.storageProvider = ( StorageProvider ) storageProvider;

        if ( !( this.storageProvider.getSessionManager() instanceof OAuth2SessionManager ) ) {
            throw new CStorageException( "This provider does not use OAuth2 authentication" );

        } else {
            sessionManager = ( OAuth2SessionManager ) this.storageProvider.getSessionManager();
        }
    }

    /**
     * Builds the authorize URI that must be loaded in a browser to allow the application to use the API
     *
     * @return The authorize URI
     */
    public URI getAuthorizeBrowserUrl()
    {
        OAuth2AppInfo appInfo = sessionManager.getAppInfo();
        state = PcsUtils.randomString( 30 );

        URI uri = new URIBuilder( URI.create( sessionManager.getAuthorizeUrl() ) )
                .addParameter( OAuth2.CLIENT_ID, appInfo.getAppId() )
                .addParameter( OAuth2.STATE, state )
                .addParameter( OAuth2.RESPONSE_TYPE, "code" )
                .addParameter( OAuth2.REDIRECT_URI, appInfo.getRedirectUrl() )
                .addParameter( OAuth2.SCOPE, sessionManager.getScopeForAuthorization() )
                .build();

        return uri;
    }

    /**
     * Gets users Credentials
     *
     * @param codeOrUrl code or redirect URL provided by the browser
     * @throws IOException
     */
    public void getUserCredentials( String codeOrUrl )
            throws IOException
    {
        if ( state == null ) {
            // should not occur if this class is used properly
            throw new IllegalStateException( "No anti CSRF state defined" );
        }

        String code;
        if ( codeOrUrl.startsWith( "http://" ) || codeOrUrl.startsWith( "https://" ) ) {
            // It's a URL : extract code and check state
            String url = codeOrUrl;

            LOGGER.debug( "redirect URL: {}", url );

            URI uri = URI.create( url );
            String error = URIUtil.getQueryParameter( uri, "error" );
            String errorDescription = URIUtil.getQueryParameter( uri, "error_description" );
            if ( error != null ) {
                String msg = "User authorization failed : " + error;
                if ( errorDescription != null ) {
                    msg += " (" + errorDescription + ")";
                }
                throw new CStorageException( msg );
            }

            String stateToTest = URIUtil.getQueryParameter( uri, "state" );
            if ( !state.equals( stateToTest ) ) {
                throw new CStorageException( "State received (" + stateToTest + ") is not state expected (" + state + ")" );
            }
            code = URIUtil.getQueryParameter( uri, "code" );
            if ( code == null ) {
                throw new CStorageException( "Can't find code in redirected URL: " + url );
            }

        } else {
            // It's a code
            code = codeOrUrl;
        }

        UserCredentials userCredentials = sessionManager.fetchUserCredentials( code );
        // From access token we can get user id...
        String userId = storageProvider.getUserId();
        LOGGER.debug( "User identifier retrieved: {}", userId );

        // ...so that by now we can persist tokens :
        userCredentials.setUserId( userId );
        sessionManager.getUserCredentialsRepository().save( userCredentials );
    }

}
