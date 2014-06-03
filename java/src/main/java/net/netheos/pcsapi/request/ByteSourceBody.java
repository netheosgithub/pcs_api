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

package net.netheos.pcsapi.request;

import net.netheos.pcsapi.bytesio.ByteSource;
import net.netheos.pcsapi.utils.PcsUtils;

import org.apache.http.entity.mime.MIME;
import org.apache.http.entity.mime.content.AbstractContentBody;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Adapter class to represent a byte source as an http entity (for uploads).
 */
public class ByteSourceBody
        extends AbstractContentBody
{

    private final ByteSource byteSource;
    private InputStream currentStream;
    private final String filename;

    public ByteSourceBody( ByteSource bs, String filename, String contentType )
    {
        super( contentType != null ? contentType : "application/octet-stream" );
        this.byteSource = bs;
        this.filename = filename;
    }

    public InputStream getContent()
            throws IOException, IllegalStateException
    {
        if ( currentStream != null ) { // should never happen
            // defensive code : we close any opened stream
            PcsUtils.closeQuietly( currentStream );
        }
        currentStream = byteSource.openStream();
        return currentStream;
    }

    @Override
    public void writeTo( OutputStream outstream )
            throws IOException
    {
        if ( outstream == null ) {
            throw new IllegalArgumentException( "Output stream may not be null" );
        }

        InputStream instream = getContent();
        try {
            byte[] buffer = new byte[ 4096 ];
            int n;
            while ( ( n = instream.read( buffer ) ) != -1 ) {
                outstream.write( buffer, 0, n );
            }
        } finally {
            PcsUtils.closeQuietly( instream );
            consumeContent();
        }
    }

    public void consumeContent()
            throws IOException
    {
        if ( currentStream != null ) {
            PcsUtils.closeQuietly( currentStream );
            currentStream = null;
        }
    }

    @Override
    public String getCharset()
    {
        return null;
    }

    @Override
    public String getTransferEncoding()
    {
        return MIME.ENC_BINARY;
    }

    @Override
    public String getFilename()
    {
        return filename;
    }

    @Override
    public long getContentLength()
    {
        return byteSource.length();
    }

}
