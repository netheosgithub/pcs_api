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

/**
 * Byte sink stream that reports progress in stream operations
 */
public class ProgressByteSinkStream
        extends ByteSinkStream
{

    private final ProgressListener listener;
    private final ByteSinkStream byteSinkStream;
    private long currentBytes;

    public ProgressByteSinkStream( ByteSinkStream byteSinkStream, ProgressListener listener )
    {
        super( byteSinkStream );

        this.listener = listener;
        this.byteSinkStream = byteSinkStream;
    }

    /**
     * Count the read bytes
     *
     * @return The bytes count
     */
    public long getCurrentBytes()
    {
        return currentBytes;
    }

    @Override
    public void write( int b )
            throws IOException
    {
        out.write( b );
        addBytes( 1 );
    }

    @Override
    public void write( byte[] b )
            throws IOException
    {
        write( b, 0, b.length );
    }

    @Override
    public void write( byte[] b, int off, int len )
            throws IOException
    {
        out.write( b, off, len );
        addBytes( len );
    }

    private void addBytes( long n )
    {
        if ( n >= 0 ) {
            currentBytes += n;
            listener.progress( currentBytes );
        }
    }

    @Override
    public void abort()
    {
        listener.aborted();
        byteSinkStream.abort();
    }

    @Override
    public boolean isAborted()
    {
        return byteSinkStream.isAborted();
    }

}
