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

import android.content.Context;
import android.test.ActivityTestCase;
import android.test.InstrumentationTestCase;

import net.netheos.pcsapi.BytesIOTest;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Created by fabien on 14/03/14.
 */
public class AndroidBytesIOTest extends InstrumentationTestCase {

    private final BytesIOTest mTest;

    public AndroidBytesIOTest() {
        mTest = new BytesIOTest();
    }

    @Override
    protected void setUp() throws Exception {
        System.setProperty("java.io.tmpdir", getInstrumentation().getContext().getCacheDir().getPath());

        super.setUp();
        mTest.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        mTest.tearDown();
    }

    public void testFileByteSink() throws Exception {
        mTest.testFileByteSink();
    }

    public void testFileByteSource() throws Exception {
        mTest.testFileByteSource();
    }

    public void testProgressByteSink() throws Exception {
        mTest.testProgressByteSink();
    }
}
