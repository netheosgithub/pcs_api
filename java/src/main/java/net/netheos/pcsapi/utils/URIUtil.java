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

package net.netheos.pcsapi.utils;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URLDecoder;
import java.util.BitSet;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * This class contains methods forked from org.apache.catalina.servlets.DefaultServlet.
 */
public class URIUtil
{

    private URIUtil()
    {
    }

    /**
     * Array containing the safe characters set as defined by RFC 1738
     */
    private static final BitSet safeCharacters;
    private static final char[] hexadecimal = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                                'A', 'B', 'C', 'D', 'E', 'F' };

    static {
        safeCharacters = new BitSet( 256 );
        int i;
        // 'lowalpha' rule
        for ( i = 'a'; i <= 'z'; i++ ) {
            safeCharacters.set( i );
        }
        // 'hialpha' rule
        for ( i = 'A'; i <= 'Z'; i++ ) {
            safeCharacters.set( i );
        }
        // 'digit' rule
        for ( i = '0'; i <= '9'; i++ ) {
            safeCharacters.set( i );
        }

        // 'safe' rule
        safeCharacters.set( '$' );
        safeCharacters.set( '-' );
        safeCharacters.set( '_' );
        safeCharacters.set( '.' );
//        safeCharacters.set( '+' );

        // 'extra' rule
        safeCharacters.set( '!' );
        safeCharacters.set( '*' );
        safeCharacters.set( '\'' );
        safeCharacters.set( '(' );
        safeCharacters.set( ')' );
        safeCharacters.set( ',' );

        // special characters common to http: file: and ftp: URLs ('fsegment' and 'hsegment' rules)
        safeCharacters.set( '/' );
//        safeCharacters.set( ':' );
        safeCharacters.set( '@' );
//        safeCharacters.set( '&' );
//        safeCharacters.set( '=' );
    }

    /**
     * Encode a path as required by the URL specification (<a href="http://www.ietf.org/rfc/rfc1738.txt">
     * RFC 1738</a>). This differs from <code>java.net.URLEncoder.encode()</code> which encodes according to the
     * <code>x-www-form-urlencoded</code> MIME format.
     *
     * @param path the path to encode
     * @return the encoded path
     */
    public static String encodePath( String path )
    {
        final StringBuilder sb = new StringBuilder( 2 * path.length() );

        for ( int i = 0; i < path.length(); i++ ) {
            final char c = path.charAt( i );
            if ( safeCharacters.get( c ) ) {
                sb.append( c );
            } else {
                final byte[] bytes = String.valueOf( c ).getBytes( PcsUtils.UTF8 ); // always work
                for ( byte b : bytes ) {
                    sb.append( '%' ) //
                            .append( hexadecimal[( b >> 4 ) & 0xf] ) //
                            .append( hexadecimal[ b & 0xf] );
                }
            }
        }

        return sb.toString();
    }

    /**
     * Retrieves a query param in a URL according to a key.
     *
     * If several parameters are to be retrieved from the same URI, better use
     * <code>parseQueryString( uri.getRawQuery() )</code>.
     *
     * @param uri
     * @param name
     * @return parameter value (if found) ; null if not found or no = after name.
     */
    public static String getQueryParameter( URI uri, String name )
    {
        Map<String, String> params = parseQueryParameters( uri.getRawQuery() );
        return params.get( name );
    }

    /**
     * Parse a query param string into a map of parameters.
     *
     * Duplicated parameters cannot be handled by this method. Parameters names not followed by '=' sign are only keys
     * but have null value. Parameters names followed by '=' sign have an empty string value.
     *
     * @param rawQuery The query param string, as returned by URI.getRawQuery() (UTF-8 charset)
     * @return a (mutable) map of parameters (empty map if rawQuery is null or empty). Order of parameters is kept.
     */
    public static Map<String, String> parseQueryParameters( String rawQuery )
    {
        Map<String, String> ret = new LinkedHashMap<String, String>();
        if ( rawQuery == null || rawQuery.length() == 0 ) {
            return ret;
        }
        try {
            String[] pairs = rawQuery.split( "&" );
            for ( String pair : pairs ) {
                int idx = pair.indexOf( "=" );
                String name;
                String value;
                if ( idx > 0 ) {
                    // name=value
                    name = pair.substring( 0, idx );
                    value = URLDecoder.decode( pair.substring( idx + 1 ), PcsUtils.UTF8.name() );
                } else if ( idx < 0 ) {
                    // name (no value)
                    name = pair;
                    value = null;
                } else {
                    // =value ?? we ignore this
                    name = null;
                    value = null;
                }
                if ( name != null ) {
                    ret.put( name, value );
                }
            }

            return ret;
        } catch ( UnsupportedEncodingException ex ) {
            throw new RuntimeException( ex );
        }
    }

}
