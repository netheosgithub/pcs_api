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

package net.netheos.pcsapi.providers;

import java.io.File;
import java.util.Arrays;
import java.util.Collection;
import java.util.Set;

import net.netheos.pcsapi.bytesio.ByteSource;
import net.netheos.pcsapi.bytesio.FileByteSink;
import net.netheos.pcsapi.bytesio.MemoryByteSink;
import net.netheos.pcsapi.bytesio.MemoryByteSource;
import net.netheos.pcsapi.bytesio.ProgressListener;
import net.netheos.pcsapi.bytesio.StdoutProgressListener;
import net.netheos.pcsapi.exceptions.CFileNotFoundException;
import net.netheos.pcsapi.exceptions.CInvalidFileTypeException;
import net.netheos.pcsapi.exceptions.CRetriableException;
import net.netheos.pcsapi.models.CBlob;
import net.netheos.pcsapi.models.CDownloadRequest;
import net.netheos.pcsapi.models.CFile;
import net.netheos.pcsapi.models.CFolder;
import net.netheos.pcsapi.models.CFolderContent;
import net.netheos.pcsapi.models.CMetadata;
import net.netheos.pcsapi.models.CPath;
import net.netheos.pcsapi.models.CQuota;
import net.netheos.pcsapi.models.CUploadRequest;
import net.netheos.pcsapi.oauth.SessionManager;
import net.netheos.pcsapi.oauth.SessionManagerUtil;
import net.netheos.pcsapi.providers.cloudme.CloudMe;
import net.netheos.pcsapi.providers.dropbox.Dropbox;
import net.netheos.pcsapi.providers.googledrive.GoogleDrive;
import net.netheos.pcsapi.providers.hubic.Hubic;
import net.netheos.pcsapi.storage.IStorageProvider;
import net.netheos.pcsapi.storage.StorageFacade;
import net.netheos.pcsapi.storage.StorageProvider;
import net.netheos.pcsapi.utils.PcsUtils;

import org.apache.commons.io.FileUtils;
import org.hamcrest.CoreMatchers;

import static org.hamcrest.CoreMatchers.*;

import org.junit.After;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import static org.junit.Assert.*;

import org.junit.Assume;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;

/**
 *
 */
@RunWith( Parameterized.class )
public class BasicTest
{

    private static final Logger LOGGER = LoggerFactory.getLogger( BasicTest.class );

    @Parameterized.Parameters( name = "{0}" )
    public static Collection<Object[]> getTestsParameters()
            throws Exception
    {
        return StorageProviderFactory.storageProviderFactory();
    }

    private File tmpDir;
    private final IStorageProvider storage;

    public BasicTest( IStorageProvider storage )
    {
        this.storage = storage;
        LOGGER.info( "Will run tests with provider : {}", storage );
    }

    @Before
    public void setUp()
            throws Exception
    {
        tmpDir = File.createTempFile( "pcs_api", ".dir" );
        FileUtils.deleteQuietly( tmpDir );
        tmpDir.mkdirs();
    }

    @After
    public void tearDown()
    {
        FileUtils.deleteQuietly( tmpDir );
    }

    @Test
    public void testRegisteredProviders()
    {
        Set<String> providerNames = StorageFacade.getRegisteredProviders();
        LOGGER.info( "Registered providers in pcs_api: {}", providerNames );
        assertTrue( providerNames.contains( Dropbox.PROVIDER_NAME ) );
        assertTrue( providerNames.contains( Hubic.PROVIDER_NAME ) );
        assertTrue( providerNames.contains( GoogleDrive.PROVIDER_NAME ) );
        assertTrue( providerNames.contains( CloudMe.PROVIDER_NAME ) );
    }

    @Ignore
    @Test
    public void testCleanupStorageBeforeTests()
            throws Exception
    {
        // Not a real test : only some cleanup before next tests
        MiscUtils.cleanupTestFolders( storage );
    }

    @Test
    public void testGetUserId()
            throws Exception
    {
        String userId = storage.getUserId();
        LOGGER.info( "Retrieved from provider {} : user_id = {}", storage.getProviderName(), userId );
        SessionManager sm = ( ( StorageProvider ) storage ).getSessionManager();
        String expectedUserId = SessionManagerUtil.getUserCredentials( sm ).getUserId();
        assertEquals( expectedUserId, userId );
    }

    @Test
    public void testDisplayQuota()
            throws Exception
    {
        CQuota quota = storage.getQuota();
        LOGGER.info( "Retrieved quota for provider {}: {} ({}% used)",
                     storage.getProviderName(), quota, quota.getPercentUsed() );
    }

    @Test
    public void testQuotaChangedAfterUpload()
            throws Exception
    {
        // Quota is not updated in real time for some providers
        Assume.assumeThat( "quota not updated in real time for provider " + storage.getProviderName(),
                           storage.getProviderName(), not( equalTo( Hubic.PROVIDER_NAME ) ) );
        Assume.assumeThat( "quota not updated in real time for provider " + storage.getProviderName(),
                           storage.getProviderName(), not( equalTo( GoogleDrive.PROVIDER_NAME ) ) );

        CQuota quotaBefore = storage.getQuota();
        LOGGER.info( "Quota BEFORE upload ({} provider) : used={} / total={}",
                     storage.getProviderName(), quotaBefore.getBytesUsed(), quotaBefore.getBytesAllowed() );

        // Upload a quite big blob :
        int fileSize = 500000;
        CPath path = MiscUtils.generateTestPath( null );
        LOGGER.info( "Uploading blob with size {} bytes to {}", fileSize, path );
        byte[] content = MiscUtils.generateRandomByteArray( fileSize, null );
        CUploadRequest uploadRequest = new CUploadRequest( path, new MemoryByteSource( content ) );
        storage.upload( uploadRequest );

        // Check uploaded blob has correct length :
        CFile cblob = storage.getFile( path );
        assertThat( "uploaded file exists", cblob, CoreMatchers.instanceOf( CBlob.class ) );
        assertEquals( fileSize, ( ( CBlob ) cblob ).length() );

        LOGGER.info( "Checking quota has changed" );
        CQuota quotaAfter = storage.getQuota();
        storage.delete( path );
        LOGGER.info( "Quota AFTER upload ({} provider) : used={} / total={}",
                     storage.getProviderName(), quotaAfter.getBytesUsed(), quotaAfter.getBytesAllowed() );
        long usedDifference = quotaAfter.getBytesUsed() - quotaBefore.getBytesUsed();
        LOGGER.info( "used bytes difference = {} (upload file size was {})",
                     usedDifference, fileSize );
        assertEquals( fileSize, usedDifference );
    }

    @Test
    public void testFileOperations()
            throws Exception
    {
        // We'll use a temp folder for tests
        CPath tempRootPath = MiscUtils.generateTestPath( null );
        LOGGER.info( "Will use test folder : {}", tempRootPath );

        // Create a sub-folder :
        CPath subPath = tempRootPath.add( "sub_folder" );
        LOGGER.info( "Creating sub_folder : {}", subPath );
        assertTrue( storage.createFolder( subPath ) ); // True because actually created
        assertFalse( storage.createFolder( subPath ) ); // False because not created

        // Check back :
        CFile subFolder = storage.getFile( subPath );
        assertEquals( subPath, subFolder.getPath() );
        assertTrue( subFolder.isFolder() );
        assertFalse( subFolder.isBlob() );
        if ( subFolder.getModificationDate() != null ) { // Not all providers have a modif time on folders
            MiscUtils.assertDatetimeIsAlmostNow( subFolder.getModificationDate() );
        }

        // Upload 2 files into this sub-folder :
        CPath fpath1 = subPath.add( "a_test_file1" );
        byte[] contentFile1 = "This is binary cont€nt of test file 1...".getBytes( PcsUtils.UTF8 );
        LOGGER.info( "Uploading blob to : {}", fpath1 );
        CUploadRequest uploadRequest = new CUploadRequest( fpath1, new MemoryByteSource( contentFile1 ) );
        storage.upload( uploadRequest );

        CPath fpath2 = subPath.add( "a_test_file2" );
        // Generate a quite big random data :
        byte[] contentFile2 = MiscUtils.generateRandomByteArray( 500000, null );
        LOGGER.info( "Uploading blob to : {}", fpath2 );
        uploadRequest = new CUploadRequest( fpath2, new MemoryByteSource( contentFile2 ) );
        storage.upload( uploadRequest );

        // Check uploaded blobs informations :
        // we check file2 first because has just been uploaded / for modif time check
        CFile cblob = storage.getFile( fpath2 );
        assertTrue( cblob.isBlob() );
        assertFalse( cblob.isFolder() );
        assertEquals( contentFile2.length, ( ( CBlob ) cblob ).length() );
        MiscUtils.assertDatetimeIsAlmostNow( cblob.getModificationDate() );

        cblob = storage.getFile( fpath1 );
        assertTrue( cblob.isBlob() );
        assertFalse( cblob.isFolder() );
        assertEquals( contentFile1.length, ( ( CBlob ) cblob ).length() );

        // Download data, and check :
        LOGGER.info( "Downloading back and checking file : {}", fpath1 );
        MemoryByteSink mbs = new MemoryByteSink();
        CDownloadRequest downloadRequest = new CDownloadRequest( fpath1, mbs );
        storage.download( downloadRequest );
        assertArrayEquals( contentFile1, mbs.getData() );

        // Check also with different Ranges:
        LOGGER.info( "Downloading back and checking file ranges: {}", fpath1 );
        downloadRequest.setRange( 5, -1 ); // starting at offset 5
        storage.download( downloadRequest );
        assertArrayEquals( Arrays.copyOfRange( contentFile1, 5, contentFile1.length ),
                           mbs.getData() );
        downloadRequest.setRange( -1, 5 ); // last 5 bytes
        storage.download( downloadRequest );
        assertArrayEquals( Arrays.copyOfRange( contentFile1, contentFile1.length - 5, contentFile1.length ),
                           mbs.getData() );
        downloadRequest.setRange( 2, 5 ); // 5 bytes at offset 2
        storage.download( downloadRequest );
        assertArrayEquals( Arrays.copyOfRange( contentFile1, 2, 7 ),
                           mbs.getData() );

        LOGGER.info( "Downloading back and checking file : {}", fpath2 );
        downloadRequest = new CDownloadRequest( fpath2, mbs );
        storage.download( downloadRequest );
        assertArrayEquals( contentFile2, mbs.getData() );

        // Check that if we upload again, blob is replaced :
        LOGGER.info( "Checking file overwrite : {}", fpath2 );
        contentFile2 = MiscUtils.generateRandomByteArray( 300000, null );
        uploadRequest = new CUploadRequest( fpath2, new MemoryByteSource( contentFile2 ) );
        storage.upload( uploadRequest );
        storage.download( downloadRequest );
        assertArrayEquals( contentFile2, mbs.getData() );

        // Check that we can replace replace existing blob with empty content:
        if ( !storage.getProviderName().equals( GoogleDrive.PROVIDER_NAME ) ) {
            LOGGER.info( "Checking file overwrite with empty file: {}", fpath2 );
            contentFile2 = new byte[ 0 ];
            uploadRequest = new CUploadRequest( fpath2, new MemoryByteSource( contentFile2 ) );
            storage.upload( uploadRequest );
            storage.download( downloadRequest );
            assertArrayEquals( contentFile2, mbs.getData() );
        }

        // Create a sub_sub_folder :
        CPath subSubPath = subPath.add( "a_sub_sub_folder" );
        LOGGER.info( "Creating sub_sub folder : {}", subSubPath );
        storage.createFolder( subSubPath );

        LOGGER.info( "Check uploaded blobs and sub_sub_folder all appear in folder list" );
        CFolderContent folderContent = storage.listFolder( ( CFolder ) subFolder );
        LOGGER.info( "sub_folder contains files : {}", folderContent );
        // It happened once here that hubic did not list fpath1 'a_test_file1' in folder_content :
        // only 2 files were present ?!
        assertEquals( 3, folderContent.size() );
        assertTrue( folderContent.containsPath( fpath1 ) );
        assertTrue( folderContent.getFile( fpath1 ).isBlob() );
        assertFalse( folderContent.getFile( fpath1 ).isFolder() );
        assertTrue( folderContent.containsPath( fpath2 ) );
        assertTrue( folderContent.getFile( fpath2 ).isBlob() );
        assertFalse( folderContent.getFile( fpath2 ).isFolder() );
        assertTrue( folderContent.containsPath( subSubPath ) );
        assertFalse( folderContent.getFile( subSubPath ).isBlob() );
        assertTrue( folderContent.getFile( subSubPath ).isFolder() );

        LOGGER.info( "Check that list of sub_sub folder is empty : {}", subSubPath );
        assertTrue( storage.listFolder( subSubPath ).isEmpty() );

        LOGGER.info( "Check that listing content of a blob raises : {}", fpath1 );
        try {
            storage.listFolder( fpath1 );
            fail( "Listing a blob should raise" );
        } catch ( CInvalidFileTypeException ex ) {
            assertEquals( fpath1, ex.getPath() );
            assertFalse( ex.isBlobExpected() );
        }

        LOGGER.info( "Delete file1 : {}", fpath1 );
        assertTrue( storage.delete( fpath1 ) ); // We have deleted the file
        assertFalse( storage.delete( fpath1 ) ); // We have not deleted anything

        LOGGER.info( "Check file1 does not appear anymore in folder : {}", subFolder );
        assertFalse( storage.listFolder( ( CFolder ) subFolder ).containsPath( fpath1 ) );
        assertNull( storage.getFile( fpath1 ) );

        LOGGER.info( "Delete whole test folder : {}", tempRootPath );
        boolean ret = storage.delete( tempRootPath );
        assertTrue( ret ); // We have deleted at least one file
        LOGGER.info( "Deleting again returns False" );
        ret = storage.delete( tempRootPath );
        assertFalse( ret ); // We have not deleted anything

        LOGGER.info( "Listing a deleted folder returns None : {}", tempRootPath );
        assertNull( storage.listFolder( tempRootPath ) );
        assertNull( storage.getFile( tempRootPath ) );
    }

    @Test
    public void testBlobContentType()
            throws Exception
    {
        Assume.assumeThat( "Dropbox ignores content type",
                           storage.getProviderName(), not( equalTo( Dropbox.PROVIDER_NAME ) ) );
        Assume.assumeThat( "Google Drive ignores content type",
                           storage.getProviderName(), not( equalTo( GoogleDrive.PROVIDER_NAME ) ) );
        Assume.assumeThat( "CloudMe ignores content type",
                           storage.getProviderName(), not( equalTo( CloudMe.PROVIDER_NAME ) ) );

        CPath tempRootPath = MiscUtils.generateTestPath( null );
        try {
            LOGGER.info( "Will use test folder :{}", tempRootPath );

            // some providers are sensitive to filename suffix, so we do not specify any here :
            CPath path = tempRootPath.add( "uploaded_blob" );
            byte[] data = "some content...".getBytes( PcsUtils.UTF8 );
            String contentType = "text/plain; charset=Latin-1";
            CUploadRequest uploadRequest = new CUploadRequest( path, new MemoryByteSource( data ) );
            uploadRequest.setContentType( contentType );
            storage.upload( uploadRequest );

            CFile file = storage.getFile( path );
            assertEquals( contentType, ( ( CBlob ) file ).getContentType() );

            // Update file content, check content-type is updated also :
            data = "some binary content...".getBytes( PcsUtils.UTF8 );
            data[4] = 0x05;
            data[11] = -1; // 0xff
            contentType = "application/octet-stream";
            uploadRequest = new CUploadRequest( path, new MemoryByteSource( data ) );
            uploadRequest.setContentType( contentType );
            storage.upload( uploadRequest );

            file = storage.getFile( path );
            assertEquals( contentType, ( ( CBlob ) file ).getContentType() );
        } finally {
            deleteQuietly( tempRootPath, storage );
        }
    }

    @Test
    public void testBlobMetadata()
            throws Exception
    {
        Assume.assumeThat( "Dropbox ignores metadata",
                           storage.getProviderName(), not( equalTo( Dropbox.PROVIDER_NAME ) ) );
        Assume.assumeThat( "GoogleDrive ignores metadata",
                           storage.getProviderName(), not( equalTo( GoogleDrive.PROVIDER_NAME ) ) );
        Assume.assumeThat( "CloudMe ignores metadata",
                           storage.getProviderName(), not( equalTo( CloudMe.PROVIDER_NAME ) ) );

        CPath tempRootPath = MiscUtils.generateTestPath( null );
        try {
            LOGGER.info( "Will use test folder : {}", tempRootPath );
            CPath path = tempRootPath.add( "uploaded_blob.txt" );
            byte[] data = "some content...".getBytes( PcsUtils.UTF8 );
            CUploadRequest uploadRequest = new CUploadRequest( path, new MemoryByteSource( data ) );

            CMetadata metadata = new CMetadata();
            metadata.put( "foo", "this is foo..." );

            try {
                metadata.put( "bar", "1€ la bière" );
                fail( "Illegal character setted in metadata value" );
            } catch ( IllegalArgumentException ex ) {
                // Error expected
            }
            uploadRequest.setMetadata( metadata );
            storage.upload( uploadRequest );

            CFile file = storage.getFile( path );
            assertEquals( file.getMetadata(), metadata );

        } finally {
            deleteQuietly( tempRootPath, storage );
        }
    }

    @Test
    public void testDeleteSingleFolder()
            throws Exception
    {
        // We'll use a temp folder for tests :
        CPath tempRootPath = MiscUtils.generateTestPath( null );
        LOGGER.info( "Will use test folder : {}", tempRootPath );

        // Create two sub folders : a/ and ab/ :
        CPath fpatha = tempRootPath.add( "a" );
        CPath fpathab = tempRootPath.add( "ab" );
        storage.createFolder( fpatha );
        storage.createFolder( fpathab );
        assertTrue( storage.getFile( fpatha ).isFolder() );
        assertTrue( storage.getFile( fpathab ).isFolder() );

        CPath path = fpatha.add( "uploaded_blob.txt" );
        byte[] data = "some content...".getBytes( PcsUtils.UTF8 );
        CUploadRequest uploadRequest = new CUploadRequest( path, new MemoryByteSource( data ) );
        storage.upload( uploadRequest );

        // Now delete folder a : folder ab should not be deleted
        storage.delete( fpatha );
        assertNull( storage.getFile( fpatha ) );
        assertNull( storage.getFile( path ) );
        assertTrue( storage.getFile( fpathab ).isFolder() );

        storage.delete( tempRootPath );
    }

    @Test
    public void testInvalidFileOperation()
            throws Exception
    {
        // We'll use a temp folder for tests :
        CPath tempRootPath = MiscUtils.generateTestPath( null );
        LOGGER.info( "Will use test folder : {}", tempRootPath );

        // Upload 1 file into this folder :
        CPath fpath1 = tempRootPath.add( "a_test_file1" );
        byte[] contentFile1 = "This is binary cont€nt of test file 1...".getBytes( PcsUtils.UTF8 );
        CUploadRequest uploadRequest = new CUploadRequest( fpath1, new MemoryByteSource( contentFile1 ) );
        storage.upload( uploadRequest );
        LOGGER.info( "Created blob : {}", fpath1 );

        // Create a sub_folder :
        CPath subFolder = tempRootPath.add( "sub_folder" );
        storage.createFolder( subFolder );

        LOGGER.info( "Check that listing content of a blob raises : {}", fpath1 );
        try {
            storage.listFolder( fpath1 );
            fail( "Listing a blob should raise" );
        } catch ( CInvalidFileTypeException ex ) {
            assertEquals( fpath1, ex.getPath() );
            assertFalse( ex.isBlobExpected() );
        }

        LOGGER.info( "Check that trying to download a folder raises : {}", subFolder );
        MemoryByteSink mbs = new MemoryByteSink();
        CDownloadRequest downloadRequest = new CDownloadRequest( subFolder, mbs );
        try {
            storage.download( downloadRequest );
            fail( "Downloading a folder should raise" );
        } catch ( CInvalidFileTypeException ex ) {
            assertEquals( subFolder, ex.getPath() );
            assertTrue( ex.isBlobExpected() );
        }

        LOGGER.info( "Check that we cannot create a folder over a blob : {}", fpath1 );
        try {
            storage.createFolder( fpath1 );
            fail( "Creating a folder over a blob should raise" );
        } catch ( CInvalidFileTypeException ex ) {
            assertEquals( fpath1, ex.getPath() );
            assertFalse( ex.isBlobExpected() );
        }

        LOGGER.info( "Check we cannot upload over an existing folder : {}", subFolder );
        try {
            byte[] content = "some data...".getBytes( PcsUtils.UTF8 );
            uploadRequest = new CUploadRequest( subFolder, new MemoryByteSource( content ) );
            storage.upload( uploadRequest );
            fail( "Uploading over a folder should raise" );

        } catch ( CInvalidFileTypeException ex ) {
            assertEquals( subFolder, ex.getPath() );
            assertTrue( ex.isBlobExpected() );
        }

        LOGGER.info( "Check that content of a never existed folder is None" );
        CPath path = new CPath( "/hope i did never exist (even for tests) !" );
        assertNull( storage.listFolder( path ) );
        LOGGER.info( "Check that get_file() returns None is file does not exist" );
        assertNull( storage.getFile( path ) );

        LOGGER.info( "Check that downloading a non-existing file raises" );
        downloadRequest = new CDownloadRequest( path, new MemoryByteSink() );
        try {
            storage.download( downloadRequest );
            fail( "Downlad a non-existing blob should raise" );
        } catch ( CFileNotFoundException ex ) {
            LOGGER.debug( "Expected exception : {}", ex.toString() );
            assertEquals( path, ex.getPath() );
        }

        storage.delete( tempRootPath );
    }

    @Test
    public void testCreateFolderOverBlob()
            throws Exception
    {
        // We'll use a temp folder for tests :
        CPath tempRootPath = MiscUtils.generateTestPath( null );
        LOGGER.info( "Will use test folder : {}", tempRootPath );

        try {
            // Upload 1 file into this folder :
            CPath fpath1 = tempRootPath.add( "a_test_file1" );
            byte[] contentFile1 = "This is binary cont€nt of test file 1...".getBytes( PcsUtils.UTF8 );
            CUploadRequest uploadRequest = new CUploadRequest( fpath1, new MemoryByteSource( contentFile1 ) );
            storage.upload( uploadRequest );
            LOGGER.info( "Created blob : {}", fpath1 );

            try {
                CPath path = fpath1.add( "sub_folder1" );
                LOGGER.info( "Check we cannot create a folder when remote path traverses a blob : {}", path );
                storage.createFolder( path );

                // This is known to fail on CloudMe :
                Assume.assumeThat( "Creating folder when path contains a blob should raise: not supported by Dropbox",
                                   storage.getProviderName(), not( equalTo( Dropbox.PROVIDER_NAME ) ) );
                fail( "Creating folder when path contains a blob should raise" );

            } catch ( CInvalidFileTypeException ex ) {
                assertEquals( fpath1, ex.getPath() );
                assertFalse( ex.isBlobExpected() );
            }

        } finally {
            deleteQuietly( tempRootPath, storage );
        }
    }

    @Test
    public void testImplicitCreateFolderOverBlob()
            throws Exception
    {
        // We'll use a temp folder for tests :
        CPath tempRootPath = MiscUtils.generateTestPath( null );
        LOGGER.info( "Will use test folder : {}", tempRootPath );

        try {
            // Upload 1 file into this folder :
            CPath fpath1 = tempRootPath.add( "a_test_file1" );
            byte[] contentFile1 = "This is binary cont€nt of test file 1...".getBytes( PcsUtils.UTF8 );
            CUploadRequest uploadRequest = new CUploadRequest( fpath1, new MemoryByteSource( contentFile1 ) );
            storage.upload( uploadRequest );
            LOGGER.info( "Created blob : {}", fpath1 );

            // Uploading blob will implicitely create intermediary folders,
            // so will try to erase fpath1
            try {
                CPath path = fpath1.add( "sub_file1" );
                LOGGER.info( "Check we cannot upload a blob when remote path traverses a blob : {}", path );
                byte[] content = "some data...".getBytes( PcsUtils.UTF8 );
                uploadRequest = new CUploadRequest( path, new MemoryByteSource( content ) );
                storage.upload( uploadRequest );

                // This is known to fail on Dropbox :
                Assume.assumeThat( "Uploading when path contains a blob should raise : not supported by Dropbox",
                                   storage.getProviderName(), not( equalTo( Dropbox.PROVIDER_NAME ) ) );
                fail( "Uploading when path contains a blob should raise" );

            } catch ( CInvalidFileTypeException ex ) {
                assertEquals( fpath1, ex.getPath() ); // this is the problematic path
                assertFalse( ex.isBlobExpected() );
            }

        } finally {
            deleteQuietly( tempRootPath, storage );
        }
    }

    @Test
    public void testFileWithSpecialChars()
            throws Exception
    {
        CPath tempRootPath = MiscUtils.generateTestPath( null );

        try {
            CPath folderPath = tempRootPath.add( "hum...\u00a0',;.:\u00a0!*%&~#{[|`_ç^@ £€" );
            assertTrue( storage.createFolder( folderPath ) );
            CFile fback = storage.getFile( folderPath );
            assertEquals( folderPath, fback.getPath() );
            assertTrue( fback.isFolder() );
            assertFalse( fback.isBlob() );

            // Folder must appear in test root folder list :
            CFolderContent rootTestContent = storage.listFolder( tempRootPath );
            assertTrue( rootTestContent.containsPath( folderPath ) );
            assertEquals( folderPath, rootTestContent.getFile( folderPath ).getPath() );
            assertTrue( rootTestContent.getFile( folderPath ).isFolder() );
            assertFalse( rootTestContent.getFile( folderPath ).isBlob() );

            // Generate a random blob name (ensure it does not start nor end with a space)
            StringBuilder nameb = new StringBuilder( "b" );
            for ( int i = 0; i < 30; i++ ) {
                nameb.append( generateRandomBlobNameChar() );
            }
            nameb.append( "e" );
            for ( int nbBlobs = 0; nbBlobs < 10; nbBlobs++ ) {
                // slightly change blob name, so that we get similar but different names :
                int index = ( int ) ( Math.random() * ( nameb.length() - 2 ) ) + 1;
                nameb.setCharAt( index, generateRandomBlobNameChar() );
                String blobName = nameb.toString();

                CPath blobPath = folderPath.add( blobName );
                LOGGER.info( "Will upload file to path: {}", blobPath );

                String contentFileStr = "This is cont€nt of test file : '" + blobName + "'";
                byte[] contentFile = contentFileStr.getBytes( PcsUtils.UTF8 );
                CUploadRequest uploadRequest = new CUploadRequest( blobPath, new MemoryByteSource( contentFile ) );
                uploadRequest.setContentType( "text/plain ; charset=UTF-8" );
                storage.upload( uploadRequest );
                CFile bback = storage.getFile( blobPath );
                assertEquals( blobPath, bback.getPath() );
                assertTrue( bback.isBlob() );
                assertFalse( bback.isFolder() );

                // Download and check content :
                MemoryByteSink mbs = new MemoryByteSink();
                CDownloadRequest downloadRequest = new CDownloadRequest( blobPath, mbs );
                storage.download( downloadRequest );
                assertArrayEquals( contentFile, mbs.getData() );

                // new blob must appear in folder list :
                CFolderContent foderContent = storage.listFolder( folderPath );
                assertTrue( foderContent.containsPath( blobPath ) );
                assertEquals( blobPath, foderContent.getFile( blobPath ).getPath() );
                assertTrue( foderContent.getFile( blobPath ).isBlob() );
                assertFalse( foderContent.getFile( blobPath ).isFolder() );
            }
        } finally {
            deleteQuietly( tempRootPath, storage );
        }
    }

    private char generateRandomBlobNameChar()
    {
        while ( true ) {
            char c = ( char ) ( ( Math.random() * Math.random() * 200 ) + 32 );
            if ( Math.random() < 0.01 ) {
                c = '€'; // longer once UTF-8 encoded
            }

            if ( c >= 127 && c < 160 ) {
                continue;
            }
            if ( c == '/' || c == '\\' ) {
                continue;
            }
            // CloudMe provider does not support quotes so we do not use them :
            if ( c == '"' && CloudMe.PROVIDER_NAME.equals( storage.getProviderName() ) ) {
                continue;
            }
            return c;
        }
    }

    @Test
    public void testAbortDuringDownload()
            throws Exception
    {
        // If download fails, check that abort is called on progress listener
        // Upload a quite big file (for download) :
        int fileSize = 500000;
        CPath path = MiscUtils.generateTestPath( null );
        LOGGER.info( "Uploading blob with size {} bytes to {}", fileSize, path );

        try {
            LOGGER.info( "Will upload a blob for download test ({} bytes) to {}", fileSize, path );
            byte[] content = MiscUtils.generateRandomByteArray( fileSize, null );
            CUploadRequest uploadRequest = new CUploadRequest( path, new MemoryByteSource( content ) );
            storage.upload( uploadRequest );

            // Now we download, asking for progress :
            LOGGER.info( "Will download this blob but fail during download..." );
            File tempFile = new File( tmpDir, "back_from_provider" );
            FileByteSink fbs = new FileByteSink( tempFile, false, true, false );
            TestAbortProgressListener pl = new TestAbortProgressListener( 1, // fail once
                                                                          fileSize / 2, // abort at half download
                                                                          false ); // not retriable
            CDownloadRequest dr = new CDownloadRequest( path, fbs ).setProgressListener( pl );

            try {
                storage.download( dr );
                fail( "Download should have failed !" );
            } catch ( Exception ex ) {
                LOGGER.info( "Download has failed (as expected)" );
            }

            // Check listener saw the abort :
            assertTrue( pl.isAborted() );

            // check file does not exist (has been removed) :
            LOGGER.info( "Check destination file does not exist : {}", tempFile.getPath() );
            assertFalse( tempFile.exists() );

        } finally {
            deleteQuietly( path, storage );
        }
    }

    @Test
    public void testAbortTwiceDuringUpload()
            throws Exception
    {
        int fileSize = 500000;
        CPath path = MiscUtils.generateTestPath( null );
        LOGGER.info( "Uploading blob with size {} bytes to {}", fileSize, path );

        try {
            LOGGER.info( "Will upload a blob for test ({} bytes) to {}, but fail temporarily during first two uploads",
                         fileSize, path );
            byte[] content = MiscUtils.generateRandomByteArray( fileSize, null );
            ByteSource bs = new MemoryByteSource( content );
            ProgressListener pl = new TestAbortProgressListener( 2, // fail twice
                                                                 fileSize / 2, // fail at half upload
                                                                 true ); // and retry
            CUploadRequest uploadRequest = new CUploadRequest( path, bs ).setProgressListener( pl );
            storage.upload( uploadRequest );

            // check it has worked :
            CFile cfile = storage.getFile( path );
            assertThat( cfile, CoreMatchers.instanceOf( CBlob.class ) );
            assertEquals( fileSize, ( ( CBlob ) cfile ).length() );

        } finally {
            deleteQuietly( path, storage );
        }
    }

    private void deleteQuietly( CPath path, IStorageProvider storage )
    {
        if ( path != null ) {
            try {
                storage.delete( path );
            } catch ( Throwable th ) {
                LOGGER.warn( "Error deleting file " + path, th );
            }
        }
    }

    private static class TestAbortProgressListener
            extends StdoutProgressListener
    {

        private final int nbFailsTotal;
        private final int limit;
        private final boolean retriableException;
        private int nbFailsCurrent;

        /**
         * @param nbFails number of times an exception will be raised
         * @param offsetLimit at which offset in byte source the exception will be raised
         * @param retriableException if true, a CRetriableException is thrown so request will be retried
         */
        private TestAbortProgressListener( int nbFails, int offsetLimit, boolean retriableException )
        {
            this.nbFailsTotal = nbFails;
            this.limit = offsetLimit;
            this.retriableException = retriableException;
        }

        @Override
        public void progress( long current )
        {
            super.progress( current );
            if ( current >= limit ) {
                if ( nbFailsCurrent < nbFailsTotal ) {
                    nbFailsCurrent++;
                    RuntimeException ex = new RuntimeException( "Test error to make up/download fail : "
                                                                + nbFailsCurrent + "/" + nbFailsTotal );
                    if ( retriableException ) {
                        ex = new CRetriableException( ex );
                    }
                    throw ex;
                }
            }
        }

    }

}
