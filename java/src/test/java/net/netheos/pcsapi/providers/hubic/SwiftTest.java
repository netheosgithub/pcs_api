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

package net.netheos.pcsapi.providers.hubic;

import java.util.Calendar;
import java.util.Date;
import java.util.TimeZone;
import net.netheos.pcsapi.request.Headers;
import org.json.JSONObject;
import static org.junit.Assert.*;
import org.junit.Test;

/**
 *
 */
public class SwiftTest
{

    @Test
    public void testParseTimestamp()
    {
        // Check we won't break if a header is missing :
        Headers headers = new Headers();
        Date timestamp = Swift.parseTimestamp( headers );
        assertNull( timestamp );

        headers.addHeader( "X-Timestamp", "1383925113.43900" ); // 2013-11-08T16:38:33.439+01:00
        timestamp = Swift.parseTimestamp( headers );

        Calendar cal = Calendar.getInstance( TimeZone.getTimeZone( "GMT" ) );
        cal.setTime( timestamp );
        System.out.println( timestamp );
        assertEquals( 2013, cal.get( Calendar.YEAR ) );
        assertEquals( Calendar.NOVEMBER, cal.get( Calendar.MONTH ) );
        assertEquals( 8, cal.get( Calendar.DAY_OF_MONTH ) );
        assertEquals( 15, cal.get( Calendar.HOUR_OF_DAY ) );
        assertEquals( 38, cal.get( Calendar.MINUTE ) );
        assertEquals( 33, cal.get( Calendar.SECOND ) );
        assertEquals( 439, cal.get( Calendar.MILLISECOND ) );

        checkTimestamp( "1383925113.430723", new Date( 1383925113430L ) );
        checkTimestamp( "1383925113.43900", new Date( 1383925113439L ) );
        checkTimestamp( "1383925113.439", new Date( 1383925113439L ) );
        checkTimestamp( "1383925113.43", new Date( 1383925113430L ) );
        checkTimestamp( "1383925113.4", new Date( 1383925113400L ) );
        checkTimestamp( "1383925113.", new Date( 1383925113000L ) );
        checkTimestamp( "1383925113", new Date( 1383925113000L ) );
    }

    private void checkTimestamp( String header_value, Date expected )
    {
        Headers headers = new Headers();
        headers.addHeader( "X-Timestamp", header_value );
        Date parsed = Swift.parseTimestamp( headers );
        assertEquals( expected, parsed );
    }

    @Test
    public void testParseLastModified()
    {
        // Check we won't break if a header is missing :
        JSONObject json = new JSONObject();
        Date timestamp = Swift.parseLastModified( json );
        assertNull( timestamp );

        json.put( "last_modified", "2014-02-12T16:13:49.346540" ); // this is UTC
        timestamp = Swift.parseLastModified( json );
        assertNotNull( timestamp );

        Calendar cal = Calendar.getInstance( TimeZone.getTimeZone( "GMT" ) );
        cal.setTime( timestamp );
        assertEquals( 2014, cal.get( Calendar.YEAR ) );
        assertEquals( Calendar.FEBRUARY, cal.get( Calendar.MONTH ) );
        assertEquals( 12, cal.get( Calendar.DAY_OF_MONTH ) );
        assertEquals( 16, cal.get( Calendar.HOUR_OF_DAY ) );
        assertEquals( 13, cal.get( Calendar.MINUTE ) );
        assertEquals( 49, cal.get( Calendar.SECOND ) );
        assertEquals( 346, cal.get( Calendar.MILLISECOND ) );

        checkLastModified( "2014-02-12T16:13:49.346540", cal.getTime());
        checkLastModified( "2014-02-12T16:13:49.3460", cal.getTime());
        checkLastModified( "2014-02-12T16:13:49.346", cal.getTime());
        cal.set( Calendar.MILLISECOND, 340 );
        checkLastModified( "2014-02-12T16:13:49.34", cal.getTime());
        cal.set( Calendar.MILLISECOND, 300 );
        checkLastModified( "2014-02-12T16:13:49.3", cal.getTime());
        cal.set( Calendar.MILLISECOND, 0 );
        checkLastModified( "2014-02-12T16:13:49.", cal.getTime());
        checkLastModified( "2014-02-12T16:13:49", cal.getTime());
    }

    private void checkLastModified( String last_modified_value, Date expected )
    {
        JSONObject json = new JSONObject();
        json.put( "last_modified", last_modified_value );
        Date parsed = Swift.parseLastModified( json );
        assertEquals( expected, parsed );
    }

}
