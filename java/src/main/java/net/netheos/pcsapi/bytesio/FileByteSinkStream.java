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

package net.netheos.pcsapi.bytesio;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Implements the file version of a byte sink stream
 */
class FileByteSinkStream
        extends ByteSinkStream
{

    private static final Logger LOGGER = LoggerFactory.getLogger( FileByteSinkStream.class );
    private final FileByteSink fileByteSink;

    public FileByteSinkStream( FileByteSink fileByteSink, FileOutputStream fos )
    {
        super( fos );
        this.fileByteSink = fileByteSink;
    }

    @Override
    public void close()
            throws IOException
    {
        boolean closedProperly = false;

        try {
            super.flush(); // cf. http://bugs.java.com/view_bug.do?bug_id=6335274
            super.close();
            closedProperly = true;

        } finally {
            if ( fileByteSink.isAborted() ) {
                LOGGER.debug( "Sink process has been aborted" );
            }

            if ( fileByteSink.isDeleteOnAbort() && !closedProperly ) {
                LOGGER.warn( "Sink file not closed properly : will be deleted" );
            }

            File file = fileByteSink.getActualFile();

            // Now two cases: it worked fine or it was aborted (or close failed)
            if ( fileByteSink.isAborted() || !closedProperly ) {

                if ( fileByteSink.isDeleteOnAbort() ) {
                    LOGGER.debug( "Will delete sink file: {}", file );
                    file.delete();
                    boolean success = file.delete();
                    LOGGER.debug( "file deletion - success: {}", success );

                } else {
                    // @TODO transferred length is not always equals to file length (mode=append)
                    long actualFileLength = file.length();
                    LOGGER.debug( "Actual file length: {}", actualFileLength );

                    if ( fileByteSink.getExpectedLength() >= 0 ) {

                        if ( actualFileLength == fileByteSink.getExpectedLength() ) {
                            LOGGER.debug( "Sink file is complete: {} ({} bytes)", file, actualFileLength );

                        } else if ( actualFileLength < fileByteSink.getExpectedLength() ) {
                            LOGGER.debug( "Sink file is too short: {} ({} bytes < {} expected)",
                                          file, actualFileLength, fileByteSink.getExpectedLength() );

                        } else {
                            LOGGER.debug( "Sink file is too long: {} ({} bytes > {} expected)",
                                          file, actualFileLength, fileByteSink.getExpectedLength() );
                        }

                    } else {
                        LOGGER.debug( "Sink file may be incomplete: {} ({} bytes)",
                                      file, actualFileLength );
                    }
                }

            } else if ( fileByteSink.isTempName() ) {

                // Everything went fine: we rename temp file to its final name
                File destFile = fileByteSink.getFile();
                if ( destFile.exists() ) {
                    destFile.delete();
                }

                boolean success = file.renameTo( destFile );
                LOGGER.debug( "file renaming - success: {}", success );
            }
        }
    }

    @Override
    public void abort()
    {
        fileByteSink.setAborted( true );
    }

    @Override
    public boolean isAborted()
    {
        return fileByteSink.isAborted();
    }

}
