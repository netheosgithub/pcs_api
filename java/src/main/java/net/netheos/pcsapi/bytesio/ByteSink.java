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
 * Represents a destination where bytes can be written
 */
public interface ByteSink
{

    /**
     * Returns a ByteSinkStream object for writing data, to be closed by caller. This method may be called several times
     * (in case of retries)"
     *
     * @return ByteSinkStream
     * @throws IOException
     */
    ByteSinkStream openStream()
            throws IOException;

    /**
     * Defines the number of bytes that are expected to be written to the stream. This value may be defined lately
     * (after stream creation)
     * <p/>
     * Note that this length may differ from the final data size, for example if bytes are appended to an already
     * existing file.
     *
     * @param expectedLength
     */
    void setExpectedLength( long expectedLength );

}
