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

/**
 * A simple progress listener that outputs to stdout.
 */
public class StdoutProgressListener
        implements ProgressListener
{

    private long total = -1;
    private long current = 0;
    private boolean aborted = false;

    @Override
    public void setProgressTotal( long total )
    {
        this.total = total;
    }

    @Override
    public void progress( long current )
    {
        this.current = current;
        System.out.print( "Progress: " + current + " / " + total + "\r" );

        if ( current == total ) {
            System.out.println( "********* END OF PROGRESS *********" );
        }
    }

    @Override
    public void aborted()
    {
        aborted = true;
        System.out.println( "Process has been aborted" );
    }

    /**
     * Indicates if the progress has been aborted
     *
     * @return true if aborted, false otherwise
     */
    public boolean isAborted()
    {
        return aborted;
    }

    public long getTotal()
    {
        return total;
    }

    public long getCurrent()
    {
        return current;
    }

}
