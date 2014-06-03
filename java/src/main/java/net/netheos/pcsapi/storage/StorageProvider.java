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

package net.netheos.pcsapi.storage;

import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.models.RetryStrategy;
import net.netheos.pcsapi.oauth.SessionManager;
import net.netheos.pcsapi.utils.PcsUtils;
import org.apache.http.client.HttpClient;

public abstract class StorageProvider<T extends SessionManager>
        implements IStorageProvider
{

    protected final RetryStrategy retryStrategy;
    protected final T sessionManager;
    private final String providerName;

    private final HttpClient httpClient;

    protected StorageProvider( String providerName,
                               T sessionManager,
                               RetryStrategy retryStrategy,
                               HttpClient httpClient )
    {
        this.providerName = providerName;
        this.sessionManager = sessionManager;
        this.retryStrategy = retryStrategy;
        this.httpClient = httpClient;
    }

    public T getSessionManager()
    {
        return sessionManager;
    }

    @Override
    public String getProviderName()
    {
        return providerName;
    }

    @Override
    public void close()
            throws CStorageException
    {
        PcsUtils.releaseHttpClient( httpClient );
    }

    @Override
    public String toString()
    {
        return getProviderName();
    }

}
