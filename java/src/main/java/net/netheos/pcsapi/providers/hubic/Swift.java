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

import net.netheos.pcsapi.bytesio.ByteSource;

import net.netheos.pcsapi.exceptions.CFileNotFoundException;
import net.netheos.pcsapi.exceptions.CInvalidFileTypeException;
import net.netheos.pcsapi.exceptions.CRetriableException;
import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.models.CBlob;
import net.netheos.pcsapi.models.CDownloadRequest;
import net.netheos.pcsapi.models.CFile;
import net.netheos.pcsapi.models.CFolder;
import net.netheos.pcsapi.models.CPath;
import net.netheos.pcsapi.request.CResponse;
import net.netheos.pcsapi.models.CUploadRequest;
import net.netheos.pcsapi.request.Headers;
import net.netheos.pcsapi.request.HttpRequestor;
import net.netheos.pcsapi.request.RequestInvoker;
import net.netheos.pcsapi.request.ResponseValidator;
import net.netheos.pcsapi.models.RetryStrategy;
import net.netheos.pcsapi.utils.PcsUtils;
import net.netheos.pcsapi.utils.URIBuilder;

import org.apache.http.Header;
import org.apache.http.client.methods.HttpDelete;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpHead;
import org.apache.http.client.methods.HttpPut;
import org.apache.http.client.methods.HttpRequestBase;
import org.json.JSONArray;
import org.json.JSONObject;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URLEncoder;
import java.text.DateFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import net.netheos.pcsapi.models.CFolderContent;
import net.netheos.pcsapi.models.CMetadata;
import net.netheos.pcsapi.request.ByteSourceEntity;
import net.netheos.pcsapi.request.HttpExecutor;

/**
 * Swift client.
 */
class Swift
{

    private static final Logger LOGGER = LoggerFactory.getLogger( Swift.class );

    private static final ResponseValidator<CResponse> SWIFT_VALIDATOR = new SwiftResponseValidator();
    private static final ResponseValidator<CResponse> SWIFT_API_VALIDATOR = new SwiftApiResponseValidator( SWIFT_VALIDATOR );
    private static final String CONTENT_TYPE_DIRECTORY = "application/directory";
    private static final DateFormat DF_LAST_MODIFIED = new SimpleDateFormat( "yyyy-MM-dd'T'HH:mm:ss.SSSZ", Locale.ENGLISH );

    private final String accountEndpoint;
    private final String authToken;
    private final RetryStrategy retryStrategy;
    private final boolean useDirectoryMarkers;
    private final HttpExecutor httpExecutor;
    private volatile Container currentContainer;

    Swift( String accountEndpoint,
           String authToken,
           RetryStrategy retryStrategy,
           boolean useDirectoryMarkers,
           HttpExecutor sessionManager )
    {
        this.accountEndpoint = accountEndpoint;
        this.authToken = authToken;
        this.retryStrategy = retryStrategy;
        this.useDirectoryMarkers = useDirectoryMarkers;
        this.httpExecutor = sessionManager;
    }

    private void configureSession( HttpRequestBase request, String format )
    {
        request.addHeader( "X-Auth-token", authToken );
        if ( format != null ) {
            try {
                URI uri = request.getURI();
                if ( uri.getRawQuery() != null ) {
                    request.setURI( URI.create( uri + "&format=" + URLEncoder.encode( format, "UTF-8" ) ) );
                } else {
                    request.setURI( URI.create( uri + "?format=" + URLEncoder.encode( format, "UTF-8" ) ) );
                }

            } catch ( UnsupportedEncodingException ex ) {
                throw new UnsupportedOperationException( "Error setting the request format", ex );
            }
        }
    }

    private RequestInvoker<CResponse> getBasicRequestInvoker( HttpRequestBase request, CPath path )
    {
        configureSession( request, null );
        return new RequestInvoker<CResponse>( new HttpRequestor( request, path, httpExecutor ), SWIFT_VALIDATOR );
    }

    private RequestInvoker<CResponse> getApiRequestInvoker( HttpRequestBase request, CPath path )
    {
        configureSession( request, "json" );
        return new RequestInvoker<CResponse>( new HttpRequestor( request, path, httpExecutor ), SWIFT_API_VALIDATOR );
    }

    /**
     * Perform a quick HEAD request on the given object, to check existence and type.
     */
    private Headers headOrNull( CPath path )
    {
        try {
            String url = getObjectUrl( path );
            HttpHead request = new HttpHead( url );

            RequestInvoker<CResponse> ri = getBasicRequestInvoker( request, null );
            CResponse response = retryStrategy.invokeRetry( ri );
            return response.getHeaders();

        } catch ( CFileNotFoundException ex ) {
            return null;
        }
    }

    public Container useFirstContainer()
            throws CStorageException
    {
        List<Container> containers = getContainers();
        if ( containers.isEmpty() ) {
            throw new IllegalStateException( "Account " + accountEndpoint + " has no container ?!" );
        } else {
            useContainer( containers.get( 0 ) );
        }
        if ( containers.size() > 1 ) {
            LOGGER.warn( "Account {} has {} containers: choosing first one as current: {}",
                         accountEndpoint, containers.size(), currentContainer );
        }
        return currentContainer;
    }

    private void useContainer( Container container )
    {
        currentContainer = container;
        LOGGER.debug( "Using container: {}", container );
    }

    private List<Container> getContainers()
            throws CStorageException
    {
        RequestInvoker<CResponse> ri = getApiRequestInvoker( new HttpGet( accountEndpoint ), null );
        JSONArray array = retryStrategy.invokeRetry( ri ).asJSONArray();

        List<Container> containers = new ArrayList<Container>( array.length() );
        for ( int i = 0; i < array.length(); i++ ) {
            containers.add( new Container( array.getJSONObject( i ) ) );
        }
        LOGGER.debug( "Available containers: {}", containers.size() );
        return containers;
    }

    /**
     * Create a folder without creating any higher level intermediary folders.
     *
     * @param path The folder path
     */
    private void rawCreateFolder( CPath path )
            throws CStorageException
    {
        CResponse response = null;
        try {
            String url = getObjectUrl( path );
            HttpPut request = new HttpPut( url );
            request.addHeader( "Content-Type", CONTENT_TYPE_DIRECTORY );

            RequestInvoker<CResponse> ri = getApiRequestInvoker( request, path );
            response = retryStrategy.invokeRetry( ri );

        } finally {
            PcsUtils.closeQuietly( response );
        }
    }

    /**
     * Create any parent folders if they do not exist, to meet old swift convention.
     * <p/>
     * hubiC requires these objects for the sub-objects to be visible in webapp. As an optimization, we consider that if
     * folder a/b/c exists, then a/ and a/b/ also exist so are not checked nor created.
     *
     * @param leafFolderPath
     */
    private void createIntermediaryFoldersObjects( CPath leafFolderPath )
            throws CStorageException
    {
        // We check for folder existence before creation,
        // as in general leaf folder is likely to already exist.
        // So we walk from leaf to root:
        CPath path = leafFolderPath;

        List<CPath> parentFolders = new LinkedList<CPath>();
        CFile file;
        while ( !path.isRoot() ) {
            file = getFile( path );
            if ( file != null ) {
                if ( file.isBlob() ) {
                    // Problem here: clash between folder and blob
                    throw new CInvalidFileTypeException( file.getPath(), false );
                }
                break;
            } else {
                LOGGER.debug( "Nothing exists at path: {}, will go up", path );
                parentFolders.add( 0, path );
                path = path.getParent();
            }
        }

        // By now we know which folders to create:
        if ( !parentFolders.isEmpty() ) {
            LOGGER.debug( "Inexisting parent_folders will be created: {}", parentFolders );
            for ( CPath parent : parentFolders ) {
                LOGGER.debug( "Creating intermediary folder: {}", parent );
                rawCreateFolder( parent );
            }
        }
    }

    /**
     * Inquire details about object at given path.
     *
     * @param path The file path
     * @return a CFolder, CBlob or None if no object exist at this path
     */
    public CFile getFile( CPath path )
    {
        Headers headers = headOrNull( path );
        if ( headers == null ) {
            return null;
        }
        if ( !headers.contains( "Content-Type" ) ) {
            LOGGER.warn( "{} object has no content type ?!", path );
            return null;
        }
        CFile file;
        if ( !CONTENT_TYPE_DIRECTORY.equals( headers.getHeaderValue( "Content-Type" ) ) ) {
            file = new CBlob( path,
                              Integer.parseInt( headers.getHeaderValue( "Content-Length" ) ),
                              headers.getHeaderValue( "Content-Type" ),
                              parseTimestamp( headers ),
                              parseMetaHeaders( headers ) );
        } else {
            file = new CFolder( path,
                                parseTimestamp( headers ),
                                parseMetaHeaders( headers ) );
        }
        return file;
    }

    private JSONArray listObjectsWithinFolder( CPath path, String delimiter )
            throws CStorageException
    {
        // prefix should not start with a slash, but end with a slash:
        // '/path/to/folder' --> 'path/to/folder/'
        String prefix = path.getPathName().substring( 1 ) + "/";
        if ( prefix.equals( "/" ) ) {
            prefix = "";
        }

        String url = getCurrentContainerUrl();
        URIBuilder builder = new URIBuilder( URI.create( url ) );
        builder.addParameter( "prefix", prefix );
        if ( delimiter != null ) {
            builder.addParameter( "delimiter", delimiter );
        }

        HttpGet request = new HttpGet( builder.build() );
        RequestInvoker<CResponse> ri = getApiRequestInvoker( request, path );
        return retryStrategy.invokeRetry( ri ).asJSONArray();
    }

    /**
     * Return map of CFile in given folder. Key is file CPath
     *
     * @param path the CFolder object or CPath to be listed
     * @return
     */
    public CFolderContent listFolder( CPath path )
            throws CStorageException
    {
        JSONArray array = listObjectsWithinFolder( path, "/" );
        CFile file;
        if ( array == null || array.length() == 0 ) {
            // List is empty ; can be caused by a really empty folder,
            // a non existing folder, or a blob
            // Distinguish the different cases :
            file = getFile( path );
            if ( file == null ) { // Nothing at that path
                return null;
            }
            if ( file.isBlob() ) { // It is a blob : error !
                throw new CInvalidFileTypeException( path, false );
            }
            return new CFolderContent( Collections.EMPTY_MAP );
        }

        Map<CPath, CFile> ret = new HashMap<CPath, CFile>();

        boolean detailed;
        JSONObject obj;
        for ( int i = 0; i < array.length(); i++ ) {
            obj = array.getJSONObject( i );
            if ( obj.has( "subdir" ) ) {
                // indicates a non empty sub directory
                // There are two cases here : provider uses directory-markers, or not.
                // - if yes, another entry should exist in json with more detailed informations.
                // - if not, this will be the only entry that indicates a sub folder,
                //   so we keep this file, but we'll memorize it only if it is not already present
                //   in returned value.
                file = new CFolder( new CPath( obj.getString( "subdir" ) ) );
                detailed = false;

            } else {
                detailed = true;
                if ( !CONTENT_TYPE_DIRECTORY.equals( obj.getString( "content_type" ) ) ) {
                    file = new CBlob( new CPath( obj.getString( "name" ) ),
                                      obj.getLong( "bytes" ),
                                      obj.getString( "content_type" ),
                                      parseLastModified( obj ),
                                      null ); // we do not have this detailed information
                } else {
                    file = new CFolder( new CPath( obj.getString( "name" ) ),
                                        parseLastModified( obj ),
                                        null ); // we do not have this detailed information
                }
            }

            if ( detailed || !ret.containsKey( path ) ) {
                // If we got a detailed file, we always store it
                // If we got only rough description, we keep it only if no detailed info already exists
                ret.put( file.getPath(), file );
            }
        }
        return new CFolderContent( ret );
    }

    public CFolderContent listFolder( CFolder path )
            throws CStorageException
    {
        return listFolder( path.getPath() );
    }

    public boolean createFolder( CPath path )
            throws CStorageException
    {
        CFile file = getFile( path );
        if ( file != null ) {
            if ( file.isFolder() ) {
                return false; // folder already exists
            }
            // It is a blob : error !
            throw new CInvalidFileTypeException( path, false );
        }
        if ( useDirectoryMarkers ) {
            createIntermediaryFoldersObjects( path.getParent() );
        }
        rawCreateFolder( path );
        return true;
    }

    /**
     * If path references a folder, this is a lengthly operation because all sub-objects must be deleted one by one. (as
     * of 2014-02, hubic swift does not seem to support bulk deletes :
     * http://docs.openstack.org/developer/swift/middleware.html#module-swift.common.middleware.bulk )
     *
     * @param path The file path
     */
    public boolean delete( CPath path )
            throws CStorageException
    {
        // Request sub-objects w/o delimiter : all sub-objects are returned
        // In case c_path is a blob, we'll get an empty list
        JSONArray array = listObjectsWithinFolder( path, null );
        LOGGER.debug( "List objects with folder {} = {}", path, array );
        // Now delete all objects ; we start with the deepest ones so that
        // in case we are interrupted, directory markers are still present
        // Note : swift may guarantee that list is ordered, but could not confirm information...
        List<String> pathnames = new ArrayList<String>( array.length() + 1 );
        JSONObject obj;
        for ( int i = 0; i < array.length(); i++ ) {
            obj = array.getJSONObject( i );
            pathnames.add( "/" + obj.getString( "name" ) );
        }
        Collections.sort( pathnames );
        Collections.reverse( pathnames );
        // Now we also add that top-level folder (or blob) to delete :
        pathnames.add( path.getPathName() );

        boolean atLeastOneDeleted = false;
        boolean lastDeleteWorked = false;
        for ( String pathname : pathnames ) {
            lastDeleteWorked = false;
            LOGGER.debug( "deleting object at path : {}", pathname );
            String url = getObjectUrl( new CPath( pathname ) );

            CResponse response = null;
            try {
                RequestInvoker<CResponse> ri = getApiRequestInvoker( new HttpDelete( url ), path );
                response = retryStrategy.invokeRetry( ri );
                atLeastOneDeleted = true;

            } catch ( CFileNotFoundException ex ) {
                // continue
            } finally {
                PcsUtils.closeQuietly( response );
            }
        }
        return atLeastOneDeleted;
    }

    public void download( CDownloadRequest downloadRequest )
            throws CStorageException
    {
        String url = getObjectUrl( downloadRequest.getPath() );

        HttpGet request = new HttpGet( url );
        for ( Header header : downloadRequest.getHttpHeaders() ) {
            request.addHeader( header );
        }

        RequestInvoker<CResponse> ri = getBasicRequestInvoker( request, downloadRequest.getPath() );

        CResponse response = null;
        try {
            response = retryStrategy.invokeRetry( ri );
            if ( CONTENT_TYPE_DIRECTORY.equals( response.getContentType() ) ) {
                throw new CInvalidFileTypeException( downloadRequest.getPath(), true );
            }
            PcsUtils.downloadDataToSink( response, downloadRequest.getByteSink() );
        } finally {
            PcsUtils.closeQuietly( response );
        }
    }

    /**
     * Url encode object path, and concatenate to current container URL to get full URL
     *
     * @param path The object path
     * @return The object url
     */
    private String getObjectUrl( CPath path )
    {
        String containerUrl = getCurrentContainerUrl();
        return containerUrl + path.getUrlEncoded();
    }

    private String getCurrentContainerUrl()
    {
        ensureCurrentContainerIsSet();
        return accountEndpoint + "/" + currentContainer;
    }

    private void ensureCurrentContainerIsSet()
    {
        if ( currentContainer == null ) {
            throw new IllegalStateException( "Undefined current container for account " + accountEndpoint );
        }
    }

    public void upload( CUploadRequest uploadRequest )
            throws CStorageException
    {
        // Check before upload : is it a folder ?
        // (uploading a blob to a folder would work, but would hide all folder sub-files)
        CFile file = getFile( uploadRequest.getPath() );
        if ( file != null && file.isFolder() ) {
            throw new CInvalidFileTypeException( uploadRequest.getPath(), true );
        }
        if ( useDirectoryMarkers ) {
            createIntermediaryFoldersObjects( uploadRequest.getPath().getParent() );
        }
        String url = getObjectUrl( uploadRequest.getPath() );
        Headers headers = new Headers();

        if ( uploadRequest.getContentType() != null ) {
            headers.addHeader( "Content-Type", uploadRequest.getContentType() );
        }
        if ( uploadRequest.getMetadata() != null ) {
            addMetadataHeaders( headers, uploadRequest.getMetadata() );
        }

        try {
            HttpPut request = new HttpPut( url );
            for ( Header header : headers ) {
                request.addHeader( header );
            }

            ByteSource bs = uploadRequest.getByteSource();
            request.setEntity( new ByteSourceEntity( bs ) );

            RequestInvoker<CResponse> ri = getBasicRequestInvoker( request, uploadRequest.getPath() );
            retryStrategy.invokeRetry( ri ).close();

        } catch ( IOException ex ) {
            throw new CStorageException( "Can't close stream", ex );
        }
    }

    static Date parseLastModified( JSONObject json )
    {
        try {
            String lm = json.optString( "last_modified", null );
            if ( lm == null ) {
                return null;
            }

            // Date format
            if ( !lm.contains( "+" ) ) {
                lm += "+0000";
            }

            // We remove the microseconds
            StringBuilder builder = new StringBuilder( lm );
            builder.delete( lm.lastIndexOf( '.' ) + 4, lm.lastIndexOf( '+' ) );

            return DF_LAST_MODIFIED.parse( builder.toString() );

        } catch ( ParseException ex ) {
            LOGGER.warn( "Error parsing date", ex );
            return null;
        }
    }

    static Date parseTimestamp( Headers headers )
    {
        String headerValue = headers.getHeaderValue( "X-Timestamp" );
        if ( headerValue == null ) {
            return null;
        }
        int index = headerValue.indexOf( '.' );
        long date = Long.parseLong( headerValue.substring( 0, index ) ) * 1000;
        date += Long.parseLong( headerValue.substring( index + 1, index + 4 ) );

        return new Date( date );
    }

    private CMetadata parseMetaHeaders( Headers headers )
    {
        Map<String, String> metadata = new HashMap<String, String>();
        for ( Header header : headers ) {
            if ( header.getName().toLowerCase().startsWith( "x-object-meta-" ) ) {
                metadata.put( header.getName().substring( 14 ).toLowerCase(), header.getValue() );
            }
        }
        return new CMetadata( metadata );
    }

    private void addMetadataHeaders( Headers headers, CMetadata metadata )
    {
        String value;
        for ( Map.Entry<String, String> item : metadata.getMap().entrySet() ) {
            value = item.getValue();
            value = value.replace( "\r", "" ).replace( "\n", "" );
            headers.addHeader( "X-Object-Meta-" + item.getKey(), value );
        }
    }

    private static class SwiftResponseValidator
            implements ResponseValidator<CResponse>
    {

        @Override
        public void validateResponse( CResponse response, CPath path )
                throws CStorageException
        {
            // A response is valid if server code is 2xx. It is recoverable in case of server error 5xx.
            LOGGER.debug( "validating swift response: {} {} : {} {}",
                          response.getMethod(),
                          PcsUtils.shortenUrl( response.getUri() ),
                          response.getStatus(),
                          response.getReason() );
            int code = response.getStatus();
            if ( code < 300 ) {
                return;
            }
            if ( code >= 500
                 || code == 498 || code == 429 ) { // too many requests
                // We force connection closing in that case,
                // to avoid being sticked on a non-healthy server :
                throw new CRetriableException( buildHttpError( response, null, path ) );
            }
            // Other cases
            throw buildHttpError( response, null, path );
        }

        private CStorageException buildHttpError( CResponse response, String errorMessage, CPath path )
        {
            // Swift error messages are only in reason, so nothing to extract here :
            return PcsUtils.buildCStorageException( response, errorMessage, path );
        }

    }

    private static class SwiftApiResponseValidator
            implements ResponseValidator<CResponse>
    {

        private final ResponseValidator<CResponse> parent;

        private SwiftApiResponseValidator( ResponseValidator<CResponse> parent )
        {
            this.parent = parent;
        }

        @Override
        public void validateResponse( CResponse response, CPath path )
                throws CStorageException
        {
            // Validate swift response, and also checks content is empty, or content-type is json.
            parent.validateResponse( response, path );
            long cl = response.getContentLength();
            if ( cl > 0 ) {
                PcsUtils.ensureContentTypeIsJson( response, false );
            }
        }

    }

    public static class Container
    {

        private final String name;

        private Container( JSONObject json )
        {
            name = json.getString( "name" );
        }

        @Override
        public String toString()
        {
            return name;
        }

    }

}
