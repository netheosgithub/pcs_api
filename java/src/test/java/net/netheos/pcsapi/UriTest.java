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

package net.netheos.pcsapi;

import java.net.URI;
import java.util.LinkedHashMap;
import java.util.Map;
import net.netheos.pcsapi.utils.URIBuilder;
import net.netheos.pcsapi.utils.URIUtil;
import static org.junit.Assert.*;
import org.junit.Test;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 *
 */
public class UriTest
{

    private static final Logger LOGGER = LoggerFactory.getLogger( UriTest.class );

    @Test
    public void testGetQueryParameter()
            throws Exception
    {
        String url = "http://www.host.net/path/test?code=edc&test=1212";
        URI uri = new URI( url );

        assertEquals( "edc", URIUtil.getQueryParameter( uri, "code" ) );
        assertNull( URIUtil.getQueryParameter( uri, "cod" ) );

        uri = URI.create( "http://www.host.net/path/test?code=edc+1&null&empty=&q=with+an+%26&test=%22foo+bar%E2%82%AC%22" );
        assertEquals( "edc 1", URIUtil.getQueryParameter( uri, "code" ) );
        assertNull( URIUtil.getQueryParameter( uri, "null" ) );
        assertEquals( "", URIUtil.getQueryParameter( uri, "empty" ) );
        assertEquals( "\"foo bar€\"", URIUtil.getQueryParameter( uri, "test" ) );
        assertEquals( "with an &", URIUtil.getQueryParameter( uri, "q" ) );
    }

    @Test
    public void testParseQueryParameters()
            throws Exception
    {
        URI uri = URI.create( "https://localhost/?a&b=&c=%22" );
        Map<String, String> params = URIUtil.parseQueryParameters( uri.getRawQuery() );
        assertTrue( params.containsKey( "a" ) );
        assertNull( params.get( "a" ) );
        assertEquals( "", params.get( "b" ) );
        assertEquals( "\"", params.get( "c" ) );

        params = URIUtil.parseQueryParameters( "" );
        assertEquals( 0, params.size() );

        params = URIUtil.parseQueryParameters( null );
        assertEquals( 0, params.size() );
    }

    @Test
    public void testURIBuilder()
            throws Exception
    {
        String url = "http://www.host.net/path/test/";
        URI uri = new URI( url );
        URIBuilder builder = new URIBuilder( uri );
        assertEquals( uri, builder.build() );

        builder.encodedPath( "/b/c" );
        assertEquals( new URI( "http://www.host.net/path/test/b/c" ), builder.build() );
        builder.encodedPath( "d" );
        assertEquals( new URI( "http://www.host.net/path/test/b/c/d" ), builder.build() );
        builder.encodedPath( "/e/" );
        assertEquals( new URI( "http://www.host.net/path/test/b/c/d/e/" ), builder.build() );
        builder.encodedPath( "/f" );
        assertEquals( new URI( "http://www.host.net/path/test/b/c/d/e/f" ), builder.build() );
        builder.encodedPath( "/" );
        assertEquals( new URI( "http://www.host.net/path/test/b/c/d/e/f/" ), builder.build() );

        url = "https://user@www.host.net";
        uri = new URI( url );
        builder = new URIBuilder( uri );
        assertEquals( uri, builder.build() );
        Map<String, String> params = new LinkedHashMap<String, String>(); // preserve params order
        params.put( "param1", "value1" );
        params.put( "paràm2", "va lu€2 " );
        builder.queryParameters( params );
        assertEquals( new URI( "https://user@www.host.net?param1=value1&par%C3%A0m2=va+lu%E2%82%AC2+" ),
                      builder.build() );
        builder.encodedPath( "" );
        assertEquals( new URI( "https://user@www.host.net?param1=value1&par%C3%A0m2=va+lu%E2%82%AC2+" ),
                      builder.build() );

        uri = URI.create( "http://host?p1&p2=&p3" );
        builder = new URIBuilder( uri );
        assertEquals( uri, builder.build() );
        builder.addParameter( "p4", null ); // ignored
        assertEquals( uri, builder.build() );
        builder.addParameter( "p5", "value" );
        assertEquals( URI.create( "http://host?p1&p2=&p3&p5=value" ), builder.build() );
        builder.addNullParameter( "p6" );
        assertEquals( URI.create( "http://host?p1&p2=&p3&p5=value&p6" ), builder.build() );
        builder.addParameter( "p7", "" );
        assertEquals( URI.create( "http://host?p1&p2=&p3&p5=value&p6&p7=" ), builder.build() );

        // replace all parameters :
        builder.queryParameters( params );
        assertEquals( new URI( "http://host?param1=value1&par%C3%A0m2=va+lu%E2%82%AC2+" ),
                      builder.build() );
    }

}
