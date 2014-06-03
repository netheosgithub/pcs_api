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

import java.io.ByteArrayOutputStream;
import java.io.IOException;

/**
 * Implements the in-memory version of a byte sink stream
 */
public class MemoryByteSinkStream
        extends ByteSinkStream
{

    private final MemoryByteSink memoryByteSink;
    private boolean aborted;

    public MemoryByteSinkStream( MemoryByteSink memoryByteSink, ByteArrayOutputStream baos )
    {
        super( baos );
        this.memoryByteSink = memoryByteSink;
    }

    @Override
    public void close()
            throws IOException
    {
        memoryByteSink.setData( ( ( ByteArrayOutputStream ) out ).toByteArray() );
        super.close();
    }

    @Override
    public void abort()
    {
        aborted = true;
    }

    @Override
    public boolean isAborted()
    {
        return aborted;
    }

}
