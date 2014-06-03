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

import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.models.CPath;

import org.apache.http.client.methods.HttpUriRequest;

/**
 * A simple requestor which execute a http request.
 */
public class HttpRequestor
        implements Requestor<CResponse>
{

    private final HttpUriRequest request;
    private final HttpExecutor executor;
    private final CPath path;

    /**
     * Create a new requestor
     *
     * @param request The http request (HttpGet, HttpPost ...) to execute
     * @param path The requested file path (may be null)
     * @param executor The object used to execute the request
     */
    public HttpRequestor( HttpUriRequest request, CPath path, HttpExecutor executor )
    {
        this.request = request;
        this.path = path;
        this.executor = executor;
    }

    /**
     * Get the http request used in this executor
     *
     * @return The request
     */
    public HttpUriRequest getRequest()
    {
        return request;
    }

    @Override
    public CPath getPath()
    {
        return path;
    }

    @Override
    public CResponse call()
            throws CStorageException
    {
        return executor.execute( request );
    }

}
