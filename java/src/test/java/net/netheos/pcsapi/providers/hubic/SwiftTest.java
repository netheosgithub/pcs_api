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
        
        Calendar cal = Calendar.getInstance();
        cal.setTime( timestamp );
        System.out.println( timestamp );
        assertEquals( 2013, cal.get( Calendar.YEAR ) );
        assertEquals( Calendar.NOVEMBER, cal.get( Calendar.MONTH ) );
        assertEquals( 8, cal.get( Calendar.DAY_OF_MONTH ) );
        assertEquals( 16, cal.get( Calendar.HOUR_OF_DAY ) );
        assertEquals( 38, cal.get( Calendar.MINUTE ) );
        assertEquals( 33, cal.get( Calendar.SECOND ) );
        assertEquals( 439, cal.get( Calendar.MILLISECOND ) );
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
        
        Calendar cal = Calendar.getInstance();
        cal.setTime( timestamp );
        assertEquals( 2014, cal.get( Calendar.YEAR ) );
        assertEquals( Calendar.FEBRUARY, cal.get( Calendar.MONTH ) );
        assertEquals( 12, cal.get( Calendar.DAY_OF_MONTH ) );
        assertEquals( 17, cal.get( Calendar.HOUR_OF_DAY ) );
        assertEquals( 13, cal.get( Calendar.MINUTE ) );
        assertEquals( 49, cal.get( Calendar.SECOND ) );
    }
    
}
