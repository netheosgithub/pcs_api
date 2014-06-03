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
 * Object that listens a progress for instances for downloads and uploads
 */
public interface ProgressListener
{

    /**
     * @param total total elements
     */
    void setProgressTotal( long total );

    /**
     * Called when observed lengthly operation has made some progress.
     * <p/>
     * Called once with current=0 to indicate process is starting. Note that progress may restart from 0 (in case an
     * upload or download fails and is restarted).
     *
     * @param current current of elements passed so far
     *
     */
    void progress( long current );

    /**
     * Called when current operation is aborted (may be retried)
     */
    void aborted();

}
