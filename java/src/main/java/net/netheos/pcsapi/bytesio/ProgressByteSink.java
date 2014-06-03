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
 * Byte Sink used to monitor progress in stream operations.
 *
 * This class is meant to be used only internally. Use CDownloadRequest.setProgressListener() instead.
 */
public class ProgressByteSink
        implements ByteSink
{

    private final ProgressListener listener;
    private final ByteSink byteSink;

    public ProgressByteSink( ByteSink byteSink, ProgressListener listener )
    {
        this.byteSink = byteSink;
        this.listener = listener;
    }

    @Override
    public ByteSinkStream openStream()
            throws IOException
    {
        ByteSinkStream bss = new ProgressByteSinkStream( byteSink.openStream(), listener );

        return bss;
    }

    @Override
    public void setExpectedLength( long expectedLength )
    {
        listener.setProgressTotal( expectedLength );
        byteSink.setExpectedLength( expectedLength );
    }

}
