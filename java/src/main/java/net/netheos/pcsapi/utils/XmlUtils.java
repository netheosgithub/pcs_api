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

import net.netheos.pcsapi.exceptions.CStorageException;

import org.w3c.dom.Document;

import java.io.StringReader;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import org.xml.sax.InputSource;

/**
 * Utils class for escaping xml strings
 */
public class XmlUtils
{
    private static final Map<Integer, String> SPECIAL_CHARS_NAMES_ARRAY;
    private static final Map<String, Integer> SPECIAL_CHARS_VALUES_ARRAY;

    static {
        Map<Integer, String> namesMap = new HashMap<Integer, String>( 5 );
        namesMap.put( 34, "quot" );  // " - double-quote
        namesMap.put( 38, "amp" ); // & - ampersand
        namesMap.put( 39, "apos" ); // XML apostrophe
        namesMap.put( 60, "lt" ); // < - less-than
        namesMap.put( 62, "gt" ); // > - greater-than
        SPECIAL_CHARS_NAMES_ARRAY = Collections.unmodifiableMap( namesMap );

        Map<String, Integer> valuesMap = new HashMap<String, Integer>( 5 );
        valuesMap.put( "quot", 34 );  // " - double-quote
        valuesMap.put( "amp", 38 ); // & - ampersand
        valuesMap.put( "apos", 39 ); // XML apostrophe
        valuesMap.put( "lt", 60 ); // < - less-than
        valuesMap.put( "gt", 62 ); // > - greater-than
        SPECIAL_CHARS_VALUES_ARRAY = Collections.unmodifiableMap( valuesMap );
    }

    /**
     * <p>
     * Escapes the characters in a <code>String</code>.</p>
     * <p/>
     * <p>
     * For example, if you have called addEntity(&quot;foo&quot;, 0xA1), escape(&quot;\u00A1&quot;) will return
     * &quot;&amp;foo;&quot;</p>
     *
     * @param str The <code>String</code> to escape.
     * @return A new escaped <code>String</code>.
     */
    public static String escape( String str )
    {
        StringBuffer buf = new StringBuffer( str.length() * 2 );
        int i;
        for ( i = 0; i < str.length(); ++i ) {
            char ch = str.charAt( i );
            String entityName = SPECIAL_CHARS_NAMES_ARRAY.get( ( int ) ch );
            if ( entityName == null ) {
                if ( ch > 0x7F ) {
                    int intValue = ch;
                    buf.append( "&#" );
                    buf.append( intValue );
                    buf.append( ';' );
                } else {
                    buf.append( ch );
                }
            } else {
                buf.append( '&' );
                buf.append( entityName );
                buf.append( ';' );
            }
        }
        return buf.toString();
    }

    /**
     * <p>
     * Unescapes the entities in a <code>String</code>.</p>
     * <p/>
     * <p>
     * For example, if you have called addEntity(&quot;foo&quot;, 0xA1), unescape(&quot;&amp;foo;&quot;) will return
     * &quot;\u00A1&quot;</p>
     *
     * @param str The <code>String</code> to escape.
     * @return A new escaped <code>String</code>.
     */
    public String unescape( String str )
    {
        StringBuffer buf = new StringBuffer( str.length() );
        int i;
        for ( i = 0; i < str.length(); ++i ) {
            char ch = str.charAt( i );
            if ( ch == '&' ) {
                int semi = str.indexOf( ';', i + 1 );
                if ( semi == -1 ) {
                    buf.append( ch );
                    continue;
                }
                String entityName = str.substring( i + 1, semi );
                int entityValue;
                if ( entityName.length() == 0 ) {
                    entityValue = -1;
                } else if ( entityName.charAt( 0 ) == '#' ) {
                    if ( entityName.length() == 1 ) {
                        entityValue = -1;
                    } else {
                        char charAt1 = entityName.charAt( 1 );
                        try {
                            if ( charAt1 == 'x' || charAt1 == 'X' ) {
                                entityValue = Integer.valueOf( entityName.substring( 2 ), 16 ).intValue();
                            } else {
                                entityValue = Integer.parseInt( entityName.substring( 1 ) );
                            }
                        } catch ( NumberFormatException ex ) {
                            entityValue = -1;
                        }
                    }
                } else {
                    entityValue = this.SPECIAL_CHARS_VALUES_ARRAY.get( entityName );
                }
                if ( entityValue == -1 ) {
                    buf.append( '&' );
                    buf.append( entityName );
                    buf.append( ';' );
                } else {
                    buf.append( ( char ) ( entityValue ) );
                }
                i = semi;
            } else {
                buf.append( ch );
            }
        }
        return buf.toString();
    }

    /**
     * Parses a string and build a DOM
     *
     * @return DOM
     */
    public static Document getDomFromString( String str )
    {
        DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
        factory.setNamespaceAware( true );

        try {
            DocumentBuilder builder = factory.newDocumentBuilder();
            return builder.parse( new InputSource( new StringReader( str ) ) );
        } catch ( Exception e ) {
            throw new CStorageException( "Error parsing the Xml response: " + str, e );
        }
    }
}
