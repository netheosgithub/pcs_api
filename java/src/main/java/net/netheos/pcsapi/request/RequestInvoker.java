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
package net.netheos.pcsapi.request;

import java.io.Closeable;
import java.util.concurrent.Callable;
import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.utils.PcsUtils;

/**
 * Invokes http request and validates http server response.
 *
 * @param <T> The request result
 */
public class RequestInvoker<T>
        implements Callable<T>
{

    protected final Requestor<T> requestor;
    protected final ResponseValidator<T> validator;

    public RequestInvoker( Requestor<T> requestor, ResponseValidator<T> validator )
    {
        this.requestor = requestor;
        this.validator = validator;
    }

    /**
     * Performs and validates http request.
     *
     * Two steps:<ol>
     * <li>requests server with doRequest()</li>
     * <li>validates response with validateResponse()</li>
     * </ol>
     *
     * @return The request result
     * @throws Exception Request invocation error
     */
    @Override
    public T call()
            throws Exception
    {
        T response = requestor.call();

        try {
            validateResponse( response );
            return response;

        } catch ( RuntimeException ex ) {
            if ( response instanceof Closeable ) {
                PcsUtils.closeQuietly( ( Closeable ) response );
            }
            throw ex;
        }
    }

    protected void validateResponse( T response )
            throws CStorageException
    {
        validator.validateResponse( response, requestor.getPath() );
    }

}
