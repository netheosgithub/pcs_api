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
 * A byte source that notifies a ProgressListener ; data is read from underlying ByteSource.
 *
 * This class is meant to be used only internally. Use CUploadRequest.setProgressListener() instead.
 */
public class ProgressByteSource
        implements ByteSource
{

    private ByteSource source;
    private ProgressListener listener;

    public ProgressByteSource( ByteSource source, ProgressListener listener )
    {
        if ( listener == null ) {
            throw new IllegalArgumentException( "ProgressListener cannot be null" );
        }

        this.source = source;
        this.listener = listener;
    }

    @Override
    public InputStream openStream()
            throws IOException
    {
        InputStream is = source.openStream();
        ProgressInputStream pis = new ProgressInputStream( is, listener );
        listener.setProgressTotal( length() );
        listener.progress( 0 );

        return pis;
    }

    @Override
    public long length()
    {
        return source.length();
    }

}
