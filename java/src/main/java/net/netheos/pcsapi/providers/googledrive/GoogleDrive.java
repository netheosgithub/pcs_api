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

package net.netheos.pcsapi.providers.googledrive;

import java.io.IOException;
import java.net.URI;
import java.text.DateFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.TimeZone;
import java.util.concurrent.Callable;
import net.netheos.pcsapi.exceptions.CAuthenticationException;
import net.netheos.pcsapi.exceptions.CFileNotFoundException;
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

import org.apache.http.client.methods.HttpGet;
import org.json.JSONObject;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import net.netheos.pcsapi.models.CFolderContent;
import net.netheos.pcsapi.request.ByteSourceBody;
import net.netheos.pcsapi.request.JSONBody;
import net.netheos.pcsapi.request.JSONEntity;
import net.netheos.pcsapi.request.MultipartRelatedEntity;
import net.netheos.pcsapi.utils.URIBuilder;
import org.apache.http.Header;
import org.apache.http.client.methods.HttpEntityEnclosingRequestBase;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.client.methods.HttpPut;
import org.apache.http.client.methods.HttpUriRequest;
import org.json.JSONArray;
import org.json.JSONException;

/**
 * Implements Google Drive storage provider.
 *
 * Note that OAuth2 refresh token is returned by oauth endpoint only if user approves an offline access. This is the
 * purpose of query parameters "access_type=offline&approval_prompt=force" in authorizeUrl. Beware that old refresh
 * tokens may be invalidated by such requests though : see https://developers.google.com/accounts/docs/OAuth2
 *
 */
public class GoogleDrive
        extends StorageProvider<OAuth2SessionManager>
{

    private static final Logger LOGGER = LoggerFactory.getLogger( GoogleDrive.class );

    public static final String PROVIDER_NAME = "googledrive";
    private static final String ENDPOINT = "https://www.googleapis.com/drive/v2";
    private static final String FILES_ENDPOINT = ENDPOINT + "/files";
    private static final String FILES_UPLOAD_ENDPOINT = "https://www.googleapis.com/upload/drive/v2/files";
    private static final String USERINFO_ENDPOINT = "https://www.googleapis.com/oauth2/v1/userinfo";
    static final String MIME_TYPE_DIRECTORY = "application/vnd.google-apps.folder";
    private static final ResponseValidator<CResponse> BASIC_VALIDATOR = new DriveResponseValidator();
    private static final ResponseValidator<CResponse> API_VALIDATOR = new ApiResponseValidator( BASIC_VALIDATOR );
    private static final String OAUTH_ROOT = "https://accounts.google.com/o/oauth2";

    public GoogleDrive( StorageBuilder builder )
    {
        super( PROVIDER_NAME,
               new OAuth2SessionManager( OAUTH_ROOT + "/auth?access_type=offline&approval_prompt=force", // authorizeUrl
                                         OAUTH_ROOT + "/token", // accessTokenUrl
                                         OAUTH_ROOT + "/token/", // refreshTokenUrl
                                         true, // scopeInAuthorization
                                         ' ', // scopePermsSeparator
                                         builder ),
               builder.getRetryStrategy(),
               builder.getHttpClient() );
    }

    private RequestInvoker<CResponse> getBasicRequestInvoker( HttpUriRequest request, CPath path )
    {
        return new DriveRequestInvoker( new HttpRequestor( request, path, sessionManager ),
                                        BASIC_VALIDATOR );
    }

    private RequestInvoker<CResponse> getApiRequestInvoker( HttpUriRequest request, CPath path )
    {
        return new DriveRequestInvoker( new HttpRequestor( request, path, sessionManager ),
                                        API_VALIDATOR );
    }

    private CFile parseCFile( CPath parentPath, JSONObject json )
    {
        String dateStr = json.getString( "modifiedDate" );
        try {
            CFile cFile;
            DateFormat format = new SimpleDateFormat( "yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.US );
            format.setLenient( false );

            if ( dateStr.endsWith( "Z" ) ) { // ISO8601 syntax
                format.setTimeZone( TimeZone.getTimeZone( "GMT" ) );
            }
            Date modified = format.parse( dateStr );
            if ( MIME_TYPE_DIRECTORY.equals( json.getString( "mimeType" ) ) ) {
                cFile = new CFolder( parentPath.add( json.getString( "title" ) ),
                                     modified,
                                     null ); // metadata
            } else {
                long fileSize;
                if ( json.has( "fileSize" ) ) {
                    fileSize = json.getLong( "fileSize" );
                } else {
                    // google apps files (application/vnd.google-apps.document, application/vnd.google-apps.spreadsheet, etc.)
                    // do not publish any size (they can not be downloaded, only exported).
                    fileSize = -1;
                }
                cFile = new CBlob( parentPath.add( json.getString( "title" ) ),
                                   fileSize,
                                   json.getString( "mimeType" ),
                                   modified,
                                   null ); // metadata
            }
            return cFile;

        } catch ( ParseException ex ) {
            throw new CStorageException( "Can't parse date modified: " + dateStr + " (" + ex.getMessage() + ")", ex );
        }
    }

    private String getFileUrl( String fileId )
    {
        return ENDPOINT + "/files/" + fileId;
    }

    /**
     * Resolve the given CPath to gather informations (mainly id and mimeType) ; returns a RemotePath object.
     *
     * Drive API does not allow this natively ; we perform a single request that returns all files (but may return too
     * much) : find files with title='a' or title='b' or title='c', then we connect children and parents to get the
     * chain of ids. TODO This fails if there are several folders with same name, and we follow the wrong branch
     */
    private RemotePath findRemotePath( CPath path, boolean detailed )
    {
        // easy special case :
        if ( path.isRoot() ) {
            return new RemotePath( path, new LinkedList<JSONObject>() );
        }
        // Here we know that we have at least one path segment

        // Build query (cf. https://developers.google.com/drive/web/search-parameters)
        List<String> segments = path.split();
        StringBuilder query = new StringBuilder( "(" );
        int i = 0;
        for ( String segment : segments ) {
            if ( i > 0 ) {
                query.append( " or " );
            }
            query.append( "(title='" )
                    .append( segment.replace( "'", "\\'" ) ) // escape ' --> \'
                    .append( "'" );
            // for all but last segment, we enforce file to be a directory
            // TODO this creates looong query string, is that interesting ?
            //if (i < segments.size()-1) {
            //   q.append(" and mimeType='").append(MIME_TYPE_DIRECTORY).append("'");
            query.append( ")" );
            i++;
        }
        query.append( ") and trashed = false" );

        // drive may not return all results in a single query :
        // FIXME ouch there seems to be some issues with pagination on the google side ?
        // http://stackoverflow.com/questions/18646004/drive-api-files-list-query-with-not-parameter-returns-empty-pages?rq=1
        // http://stackoverflow.com/questions/18355113/paging-in-files-list-returns-endless-number-of-empty-pages?rq=1
        // http://stackoverflow.com/questions/19679190/is-paging-broken-in-drive?rq=1
        // http://stackoverflow.com/questions/16186264/files-list-reproducibly-returns-incomplete-list-in-drive-files-scope
        List<JSONObject> items = new ArrayList<JSONObject>( segments.size() );
        String nextPageToken = null;
        while ( true ) {
            // Execute request ; we ask for specific fields only
            String fieldsFilter = "id,title,mimeType,parents/id,parents/isRoot";
            if ( detailed ) {
                fieldsFilter += ",downloadUrl,modifiedDate,fileSize";
            }
            fieldsFilter = "nextPageToken,items(" + fieldsFilter + ")";
            URIBuilder builder = new URIBuilder( URI.create( FILES_ENDPOINT ) );
            builder.addParameter( "q", query.toString() );
            builder.addParameter( "fields", fieldsFilter );
            if ( nextPageToken != null ) {
                builder.addParameter( "pageToken", nextPageToken );
            }
            builder.addParameter( "maxResults", "1000" );

            HttpGet request = new HttpGet( builder.build() );
            RequestInvoker<CResponse> ri = getApiRequestInvoker( request, null );
            JSONObject jresp = retryStrategy.invokeRetry( ri ).asJSONObject();
            JSONArray itemsInPage = jresp.getJSONArray( "items" );
            for ( i = 0; i < itemsInPage.length(); i++ ) {
                items.add( itemsInPage.getJSONObject( i ) );
            }
            // Is it the last page ?
            nextPageToken = jresp.optString( "nextPageToken", null );
            if ( nextPageToken != null ) {
                LOGGER.debug( "findRemotePath() will loop : ({} items in this page)", itemsInPage.length() );
            } else {
                LOGGER.debug( "findRemotePath() : no more data for this query" );
                break;
            }
        }

        // Now connect parent/children to build the path :
        LinkedList<JSONObject> filesChain = new LinkedList<JSONObject>();
        i = 0;
        for ( String searchedSegment : segments ) {
            boolean firstSegment = ( i == 0 );  // this changes parent condition (isRoot, or no parent for shares)
//            boolean lastSegment = ( i == segments.size() - 1 );
            i++;
            //print("searching segment ",searched_segment)
            JSONObject nextItem = null;
            for ( JSONObject item : items ) {
                //print("examaning item=",item)
                // We match title
                // FIXME and enforce type is directory if not last segment :
                if ( item.getString( "title" ).equals( searchedSegment ) ) {
                    // && (last_segment or item['mimeType'] == self.MIME_TYPE_DIRECTORY)):
                    JSONArray parents = item.optJSONArray( "parents" );
                    if ( firstSegment ) {
                        if ( parents == null || parents.length() == 0 ) { // no parents (shared folder ?)
                            nextItem = item;
                            break;
                        }
                        for ( int k = 0; k < parents.length(); k++ ) {
                            JSONObject p = parents.getJSONObject( k );
                            if ( p.getBoolean( "isRoot" ) ) { // at least one parent is root
                                nextItem = item;
                                break;
                            }
                        }
                    } else {
                        for ( int k = 0; k < parents.length(); k++ ) {
                            JSONObject p = parents.getJSONObject( k );
                            if ( p.getString( "id" ).equals( filesChain.getLast().getString( "id" ) ) ) {
                                //at least one parent id is last parent id
                                nextItem = item;
                                break;
                            }
                        }
                    }
                    if ( nextItem != null ) {
                        break;
                    }
                }
            }
            if ( nextItem == null ) {
                break;
            }
            filesChain.add( nextItem );
        }
        return new RemotePath( path, filesChain );
    }

    @Override
    public String getUserId()
            throws CStorageException
    {
        // user_id is email in case of googledrive
        String url = USERINFO_ENDPOINT;
        RequestInvoker<CResponse> ri = getApiRequestInvoker( new HttpGet( url ), null );
        JSONObject json = retryStrategy.invokeRetry( ri ).asJSONObject();
        return json.getString( "email" );
    }

    @Override
    public CQuota getQuota()
            throws CStorageException
    {
        // Return a CQuota object
        String url = ENDPOINT + "/about";
        RequestInvoker<CResponse> ri = getApiRequestInvoker( new HttpGet( url ), null );
        JSONObject json = retryStrategy.invokeRetry( ri ).asJSONObject();
        return new CQuota( json.getLong( "quotaBytesUsed" ), json.getLong( "quotaBytesTotal" ) );
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
        RemotePath remotePath = findRemotePath( path, true );
        if ( !remotePath.exists() ) {
            // per contract, listing a non existing folder must return null
            return null;
        }
        if ( remotePath.lastIsBlob() ) {
            throw new CInvalidFileTypeException( path, false );
        }
        // Now we inquire for children of leaf folder :
        String folderId = remotePath.getDeepestFolderId();
        StringBuilder query = new StringBuilder();
        query.append( "('" ).append( folderId ).append( "' in parents" );
        if ( path.isRoot() ) {
            // If we list root folder, also list shared files, as they appear here :
            query.append( " or sharedWithMe" );
        }
        query.append( ") and trashed=false" );
        String fieldsFilter = "nextPageToken,items(id,title,mimeType,fileSize,modifiedDate)";
        URIBuilder builder = new URIBuilder( URI.create( FILES_ENDPOINT ) );
        builder.addParameter( "q", query.toString() );
        builder.addParameter( "fields", fieldsFilter );
        HttpGet request = new HttpGet( builder.build() );
        RequestInvoker<CResponse> ri = getApiRequestInvoker( request, null );
        JSONObject jresp = retryStrategy.invokeRetry( ri ).asJSONObject();

        Map<CPath, CFile> map = new HashMap<CPath, CFile>();
        JSONArray array = jresp.getJSONArray( "items" );
        for ( int i = 0; i < array.length(); i++ ) {
            JSONObject itemObj = array.getJSONObject( i );
            CFile file = parseCFile( path, itemObj );
            map.put( file.getPath(), file );
        }

        return new CFolderContent( map );
    }

    @Override
    public CFolderContent listFolder( CFolder folder )
            throws CStorageException
    {
        return listFolder( folder.getPath() );
    }

    /**
     * Create a folder without creating any higher level intermediary folders, and returned id of created folder.
     *
     * @param path
     * @param parentId
     * @return id of created folder
     */
    private String rawCreateFolder( CPath path, String parentId )
    {
        JSONObject body = new JSONObject();
        body.put( "title", path.getBaseName() );
        body.put( "mimeType", MIME_TYPE_DIRECTORY );
        JSONArray ids = new JSONArray();
        JSONObject idObj = new JSONObject();
        idObj.put( "id", parentId );
        ids.put( idObj );
        body.put( "parents", ids );

        HttpPost request = new HttpPost( FILES_ENDPOINT + "?fields=id" );
        request.setEntity( new JSONEntity( body ) );

        RequestInvoker<CResponse> ri = getApiRequestInvoker( request, path );
        JSONObject jresp = retryStrategy.invokeRetry( ri ).asJSONObject();
        return jresp.getString( "id" );
    }

    @Override
    public boolean createFolder( final CPath path )
            throws CStorageException
    {
        // we have to check before if folder already exists :
        // (and also to determine what folders must be created)
        RemotePath remotePath = findRemotePath( path, false );
        if ( remotePath.lastIsBlob() ) {
            // A blob exists along that path : wrong !
            throw new CInvalidFileTypeException( remotePath.lastCPath(), false );
        }
        if ( remotePath.exists() ) {
            // folder already exists :
            return false;
        }

        // we may have to create any intermediary folders :
        String parentId = remotePath.getDeepestFolderId();
        int i = remotePath.filesChain.size();
        while ( i < remotePath.segments.size() ) {
            CPath currentPath = remotePath.getFirstSegmentsPath( i + 1 );
            parentId = rawCreateFolder( currentPath, parentId );
            i++;
        }
        return true;
    }

    private void deleteById( CPath path, String fileId )
    {
        try {
            String url = getFileUrl( fileId ) + "/trash";
            HttpPost request = new HttpPost( url );
            RequestInvoker<CResponse> ri = getApiRequestInvoker( request, path );
            retryStrategy.invokeRetry( ri ).close();

        } catch ( IOException ex ) {
            throw new CStorageException( "Error deleting file", ex );
        }
    }

    @Override
    public boolean delete( final CPath path )
            throws CStorageException
    {
        // Move file to trash
        if ( path.isRoot() ) {
            throw new CStorageException( "Can not delete root folder" );
        }

        RemotePath remotePath = findRemotePath( path, false );
        if ( !remotePath.exists() ) {
            return false;
        }
        // We have at least one segment ; this is either a folder or a blob
        // (so we cannot rely on deepest_folder_id() as it works only for folders)
        deleteById( path, remotePath.filesChain.getLast().getString( "id" ) );
        return true;
    }

    @Override
    public CFile getFile( final CPath path )
            throws CStorageException
    {
        // Get CFile for given path, or None if no object exists with that path
        if ( path.isRoot() ) {
            return new CFolder( CPath.ROOT );
        }
        RemotePath remotePath = findRemotePath( path, true );
        if ( !remotePath.exists() ) {
            return null;
        }
        return parseCFile( path.getParent(), remotePath.filesChain.getLast() );
    }

    @Override
    public void download( final CDownloadRequest downloadRequest )
            throws CStorageException
    {
        retryStrategy.invokeRetry( new Callable<Void>()
        {

            @Override
            public Void call()
                    throws Exception
            {
                // This method does NOT retry request
                CPath path = downloadRequest.getPath();
                RemotePath remotePath = findRemotePath( path, true );
                if ( !remotePath.exists() ) {
                    throw new CFileNotFoundException( "File not found: " + path, path );
                }
                if ( remotePath.exists() && !remotePath.lastIsBlob() ) {
                    // path refer to an existing folder : wrong !
                    throw new CInvalidFileTypeException( path, true );
                }

                JSONObject blob = remotePath.getBlob();
                if ( !blob.has( "downloadUrl" ) ) {
                    // A blob without a download url is likely a google doc, not downloadable :
                    if ( blob.has( "mimeType" )
                         && blob.getString( "mimeType" ).startsWith( "application/vnd.google-apps." ) ) {
                        throw new CInvalidFileTypeException( "google docs are not downloadable: " + path, path, true );
                    }
                    throw new CStorageException( "No downloadUrl defined for blob: " + path );
                }
                String url = blob.getString( "downloadUrl" );
                HttpGet request = new HttpGet( url );
                for ( Header header : downloadRequest.getHttpHeaders() ) {
                    request.addHeader( header );
                }
                RequestInvoker<CResponse> ri = getBasicRequestInvoker( request, path );
                PcsUtils.downloadDataToSink( ri.call(), downloadRequest.getByteSink() );
                return null;
            }

        } );
    }

    @Override
    public void upload( final CUploadRequest uploadRequest )
            throws CStorageException
    {
        retryStrategy.invokeRetry( new Callable<Void>()
        {

            @Override
            public Void call()
                    throws Exception
            {
                // Check before upload : is it a folder ?
                // (uploading a blob would create another file with the same name : bad)
                CPath path = uploadRequest.getPath();
                RemotePath remotePath = findRemotePath( path, false );
                if ( remotePath.exists() && !remotePath.lastIsBlob() ) {
                    // path refer to an existing folder : wrong !
                    throw new CInvalidFileTypeException( path, true );
                }
                if ( !remotePath.exists() && remotePath.lastIsBlob() ) {
                    // some blob exists in path : wrong !
                    throw new CInvalidFileTypeException( remotePath.lastCPath(), false );
                }

                // only one of these 2 will be set :
                String fileId = null;
                String parentId = null;
                if ( remotePath.exists() ) {
                    // Blob already exists : we'll update it
                    fileId = remotePath.getBlob().getString( "id" );
                } else {
                    parentId = remotePath.getDeepestFolderId();
                    // We may need to create intermediary folders first :
                    // create intermediary folders, in case :
                    int i = remotePath.filesChain.size();
                    while ( i < remotePath.segments.size() - 1 ) {
                        CPath currentPath = remotePath.getFirstSegmentsPath( i + 1 );
                        parentId = rawCreateFolder( currentPath, parentId );
                        i++;
                    }
                }

                // By now we can upload a new blob to folder with id=parent_id,
                // or update existing blob with id=file_id :
                // TODO handle metadata :
                // if upload_request._medatada:
                JSONObject jsonMeta = new JSONObject();
                if ( fileId != null ) {
                    // Blob update
                } else {
                    // Blob creation
                    jsonMeta.put( "title", path.getBaseName() );
                    JSONArray idsArray = new JSONArray();
                    JSONObject idObj = new JSONObject();
                    idObj.put( "id", parentId );
                    idsArray.put( idObj );
                    jsonMeta.put( "parents", idsArray );
                }
                if ( uploadRequest.getContentType() != null ) {
                    // It seems that drive distinguishes between mimeType defined here,
                    // and Content-Type defined in part header.
                    // Drive also tries to guess mimeType...
                    jsonMeta.put( "mimeType", uploadRequest.getContentType() ); // ignored, not updatable ?
                }

                MultipartRelatedEntity multipart = new MultipartRelatedEntity();
                multipart.addPart( "", new JSONBody( jsonMeta, null ) );
                multipart.addPart( "", new ByteSourceBody( uploadRequest.getByteSource(),
                                                           null, // filename, not set in part header
                                                           uploadRequest.getContentType() ) );

                HttpEntityEnclosingRequestBase request;
                if ( fileId != null ) {
                    // Updating existing file :
                    request = new HttpPut( FILES_UPLOAD_ENDPOINT + "/" + fileId + "?uploadType=multipart" );

                } else {
                    // uploading a new file :
                    request = new HttpPost( FILES_UPLOAD_ENDPOINT + "?uploadType=multipart" );
                }
                request.setEntity( multipart );

                RequestInvoker<CResponse> ri = getApiRequestInvoker( request, path );
                ri.call().close();
                return null;
            }

        } );
    }

    /**
     * Validates a response from Google Drive API.
     * <p/>
     * A response is valid if server code is 2xx and content-type JSON.
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
     * Drive basic response validator.
     *
     * Only server code is checked (content-type is ignored). Request is retriable in case of server error 5xx or some
     * 403 errors with rate limit."""
     */
    private static class DriveResponseValidator
            implements ResponseValidator<CResponse>
    {

        @Override
        public void validateResponse( CResponse response, CPath path )
                throws CStorageException
        {
            // A response is valid if server code is 2xx and content-type JSON.
            // It is recoverable in case of server error 5xx.
            LOGGER.debug( "validating googledrive response: {} {} : {} {}",
                          response.getMethod(),
                          response.getUri(),
                          response.getStatus(),
                          response.getReason() );

            if ( response.getStatus() >= 300 ) {
                CStorageException cse = buildHttpError( response, path );
                if ( response.getStatus() >= 500 ) {
                    throw new CRetriableException( cse );
                }
                // Some 403 errors (rate limit) may be retriable :
                if ( response.getStatus() == 403
                     && cse.getMessage() != null
                     && cse.getMessage().startsWith( "[403/rateLimitExceeded]" ) ) {
                    throw new CRetriableException( cse );
                }
                // Other errors are not retriable :
                throw cse;
            }

            // OK, response looks fine
        }

        private CStorageException buildHttpError( CResponse response, CPath path )
        {
            // Try to extract error message from json body :
            String message = null;
            String json_str = null;
            try {
                json_str = response.asString();
                JSONObject json = new JSONObject( json_str );
                JSONObject error = json.getJSONObject( "error" );
                int jcode = error.getInt( "code" );
                String jreason = error.getJSONArray( "errors" ).getJSONObject( 0 ).getString( "reason" );
                message = String.format( "[%d/%s] ", jcode, jreason );
                message += error.getString( "message" );
                if ( jcode == 403 && "userAccess".equals( jreason ) ) {
                    // permission error : indicate failing path helps :
                    message += " (" + path + ")";
                }
            } catch ( JSONException ex ) {
                LOGGER.warn( "Unparsable server error message: {}", json_str );
                // we failed... server returned strange string
            }
            return PcsUtils.buildCStorageException( response, message, path );
        }

    }

    /**
     * This special class is for refreshing access_token once if we get an authentication error (it seems to happen that
     * sometimes google drive returns spurious 401 http status ?!).
     */
    private class DriveRequestInvoker
            extends RequestInvoker<CResponse>
    {

        private boolean alreadyRefreshedToken = false;

        public DriveRequestInvoker( HttpRequestor requestor, ResponseValidator<CResponse> validator )
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

}
