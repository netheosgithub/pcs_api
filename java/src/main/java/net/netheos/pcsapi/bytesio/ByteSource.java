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
 * Readable source of bytes that supplies an ByteSink
 */
public interface ByteSource
{

    /**
     * Returns a ByteSourceStream object for reading data, to be closed by caller
     *
     * @return the underlying stream
     * @throws IOException
     */
    InputStream openStream()
            throws IOException;

    /**
     * Return length of stream (must be known before consuming stream)
     *
     * @return The stream bytes count
     */
    long length();

}
