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

package net.netheos.pcsapi;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.List;

import net.netheos.pcsapi.bytesio.*;
import net.netheos.pcsapi.utils.PcsUtils;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.junit.After;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import static org.junit.Assert.*;
import org.junit.Before;
import org.junit.Test;

/**
 *
 */
public class BytesIOTest
{

    private static final Logger LOGGER = LoggerFactory.getLogger( BytesIOTest.class );
    private File tmpDir;

    @Before
    public void setUp()
            throws Exception
    {
        tmpDir = File.createTempFile( "pcs_api", ".dir" );
        tmpDir.delete();
        tmpDir.mkdirs();
    }

    @After
    public void tearDown()
    {
        FileUtils.deleteQuietly( tmpDir );
    }

    @Test
    public void testFileByteSource()
            throws Exception
    {
        String strContent = "This 1€ file is the test content of a file byte source... (70 bytes)";
        byte[] byteContent = strContent.getBytes( PcsUtils.UTF8 );

        File tmpFile = new File( tmpDir, "byte_source.txt" );
        FileUtils.writeByteArrayToFile( tmpFile, byteContent );

        ByteSource bs = new FileByteSource( tmpFile );
        checkByteSource( bs, byteContent );

        bs = new MemoryByteSource( byteContent );
        checkByteSource( bs, byteContent );
    }

    private void checkByteSource( ByteSource bs, byte[] expectedContent )
            throws IOException
    {
        assertEquals( 70, bs.length() );

        InputStream is = bs.openStream();
        byte[] b = IOUtils.toByteArray( is );
        assertTrue( Arrays.equals( expectedContent, b ) );
        is.close();

        // Create a range view of this byte source : 25 bytes starting at offset 5 :
        RangeByteSource rbs = new RangeByteSource( bs, 5, 25 );
        assertEquals( 25, rbs.length() );
        is = rbs.openStream();

        try {
            b = new byte[ 1 ];
            is.read( b );
            assertEquals( "1", new String( b, PcsUtils.UTF8 ) );

            b = new byte[ 3 ];
            is.read( b );
            assertEquals( "€", new String( b, PcsUtils.UTF8 ) );

            b = new byte[ 100 ];
            int len = is.read( b );
            assertEquals( 21, len );
            final String actual = new String( b, 0, 21, PcsUtils.UTF8 );
            assertEquals( " file is the test con", actual );

            b = new byte[ 100 ];
            len = is.read( b );
            assertEquals( 0, len );

        } finally {
            IOUtils.closeQuietly( is );
        }

        // Now decorate again with a progress byte source
        StdoutProgressListener pl = new StdoutProgressListener();
        ProgressByteSource pbs = new ProgressByteSource( rbs, pl );
        assertEquals( rbs.length(), pbs.length() );
        is = pbs.openStream();

        try {
            assertEquals( pbs.length(), pl.getTotal() );
            assertEquals( 0, pl.getCurrent() );
            assertFalse( pl.isAborted() );

            b = new byte[ 1 ];
            is.read( b );
            assertEquals( 1, pl.getCurrent() );

            b = new byte[ 10 ];
            is.read( b );
            assertEquals( 11, pl.getCurrent() );

            b = new byte[ 500 ];
            int len = is.read( b );
            assertEquals( 14, len );
            assertEquals( 25, pl.getCurrent() );

        } finally {
            IOUtils.closeQuietly( is );
        }
    }

    @Test
    public void testFileByteSink()
            throws Exception
    {
        String strContent = "This 1€ file is the test content of a file byte source... (70 bytes)";
        byte[] byteContent = strContent.getBytes( PcsUtils.UTF8 );

        File tmpFile = new File( tmpDir, "byte_sink.txt" );
        FileUtils.writeByteArrayToFile( tmpFile, byteContent );

        checkFileByteSinkAllFlags( tmpFile, byteContent );
        checkMemoryByteSink( byteContent );
    }

    private void checkMemoryByteSink( byte[] byteContent )
            throws IOException
    {
        MemoryByteSink sink = new MemoryByteSink();
        ByteSinkStream stream = sink.openStream();
        try {
            stream.write( byteContent );
        } finally {
            IOUtils.closeQuietly( stream );
        }

        byte[] storedData = sink.getData();
        assertTrue( Arrays.equals( storedData, byteContent ) );
    }

    private void checkFileByteSinkAllFlags( File file, byte[] byteContent )
            throws IOException
    {
        List<Boolean> booleans = Arrays.asList( true, false );

        for ( boolean abort : booleans ) {
            for ( boolean tempNameDuringWrites : booleans ) {
                for ( boolean deleteOnAbort : booleans ) {
                    LOGGER.debug( "checkFileByteSinkAllFlags - abort: {} - tempName : {} - deleteOnAbort"
                                  + " : {} ", abort, tempNameDuringWrites, deleteOnAbort );
                    ByteSink fbs = new FileByteSink( file, tempNameDuringWrites, deleteOnAbort, false );
                    checkFileByteSink( byteContent, abort, fbs, file, tempNameDuringWrites, deleteOnAbort );
                }
            }
        }
    }

    private void checkFileByteSink( byte[] dataToWrite, boolean abort, ByteSink fbs, File file, boolean withTempName,
                                    boolean deleteOnAbort )
            throws IOException
    {
        File actualFile = file;
        if ( withTempName ) {
            actualFile = new File( file.getPath() + ".part" );
        }
        ByteSinkStream stream = fbs.openStream();
        try {
            // check file exists :
            assertTrue( actualFile.exists() );
            fbs.setExpectedLength( dataToWrite.length );

            // write not all bytes, only beginning of data :
            stream.write( Arrays.copyOfRange( dataToWrite, 0, 10 ) );
            stream.flush();
            assertEquals( 10, actualFile.length() );

            if ( abort ) {
                LOGGER.info( "Aborting stream of byte sink !" );
                stream.abort();
            }

        } finally {
            IOUtils.closeQuietly( stream );
        }

        boolean fileStillExists = file.exists();
        boolean tempFileStillExists = actualFile.exists();
        if ( !abort ) {
            // operation has not been aborted : so file never deleted
            assertTrue( fileStillExists );
        } else {
            // operation has been aborted :
            // if a temp file is used and not deleted, it has not been renamed :
            assertTrue( tempFileStillExists != deleteOnAbort );
        }
    }

    @Test
    public void testProgressByteSink()
            throws Exception
    {
        byte[] byteContent = "hello world !".getBytes( PcsUtils.UTF8 );
        File testFile = new File( tmpDir, "byte_sink_progress.txt" );

        ByteSink fbs = new FileByteSink( testFile );
        checkProgressByteSink( fbs, byteContent );

        ByteSink mbs = new MemoryByteSink();
        checkProgressByteSink( mbs, byteContent );
    }

    private void checkProgressByteSink( ByteSink sink, byte[] byteContent )
            throws IOException
    {
        StdoutProgressListener pl = new StdoutProgressListener();
        ProgressByteSink pbs = new ProgressByteSink( sink, pl );

        ByteSinkStream stream = pbs.openStream();
        try {
            assertEquals( -1, pl.getTotal() );
            assertEquals( 0, pl.getCurrent() );
            assertFalse( pl.isAborted() );

            pbs.setExpectedLength( byteContent.length );
            assertEquals( byteContent.length, pl.getTotal() );

            stream.write( Arrays.copyOfRange( byteContent, 0, 1 ) );

            assertEquals( 1, pl.getCurrent() );

            byte[] remaining = Arrays.copyOfRange( byteContent, 1, byteContent.length );

            stream.write( remaining );
            assertEquals( pl.getTotal(), pl.getCurrent() );

            stream.abort();
            assertTrue( stream.isAborted() );

        } finally {
            IOUtils.closeQuietly( stream );
        }
    }

}
