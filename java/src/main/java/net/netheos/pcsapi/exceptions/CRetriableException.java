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

package net.netheos.pcsapi.exceptions;

/**
 * Thrown by RequestInvoker validation method, when request has failed but should be retried.
 * <p/>
 * This class is only a marker ; the underlying root exception is given by the 'cause' attribute. The optional 'delay'
 * specifies how much one should wait before retrying
 */
public class CRetriableException
        extends CStorageException
{

    private final long delay;

    public CRetriableException( Throwable cause )
    {
        this( cause, -1 );
    }

    /**
     * Build a CRetriableException and tell engine to wait the specified amount of time.
     *
     * @param cause
     * @param delay if &lt; 0, delay is unspecified
     */
    public CRetriableException( Throwable cause, long delay )
    {
        super( cause.getMessage(), cause );
        this.delay = delay;
    }

    /**
     * Get the delay before retrying the request
     *
     * @return The delay in millis (if negative, no delay is specified).
     */
    public long getDelay()
    {
        return delay;
    }

}
