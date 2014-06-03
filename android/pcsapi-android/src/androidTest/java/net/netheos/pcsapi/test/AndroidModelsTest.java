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

package net.netheos.pcsapi.test;

import android.test.ActivityTestCase;

import net.netheos.pcsapi.ModelsTest;

/**
 * Created by fabien on 14/03/14.
 */
public class AndroidModelsTest extends ActivityTestCase {

    private final ModelsTest mTest;

    public AndroidModelsTest() {
        mTest = new ModelsTest();
    }

    public void testCPath() throws Exception {
        mTest.testCPath();
    }

    public void testCPathAsKey() throws Exception {
        mTest.testCPathAsKey();
    }

    public void testCPathUrlEncoded() throws Exception {
        mTest.testCPathUrlEncoded();
    }

    public void testInvalidCPath() throws Exception {
        mTest.testInvalidCPath();
    }

    public void testDownloadRequestBytesRange() throws Exception {
        mTest.testDownloadRequestBytesRange();
    }

    public void testDownloadRequestProgressListener() throws Exception {
        mTest.testDownloadRequestProgressListener();
    }

    public void testUploadRequestProgressListener() throws Exception {
        mTest.testUploadRequestProgressListener();
    }

}
