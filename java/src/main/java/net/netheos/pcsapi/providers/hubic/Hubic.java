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

package net.netheos.pcsapi.providers.hubic;

import java.util.concurrent.Callable;
import net.netheos.pcsapi.exceptions.CAuthenticationException;
import net.netheos.pcsapi.exceptions.CInvalidFileTypeException;
import net.netheos.pcsapi.exceptions.CRetriableException;
import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.models.CDownloadRequest;
import net.netheos.pcsapi.models.CFile;
import net.netheos.pcsapi.models.CFolder;
import net.netheos.pcsapi.models.CPath;
import net.netheos.pcsapi.models.CQuota;
import net.netheos.pcsapi.request.CResponse;
import net.netheos.pcsapi.models.CUploadRequest;
import net.netheos.pcsapi.request.HttpRequestor;
import net.netheos.pcsapi.request.RequestInvoker;
import net.netheos.pcsapi.request.Requestor;
import net.netheos.pcsapi.request.ResponseValidator;
import net.netheos.pcsapi.models.RetryStrategy;
import net.netheos.pcsapi.oauth.OAuth2SessionManager;
import net.netheos.pcsapi.storage.StorageBuilder;
import net.netheos.pcsapi.storage.StorageProvider;
import net.netheos.pcsapi.utils.PcsUtils;

import org.apache.http.client.methods.HttpGet;
import org.json.JSONObject;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import net.netheos.pcsapi.models.CFolderContent;
import net.netheos.pcsapi.request.HttpExecutor;
import net.netheos.pcsapi.request.SimpleHttpExecutor;
import org.apache.http.client.methods.HttpUriRequest;
import org.json.JSONException;

/**
 * Implements Hubic storage provider
 */
public class Hubic
        extends StorageProvider<OAuth2SessionManager>
{

    public static final String PROVIDER_NAME = "hubic";
    private static final Logger LOGGER = LoggerFactory.getLogger( Hubic.class );
    private static final String ROOT = "https://api.hubic.com";
    private static final String ENDPOINT = ROOT + "/1.0";
    private static final ResponseValidator<CResponse> API_VALIDATOR = new HubicResponseValidator();

    private final HttpExecutor httpExecutor;
    private Swift swiftClient;

    public Hubic( StorageBuilder builder )
    {
        super( PROVIDER_NAME,
               new OAuth2SessionManager( ROOT + "/oauth/auth/", // authorizeUrl
                                         ROOT + "/oauth/token/", // accessTokenUrl
                                         ROOT + "/oauth/token/", // refreshTokenUrl
                                         true, // scopeInAuthorization
                                         ',', // scopePermsSeparator
                                         builder ),
               builder.getRetryStrategy(),
               builder.getHttpClient() );
        httpExecutor = new SimpleHttpExecutor( builder.getHttpClient() );
    }

    private RequestInvoker<CResponse> getApiRequestInvoker( HttpUriRequest request )
    {
        return new HubicRequestInvoker( new HttpRequestor( request, null, sessionManager ),
                                        API_VALIDATOR );
    }

    /**
     * Return current swift client, or create one if none exists yet. The internal swift client does NOT retry its
     * requests. Retries are performed by this class.
     *
     * @return The swift client
     */
    private synchronized Swift getSwiftClient()
            throws CStorageException
    {
        if ( swiftClient != null ) {
            return swiftClient;
        }

        // Only a single thread should create this client
        String url = ENDPOINT + "/account/credentials";
        RequestInvoker<CResponse> ri = getApiRequestInvoker( new HttpGet( url ) );
        JSONObject info = retryStrategy.invokeRetry( ri ).asJSONObject();
        swiftClient = new Swift( info.getString( "endpoint" ),
                                 info.getString( "token" ),
                                 new NoRetryStrategy(),
                                 true, // useDirectoryMarkers
                                 httpExecutor );
        swiftClient.useFirstContainer();
        return swiftClient;
    }

    @Override
    public String getUserId()
            throws CStorageException
    {
        // user_id is email in case of hubic
        String url = ENDPOINT + "/account";
        RequestInvoker<CResponse> ri = getApiRequestInvoker( new HttpGet( url ) );
        JSONObject json = retryStrategy.invokeRetry( ri ).asJSONObject();
        return json.getString( "email" );
    }

    @Override
    public CQuota getQuota()
            throws CStorageException
    {
        // Return a CQuota object
        String url = ENDPOINT + "/account/usage";
        RequestInvoker<CResponse> ri = getApiRequestInvoker( new HttpGet( url ) );
        JSONObject json = retryStrategy.invokeRetry( ri ).asJSONObject();
        return new CQuota( json.getLong( "used" ), json.getLong( "quota" ) );
    }

    @Override
    public CFolderContent listRootFolder()
            throws CInvalidFileTypeException
    {
        return listFolder( CPath.ROOT );
    }

    @Override
    public CFolderContent listFolder( final CPath path )
            throws CStorageException
    {
        SwiftCaller<CFolderContent> caller = new SwiftCaller<CFolderContent>( path )
        {

            @Override
            protected CFolderContent execute( Swift swiftClient )
                    throws CStorageException
            {
                return swiftClient.listFolder( path );
            }

        };

        return retryStrategy.invokeRetry( caller );
    }

    @Override
    public CFolderContent listFolder( CFolder folder )
            throws CStorageException
    {
        return listFolder( folder.getPath() );
    }

    @Override
    public boolean createFolder( final CPath path )
            throws CStorageException
    {
        SwiftCaller<Boolean> caller = new SwiftCaller<Boolean>( path )
        {

            @Override
            protected Boolean execute( Swift swiftClient )
                    throws CStorageException
            {
                return swiftClient.createFolder( path );
            }

        };

        return retryStrategy.invokeRetry( caller );
    }

    @Override
    public boolean delete( final CPath path )
            throws CStorageException
    {
        SwiftCaller<Boolean> caller = new SwiftCaller<Boolean>( path )
        {

            @Override
            protected Boolean execute( Swift swiftClient )
                    throws CStorageException
            {
                return swiftClient.delete( path );
            }

        };

        return retryStrategy.invokeRetry( caller );
    }

    @Override
    public CFile getFile( final CPath path )
            throws CStorageException
    {
        SwiftCaller<CFile> caller = new SwiftCaller<CFile>( path )
        {

            @Override
            protected CFile execute( Swift swiftClient )
                    throws CStorageException
            {
                return swiftClient.getFile( path );
            }

        };

        return retryStrategy.invokeRetry( caller );
    }

    @Override
    public void download( final CDownloadRequest downloadRequest )
            throws CStorageException
    {
        SwiftCaller<Void> caller = new SwiftCaller<Void>( downloadRequest.getPath() )
        {

            @Override
            protected Void execute( Swift swiftClient )
                    throws CStorageException
            {
                swiftClient.download( downloadRequest );
                return null;
            }

        };

        retryStrategy.invokeRetry( caller );
    }

    @Override
    public void upload( final CUploadRequest uploadRequest )
            throws CStorageException
    {
        SwiftCaller<Void> caller = new SwiftCaller<Void>( uploadRequest.getPath() )
        {

            @Override
            protected Void execute( Swift swiftClient )
                    throws CStorageException
            {
                swiftClient.upload( uploadRequest );
                return null;
            }

        };

        retryStrategy.invokeRetry( caller );
    }

    private static class HubicResponseValidator
            implements ResponseValidator<CResponse>
    {

        @Override
        public void validateResponse( CResponse response, CPath path )
                throws CStorageException
        {
            // A response is valid if server code is 2xx and content-type JSON.
            // It is recoverable in case of server error 5xx.
            LOGGER.debug( "validating hubic response: {} {} : {} {}",
                          response.getMethod(),
                          response.getUri(),
                          response.getStatus(),
                          response.getReason() );

            if ( response.getStatus() >= 500 ) {
                throw new CRetriableException( buildHttpError( response, path ) );
            }
            if ( response.getStatus() >= 300 ) {
                throw buildHttpError( response, path );
            }

            // Server has answered 2xx, looks correct ; however check content type is json:
            // (sometimes hubic spuriously redirects to html page /error.xml)
            PcsUtils.ensureContentTypeIsJson( response,
                                              true ); // raiseRetriable
            // OK, response looks fine
        }

        private CStorageException buildHttpError( CResponse response, CPath path )
        {
            // Try to extract error message from json body :
            // (this body can be json even if content type header is text/html !)
            // { "error":"invalid_token", "error_description":"not found" }
            String serverErrorMsg = null;
            try {
                JSONObject json = response.asJSONObject();
                serverErrorMsg = json.getString( "error" );
                serverErrorMsg += " (" + json.getString( "error_description" ) + ")";
            }
            catch ( JSONException ex ) {
                // Could not read server error msg... happens when content is pure html
            }
            catch ( CStorageException ex ) {
                // Could not read server error msg... happens when content is pure html
            }
            return PcsUtils.buildCStorageException( response, serverErrorMsg, path );
        }

    }

    private abstract class SwiftCaller<T>
            implements Requestor<T>
    {

        private final CPath path;

        private SwiftCaller( CPath path )
        {
            this.path = path;
        }

        @Override
        public T call()
                throws CStorageException
        {
            try {
                Swift swiftClient = getSwiftClient();
                return execute( swiftClient );

            } catch ( CAuthenticationException ex ) {
                LOGGER.warn( "Swift authentication error : swift client invalidated" );
                Hubic.this.swiftClient = null;
                // Wrap as a retriable error without wait, so that retrier will not abort :
                throw new CRetriableException( ex, 0 );
            }
        }

        @Override
        public CPath getPath()
        {
            return path;
        }

        protected abstract T execute( Swift swiftClient )
                throws CStorageException;

    }

    /**
     * This special class is for refreshing access_token once if we get an authentication error (it seems to happen that
     * hubic gives sometimes invalid access_tokens ?!).
     */
    private class HubicRequestInvoker
            extends RequestInvoker<CResponse>
    {

        private boolean alreadyRefreshedToken = false;

        public HubicRequestInvoker( HttpRequestor requestor, ResponseValidator<CResponse> validator )
        {
            super( requestor, validator );
        }

        @Override
        protected void validateResponse( CResponse response )
        {
            try {
                super.validateResponse( response );

            } catch ( CAuthenticationException ex ) {
                // Request has failed with an authentication problem :
                // as tokens expiration dates are checked before requests,
                // this should not occur (but in practice, it has been seen)
                LOGGER.warn( "Got an unexpected CAuthenticationError : {}", ex.getMessage() );
                if ( !alreadyRefreshedToken ) {
                    // If we didn't try already, get a new access_token :
                    LOGGER.warn( "Will refresh access_token (in case it is broken?)" );
                    sessionManager.refreshToken();
                    alreadyRefreshedToken = true;
                    throw new CRetriableException( ex, 0 );
                }
                throw ex;
            }
        }

    }

    private static class NoRetryStrategy
            extends RetryStrategy
    {

        public NoRetryStrategy()
        {
            super( 1, 0 );
        }

        @Override
        public <T> T invokeRetry( Callable<T> invoker )
                throws CStorageException
        {
            try {
                return invoker.call();
            } catch ( CStorageException ex ) {
                throw ex;
            } catch ( Exception ex ) {
                throw new CStorageException( "Invocation failure", ex );
            }
        }

    }

}
