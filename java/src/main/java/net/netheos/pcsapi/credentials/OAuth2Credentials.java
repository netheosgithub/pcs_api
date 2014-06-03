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

import java.util.Date;
import net.netheos.pcsapi.oauth.OAuth2;
import org.json.JSONObject;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Credentials contains the OAuth informations.
 */
public class OAuth2Credentials
        extends Credentials
{

    private static final Logger LOGGER = LoggerFactory.getLogger( OAuth2Credentials.class );

    static final String ACCESS_TOKEN = "access_token";
    static final String EXPIRES_IN = "expires_in";
    static final String EXPIRES_AT = "expires_at";
    static final String TOKEN_TYPE = "token_type";

    private String accessToken;
    private Date expiresAt;
    private String refreshToken;
    private String tokenType;

    OAuth2Credentials( String accessToken, Date expiresAt, String refreshToken, String tokenType )
    {
        this.accessToken = accessToken;
        this.expiresAt = expiresAt;
        this.refreshToken = refreshToken;
        this.tokenType = tokenType;
    }

    /**
     * Indicates if the access token has expired
     *
     * @return true if expired, false otherwise
     */
    public boolean hasExpired()
    {
        if ( expiresAt == null ) {
            LOGGER.debug( "hasExpired - token is not expirable" );
            return false;
        }

        long now = System.currentTimeMillis();
        if ( LOGGER.isDebugEnabled() ) {
            LOGGER.debug( "hasExpired? - now: {} expiredAt: {} ", new Date( now ), expiresAt );
        }
        return now > expiresAt.getTime();
    }

    /**
     * Get the OAuth access token
     *
     * @return The token
     */
    public String getAccessToken()
    {
        return accessToken;
    }

    /**
     * Get the refresh token (used to renew the access token)
     *
     * @return The token or null il the access token never expires
     */
    public String getRefreshToken()
    {
        return refreshToken;
    }

    /**
     * Get the access token type
     *
     * @return The token type (ie. "Bearer")
     */
    public String getTokenType()
    {
        return tokenType;
    }

    /**
     * Update the credentials from a JSON request response
     *
     * @param json The JSON object containing the values to update
     */
    public void update( JSONObject json )
    {
        accessToken = json.getString( ACCESS_TOKEN );
        tokenType = json.getString( TOKEN_TYPE );
        expiresAt = new Date( System.currentTimeMillis() + ( json.getInt( EXPIRES_IN ) * 1000 ) );
        if ( json.has( OAuth2.REFRESH_TOKEN ) ) {
            refreshToken = json.getString( OAuth2.REFRESH_TOKEN );
        }
    }

    @Override
    public String toJson()
    {
        JSONObject jObj = new JSONObject();
        jObj.put( ACCESS_TOKEN, accessToken );
        if ( expiresAt != null ) {
            jObj.put( EXPIRES_AT, expiresAt.getTime() / 1000 ); // Store date in seconds
        }
        if ( refreshToken != null ) {
            jObj.put( OAuth2.REFRESH_TOKEN, refreshToken );
        }
        if ( tokenType != null ) {
            jObj.put( TOKEN_TYPE, tokenType );
        }
        return jObj.toString();
    }

    @Override
    public boolean equals( Object obj )
    {
        if ( obj == null ) {
            return false;
        }
        if ( obj instanceof OAuth2Credentials ) {
            OAuth2Credentials creds = ( OAuth2Credentials ) obj;
            return accessToken.equals( creds.accessToken );
        }
        return false;
    }

    @Override
    public int hashCode()
    {
        int hash = 3;
        hash = 47 * hash + ( this.accessToken != null ? this.accessToken.hashCode() : 0 );
        return hash;
    }

    @Override
    public String toString()
    {
        return "OAuth2Credentials {expiresAt=" + expiresAt + ", tokenType='" + tokenType + "'}";
    }

}
