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
 * A stream that reads data from an underlying stream, but stops reading after a limit has been reached
 */
class LimitedInputStream
        extends FilterInputStream
{

    private long remaining;

    LimitedInputStream( InputStream stream, long limit )
    {
        super( stream );

        this.remaining = limit;
    }

    @Override
    public int read()
            throws IOException
    {
        if ( remaining <= 0 ) {
            return -1;
        }

        int byteRead = super.read();

        if ( byteRead != -1 ) {
            remaining--;
        }

        return byteRead;
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
        int nbBytesToRead = Math.min( len, ( int ) remaining );

        int nbBytesRead = super.read( b, off, nbBytesToRead );
        remaining -= nbBytesRead;

        return nbBytesRead;
    }

    @Override
    public long skip( long n )
            throws IOException
    {
        throw new UnsupportedOperationException();
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
