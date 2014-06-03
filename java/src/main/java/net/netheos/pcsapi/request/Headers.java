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

import org.apache.http.Header;
import org.apache.http.message.BasicHeader;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.ListIterator;

/**
 * Centralizes the whole headers on a request or in a http response.
 */
public class Headers
        implements Iterable<Header>
{

    private final List<Header> headers = new ArrayList<Header>();

    Headers( Header[] headers )
    {
        this.headers.addAll( Arrays.asList( headers ) );
    }

    public Headers()
    {
    }

    /**
     * Add a new header to the list
     *
     * @param name The header name
     * @param value The header value
     */
    public void addHeader( String name, String value )
    {
        headers.add( new BasicHeader( name, value ) );
    }

    /**
     * Add a new header to the list
     *
     * @param header The http header to set
     */
    public void addHeader( Header header )
    {
        headers.add( header );
    }

    /**
     * Remove a header from the list
     *
     * @param name The header name
     */
    public void removeHeader( String name )
    {
        ListIterator<Header> it = headers.listIterator();
        while ( it.hasNext() ) {
            if ( it.next().getName().equals( name ) ) {
                it.remove();
            }
        }
    }

    /**
     * Indicates if a header exists with the given name
     *
     * @param name The header name
     * @return true if a header exists with the given name, false otherwise
     */
    public boolean contains( String name )
    {
        for ( Header header : headers ) {
            if ( header.getName().equals( name ) ) {
                return true;
            }
        }
        return false;
    }

    /**
     * Indicates if no header sets
     *
     * @return true if no headers are found, false otherwise
     */
    public boolean isEmpty()
    {
        return headers.isEmpty();
    }

    /**
     * Get a header value
     *
     * @param name The header name
     * @return The header value
     */
    public String getHeaderValue( String name )
    {
        for ( Header header : headers ) {
            if ( header.getName().equals( name ) ) {
                return header.getValue();
            }
        }
        return null;
    }

    @Override
    public Iterator<Header> iterator()
    {
        return headers.iterator();
    }

    @Override
    public String toString()
    {
        return headers.toString();
    }

}
