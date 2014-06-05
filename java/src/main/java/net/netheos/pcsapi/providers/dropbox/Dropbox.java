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

package net.netheos.pcsapi.providers.dropbox;

import net.netheos.pcsapi.exceptions.CFileNotFoundException;
import net.netheos.pcsapi.exceptions.CHttpException;
import net.netheos.pcsapi.exceptions.CInvalidFileTypeException;
import net.netheos.pcsapi.exceptions.CRetriableException;
import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.models.CBlob;
import net.netheos.pcsapi.models.CDownloadRequest;
import net.netheos.pcsapi.models.CFile;
import net.netheos.pcsapi.models.CFolder;
import net.netheos.pcsapi.models.CPath;
import net.netheos.pcsapi.models.CQuota;
import net.netheos.pcsapi.request.CResponse;
import net.netheos.pcsapi.models.CUploadRequest;
import net.netheos.pcsapi.request.HttpRequestor;
import net.netheos.pcsapi.request.RequestInvoker;
import net.netheos.pcsapi.request.ResponseValidator;
import net.netheos.pcsapi.oauth.OAuth2SessionManager;
import net.netheos.pcsapi.storage.StorageBuilder;
import net.netheos.pcsapi.storage.StorageProvider;
import net.netheos.pcsapi.utils.PcsUtils;
import net.netheos.pcsapi.utils.URIBuilder;

import org.apache.http.NameValuePair;
import org.apache.http.client.entity.UrlEncodedFormEntity;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.client.methods.HttpPut;
import org.apache.http.message.BasicNameValuePair;
import org.json.JSONArray;
import org.json.JSONObject;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import net.netheos.pcsapi.bytesio.ByteSource;
import net.netheos.pcsapi.credentials.OAuth2AppInfo;
import net.netheos.pcsapi.models.CFolderContent;
import net.netheos.pcsapi.request.ByteSourceEntity;
import org.apache.http.Header;
import org.apache.http.client.methods.HttpRequestBase;

public class Dropbox
        extends StorageProvider<OAuth2SessionManager>
{

    private static final Logger LOGGER = LoggerFactory.getLogger( Dropbox.class );
    public static final String PROVIDER_NAME = "dropbox";

    private static final String END_POINT = "https://api.dropbox.com/1";
    private static final URI URI_END_POINT = URI.create( END_POINT );
    private static final String CONTENT_END_POINT = "https://api-content.dropbox.com/1";
    private static final String METADATA = "metadata";
    private static final ResponseValidator<CResponse> BASIC_RESPONSE_VALIDATOR = new BasicResponseValidator();
    private static final ResponseValidator<CResponse> API_RESPONSE_VALIDATOR = new ApiResponseValidator( BASIC_RESPONSE_VALIDATOR );

    private final String scope;

    /**
     * @param builder builder used to feed the storage provider
     */
    public Dropbox( StorageBuilder builder )
    {
        super( PROVIDER_NAME,
               new OAuth2SessionManager( END_POINT + "/oauth2/authorize",
                                         END_POINT + "/oauth2/token",
                                         null, // refreshTokenUrl
                                         false, // scopeInAuthorization
                                         null, // scopePermsSeparator
                                         builder ),
               builder.getRetryStrategy(),
               builder.getHttpClient() );

        List<String> perms = ( ( OAuth2AppInfo ) builder.getAppInfo() ).getScope();
        if ( perms == null || perms.isEmpty() ) {
            throw new IllegalStateException( "Missing scope for Dropbox provider" );
        }
        this.scope = perms.get( 0 ); // "sandbox" (single folder) or "dropbox" (full access)
    }

    /**
     * An invoker that checks response content type = json : to be used by all API requests
     *
     * @param request API request
     * @return a request invoker specific for API requests
     */
    private RequestInvoker<CResponse> getApiRequestInvoker( HttpRequestBase request, CPath path )
    {
        return new RequestInvoker<CResponse>( new HttpRequestor( request, path, sessionManager ),
                                              API_RESPONSE_VALIDATOR );
    }

    /**
     * An invoker that does not check response content type : to be used for files downloading
     *
     * @param request Http request
     * @return the request invoker for basic requests
     */
    private RequestInvoker<CResponse> getBasicRequestInvoker( HttpRequestBase request, CPath path )
    {
        return new RequestInvoker<CResponse>( new HttpRequestor( request, path, sessionManager ),
                                              BASIC_RESPONSE_VALIDATOR );
    }

    @Override
    public String getUserId()
            throws CStorageException
    {
        JSONObject jsonAccount = getAccount();
        return jsonAccount.getString( "email" );
    }

    /**
     * @return account json information
     * @throws CStorageException
     */
    private JSONObject getAccount()
            throws CStorageException
    {
        URIBuilder builder = new URIBuilder( URI_END_POINT );
        builder.encodedPath( "/account/info" );
        HttpGet request = new HttpGet( builder.build() );
        CResponse response = retryStrategy.invokeRetry( getApiRequestInvoker( request, null ) );

        return response.asJSONObject();
    }

    @Override
    public CQuota getQuota()
            throws CStorageException
    {
        JSONObject jsonQuota = getAccount().getJSONObject( "quota_info" );

        long bytesUsed = jsonQuota.getLong( "shared" ) + jsonQuota.getLong( "normal" );
        long bytesAllowed = jsonQuota.getLong( "quota" );

        return new CQuota( bytesUsed, bytesAllowed );
    }

    @Override
    public CFolderContent listRootFolder()
            throws CInvalidFileTypeException
    {
        return listFolder( CPath.ROOT );
    }

    @Override
    public CFolderContent listFolder( final CPath cpath )
            throws CStorageException
    {
        String url = buildUrl( METADATA, cpath );
        HttpGet request = new HttpGet( url );

        RequestInvoker<CResponse> invoker = getApiRequestInvoker( request, cpath );
        CResponse response;
        JSONObject jObj;

        try {
            response = retryStrategy.invokeRetry( invoker );
            jObj = response.asJSONObject();
        } catch ( CFileNotFoundException ex ) {
            // Non existing folder
            return null;
        }

        LOGGER.debug( "listFolder - json: {}", jObj );

        if ( jObj.optBoolean( "is_deleted", false ) ) {
            // File is logically deleted
            return null;
        }

        if ( !jObj.has( "is_dir" ) ) {
            throw PcsUtils.buildCStorageException( response, "No 'is_dir' key in JSON metadata", cpath );
        }

        if ( !jObj.getBoolean( "is_dir" ) ) {
            throw new CInvalidFileTypeException( cpath, false );
        }

        JSONArray jContents = jObj.getJSONArray( "contents" );

        Map<CPath, CFile> map = new HashMap<CPath, CFile>( jContents.length() );

        for ( int i = 0; i < jContents.length(); i++ ) {
            CFile cfile = parseCFile( jContents.getJSONObject( i ) );
            map.put( cfile.getPath(), cfile );
        }

        return new CFolderContent( map );
    }

    /**
     * Url encodes object path, and concatenate to API endpoint and method to get full URL
     *
     * @param methodPath API method path
     * @param path path
     * @return built url
     */
    private String buildUrl( String methodPath, CPath path )
    {
        StringBuilder sb = new StringBuilder();

        sb.append( END_POINT ).append( '/' ).append( methodPath );

        if ( path != null ) {
            sb.append( '/' ).append( scope ).append( path.getUrlEncoded() );
        }

        return sb.toString();
    }

    @Override
    public CFolderContent listFolder( CFolder folder )
            throws CStorageException
    {
        return listFolder( folder.getPath() );
    }

    @Override
    public boolean createFolder( final CPath cpath )
            throws CStorageException
    {
        String url = buildUrl( "fileops/create_folder" );
        HttpPost request = new HttpPost( url );

        try {
            List<NameValuePair> parameters = new ArrayList<NameValuePair>();
            parameters.add( new BasicNameValuePair( "root", scope ) );
            parameters.add( new BasicNameValuePair( "path", cpath.getPathName() ) );

            request.setEntity( new UrlEncodedFormEntity( parameters, PcsUtils.UTF8.name() ) );

        } catch ( UnsupportedEncodingException ex ) {
            throw new CStorageException( ex.getMessage(), ex );
        }

        RequestInvoker<CResponse> invoker = getApiRequestInvoker( request, cpath );

        CResponse response = null;
        try {
            response = retryStrategy.invokeRetry( invoker );
            return true;

        } catch ( CHttpException e ) {

            if ( e.getStatus() == 403 ) {
                // object already exists, check if real folder or blob :
                CFile cfile = getFile( cpath );

                if ( !cfile.isFolder() ) {
                    // a blob already exists at this path : error !
                    throw new CInvalidFileTypeException( cpath, false );
                }
                // Already existing folder
                return false;
            }
            // Other exception :
            throw e;

        } finally {
            PcsUtils.closeQuietly( response );
        }
    }

    /**
     * Concatenates API endpoint and method to get full URL
     *
     * @param methodPath method path
     * @return built URL
     */
    private String buildUrl( String methodPath )
    {
        return buildUrl( methodPath, null );
    }

    @Override
    public boolean delete( final CPath cpath )
            throws CStorageException
    {
        String url = buildUrl( "fileops/delete" );
        HttpPost request = new HttpPost( url );

        try {
            List<NameValuePair> parameters = new ArrayList<NameValuePair>();
            parameters.add( new BasicNameValuePair( "root", scope ) );
            parameters.add( new BasicNameValuePair( "path", cpath.getPathName() ) );

            request.setEntity( new UrlEncodedFormEntity( parameters, PcsUtils.UTF8.name() ) );

        } catch ( UnsupportedEncodingException ex ) {
            throw new CStorageException( ex.getMessage(), ex );
        }

        RequestInvoker<CResponse> invoker = getApiRequestInvoker( request, cpath );

        CResponse response = null;
        try {
            response = retryStrategy.invokeRetry( invoker );
            return true;

        } catch ( CFileNotFoundException e ) {
            // Non existing file
            return false;

        } finally {
            PcsUtils.closeQuietly( response );
        }
    }

    @Override
    public CFile getFile( final CPath cpath )
            throws CStorageException
    {
        String url = buildUrl( METADATA, cpath );
        URI uri = new URIBuilder( URI.create( url ) ).addParameter( "list", "false" ).build();
        HttpGet request = new HttpGet( uri );

        RequestInvoker<CResponse> invoker = getApiRequestInvoker( request, cpath );
        JSONObject jObj;
        try {
            CResponse response = retryStrategy.invokeRetry( invoker );
            jObj = response.asJSONObject();
        } catch ( CFileNotFoundException ex ) {
            return null;
        }

        if ( jObj.optBoolean( "is_deleted", false ) ) {
            // File is logically deleted
            LOGGER.debug( "CFile {} is deleted", cpath );
            return null;
        }

        return parseCFile( jObj );
    }

    @Override
    public void download( final CDownloadRequest downloadRequest )
            throws CStorageException
    {
        String url = buildContentUrl( "files", downloadRequest.getPath() );
        URI uri = new URIBuilder( URI.create( url ) ).build();

        HttpGet request = new HttpGet( uri );
        for ( Header header : downloadRequest.getHttpHeaders() ) {
            request.addHeader( header );
        }

        RequestInvoker<CResponse> invoker = getBasicRequestInvoker( request, downloadRequest.getPath() );

        try {
            CResponse response = ( CResponse ) retryStrategy.invokeRetry( invoker );
            PcsUtils.downloadDataToSink( response, downloadRequest.getByteSink() );

        } catch ( CFileNotFoundException e ) {
            // We have to distinguish here between "nothing exists at that path",
            // and "a folder exists at that path" :
            CFile cfile = getFile( downloadRequest.getPath() );

            if ( cfile == null ) {
                throw e;

            } else if ( cfile.isFolder() ) {
                throw new CInvalidFileTypeException( cfile.getPath(), true );

            } else {
                //Should not happen : a file exists but can not be downloaded ?!
                throw new CStorageException( "Not downloadable file : " + cfile, e );
            }
        }
    }

    /**
     * Url encodes blob path, and concatenate to content endpoint to get full URL
     *
     * @param methodPath
     * @param path
     * @return URL
     */
    private String buildContentUrl( String methodPath, CPath path )
    {
        return CONTENT_END_POINT + '/' + methodPath + '/' + scope + path.getUrlEncoded();
    }

    @Override
    public void upload( final CUploadRequest uploadRequest )
            throws CStorageException
    {
        // Check before upload : is it a folder ? (uploading a blob to a folder would work,
        // but would rename uploaded file).
        CFile cfile = getFile( uploadRequest.getPath() );
        if ( cfile != null && cfile.isFolder() ) {
            throw new CInvalidFileTypeException( cfile.getPath(), true );
        }

        String url = buildContentUrl( "files_put", uploadRequest.getPath() );
        URI uri = URI.create( url );

        HttpPut request = new HttpPut( uri );
        // Dropbox does not support content-type nor file meta information :

        try {
            ByteSource bs = uploadRequest.getByteSource();
            request.setEntity( new ByteSourceEntity( bs ) );
            RequestInvoker<CResponse> invoker = getBasicRequestInvoker( request, uploadRequest.getPath() );

            retryStrategy.invokeRetry( invoker ).close();

        } catch ( IOException ex ) {
            throw new CStorageException( ex.getMessage(), ex );
        }
    }

    /**
     * Generates a CFile from its json representation
     *
     * @param jObj JSON object representing a CFile
     * @return the CFile object corresponding to the JSON object
     */
    private CFile parseCFile( JSONObject jObj )
    {
        CFile cfile;

        if ( jObj.optBoolean( "is_dir", false ) ) {
            cfile = new CFolder( new CPath( jObj.getString( "path" ) ) );

        } else {
            cfile = new CBlob( new CPath( jObj.getString( "path" ) ), jObj.getLong( "bytes" ), jObj.getString( "mime_type" ) );
            String stringDate = jObj.getString( "modified" );

            try {
                // stringDate looks like: "Fri, 07 Mar 2014 17:47:55 +0000"
                SimpleDateFormat sdf = new SimpleDateFormat( "EEE, dd MMM yyyy HH:mm:ss Z", Locale.US );
                Date modified = sdf.parse( stringDate );
                cfile.setModificationDate( modified );

            } catch ( ParseException ex ) {
                throw new CStorageException( "Can't parse date modified: " + stringDate + " (" + ex.getMessage() + ")", ex );
            }
        }

        return cfile;
    }

    /**
     * Validates a response from dropbox API.
     * <p/>
     * A response is valid if server code is 2xx and content-type JSON. Request is retriable in case of server error 5xx
     * (except 507)
     */
    private static class ApiResponseValidator
            implements ResponseValidator<CResponse>
    {

        private final ResponseValidator<CResponse> parent;

        public ApiResponseValidator( ResponseValidator<CResponse> parent )
        {
            this.parent = parent;
        }

        @Override
        public void validateResponse( CResponse response, CPath path )
                throws CStorageException
        {
            parent.validateResponse( response, path );

            LOGGER.debug( "ValidateResponse - server response OK" );
            PcsUtils.ensureContentTypeIsJson( response, true );

            LOGGER.debug( "ValidateResponse - all is OK" );
        }

    }

    /**
     * Validates a response for a file download or API request.
     * <p/>
     * Only server code is checked (content-type is ignored). Request is retriable in case of server error 5xx (except
     * 507).
     */
    private static class BasicResponseValidator
            implements ResponseValidator<CResponse>
    {

        @Override
        public void validateResponse( CResponse response, CPath path )
                throws CStorageException
        {
            int status = response.getStatus();

            if ( status == 507 ) {
                // User is over Dropbox storage quota: no need to retry then
                throw buildCHttpException( response, "Quota exceeded", path );

            } else if ( status >= 500 ) {
                throw new CRetriableException( buildCHttpException( response, null, path ) );

            } else if ( status >= 300 ) {
                throw buildCHttpException( response, null, path );
            }
            // Everything is OK
        }

        private CStorageException buildCHttpException( CResponse response, String errorMessage, CPath path )
        {
            String ct = response.getContentType();
            if ( ct != null && ( ct.contains( "text/javascript" ) || ct.contains( "application/json" ) ) ) {
                JSONObject json = response.asJSONObject();
                errorMessage = json.getString( "error" );
            }
            return PcsUtils.buildCStorageException( response, errorMessage, path );
        }

    }

}
