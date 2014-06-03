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

/**
 * Implementation of ByteSource in memory
 */
public class MemoryByteSink
        implements ByteSink
{

    private byte[] data;

    public MemoryByteSink()
    {
        this.data = null;
    }

    @Override
    public ByteSinkStream openStream()
    {
        ByteSinkStream bss = new MemoryByteSinkStream( this, new ByteArrayOutputStream() );
        return bss;
    }

    @Override
    public void setExpectedLength( long expectedLength )
    {
        // NOOP
    }

    void setData( byte[] data )
    {
        this.data = data;
    }

    /**
     * Get the data stored in the sink
     *
     * @return The stored data
     */
    public byte[] getData()
    {
        return data;
    }

}
