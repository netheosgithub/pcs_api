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

package net.netheos.pcsapi.models;

import net.netheos.pcsapi.request.Headers;
import net.netheos.pcsapi.bytesio.ByteSink;
import net.netheos.pcsapi.bytesio.ProgressByteSink;
import net.netheos.pcsapi.bytesio.ProgressListener;

import org.apache.http.Header;
import org.apache.http.message.BasicHeader;

/**
 * Class that parametrizes a download request: path + bytes sink + (optional) byte range
 */
public class CDownloadRequest
{

    private final CPath path;
    private final ByteSink byteSink;
    private ByteRange byteRange;
    private ProgressListener progressListener;

    public CDownloadRequest( CPath path, ByteSink byteSink )
    {
        this.path = path;
        this.byteSink = byteSink;
        this.byteRange = null;
    }

    /**
     * Get the file path to download
     *
     * @return The file path
     */
    public CPath getPath()
    {
        return path;
    }

    /**
     * Get the HTTP headers to be used for download request.
     *
     * Default implementation only handles Range header.
     *
     * @return The headers
     */
    public Headers getHttpHeaders()
    {
        Headers headers = new Headers();

        if ( byteRange != null ) {
            StringBuilder rangeBuilder = new StringBuilder( "bytes=" );

            long start;
            if ( byteRange.offset >= 0 ) {
                rangeBuilder.append( byteRange.offset );
                start = byteRange.offset;
            } else {
                start = 1;
            }
            rangeBuilder.append( "-" );
            if ( byteRange.length > 0 ) {
                rangeBuilder.append( start + byteRange.length - 1 );
            }

            Header rangeHeader = new BasicHeader( "Range", rangeBuilder.toString() );
            headers.addHeader( rangeHeader );
        }
        return headers;
    }

    /**
     * Defines a range for partial content download. Note that second parameter is a length, not an offset (this differs
     * from http header Range header raw value).
     * <ul>
     * <li>(-1, -1) = default : download full blob.</li>
     * <li>(10, -1) = download, starting at offset 10</li>
     * <li>(-1, 10) = download last 10 bytes</li>
     * <li>(10, 100) = download 100 bytes, starting at offset 10.</li>
     * </ul>
     *
     * @param offset The start offset to download the file, or &lt;0 for downloading only end of file
     * @param length (never 0) The data length to download, or &lt;0 for downloading up to end of file
     * @return This download request
     */
    public CDownloadRequest setRange( long offset, long length )
    {
        if ( length == 0 ) {
            // Indicate we want to download 0 bytes ?! We ignore such specification
            length = -1;
        }
        if ( offset < 0 && length < 0 ) {
            byteRange = null;
        } else {
            byteRange = new ByteRange( offset, length );
        }
        return this;
    }

    /**
     * Defines an object that will be notified during download.
     *
     * @param pl the progress listener
     * @return this download request
     */
    public CDownloadRequest setProgressListener( ProgressListener pl )
    {
        this.progressListener = pl;
        return this;
    }

    /**
     * If no progress listener has been set, return the byte sink defined in constructor, otherwise decorate it.
     *
     * @return the byte sink to be used for download operations.
     */
    public ByteSink getByteSink()
    {
        ByteSink bs = byteSink;
        if ( progressListener != null ) {
            bs = new ProgressByteSink( bs, progressListener );
        }
        return bs;
    }

    private static class ByteRange
    {

        private final long offset;
        private final long length;

        public ByteRange( long offset, long length )
        {
            this.offset = offset;
            this.length = length;
        }

    }

}
