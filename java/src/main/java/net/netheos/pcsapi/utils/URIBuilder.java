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
import java.net.URISyntaxException;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 *
 */
public class URIBuilder
{

    private static final String SEPARATOR = "/";
    private final URI baseURI;
    private final StringBuilder path = new StringBuilder();
    private final Map<String, String> queryParams;

    /**
     *
     * @param baseURI should NOT contain any query
     */
    public URIBuilder( URI baseURI )
    {
        try {
            this.baseURI = new URI( baseURI.getScheme(),
                                    baseURI.getAuthority(),
                                    null, null, null ); // encodedPath, query, fragment
            encodedPath( baseURI.getRawPath() );
            queryParams = URIUtil.parseQueryParameters( baseURI.getRawQuery() );
        } catch ( URISyntaxException ex ) {
            // does not occur, but in case :
            throw new RuntimeException( ex );
        }
    }

    /**
     * Add an already escaped encodedPath part. This object takes cares of slash separators between successive calls to
     * this method (but it does not de-duplicates slashs in given argument).
     *
     * @param encodedPath The url encoded encodedPath (may contain slashs)
     * @return This builder
     */
    public final URIBuilder encodedPath( String encodedPath )
    {
        if ( this.path.length() > 0 ) {
            if ( this.path.charAt( this.path.length() - 1 ) != '/'
                 && !encodedPath.startsWith( SEPARATOR ) ) {
                // concatenating /a and b/c
                this.path.append( SEPARATOR );
            } else if ( this.path.charAt( this.path.length() - 1 ) == '/'
                        && encodedPath.charAt( 0 ) == '/' ) {
                // concatenating /a/ and /b/c
                this.path.deleteCharAt( this.path.length() - 1 );
            }
        }
        this.path.append( encodedPath );
        return this;
    }

    /**
     * Set the query parameters
     *
     * @param queryParams The parameters to use in the URI (not url encoded)
     * @return The builder
     */
    public URIBuilder queryParameters( Map<String, String> queryParams )
    {
        this.queryParams.clear();
        this.queryParams.putAll( queryParams );
        return this;
    }

    /**
     * Add a null query parameter (without any value). This will create a query string like : "key" (without '=' sign)
     *
     * @param key The parameter key (not url encoded)
     * @return The builder
     */
    public URIBuilder addNullParameter( String key )
    {
        if ( key != null ) {
            queryParams.put( key, null );
        }

        return this;
    }

    /**
     * Add a single query parameter. Parameter is ignored if it has null value.
     *
     * @param key The parameter key (not url encoded)
     * @param value The parameter value (not url encoded)
     * @return The builder
     */
    public URIBuilder addParameter( String key, String value )
    {
        if ( key != null && value != null ) {
            queryParams.put( key, value );
        }

        return this;
    }

    @Override
    public String toString()
    {
        return build().toString();
    }

    /**
     * Build the URI with the given informations
     *
     * @return The URI
     */
    public URI build()
    {
        StringBuilder builder = new StringBuilder( baseURI.toString() );
        builder.append( path.toString() );
        if ( queryParams != null && !queryParams.isEmpty() ) {
            builder.append( queryParameters() );
        }
        return URI.create( builder.toString() );
    }

    private CharSequence queryParameters()
    {
        StringBuilder builder = new StringBuilder( "?" );

        List<Map.Entry<String, String>> entries = new ArrayList<Map.Entry<String, String>>( queryParams.entrySet() );
        for ( int i = 0; i < entries.size(); i++ ) {
            builder.append( urlEncode( entries.get( i ).getKey() ) );
            String value = entries.get( i ).getValue();
            if ( value != null ) {
                builder.append( "=" ).append( urlEncode( value ) );
            }
            if ( i < entries.size() - 1 ) {
                builder.append( "&" );
            }
        }
        return builder;
    }

    private String urlEncode( String value )
    {
        try {
            return URLEncoder.encode( value, PcsUtils.UTF8.name() );
        } catch ( UnsupportedEncodingException ex ) {
            throw new UnsupportedOperationException( "Error generating query parameters", ex );
        }
    }

}
