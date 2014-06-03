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

import android.net.http.AndroidHttpClient;
import android.os.Environment;
import android.test.InstrumentationTestCase;

import net.netheos.pcsapi.credentials.AppInfoFileRepository;
import net.netheos.pcsapi.credentials.UserCredentialsFileRepository;
import net.netheos.pcsapi.credentials.UserCredentialsRepository;
import net.netheos.pcsapi.providers.dropbox.Dropbox;
import net.netheos.pcsapi.providers.hubic.Hubic;
import net.netheos.pcsapi.providers.googledrive.GoogleDrive;
import net.netheos.pcsapi.providers.cloudme.CloudMe;
import net.netheos.pcsapi.providers.StressTest;
import net.netheos.pcsapi.storage.IStorageProvider;
import net.netheos.pcsapi.storage.StorageBuilder;
import net.netheos.pcsapi.storage.StorageFacade;

import java.io.File;

/**
 * Android does not support test ng (nor junit4), only junit3.
 *
 * This is an adapter class.
 */
public class AndroidStressTest extends InstrumentationTestCase {

    @Override
    protected void setUp() throws Exception {
        System.setProperty("java.io.tmpdir", getInstrumentation().getContext().getCacheDir().getPath());
        super.setUp();
    }

    public void testHubicProvider() throws Exception {
        testProvider(Hubic.PROVIDER_NAME);
    }

    public void testDropboxProvider() throws Exception {
        testProvider(Dropbox.PROVIDER_NAME);
    }

    public void testGoogleDriveProvider() throws Exception {
        testProvider(GoogleDrive.PROVIDER_NAME);
    }

    public void testCloudMeProvider() throws Exception {
        testProvider(CloudMe.PROVIDER_NAME);
    }

    private void testProvider(String name) throws Exception {
        File sdCard = new File(Environment.getExternalStorageDirectory(), "pcsapi");
        sdCard.mkdirs();

        AppInfoFileRepository appRepo = new AppInfoFileRepository(new File(sdCard, "app_info_data.txt"));
        UserCredentialsRepository credentialsRepo = new UserCredentialsFileRepository(new File(sdCard, "user_credentials_data.txt"));

        StorageBuilder builder = StorageFacade.forProvider(name);
        builder.setAppInfoRepository(appRepo, appRepo.get(name).getAppName());
        builder.setUserCredentialsRepository(credentialsRepo, null);
        builder.setHttpClient(AndroidHttpClient.newInstance("pcsapi-android-test", getInstrumentation().getContext()));
        IStorageProvider storage = builder.build();

        StressTest mTest = new StressTest(storage);
        mTest.setUp();
        try {
            mTest.testCrud();
            mTest.testMultiCrud();

        } finally {
            mTest.tearDown();
        }
    }
}
