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

import net.netheos.pcsapi.utils.PcsUtils;
import org.junit.Test;
import static org.junit.Assert.*;

public class PcsUtilsTest
{

    @Test
    public void testAbbreviate()
    {
        assertNull( PcsUtils.abbreviate( null, 0 ) );
        assertNull( PcsUtils.abbreviate( null, 10 ) );
        String test = "foobar";
        assertEquals( test, PcsUtils.abbreviate( test, 10 ) );
        assertEquals( test, PcsUtils.abbreviate( test, 6 ) );
        assertEquals( "fooba...", PcsUtils.abbreviate( test, 5 ) );
        assertEquals( "...", PcsUtils.abbreviate( test, 0 ) );
    }
}
