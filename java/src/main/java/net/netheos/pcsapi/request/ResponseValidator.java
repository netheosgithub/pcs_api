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

/**
 * Represents an object that validates an HTTP response
 *
 * @param <T> The request result to validate
 */
public interface ResponseValidator<T>
{

    /**
     * Validate the response
     *
     * @param response The response to validate
     * @param path The file path used in the request (may be null)
     * @throws CStorageException Invalid response
     */
    void validateResponse( T response, CPath path )
            throws CStorageException;

}
