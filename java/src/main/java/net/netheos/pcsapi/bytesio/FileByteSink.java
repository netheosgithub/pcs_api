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

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;

/**
 * ByteSink where bytes are written to a file
 */
public class FileByteSink
        implements ByteSink
{

    private final File file;
    private final boolean appended;
    private final boolean tempName;
    private final boolean deleteOnAbort;
    private long expectedLength;
    private boolean aborted;

    public FileByteSink( File file )
    {
        this( file, false, false, false );
    }

    /**
     * @param file name of output file
     * @param appended if true, file is appended else file is overwritten
     * @param tempName if true, data will first be written to tempfile "filename.part" and when this file stream is
     * closed properly (without having been aborted) it is renamed to "filename"
     * @param deleteOnAbort if true, created file is deleted if stream is aborted or not closed properly.
     */
    public FileByteSink( File file, boolean tempName, boolean deleteOnAbort, boolean appended )
    {
        this.file = file;
        this.appended = appended;
        this.tempName = tempName;
        this.deleteOnAbort = deleteOnAbort;
        this.expectedLength = 0;
        this.aborted = false;
    }

    /**
     * Get the file used in this sink
     *
     * @return The file
     */
    File getFile()
    {
        return file;
    }

    boolean isTempName()
    {
        return tempName;
    }

    long getExpectedLength()
    {
        return expectedLength;
    }

    @Override
    public void setExpectedLength( long expectedLength )
    {
        this.expectedLength = expectedLength;
    }

    boolean isAppended()
    {
        return appended;
    }

    boolean isDeleteOnAbort()
    {
        return deleteOnAbort;
    }

    boolean isAborted()
    {
        return aborted;
    }

    void setAborted( boolean aborted )
    {
        this.aborted = aborted;
    }

    @Override
    public ByteSinkStream openStream()
            throws FileNotFoundException
    {
        final File actualFile = getActualFile();
        ByteSinkStream bss = new FileByteSinkStream( this, new FileOutputStream( actualFile, appended ) );
        aborted = false;

        return bss;
    }

    File getActualFile()
    {
        if ( tempName ) {
            return new File( file + ".part" );
        } else {
            return file;
        }
    }

    @Override
    public String toString()
    {
        return "FileByteSink {fileName='" + file + "'}";
    }

}
