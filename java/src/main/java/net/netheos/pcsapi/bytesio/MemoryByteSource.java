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

import java.io.ByteArrayInputStream;
import java.io.InputStream;

/**
 * Implementation of an in-memory ByteSource
 */
public class MemoryByteSource
        implements ByteSource
{

    private final byte[] data;

    public MemoryByteSource( byte[] data )
    {
        this.data = data;
    }

    @Override
    public InputStream openStream()
    {
        InputStream is = new ByteArrayInputStream( data );

        return is;
    }

    @Override
    public long length()
    {
        return data.length;
    }

    @Override
    public String toString()
    {
        return "MemoryByteSource {length='" + length() + "'}";
    }

}
