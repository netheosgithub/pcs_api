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

import java.util.Collection;
import java.util.Iterator;
import net.netheos.pcsapi.bytesio.MemoryByteSource;
import net.netheos.pcsapi.models.CFile;
import net.netheos.pcsapi.models.CFolder;
import net.netheos.pcsapi.models.CFolderContent;
import net.netheos.pcsapi.models.CPath;
import net.netheos.pcsapi.models.CUploadRequest;
import net.netheos.pcsapi.providers.MiscUtils;
import net.netheos.pcsapi.providers.StorageProviderFactory;
import net.netheos.pcsapi.storage.IStorageProvider;
import net.netheos.pcsapi.storage.StorageProvider;
import org.hamcrest.CoreMatchers;
import static org.junit.Assert.*;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;

/**
 *
 */
public class GoogleDriveTest
{

    private IStorageProvider storage;

    @Before
    public void setUp()
            throws Exception
    {
        Collection<Object[]> providers = StorageProviderFactory.storageProviderFactory();
        // Find google in this list:
        for (Iterator<Object[]> it=providers.iterator() ; it.hasNext() ; ) {
            Object[] prov = it.next();
            if ( prov[0] instanceof GoogleDrive) {
                storage = (StorageProvider)prov[0];
                break;
            }
        }
        Assume.assumeThat( "GoogleDrive provider found", storage, CoreMatchers.notNullValue() );
    }

    // Quick check google shared file access.
    @Test
    public void testSharedFiles()
            throws Exception
    {
        CPath tempRootPath = null;
        try {
            tempRootPath = MiscUtils.generateTestPath( null );
            // Search a shared folder named "a RW shared folder..." (must be present at the test start)
            CFolderContent content = storage.listRootFolder();
            CFolder sharedFolder = null;
            for ( CFile file : content ) {
                if ( file.getPath().getBaseName().equals( "a RW shared folder..." ) ) {
                    sharedFolder = ( CFolder ) file;
                }
            }

            // If file not present, we skip the test
            Assume.assumeTrue( "shared folder found", sharedFolder != null );

            CPath sharedFilePath = sharedFolder.getPath().add( "shared.jpg" );
            byte[] data = MiscUtils.generateRandomByteArray( 128000, null );
            storage.upload( new CUploadRequest( sharedFilePath, new MemoryByteSource( data ) ) );

            // check file exists
            CFile file = storage.getFile( sharedFilePath );
            assertNotNull( file );

            // Delete file
            assertTrue( storage.delete( sharedFilePath ) );
            file = storage.getFile( sharedFilePath );
            assertNull( file );

        } finally {
            if ( tempRootPath != null ) {
                storage.delete( tempRootPath );
            }
        }
    }

}
