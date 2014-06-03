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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.Random;
import net.netheos.pcsapi.bytesio.MemoryByteSink;
import net.netheos.pcsapi.bytesio.MemoryByteSource;
import net.netheos.pcsapi.models.CBlob;
import net.netheos.pcsapi.models.CDownloadRequest;
import net.netheos.pcsapi.models.CFile;
import net.netheos.pcsapi.models.CFolderContent;
import net.netheos.pcsapi.models.CPath;
import net.netheos.pcsapi.models.CUploadRequest;
import net.netheos.pcsapi.storage.IStorageProvider;
import org.junit.After;
import static org.junit.Assert.*;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 *
 */
@RunWith( Parameterized.class )
public class StressTest
{

    private static final Logger LOGGER = LoggerFactory.getLogger( StressTest.class );

    private static final long DURATION_ms = 1000 * 60 * 1; // 1 min
    private static final int NB_THREADS = 4;

    @Parameterized.Parameters
    public static Collection<Object[]> getTestsParameters()
            throws Exception
    {
        return StorageProviderFactory.storageProviderFactory();
    }

    private final IStorageProvider storage;

    public StressTest( IStorageProvider storage )
    {
        this.storage = storage;
    }

    @Before
    public void setUp()
            throws Exception
    {
        // for junit3, in case
    }

    @After
    public void tearDown()
    {
        // for junit3, in case
    }

    @Test
    public void testCrud()
    {
        new CrudRunnable( storage ).run();
    }

    /**
     * Multi-threaded test of test_crud().
     *
     * @throws InterruptedException
     */
    @Test
    public void testMultiCrud()
            throws InterruptedException
    {
        MiscUtils.testInThreads( NB_THREADS, new CrudRunnable( storage ) );
    }

    private static class CrudRunnable
            implements Runnable
    {

        private final IStorageProvider storage;

        private CrudRunnable( IStorageProvider storage )
        {
            this.storage = storage;
        }

        @Override
        public void run()
        {
            long start = System.currentTimeMillis();
            LOGGER.info( "Test starting time={}", start );
            long now = start;
            // if start > now then time has gone back :
            // we give up instead of being trapped in a potentially very long loop
            while ( start <= now && ( now - start ) < DURATION_ms ) {
                LOGGER.info( "============= Thread {} : (elapsed={} < {} ms) ================",
                             Thread.currentThread().getName(), now - start, DURATION_ms );
                CPath testRootPath = MiscUtils.generateTestPath( null );
                storage.getUserId(); // only to request hubic API
                uploadAndCheckRandomFiles( storage, testRootPath );
                now = System.currentTimeMillis();
            }
        }

        private void uploadAndCheckRandomFiles( IStorageProvider storage, CPath rootPath )
        {
            try {
                Random rnd = new Random(); // not shared (in case multi-thread)
                CPath tmpPath = new CPath( rootPath.getPathName() );

                for ( int i = 0; i < rnd.nextInt( 4 ) + 1; i++ ) {
                    CPath path = MiscUtils.generateTestPath( tmpPath );

                    if ( rnd.nextInt( 2 ) == 0 ) {
                        // Create folder :
                        storage.createFolder( path );
                        // And sometimes go inside :
                        if ( rnd.nextInt( 2 ) == 0 ) {
                            tmpPath = path;
                        }

                    } else {
                        // Create file (deterministic content for later check):
                        rnd.setSeed( path.hashCode() );
                        int fileSize = rnd.nextInt( 1000 ) * rnd.nextInt( 1000 ); // prefer small files
                        byte[] data = MiscUtils.generateRandomByteArray( fileSize, rnd );
                        CUploadRequest ur = new CUploadRequest( path, new MemoryByteSource( data ) );
                        storage.upload( ur );
                    }
                }

                // Check blob files content :
                List<CBlob> allBlobs = recursivelyListBlobs( storage, rootPath );
                LOGGER.info( "All uploaded blobs = {}", allBlobs );
                for ( CBlob blob : allBlobs ) {
                    rnd.setSeed( blob.getPath().hashCode() );
                    int fileSize = rnd.nextInt( 1000 ) * rnd.nextInt( 1000 ); // same formula as above
                    assertEquals( blob.length(), fileSize );
                    byte[] expectedData = MiscUtils.generateRandomByteArray( fileSize, rnd );
                    MemoryByteSink bsink = new MemoryByteSink();
                    CDownloadRequest dr = new CDownloadRequest( blob.getPath(), bsink );
                    storage.download( dr );
                    assertTrue( Arrays.equals( expectedData, bsink.getData() ) );
                }

            } finally {
                // Delete all files
                storage.delete( rootPath );
            }
        }

        private List<CBlob> recursivelyListBlobs( IStorageProvider storage, CPath path )
        {
            List<CBlob> retList = new ArrayList<CBlob>();
            CFolderContent files = storage.listFolder( path );
            for ( CFile file : files ) {
                if ( file.isBlob() ) {
                    retList.add( ( CBlob ) file );
                } else {
                    retList.addAll( recursivelyListBlobs( storage, file.getPath() ) );
                }
            }
            return retList;
        }

        @Override
        public String toString()
        {
            return "StressTest runnable for " + storage.getProviderName();
        }

    }

}
