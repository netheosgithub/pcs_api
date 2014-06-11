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

package net.netheos.pcsapi.sample;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.widget.ListView;
import android.widget.Toast;

import net.netheos.pcsapi.OAuth2AuthorizationActivity;
import net.netheos.pcsapi.models.CFile;

import java.io.File;
import java.util.List;

/**
 * Rough android sample application : oauth2 authorization, then recursively
 * list files.
 * <p/>
 * This is sample code only, not production code. application info are searched
 * in a (public) file on sd-card user credentials are persisted in application
 * own directory. Errors are not handled properly.
 */
public class MainActivity extends Activity
{
    public static final String EXTRA_PROVIDER_NAME = "EXTRA_PROVIDER_NAME";
    private static final int ACTIVITY_GET_AUTHENTICATION = 1;

    private String mProviderName;
    private File mCredentialsFile;
    private View mProgressBar;
    private ListView mListView;
    private File mAppInfoFile;

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mProviderName = getIntent().getStringExtra(EXTRA_PROVIDER_NAME);

        mProgressBar = findViewById(R.id.progressBar);
        mListView = (ListView) findViewById(R.id.listView);

        File dir = getDir("repository", Context.MODE_PRIVATE);

        if (mProviderName.equals("cloudme")){
            mCredentialsFile = new File(Environment.getExternalStoragePublicDirectory("pcs_api"), "user_credentials_data.txt");
        } else {
            mCredentialsFile = new File(dir, "credentials.conf");
        }

        // This may be quite tricky, depending on phones...
        mAppInfoFile = new File(Environment.getExternalStoragePublicDirectory("pcs_api"), "app_info_data.txt");
        if (!mAppInfoFile.exists()) {
            mAppInfoFile = new File("/storage/extSdCard/pcs_api", "app_info_data.txt");
        }

        if (!mCredentialsFile.exists()) {
            launchAuthentication();

        } else {
            // Starts files loading
            loadContent();
        }
    }

    private void launchAuthentication()
    {
        Intent intent = new Intent(this, OAuth2AuthorizationActivity.class);
        intent.putExtra(OAuth2AuthorizationActivity.EXTRA_PROVIDER_NAME, mProviderName);
        intent.putExtra(OAuth2AuthorizationActivity.EXTRA_CREDENTIALS_FILE, mCredentialsFile);
        intent.putExtra(OAuth2AuthorizationActivity.EXTRA_APP_INFO_FILE, mAppInfoFile);

        startActivityForResult(intent, ACTIVITY_GET_AUTHENTICATION);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == ACTIVITY_GET_AUTHENTICATION) {

            if (resultCode == RESULT_OK) {
                loadContent();
            } else {
                Toast.makeText(this, R.string.auth_error, Toast.LENGTH_LONG).show();
                mProgressBar.setVisibility(View.GONE);
                finish();
            }
        }
    }

    private void loadContent()
    {
        ProviderFilesLoader loader = new ProviderFilesLoader(mProviderName, mAppInfoFile, mCredentialsFile, this)
        {
            @Override
            protected void onCancelled(List<CFile> cFiles)
            {
                Toast.makeText(MainActivity.this, "Can't find credentials", Toast.LENGTH_LONG).show();
                launchAuthentication();
            }

            @Override
            protected void onPostExecute(List<CFile> files)
            {
                mProgressBar.setVisibility(View.GONE);
                if (files != null) {
                    // Display files in list
                    mListView.setAdapter(new CFilesAdapter(files, MainActivity.this));
                }
            }

        };
        loader.execute();
    }

}
