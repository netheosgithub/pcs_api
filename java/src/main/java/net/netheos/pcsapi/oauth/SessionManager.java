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

package net.netheos.pcsapi.oauth;

import net.netheos.pcsapi.credentials.Credentials;
import net.netheos.pcsapi.credentials.UserCredentials;
import net.netheos.pcsapi.request.HttpExecutor;

/**
 * Binds HTTP requests with an authentication protocol
 *
 * @param <T> The credentials type used in the manager
 */
public abstract class SessionManager<T extends Credentials>
        implements HttpExecutor
{

    protected UserCredentials<T> userCredentials;

    protected SessionManager( UserCredentials userCredentials )
    {
        this.userCredentials = userCredentials;
    }

}
