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

import net.netheos.pcsapi.request.CResponse;
import net.netheos.pcsapi.utils.PcsUtils;

/**
 * Thrown when provider server answers non OK answers.
 */
public class CHttpException
        extends CStorageException
{

    private final String requestMethod;
    private final String requestUri;
    private final int status;
    private final String reason;

    public CHttpException( String message, CResponse response )
    {
        super( message );
        this.requestMethod = response.getMethod();
        this.requestUri = PcsUtils.shortenUrl( response.getUri() );
        this.status = response.getStatus();
        this.reason = response.getReason();
    }

    /**
     * Get the HTTP status code
     *
     * @return The status code
     */
    public int getStatus()
    {
        return status;
    }

    /**
     * Get the requested URI
     *
     * @return The requested URI
     */
    public String getRequestUri()
    {
        return requestUri;
    }

    /**
     * Get the request method
     *
     * @return The method (GET, POST ...)
     */
    public String getRequestMethod()
    {
        return requestMethod;
    }

    @Override
    public String toString()
    {
        return getClass().getSimpleName() + "{ "
               + requestMethod + " " + requestUri
               + " [" + status + "/" + reason + "] " + getMessage() + " }";
    }

}
