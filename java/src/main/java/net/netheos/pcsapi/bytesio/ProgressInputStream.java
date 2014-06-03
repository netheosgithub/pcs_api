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

import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * Byte sink stream that reports progress in stream operations
 */
class ProgressInputStream
        extends FilterInputStream
{

    private ProgressListener listener;
    private long currentBytes;

    ProgressInputStream( InputStream stream, ProgressListener listener )
    {
        this( stream, listener, 0 );
    }

    ProgressInputStream( InputStream stream, ProgressListener listener, long currentBytes )
    {
        super( stream );

        this.listener = listener;
        this.currentBytes = currentBytes;
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

    private void addBytes( long n )
    {
        if ( n >= 0 ) {
            currentBytes += n;
            listener.progress( currentBytes );
        }
    }

    @Override
    public int read()
            throws IOException
    {
        int ret = in.read();
        if (ret >= 0) {
            addBytes( 1 );
        }
        return ret;
    }

    @Override
    public int read( byte[] b )
            throws IOException
    {
        return read( b, 0, b.length );
    }

    @Override
    public int read( byte[] b, int off, int len )
            throws IOException
    {
        int nbBytesRead = in.read( b, off, len );
        addBytes( nbBytesRead );

        return nbBytesRead;
    }

    @Override
    public long skip( long n )
            throws IOException
    {
        long nbBytesSkipped = super.skip( n );
        addBytes( nbBytesSkipped );

        return nbBytesSkipped;
    }

    @Override
    public void mark( int readlimit )
    {
        throw new UnsupportedOperationException();
    }

    @Override
    public void reset()
            throws IOException
    {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean markSupported()
    {
        return false;
    }

}
