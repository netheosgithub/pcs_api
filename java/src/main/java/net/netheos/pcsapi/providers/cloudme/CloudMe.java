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

package net.netheos.pcsapi.providers.cloudme;

import net.netheos.pcsapi.exceptions.CFileNotFoundException;
import net.netheos.pcsapi.exceptions.CInvalidFileTypeException;
import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.models.CBlob;
import net.netheos.pcsapi.models.CDownloadRequest;
import net.netheos.pcsapi.models.CFile;
import net.netheos.pcsapi.models.CFolder;
import net.netheos.pcsapi.models.CFolderContent;
import net.netheos.pcsapi.models.CPath;
import net.netheos.pcsapi.models.CQuota;
import net.netheos.pcsapi.models.CUploadRequest;
import net.netheos.pcsapi.oauth.PasswordSessionManager;
import net.netheos.pcsapi.request.ByteSourceBody;
import net.netheos.pcsapi.request.CResponse;
import net.netheos.pcsapi.request.CloudMeMultipartEntity;
import net.netheos.pcsapi.request.Headers;
import net.netheos.pcsapi.request.HttpRequestor;
import net.netheos.pcsapi.request.RequestInvoker;
import net.netheos.pcsapi.request.ResponseValidator;
import net.netheos.pcsapi.storage.StorageBuilder;
import net.netheos.pcsapi.storage.StorageProvider;
import net.netheos.pcsapi.utils.PcsUtils;
import net.netheos.pcsapi.utils.URIBuilder;
import net.netheos.pcsapi.utils.XmlUtils;

import org.apache.http.Header;
import org.apache.http.HttpEntity;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.client.methods.HttpRequestBase;
import org.apache.http.entity.StringEntity;
import org.apache.http.entity.mime.MultipartEntity;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Class that implements the CloudMe storage provider.
 */
public class CloudMe
        extends StorageProvider<PasswordSessionManager>
{

    private static final Logger LOGGER = LoggerFactory.getLogger( CloudMe.class );
    public static final String PROVIDER_NAME = "cloudme";
    private final static String BASE_URL = "https://www.cloudme.com/v1";

    final private static String SOAP_HEADER = "<SOAP-ENV:Envelope xmlns:"
                                              + "SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                                              + "SOAP-ENV:encodingStyle=\"\" "
                                              + "xmlns:xsi=\"http://www.w3.org/1999/XMLSchema-instance\" "
                                              + "xmlns:xsd=\"http://www.w3.org/1999/XMLSchema\">"
                                              + "<SOAP-ENV:Body>";

    final private static String SOAP_FOOTER = "</SOAP-ENV:Body></SOAP-ENV:Envelope>";

    private static final ResponseValidator<CResponse> BASIC_RESPONSE_VALIDATOR = new BasicResponseValidator();
    private static final ResponseValidator<CResponse> API_RESPONSE_VALIDATOR = new ApiResponseValidator(
            BASIC_RESPONSE_VALIDATOR );

    /**
     * Id of account's root folder (supposed to be constant, cached for performance).
     */
    private String rootId;

    /**
     * @param builder builder used to feed the storage provider
     */
    public CloudMe( StorageBuilder builder )
    {
        super( PROVIDER_NAME,
               new PasswordSessionManager( builder ),
               builder.getRetryStrategy(),
               builder.getHttpClient() );
    }

    /**
     * An invoker that checks response content type = XML : to be used by all API requests
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
        return getLogin().getElementsByTagName( "username" ).item( 0 ).getTextContent();
    }

    /**
     * @return a CloudMe login data
     * @throws net.netheos.pcsapi.exceptions.CStorageException
     */
    private Document getLogin()
            throws CStorageException
    {
        HttpPost request = buildSoapRequest( "login", "" );
        CResponse response = retryStrategy.invokeRetry( getApiRequestInvoker( request, null ) );

        return response.asDom();
    }

    @Override
    public CQuota getQuota()
            throws CStorageException
    {
        Document dom = getLogin();

        long used = getLongFromDom( dom, "currentSize" );
        long limit = getLongFromDom( dom, "quotaLimit" );

        return new CQuota( used, limit );
    }

    @Override
    public CFolderContent listRootFolder()
            throws CInvalidFileTypeException
    {
        return listFolder( CPath.ROOT );
    }

    /**
     * Retrieves a long value according to a Dom and a tag
     *
     * @param dom
     * @param tag
     * @return long value
     */
    private long getLongFromDom( Document dom, String tag )
    {
        return Long.parseLong( dom.getElementsByTagName( tag ).item( 0 ).getTextContent() );
    }

    /**
     * Lists all blobs (ie files that are not folders) in a given cloudme folder.
     *
     * @param cmFolder CloudMe folder to be listed
     * @return list of all blobs
     */
    private List<CMBlob> listBlobs( CMFolder cmFolder )
            throws ParseException
    {
        HttpPost request = buildSoapRequest( "queryFolder", "<folder id='" + cmFolder.getId() + "'/>" );
        CResponse response = retryStrategy.invokeRetry( getApiRequestInvoker( request, cmFolder.getCPath() ) );

        Document dom = response.asDom();
        NodeList elementList = dom.getElementsByTagNameNS( "*", "entry" );

        List<CMBlob> cmBlobs = new ArrayList<CMBlob>();

        for ( int i = 0; i < elementList.getLength(); i++ ) {
            Element element = ( Element ) elementList.item( i );
            cmBlobs.add( CMBlob.buildCMFile( cmFolder, element ) );
        }

        return cmBlobs;
    }

    /**
     * Gets a blob according to the parent folder id of the folder.
     *
     * @param cmFolder parent folder
     * @param baseName base name of the file to find
     * @return the CloudMe blob, or null if not found
     * @throws ParseException
     */
    private CMBlob getBlobByName( CMFolder cmFolder, String baseName )
            throws ParseException
    {
        StringBuilder innerXml = new StringBuilder();
        innerXml.append( "<folder id=" ).append( "'" ).append( cmFolder.getId() ).append( "'" ).append( "/>" )
                .append( "<query>" )
                .append( "\"" ).append( XmlUtils.escape( baseName ) ).append( "\"" )
                .append( "</query>" )
                .append( "<count>1</count>" );

        HttpPost request = buildSoapRequest( "queryFolder", innerXml.toString() );
        CResponse response = retryStrategy.invokeRetry( getApiRequestInvoker( request, cmFolder.getCPath() ) );

        Document dom = response.asDom();
        NodeList elementList = dom.getElementsByTagNameNS( "*", "entry" );

        if ( elementList.getLength() == 0 ) {
            return null;
        }

        CMBlob cmBlob = CMBlob.buildCMFile( cmFolder, ( Element ) elementList.item( 0 ) );

        return cmBlob;
    }

    /**
     * Gets the folders tree structure beginning from the root folder.
     *
     * @return the root folder
     */
    private CMFolder loadFoldersStructure()
    {
        CMFolder rootFolder = new CMFolder( getRootId(), "root" );

        HttpPost request = buildSoapRequest( "getFolderXML", "<folder id='" + rootFolder.getId() + "'/>" );
        CResponse response = retryStrategy.invokeRetry( getApiRequestInvoker( request, null ) );
        Element rootElement = findRootFolderElement( response.asDom() );

        scanFolderLevel( rootElement, rootFolder );

        return rootFolder;
    }

    /**
     * Gets the element corresponding to the root folder in the DOM.
     *
     * @param dom document where the root element is to be found
     * @return the element corresponding to the root folder
     */
    private Element findRootFolderElement( Document dom )
    {
        NodeList list = dom.getElementsByTagNameNS( "*", "folder" );

        final String localRootId = getRootId(); // cache it also in this method

        for ( int i = 0; i < list.getLength(); i++ ) {
            Element e = ( Element ) list.item( i );
            if ( e.getAttribute( "id" ).equals( localRootId ) ) {
                return e;
            }
        }
        throw new CStorageException( "Could not find root folder with id=" + localRootId );
    }

    /**
     * Recursive method that parses folders XML and builds CMFolder structure.
     *
     * @param element
     * @param cmFolder
     */
    private void scanFolderLevel( Element element, CMFolder cmFolder )
    {
        NodeList nodeList = element.getChildNodes();

        for ( int i = 0; i < nodeList.getLength(); i++ ) {
            Node currentNode = nodeList.item( i );

            if ( currentNode.getNodeType() != Node.ELEMENT_NODE ) {
                continue;
            }

            Element currentElement = ( Element ) currentNode;

            if ( !currentElement.getLocalName().equals( "folder" ) ) {
                continue;
            }

            //calls this method for all the children which is Element
            CMFolder childFolder = cmFolder.addChild( currentElement.getAttribute( "id" ),
                                                      currentElement.getAttribute( "name" ) );

            scanFolderLevel( currentElement, childFolder );
        }
    }

    /**
     * There are 3 main steps to list a folder: - generate the tree view of CloudMe storage - list all the subfolders -
     * list all the blobs
     *
     * @param cpath
     * @return CFolderContent
     * @throws CStorageException
     */
    @Override
    public CFolderContent listFolder( final CPath cpath )
            throws CStorageException
    {
        CMFolder cmRoot = loadFoldersStructure();
        CMFolder cmFolder = cmRoot.getFolder( cpath );

        if ( cmFolder == null ) {

            CMFolder cmParentFolder = cmRoot.getFolder( cpath.getParent() );

            if ( cmParentFolder != null ) {
                CMBlob cmBlob;

                try {
                    cmBlob = getBlobByName( cmParentFolder, cpath.getBaseName() );
                } catch ( ParseException e ) {
                    throw new CStorageException( "Can't parse blob at " + cpath, e );
                }

                if ( cmBlob != null ) {
                    throw new CInvalidFileTypeException( cpath, false );
                }
            }

            return null;
        }

        List<CMBlob> cmBlobs = null;

        try {
            cmBlobs = listBlobs( cmFolder );
        } catch ( ParseException e ) {
            throw new CStorageException( e.getMessage(), e );
        }

        Map<CPath, CFile> map = new HashMap<CPath, CFile>();

        // Adding folders
        for ( CMFolder childFolder : cmFolder ) {
            CFile cFile = childFolder.toCFolder();
            map.put( cFile.getPath(), cFile );
        }

        // Adding blobs
        for ( CMBlob cmBLob : cmBlobs ) {
            CBlob cBlob = cmBLob.toCBlob();
            map.put( cBlob.getPath(), cBlob );
        }

        return new CFolderContent( map );
    }

    /**
     * Gets the id of root folder.
     * <p/>
     * If the root id is already known it is returned else it is fetched then cached.
     *
     * @return the id of the root folder
     */
    private String getRootId()
    {
        if ( rootId == null ) {
            rootId = getLogin().getElementsByTagName( "home" ).item( 0 ).getTextContent();
        }

        return rootId;
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
        if ( cpath.isRoot() ) {
            return false;
        }

        CMFolder cmRoot = loadFoldersStructure();
        CMFolder cmFolder = cmRoot.getFolder( cpath );

        if ( cmFolder != null ) {
            // folder already exists
            return false;
        }

        cmFolder = createIntermediaryFolders( cmRoot, cpath );

        return ( cmFolder != null );
    }

    /**
     * Creates folder with given path, with required intermediary folders.
     *
     * @param cmRoot contains the whole folders structure
     * @param cpath path of folder to create
     * @return the createdfolder corresponding to targeted cpath
     * @throws CInvalidFileTypeException if a blob exists along that path
     */
    private CMFolder createIntermediaryFolders( CMFolder cmRoot, CPath cpath )
    {
        List<String> baseNames = cpath.split();

        CMFolder currentFolder = cmRoot;
        CMFolder childFolder = null;
        boolean firstFolderCreation = true;

        for ( String baseName : baseNames ) {
            childFolder = currentFolder.getChildByName( baseName );

            if ( childFolder == null ) {
                // Intermediary folder does not exist : has to be created

                if ( firstFolderCreation ) {
                    // This is the first intermediary folder to create:
                    // let's check that there is no blob with that name already existing
                    try {
                        CMBlob cmBlob = getBlobByName( currentFolder, baseName );

                        if ( cmBlob != null ) {
                            throw new CInvalidFileTypeException( cmBlob.getPath(), false );
                        }

                    } catch ( ParseException e ) {
                        throw new CStorageException( e.getMessage(), e );
                    }

                    firstFolderCreation = false;
                }

                childFolder = rawCreateFolder( currentFolder, baseName );
            }

            currentFolder = childFolder;
        }

        return childFolder;
    }

    /**
     * Issue request to create a folder with given parent folder and name.
     * <p/>
     * No checks are performed before request.
     *
     * @param cmParentFolder folder of the container
     * @param name base name of the folder to be created
     * @return the new CloudMe folder
     */
    private CMFolder rawCreateFolder( CMFolder cmParentFolder, String name )
    {
        StringBuilder innerXml = new StringBuilder();
        innerXml.append( "<folder id='" ).append( cmParentFolder.getId() ).append( "'/>" );
        innerXml.append( "<childFolder>" ).append( XmlUtils.escape( name ) ).append( "</childFolder>" );

        HttpPost request = buildSoapRequest( "newFolder", innerXml.toString() );
        CResponse response = retryStrategy.invokeRetry( getApiRequestInvoker( request, null ) );

        Document dom = response.asDom();
        String newFolderId = dom.getElementsByTagNameNS( "*", "newFolderId" ).item( 0 ).getTextContent();

        return cmParentFolder.addChild( newFolderId, name );
    }

    @Override
    public boolean delete( final CPath cpath )
            throws CStorageException
    {
        if ( cpath.isRoot() ) {
            throw new CStorageException( "Can't delete root folder" );
        }

        CMFolder cmRoot = loadFoldersStructure();
        CMFolder cmParentFolder = cmRoot.getFolder( cpath.getParent() );

        if ( cmParentFolder == null ) {
            // parent folder of given path does exist => path does not exist
            return false;
        }

        CMFolder cmFolder = cmParentFolder.getChildByName( cpath.getBaseName() );

        if ( cmFolder != null ) {
            // We have to delete a folder
            StringBuilder innerXml = new StringBuilder();
            innerXml.append( "<folder id='" ).append( cmParentFolder.getId() ).append( "'/>" );
            innerXml.append( "<childFolder id='" ).append( cmFolder.getId() ).append( "'/>" );
            HttpPost request = buildSoapRequest( "deleteFolder", innerXml.toString() );
            CResponse response = retryStrategy.invokeRetry( getApiRequestInvoker( request, null ) );

            String result = response.asDom().getElementsByTagNameNS( "*", "deleteFolderResponse" ).item( 0 )
                    .getTextContent();

            return result.trim().equalsIgnoreCase( "OK" );

        } else {
            // It's not a folder, it should be a blob...
            try {
                CMBlob cmBlob = getBlobByName( cmParentFolder, cpath.getBaseName() );

                if ( cmBlob == null ) {
                    // The blob does not exist...
                    return false;
                }

                StringBuilder innerXml = new StringBuilder( "<folder id='" ).append( cmParentFolder.getId() ).append(
                        "'/>" );
                innerXml.append( "<document id='" ).append( cmBlob.getId() ).append( "'/>" );

                HttpPost request = buildSoapRequest( "deleteDocument", innerXml.toString() );
                CResponse response = retryStrategy.invokeRetry( getApiRequestInvoker( request, null ) );

                String result = response.asDom().getElementsByTagNameNS( "*",
                                                                         "deleteDocumentResponse" ).item( 0 )
                        .getTextContent();

                return result.trim().equalsIgnoreCase( "OK" );

            } catch ( ParseException e ) {
                throw new CStorageException( e.getMessage(), e );
            }
        }
    }

    @Override
    public CFile getFile( final CPath cpath )
            throws CStorageException
    {
        CMFolder cmRoot = loadFoldersStructure();
        CMFolder cmParentFolder = cmRoot.getFolder( cpath.getParent() );

        if ( cmParentFolder == null ) {
            // parent folder of given path does exist => path does not exist
            return null;
        }

        CMFolder cmFolder = cmParentFolder.getChildByName( cpath.getBaseName() );

        if ( cmFolder != null ) {
            // the path corresponds to a folder
            return new CFolder( cmParentFolder.getCPath().add( cpath.getBaseName() ) );

        } else {
            // It's not a folder, it should be a blob...
            try {
                CMBlob cmBlob = getBlobByName( cmParentFolder, cpath.getBaseName() );

                if ( cmBlob == null ) {
                    return null;
                }

                return cmBlob.toCBlob();

            } catch ( ParseException e ) {
                throw new CStorageException( e.getMessage(), e );
            }
        }
    }

    @Override
    public void download( final CDownloadRequest downloadRequest )
            throws CStorageException
    {
        CPath path = downloadRequest.getPath();

        CMFolder cmRoot = loadFoldersStructure();
        CMFolder cmParentFolder = cmRoot.getFolder( path.getParent() );

        if ( cmParentFolder == null ) {
            // parent folder of given path does exist => file does not exist
            throw new CFileNotFoundException( "This file does not exist", downloadRequest.getPath() );
        }

        CMFolder cmFolder = cmParentFolder.getChildByName( path.getBaseName() );

        if ( cmFolder != null ) {
            // the path corresponds to a folder
            throw new CInvalidFileTypeException( downloadRequest.getPath(), true );

        } else {
            // It's not a folder, it should be a blob...
            try {
                CMBlob cmBlob = getBlobByName( cmParentFolder, path.getBaseName() );

                if ( cmBlob == null ) {
                    throw new CFileNotFoundException( "Can't download this file, it does not exist", path );
                }

                String url = buildRestUrl( "documents" ) + cmParentFolder.getId() + '/' + cmBlob.getId() + "/1";
                URI uri = new URIBuilder( URI.create( url ) ).build();

                HttpGet request = new HttpGet( uri );
                for ( Header header : downloadRequest.getHttpHeaders() ) {
                    request.addHeader( header );
                }

                RequestInvoker<CResponse> invoker = getBasicRequestInvoker( request,
                                                                            downloadRequest.getPath() );

                CResponse response = retryStrategy.invokeRetry( invoker );
                PcsUtils.downloadDataToSink( response, downloadRequest.getByteSink() );

            } catch ( ParseException e ) {
                throw new CStorageException( e.getMessage(), e );
            }
        }
    }

    @Override
    public void upload( final CUploadRequest ur )
            throws CStorageException
    {
        CPath path = ur.getPath();

        CMFolder cmRoot = loadFoldersStructure();
        CMFolder cmParentFolder = cmRoot.getFolder( path.getParent() );

        if ( cmParentFolder == null ) {
            // parent folder of given path does exist => folders needs to be created
            cmParentFolder = createIntermediaryFolders( cmRoot, path.getParent() );
        } else {

            CMFolder cmFolder = cmParentFolder.getChildByName( path.getBaseName() );

            if ( cmFolder != null ) {
                // The CPath corresponds to an existing folder, upload is not possible
                throw new CInvalidFileTypeException( path, true );
            }
        }

        String url = buildRestUrl( "documents" ) + cmParentFolder.getId();
        URI uri = URI.create( url );
        HttpPost request = new HttpPost( uri );

        Headers headers = new Headers();
        // TODO : Add Headers cf. Swift ?

        try {
            ByteSourceBody bsBody = new ByteSourceBody( ur.getByteSource(), path.getBaseName(), ur.getContentType() );
            MultipartEntity reqEntity = new CloudMeMultipartEntity();
            reqEntity.addPart( "bin", bsBody );
            request.setEntity( reqEntity );

            RequestInvoker<CResponse> invoker = getBasicRequestInvoker( request, ur.getPath() );
            CResponse response = retryStrategy.invokeRetry( invoker );
            response.close();

        } catch ( IOException ex ) {
            throw new CStorageException( "Can't close stream", ex );
        }
    }

    /**
     * builds an URL to call Rest services
     *
     * @param methodPath
     * @return URL
     */
    private String buildRestUrl( String methodPath )
    {
        return BASE_URL + '/' + methodPath + '/';
    }

    /**
     * Builds a SOAP request (NB: part of the CloudMe API is REST-based).
     *
     * @param action
     * @param innerXml
     * @return HttpPost
     */
    private HttpPost buildSoapRequest( String action, String innerXml )
    {
        HttpPost httpPost = new HttpPost( BASE_URL );
        httpPost.setHeader( "soapaction", action );
        httpPost.setHeader( "Content-Type", "text/xml; charset=utf-8" );

        final StringBuilder soap = new StringBuilder();
        soap.append( SOAP_HEADER );
        soap.append( "<" ).append( action ).append( ">" );
        soap.append( innerXml );
        soap.append( "</" ).append( action ).append( ">" );
        soap.append( SOAP_FOOTER );

        HttpEntity entity;

        try {
            entity = new StringEntity( soap.toString(), PcsUtils.UTF8.name() );
        } catch ( UnsupportedEncodingException e ) {
            throw new CStorageException( e.getMessage(), e );
        }

        httpPost.setEntity( entity );

        return httpPost;
    }

    /**
     * Validates a response from CloudMe XML API.
     * <p/>
     * An API response is valid if response is valid, and content-type is xml.
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
            PcsUtils.ensureContentTypeIsXml( response, true );

            LOGGER.debug( "ValidateResponse - all is OK" );
        }

    }

    /**
     * Validates a response for a file download or API request.
     * <p/>
     * Only server code is checked (content-type is ignored).
     */
    private static class BasicResponseValidator
            implements ResponseValidator<CResponse>
    {

        @Override
        public void validateResponse( CResponse response, CPath path )
                throws CStorageException
        {
            int status = response.getStatus();

            if ( status >= 300 ) {
                throw buildCHttpException( response, path );
            }
            // Everything is OK
        }

        /**
         * Builds a CHttpException
         *
         * @param response
         * @param cpath
         * @return CStorageException
         */
        private CStorageException buildCHttpException( CResponse response, CPath cpath )
        {
            StringBuilder sbHeaders = new StringBuilder();

            for ( Header header : response.getHeaders() ) {
                sbHeaders.append( header.getName() ).append( ": " ).append( header.getValue() ).append( '\n' );
            }

            String stringResponse = response.asString();

            /**
             * soap errors generates http 500 Internal server errors, and body looks like :
             * <code>
             * <?xml version='1.0' encoding='utf-8'?>
             * <SOAP-ENV:Envelope xmlns:SOAP-ENV='http://schemas.xmlsoap.org/soap/envelope/'
             * SOAP-ENV:encodingStyle='http://schemas.xmlsoap.org/soap/encoding/'>
             * <SOAP-ENV:Body>
             * <SOAP-ENV:Fault>
             * <faultcode>SOAP-ENV:Client</faultcode>
             * <faultstring>Not Found</faultstring>
             * <detail>
             * <error number='0' code='404' description='Not Found'>Document not found.</error>
             * </detail>
             * </SOAP-ENV:Fault>
             * </SOAP-ENV:Body>
             * </SOAP-ENV:Envelope>
             * </code>
             */
            String message = null;
            String ct = response.getContentType();

            if ( ct.contains( "application/xml" ) || ct.contains( "text/xml" ) ) {

                Document dom = XmlUtils.getDomFromString( stringResponse );

                NodeList faultCodeElements = dom.getElementsByTagNameNS( "*", "faultcode" );

                if ( faultCodeElements.getLength() > 0 ) {
                    String faultCode = faultCodeElements.item( 0 ).getTextContent();

                    if ( faultCode.equals( "SOAP-ENV:Client" ) ) {

                        NodeList faultStringElements = dom.getElementsByTagNameNS( "*", "faultstring" );
                        if ( faultStringElements.getLength() > 0 ) {
                            message = faultStringElements.item( 0 ).getTextContent();
                        }
                    }

                    NodeList errorElements = dom.getElementsByTagNameNS( "*", "error" );

                    if ( errorElements.getLength() > 0 ) {
                        Element errorElement = ( Element ) errorElements.item( 0 );

                        if ( !errorElement.getAttribute( "code" ).equals( "" ) ) {

                            String code = errorElement.getAttribute( "code" );
                            String reason = errorElement.getAttribute( "description" );
                            String number = errorElement.getAttribute( "number" );
                            message = "[" + code + " " + reason + " " + number + "] " + errorElement
                                    .getTextContent();

                            if ( cpath != null ) {
                                message += " (" + cpath + ")";
                            }

                            if ( code.trim().equals( "404" ) ) {
                                return new CFileNotFoundException( message, cpath );

                            }
                        }
                    }
                }
            } else {
                // We haven't received a standard server error message ?!
                LOGGER.error( "Unparsable server error: {}", stringResponse );
                // Construct some message for exception:
                message = PcsUtils.abbreviate( "Unparsable server error: " + stringResponse, 200 );
                LOGGER.error( "Unparsable server error has headers: {}", sbHeaders );
            }

            return PcsUtils.buildCStorageException( response, message, cpath );
        }

    }

}
