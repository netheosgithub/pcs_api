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

package net.netheos.pcsapi.credentials;

import org.json.JSONObject;

/**
 * Credentials containing password information.
 */
public class PasswordCredentials
        extends Credentials
{

    static final String PASSWORD = "password";

    private final String password;

    PasswordCredentials( String password )
    {
        this.password = password;
    }

    /**
     * Get the password
     *
     * @return The password
     */
    public String getPassword()
    {
        return password;
    }

    @Override
    public String toJson()
    {
        JSONObject jObj = new JSONObject();
        jObj.put( PASSWORD, password );
        return jObj.toString();
    }

}
