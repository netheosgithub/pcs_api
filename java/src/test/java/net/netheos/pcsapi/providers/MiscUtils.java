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

import java.util.ArrayList;
import java.util.Collection;
import java.util.Date;
import java.util.List;
import java.util.Random;
import net.netheos.pcsapi.exceptions.CInvalidFileTypeException;
import net.netheos.pcsapi.models.CPath;
import net.netheos.pcsapi.storage.IStorageProvider;
import net.netheos.pcsapi.utils.PcsUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 *
 */
public class MiscUtils
{
    
    private static final Logger LOGGER = LoggerFactory.getLogger( MiscUtils.class );
    
    private static final long TIME_ALLOWED_DELTA_SEC = 120L;
    private static final String TEST_FOLDER_PREFIX = "/pcsapi_tmptest_";
    
    private MiscUtils()
    {
    }
    
    public static void assertDatetimeIsAlmostNow( Date date )
    {
        assertDatetimesAlmostEquals( new Date(), date );
    }
    
    public static void assertDatetimesAlmostEquals( Date expected, Date actual )
    {
        long delta = expected.getTime() - actual.getTime();
        long diff = Math.abs( delta / 1000 );
        if ( diff > TIME_ALLOWED_DELTA_SEC ) {
            throw new AssertionError( "Datetimes are very different : exp=" + expected + " actual=" + actual );
        }
    }

    /**
     * Cleanup any test folder that may still exist at the end of tests
     *
     * @param storage The storage to clean
     * @throws CInvalidFileTypeException Cleanup failure
     */
    public static void cleanupTestFolders( IStorageProvider storage )
            throws CInvalidFileTypeException
    {
        Collection<CPath> rootContent = storage.listRootFolder().getPaths();
        for ( CPath path : rootContent ) {
            if ( path.getPathName().startsWith( TEST_FOLDER_PREFIX ) ) {
                LOGGER.info( "Deleting old test folder: {}", path );
                storage.delete( path );
            }
        }
    }

    /**
     * Generate a temp folder for tests
     *
     * @param root returned path parent folder (may be null for designating root folder)
     * @return
     */
    public static CPath generateTestPath( CPath root )
    {
        String tempPathname = PcsUtils.randomString( 6 );
        CPath tempPath;
        if ( root == null || root.isRoot() ) {
            tempPath = new CPath( TEST_FOLDER_PREFIX + tempPathname );
        } else {
            tempPath = root.add( tempPathname );
        }
        return tempPath;
    }

    /**
     * Return a bytearray of random data with specified size. If seed is specified, it permits to generate pseudo-random
     * data
     *
     * @param size
     * @param rnd
     * @return
     */
    public static byte[] generateRandomByteArray( int size, Random rnd )
    {
        if ( rnd == null ) {
            rnd = new Random();
        }
        byte[] data = new byte[ size ];
        rnd.nextBytes( data );
        return data;
    }
    
    public static void testInThreads( int nbThreads, Runnable r )
            throws InterruptedException
    {
        Thread[] threads = new Thread[ nbThreads ];
        ThreadExceptionCounter[] counters = new ThreadExceptionCounter[ nbThreads ];
        for ( int i = 0; i < nbThreads; i++ ) {
            threads[i] = new Thread( r );
            counters[i] = new ThreadExceptionCounter();
            threads[i].setUncaughtExceptionHandler( counters[i] );
            threads[i].start();
        }

        // Now wait for threads end (handle Ctrl^C):
        for ( Thread thread : threads ) {
            thread.join();
        }
        
        LOGGER.info( "==== threaded test execution summary ====" );
        int totalThreadsInError = 0;
        for ( int i = 0; i < nbThreads; i++ ) {
            ThreadExceptionCounter counter = counters[i];
            if ( !counter.errors.isEmpty() ) {
                totalThreadsInError++;
                LOGGER.error( "Errors in thread {} ({}):", threads[i], r );
                for ( int k = 0; k < counter.errors.size(); k++ ) {
                    Throwable th = counter.errors.get( k );
                    long timestamp = counter.timestamps.get( k );
                    LOGGER.error( "{} : {}", new Date( timestamp ), th.toString() );
                }
            }
        }
        if ( totalThreadsInError == 0 ) {
            LOGGER.info( "No errors" );
        }
        LOGGER.info( "==== end threaded test execution ====" );
        if ( totalThreadsInError > 0 ) {
            throw new AssertionError( totalThreadsInError + " thread(s) have failed" );
        }
    }
    
    private static class ThreadExceptionCounter
            implements Thread.UncaughtExceptionHandler
    {
        
        private final List<Throwable> errors = new ArrayList<Throwable>();
        private final List<Long> timestamps = new ArrayList<Long>();
        
        @Override
        public void uncaughtException( Thread thread, Throwable th )
        {
            LOGGER.error( "Thread " + thread + " failed : " + th.getMessage(), th );
            errors.add( th );
            timestamps.add( System.currentTimeMillis() );
        }
        
    }
    
}
