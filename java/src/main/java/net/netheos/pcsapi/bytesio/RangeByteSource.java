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

package net.netheos.pcsapi.bytesio;

import java.io.IOException;
import java.io.InputStream;

/**
 * A byte source view of a range of bytes from an underlying byte source.
 *
 * TODO : only 1 RangeByteSource is acceptable in pipeline, because of seek()
 */
public class RangeByteSource
        implements ByteSource
{

    private ByteSource source;
    private long start;
    private long length;

    public RangeByteSource( ByteSource source )
    {
        this( source, 0 );
    }

    public RangeByteSource( ByteSource source, long startOffset )
    {
        this( source, startOffset, source.length() - startOffset );
    }

    public RangeByteSource( ByteSource source, long startOffset, long length )
    {
        long sourceLength = source.length();

        if ( startOffset >= sourceLength ) {
            throw new IllegalArgumentException( String.format( "startOffset (%d) should be smaller than source.length (%d)", startOffset, sourceLength ) );

        } else if ( length < 0 ) {
            throw new IllegalArgumentException( String.format( "length (%d) should be bigger than 0", length ) );

        } else if ( startOffset + length > sourceLength ) {
            throw new IllegalArgumentException( String.format( "startOffset (%d) + length (%d) should be smaller than source.length (%d)", startOffset, length, sourceLength ) );
        }

        this.source = source;
        this.start = startOffset;
        this.length = length;
    }

    @Override
    public InputStream openStream()
            throws IOException
    {
        InputStream is = source.openStream();
        is.skip( start );

        LimitedInputStream lis = new LimitedInputStream( is, length );

        return lis;
    }

    @Override
    public long length()
    {
        return length;
    }

    @Override
    public String toString()
    {
        return "RangeByteSource {source=" + source + ", start=" + start + ", length=" + length + "}";
    }

}
