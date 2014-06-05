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

import net.netheos.pcsapi.bytesio.ByteSink;
import net.netheos.pcsapi.bytesio.ByteSinkStream;
import net.netheos.pcsapi.exceptions.CAuthenticationException;
import net.netheos.pcsapi.exceptions.CFileNotFoundException;
import net.netheos.pcsapi.exceptions.CHttpException;
import net.netheos.pcsapi.exceptions.CRetriableException;
import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.request.CResponse;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.nio.charset.Charset;
import java.security.SecureRandom;
import java.util.Random;
import net.netheos.pcsapi.models.CPath;
import org.apache.http.client.HttpClient;

/**
 * Utility class for the project
 */
public final class PcsUtils
{

    public static final Charset UTF8 = Charset.forName( "UTF-8" );
    private static final char[] CHARS = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKMNPQRSTUVWXYZ23456789".toCharArray();
    public static final boolean ANDROID = System.getProperty( "java.vm.name" ).toLowerCase().contains( "dalvik" );

    private PcsUtils()
    {
    }

    /**
     * Extract content type from response headers, and ensure it is application/json or text/javascript. If no
     * content-type is defined, or content-type is not json, raises a CHttpError.
     *
     * @param response the response to check
     * @param isRetriable if True, raised exception is wrapped into a CRetriable
     */
    public static void ensureContentTypeIsJson( CResponse response, boolean isRetriable )
            throws CStorageException
    {
        String contentType = response.getContentType();

        CStorageException ex = null;
        if ( contentType == null ) {
            ex = buildCStorageException( response, "Undefined Content-Type in server response", null );

        } else if ( !contentType.contains( "application/json" ) && !contentType.contains( "text/javascript" ) ) {
            ex = buildCStorageException( response, "Content-Type is not json: " + contentType, null );
        }

        if ( ex != null ) {
            if ( isRetriable ) {
                ex = new CRetriableException( ex );
            }
            throw ex;
        }
    }

    /**
     * Extract content type from response headers, and ensure it is application/xml.
     *
     * If no content-type is defined, or content-type is not xml, raises a CHttpError.
     *
     * @param response the response to check
     * @param isRetriable if True, raised exception is wrapped into a CRetriable
     */
    public static void ensureContentTypeIsXml( CResponse response, boolean isRetriable )
            throws CStorageException
    {
        String contentType = response.getContentType();

        CStorageException ex = null;
        if ( contentType == null ) {
            ex = buildCStorageException( response, "Undefined Content-Type in server response", null );

        } else if ( !contentType.contains( "application/xml" ) && !contentType.contains( "text/xml" ) ) {
            ex = buildCStorageException( response, "Content-Type is not xml: " + contentType, null );
        }

        if ( ex != null ) {
            if ( isRetriable ) {
                ex = new CRetriableException( ex );
            }
            throw ex;
        }
    }

    /**
     * Some common code between providers. Handles the different status codes, and generates a nice exception
     *
     * @param response The wrapped HTTP response
     * @param message The error message (provided by the server or by the application)
     * @param path The file requested (which failed)
     * @return The exception
     */
    public static CStorageException buildCStorageException( CResponse response, String message, CPath path )
    {
        switch ( response.getStatus() ) {
            case 401:
                return new CAuthenticationException( message, response );

            case 404:
                message = "No file found at URL " + shortenUrl( response.getUri() ) + " (" + message + ")";
                return new CFileNotFoundException( message, path );

            default:
                return new CHttpException( message, response );
        }
    }

    /**
     * Removes query parameters from url (only for logging, as query parameters may contain sensible informations)
     *
     * @param uri
     * @return URI without parameters
     */
    public static String shortenUrl( URI uri )
    {
        return uri.getScheme() + "://" + uri.getHost() + uri.getPath();
    }

    /**
     * Abbreviate a string if longer than maxLen.
     *
     * @param source may be null
     * @param maxLen must be &gt;= 0. Up to this length, string is not truncated; otherwise truncated to maxLen and
     * three dots as allipsis are added.
     * @return original or truncated string (up tomaxLen+3 characters).
     */
    public static String abbreviate( String source, int maxLen )
    {
        if ( source == null || source.length() <= maxLen ) {
            return source;
        }
        return source.substring( 0, maxLen ) + "...";
    }

    /**
     * Server has answered OK with a file to download as stream.
     *
     * Open byte sink stream, and copy data from server stream to sink stream
     *
     * @param response server response to read from
     * @param byteSink destination
     */
    public static void downloadDataToSink( CResponse response, ByteSink byteSink )
    {
        InputStream is = null;
        ByteSinkStream bss = null;
        boolean success = false;
        try {

            long contentLength = response.getContentLength();
            if ( contentLength >= 0 ) {
                // content length is known: inform any listener
                byteSink.setExpectedLength( contentLength );
            }

            long current = 0;
            is = response.openStream();
            bss = byteSink.openStream();

            byte[] buffer = new byte[ 8 * 1024 ];
            int len;
            while ( ( len = is.read( buffer ) ) != -1 ) {
                current += len;
                bss.write( buffer, 0, len );
            }

            if ( contentLength < 0 ) {
                // content length was unknown; inform listener operation is terminated
                byteSink.setExpectedLength( current );
            }

            bss.flush();
            bss.close();
            bss = null;

            success = true;

        } catch ( IOException ex ) {
            throw new CStorageException( ex.getMessage(), ex );
        } finally {
            if ( bss != null && !success ) {
                bss.abort();
            }

            PcsUtils.closeQuietly( is );
            PcsUtils.closeQuietly( bss );
        }
    }

    public static void closeQuietly( Closeable c )
    {
        if ( c != null ) {
            try {
                c.close();
            } catch ( Exception ex ) {
            }
        }
    }

    /**
     * Generate a random String
     *
     * @param len The number of characters in the output String
     * @return The randomized String
     */
    public static String randomString( int len )
    {
        return randomString( CHARS, len );
    }

    /**
     * Generate a random String
     *
     * @param values The characters list to use in the randomization
     * @param len The number of characters in the output String
     * @return The randomized String
     */
    public static String randomString( char[] values, int len )
    {
        Random rnd = new SecureRandom();
        StringBuilder sb = new StringBuilder( len );
        for ( int i = 0; i < len; i++ ) {
            sb.append( values[ rnd.nextInt( values.length )] );
        }
        return sb.toString();
    }

    public static void releaseHttpClient( HttpClient httpClient )
    {
        if ( ANDROID ) {
            // Special close for android
            if ( httpClient instanceof android.net.http.AndroidHttpClient ) {
                ( ( android.net.http.AndroidHttpClient ) httpClient ).close();
            }
        }
        httpClient.getConnectionManager().shutdown();
    }

}
