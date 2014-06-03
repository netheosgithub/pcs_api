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

import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import net.netheos.pcsapi.bytesio.ByteSink;
import net.netheos.pcsapi.bytesio.ByteSource;
import net.netheos.pcsapi.bytesio.MemoryByteSink;
import net.netheos.pcsapi.bytesio.MemoryByteSource;
import net.netheos.pcsapi.bytesio.ProgressByteSink;
import net.netheos.pcsapi.bytesio.ProgressByteSource;
import net.netheos.pcsapi.bytesio.StdoutProgressListener;
import net.netheos.pcsapi.models.CDownloadRequest;
import net.netheos.pcsapi.models.CPath;
import net.netheos.pcsapi.models.CUploadRequest;
import net.netheos.pcsapi.utils.PcsUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import static org.junit.Assert.*;
import org.junit.Test;
import static org.hamcrest.CoreMatchers.*;

/**
 *
 */
public class ModelsTest
{

    private static final Logger LOGGER = LoggerFactory.getLogger( ModelsTest.class );

    @Test
    public void testCPath()
    {
        CPath path = new CPath( "/foo//bar€/" );
        assertEquals( "/foo/bar€", path.getPathName() );
        assertEquals( "/foo/bar%E2%82%AC", path.getUrlEncoded() );
        assertEquals( "bar€", path.getBaseName() );
        assertEquals( new CPath( "/foo" ), path.getParent() );
        assertEquals( path.add( "a,file..." ), new CPath( "/foo/bar€/a,file..." ) );
        assertEquals( path.add( "/a,file..." ), new CPath( "/foo/bar€/a,file..." ) );
        assertEquals( path.add( "a,file.../" ), new CPath( "/foo/bar€/a,file..." ) );
        assertEquals( path.add( "/several//folders/he re/" ), new CPath( "/foo/bar€/several/folders/he re" ) );
        assertFalse( path.isRoot() );
        assertFalse( path.getParent().isRoot() );

        CPath root = path.getParent().getParent();
        assertTrue( root.isRoot() );
        assertTrue( root.getParent().isRoot() );
        assertEquals( root, new CPath( "/" ) );
        assertEquals( root, new CPath( "" ) );
        assertEquals( root.getBaseName(), "" );

        assertEquals( Collections.EMPTY_LIST, root.split() );
        assertEquals( Collections.EMPTY_LIST, new CPath( "" ).split() );
        assertEquals( Collections.singletonList( "a" ), new CPath( "/a" ).split() );
        List<String> expected = new LinkedList<String>();
        expected.add( "alpha" );
        expected.add( "\"beta" );
        assertEquals( expected, new CPath( "/alpha/\"beta" ).split() );
    }

    @Test
    public void testCPathAsKey()
    {
        Map<CPath, String> adict = new HashMap<CPath, String>();
        adict.put( new CPath( "/a" ), "file_a" );
        adict.put( new CPath( "/a/b" ), "file_b" );

        assertTrue( adict.containsKey( new CPath( "/a" ) ) );
        assertEquals( "file_a", adict.get( new CPath( "/a" ) ) );

        assertTrue( adict.containsKey( new CPath( "/a/b" ) ) );
        assertEquals( "file_b", adict.get( new CPath( "/a/b" ) ) );

        assertFalse( adict.containsKey( new CPath( "/b" ) ) );
    }

    @Test
    public void testInvalidCPath()
    {
        List<String> pathnames = Arrays.asList( "\\no anti-slash is allowed",
                                                "This is an inv\\u001Flid pathname !",
                                                "This is an \t invalid pathname !",
                                                "This/ is/an invalid pathname !",
                                                "This/is /also an invalid pathname !",
                                                // "\u00a0bad", "bad\u00a0",
                                                " bad", "bad " );
        for ( String pathname : pathnames ) {
            try {
                LOGGER.info( "Checking CPath is invalid : {}", pathname );
                new CPath( pathname );
                fail( "CPath creation should have failed for pathname='" + pathname + "'" );
            } catch ( IllegalArgumentException ex ) {
                LOGGER.info( "CPath validation failed as expected: {}", ex.getMessage() );
            }
        }
    }

    @Test
    public void testCPathUrlEncoded()
    {
        assertEquals( "/a%20%2B%25b/c", new CPath( "/a +%b/c" ).getUrlEncoded() );
        assertEquals( "/a%3Ab", new CPath( "/a:b" ).getUrlEncoded() );
        assertEquals( "/%E2%82%AC", new CPath( "/€" ).getUrlEncoded() );
    }

    @Test
    public void testDownloadRequestBytesRange()
    {
        CDownloadRequest dr = new CDownloadRequest( new CPath( "/foo" ), new MemoryByteSink() );

        // By default we have no range header:
        assertFalse( dr.getHttpHeaders().contains( "Range" ) );

        dr.setRange( -1, 100 );
        assertTrue( dr.getHttpHeaders().contains( "Range" ) );
        assertEquals( "bytes=-100", dr.getHttpHeaders().getHeaderValue( "Range" ) );

        dr.setRange( 10, 100 );
        assertEquals( "bytes=10-109", dr.getHttpHeaders().getHeaderValue( "Range" ) );

        dr.setRange( 0, -1 );
        assertEquals( "bytes=0-", dr.getHttpHeaders().getHeaderValue( "Range" ) );

        dr.setRange( 100, -1 );
        assertEquals( "bytes=100-", dr.getHttpHeaders().getHeaderValue( "Range" ) );

        dr.setRange( -1, -1 );
        assertFalse( dr.getHttpHeaders().contains( "Range" ) );
    }

    @Test
    public void testDownloadRequestProgressListener()
            throws Exception
    {
        MemoryByteSink mbs = new MemoryByteSink();
        CDownloadRequest dr = new CDownloadRequest( new CPath( "/foo" ), mbs );
        // Without progress listener we get the same byte sink :
        assertSame( mbs, dr.getByteSink() );

        // Now if we decorate :
        StdoutProgressListener pl = new StdoutProgressListener();
        dr.setProgressListener( pl );
        ByteSink bs = dr.getByteSink();
        assertThat( bs, instanceOf( ProgressByteSink.class ) );

        // Check that listener is notified :
        OutputStream os = bs.openStream();
        try {
            os.write( 65 );
            os.flush();
            assertEquals( 1, pl.getCurrent() );
        } finally {
            PcsUtils.closeQuietly( os );
        }
    }

    @Test
    public void testUploadRequestProgressListener()
            throws Exception
    {
        byte[] data = "content".getBytes( "UTF-8" );
        MemoryByteSource mbs = new MemoryByteSource( data );
        CUploadRequest ur = new CUploadRequest( new CPath( "/foo" ), mbs );
        // Without progress listener we get the same byte source :
        assertSame( mbs, ur.getByteSource() );

        // Now if we decorate :
        StdoutProgressListener pl = new StdoutProgressListener();
        ur.setProgressListener( pl );
        ByteSource bs = ur.getByteSource();
        assertThat( bs, instanceOf( ProgressByteSource.class ) );

        // Check that listener is notified :
        InputStream is = bs.openStream();
        try {
            is.read();
            assertEquals( 1, pl.getCurrent() );
        } finally {
            PcsUtils.closeQuietly( is );
        }
    }

}
