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

import net.netheos.pcsapi.utils.PcsUtils;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.util.HashMap;
import java.util.Map;

/**
 * This class is a simple reader/writer of users credentials applications information from a plain text file with
 * format:
 * <pre>
 * provider1.app_name_1.user_id1 = json_object provider1.app_name_1.user_id2 = json_object
 * </pre>
 * <p/>
 * Sample code only: data is NOT encrypted in file.
 */
public class UserCredentialsFileRepository
        implements UserCredentialsRepository
{

    private static final Logger LOGGER = LoggerFactory.getLogger( UserCredentialsFileRepository.class );

    private final File file;
    private final Map<String, Credentials> credentialsMap;

    public UserCredentialsFileRepository( File file )
            throws IOException
    {
        this.file = file;

        credentialsMap = new HashMap<String, Credentials>();

        if ( file.exists() ) {
            readFile();
        }
    }

    /**
     * Parses a file of credentials
     *
     * @throws IOException Error reading the file
     */
    private void readFile()
            throws IOException
    {
        BufferedReader reader = null;

        try {
            reader = new BufferedReader( new InputStreamReader( new FileInputStream( file ), PcsUtils.UTF8 ) );

            LOGGER.debug( "Reading credentials file {}", file );

            String line;
            int index = 1;

            while ( ( line = reader.readLine() ) != null ) {
                line = line.trim();

                if ( line.startsWith( "#" ) || line.length() == 0 ) {
                    continue;
                }

                String[] userCredentialsArray = line.split( "=", 2 );

                if ( userCredentialsArray.length != 2 ) {
                    throw new IllegalArgumentException( "Not parsable line #" + index );
                }

                // Key
                final String key = userCredentialsArray[0].trim();
                final Credentials value = Credentials.createFromJson( userCredentialsArray[1].trim() );

                credentialsMap.put( key, value );

                index++;
            }

        } finally {
            PcsUtils.closeQuietly( reader );
        }
    }

    @Override
    public synchronized void save( UserCredentials<?> userCredentials )
            throws IOException
    {
        String key = getUserKey( userCredentials.getAppInfo(), userCredentials.getUserId() );
        credentialsMap.put( key, userCredentials.getCredentials() );
        writeFile();
    }

    @Override
    public synchronized UserCredentials<?> get( AppInfo appInfo, String userId )
    {
        Credentials credentials = null;

        if ( userId != null ) {
            final String key = getUserKey( appInfo, userId );
            credentials = credentialsMap.get( key );

        } else {
            // loop over all credential
            final String prefix = getAppPrefix( appInfo );

            for ( String key : credentialsMap.keySet() ) {
                if ( key.startsWith( prefix ) ) {
                    userId = key.substring( prefix.length() );

                    if ( credentials != null ) {
                        throw new IllegalStateException( "Several user credentials found for application " + appInfo );
                    }

                    credentials = credentialsMap.get( key );
                }
            }
        }

        if ( credentials == null ) {
            throw new IllegalStateException( "No user credentials found for application " + appInfo );
        }

        return new UserCredentials( appInfo, userId, credentials );
    }

    /**
     * Serializes credentials data
     *
     * @throws IOException Error while saving the file
     */
    private void writeFile()
            throws IOException
    {
        LOGGER.debug( "Writing credentials file to {}", file );
        File tempFile = new File( file.getPath() + ".tmp" );

        Writer writer = null;
        try {
            writer = new OutputStreamWriter( new FileOutputStream( tempFile ), PcsUtils.UTF8 );

            for ( String key : credentialsMap.keySet() ) {
                Credentials cred = credentialsMap.get( key );
                final String line = key + "=" + cred.toJson() + "\n";
                writer.write( line );
            }
            writer.flush();

        } finally {
            PcsUtils.closeQuietly( writer );
        }

        file.delete();
        tempFile.renameTo( file );
    }

    /**
     * Builds the key of a credentials according to a given appInfo and userId
     *
     * @param appInfo The application informations
     * @param userId The user identifier
     * @return The user key
     */
    private String getUserKey( AppInfo appInfo, String userId )
    {
        if ( userId == null ) {
            throw new IllegalArgumentException( "userId should not be null" );
        }

        return String.format( "%s%s", getAppPrefix( appInfo ), userId );
    }

    /**
     * Builds a key prefix according to a given appInfo
     *
     * @param appInfo The application informations
     * @return The application prefix
     */
    private String getAppPrefix( AppInfo appInfo )
    {
        /// @TODO scope is ignored in key
        return String.format( "%s.%s.", appInfo.getProviderName(), appInfo.getAppName() );
    }

    @Override
    public String toString()
    {
        return "UserCredentialsRepo{"
               + "file='" + file + '\''
               + ", credentialsMap=" + credentialsMap
               + '}';
    }

}
