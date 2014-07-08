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

    private static final String ACCESS_TOKEN = "access_token";
    private static final String EXPIRES_IN = "expires_in";
    private static final String EXPIRES_AT = "expires_at";
    private static final String TOKEN_TYPE = "token_type";

    private String accessToken;
    private Date expiresAt; // may be null if no expiration date
    private String refreshToken;
    private String tokenType;

    // package private
    static OAuth2Credentials fromJson( JSONObject jsonObj )
    {
        String accessToken = jsonObj.optString( OAuth2Credentials.ACCESS_TOKEN, null );
        Date expiresAt = calculateExpiresAt( jsonObj );
        String refreshToken = jsonObj.optString( OAuth2.REFRESH_TOKEN, null );
        String tokenType = jsonObj.optString( OAuth2Credentials.TOKEN_TYPE, null );

        return new OAuth2Credentials( accessToken, expiresAt, refreshToken, tokenType );
    }

    private OAuth2Credentials( String accessToken, Date expiresAt, String refreshToken, String tokenType )
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
     * Update the credentials from a JSON request response.
     *
     * @param json The JSON object containing the values to update
     */
    public void update( JSONObject json )
    {
        accessToken = json.getString( ACCESS_TOKEN );
        tokenType = json.getString( TOKEN_TYPE );
        expiresAt = calculateExpiresAt( json );
        if ( json.has( OAuth2.REFRESH_TOKEN ) ) {
            refreshToken = json.getString( OAuth2.REFRESH_TOKEN );
        }
    }

    /**
     * Calculate expiration timestamp, if not defined.
     * 
     * Several cases: absolute expiration time exists in json, or only relative, or no expiration.
     * 
     * @param jsonObj
     * @return expiration date, or null if none.
     */
    private static Date calculateExpiresAt( JSONObject jsonObj )
    {
        long expiresAt_s = jsonObj.optLong( OAuth2Credentials.EXPIRES_AT, -1 );
        // If nothing specified in json, check if expires_in is present:
        // this happens when token is received from oauth server
        if ( expiresAt_s < 0 && jsonObj.has( OAuth2Credentials.EXPIRES_IN ) ) {
            long expiresIn_s = jsonObj.getLong( OAuth2Credentials.EXPIRES_IN );
            // We take a margin to be safe: it appears that some providers do NOT take any margin
            // so token will not be early refreshed
            if ( expiresIn_s > 6 * 60 ) {  // should be always true
                expiresIn_s -= 5 * 60;  // 5 minutes to be safe
            }
            expiresAt_s = System.currentTimeMillis() / 1000 + expiresIn_s;
        }

        if ( expiresAt_s < 0 ) {
            return null;
        }
        return new Date( expiresAt_s * 1000 );
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
