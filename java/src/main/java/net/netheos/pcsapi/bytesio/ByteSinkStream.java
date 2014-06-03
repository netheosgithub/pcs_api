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

import java.io.FilterOutputStream;
import java.io.OutputStream;

/**
 * ByteSinkStream are FilterOutputStream that can abort current operations
 */
public abstract class ByteSinkStream
        extends FilterOutputStream
{

    /**
     * Creates a <code>ByteSinkStream</code> by assigning the argument <code>out</code> to the field
     * <code>this.in</code> so as to remember it for later use.
     *
     * @param out the underlying output stream, or <code>null</code> if this instance is to be created without an
     * underlying stream.
     */
    protected ByteSinkStream( OutputStream out )
    {
        super( out );
    }

    /**
     * Aborts operations with this stream (writes will stop before expected end). Implementations may use this method to
     * delete temporary file or indicate some warning.
     * <p/>
     * abort() must be called before close()
     */
    public abstract void abort();

    /**
     * Indicates if the sink has been aborted
     *
     * @return true if aborted, false otherwise
     */
    public abstract boolean isAborted();

}
