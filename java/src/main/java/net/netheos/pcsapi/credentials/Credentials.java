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
import org.json.JSONTokener;

/**
 * This class contains credentials token and user/password
 */
public abstract class Credentials
{

    /**
     * Serialize the credentials in a JSON String.
     *
     * @return the json representation of credentials
     */
    abstract String toJson();

    /**
     * Create a Credential object from a JSON value. It detects automaticaly which credential is defined (OAuth or
     * password)
     *
     * @param json The JSON to parse
     * @return The credentials object
     */
    public static Credentials createFromJson( String json )
    {
        JSONObject jsonObj = ( JSONObject ) new JSONTokener( json ).nextValue();

        if ( jsonObj.has( PasswordCredentials.PASSWORD ) ) {
            // Password credentials
            return PasswordCredentials.fromJson( jsonObj );
        } else {
            // OAuth2 credentials
            return OAuth2Credentials.fromJson( jsonObj );
        }
    }

}
