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

package net.netheos.pcsapi;

import android.app.Activity;
import android.net.http.AndroidHttpClient;
import android.os.AsyncTask;
import android.os.Bundle;
import android.view.View;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import net.netheos.pcsapi.credentials.AppInfoFileRepository;
import net.netheos.pcsapi.credentials.OAuth2AppInfo;
import net.netheos.pcsapi.credentials.UserCredentialsFileRepository;
import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.oauth.OAuth2Bootstrapper;
import net.netheos.pcsapi.storage.IStorageProvider;
import net.netheos.pcsapi.storage.StorageFacade;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.net.URI;

/**
 * Activity which ask the user to grant access to the API.
 * It only works for {@link net.netheos.pcsapi.storage.IStorageProvider} using OAuth2 authentication.<br/>
 * <br/>
 * To use this activity, the following extra must be provided in the Intent:
 * <li>{@link OAuth2AuthorizationActivity#EXTRA_PROVIDER_NAME}: the storage provider name ("dropbox" or "clubic"</li>
 * <li>{@link OAuth2AuthorizationActivity#EXTRA_CREDENTIALS_FILE}: the file which will save the credentials</li>
 * <li>{@link OAuth2AuthorizationActivity#EXTRA_APP_INFO_FILE}: the file containing the provider informations</li>
 */
public class OAuth2AuthorizationActivity extends Activity {
    private static final Logger LOGGER = LoggerFactory.getLogger(OAuth2AuthorizationActivity.class);

    public static final String EXTRA_PROVIDER_NAME = "EXTRA_PROVIDER_NAME";
    public static final String EXTRA_CREDENTIALS_FILE = "EXTRA_CREDENTIALS_FILE";
    public static final String EXTRA_APP_INFO_FILE = "EXTRA_APP_INFO_FILE";

    private WebView mWebView;
    private IStorageProvider mProvider;
    private OAuth2AppInfo mAppInfo;
    private AppInfoFileRepository mAppRepo;
    private UserCredentialsFileRepository mCredentialsRepo;
    private OAuth2Bootstrapper mBootstrapper;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_get_authorization);

        String providerName = getIntent().getStringExtra(EXTRA_PROVIDER_NAME);
        File credentialsFile = (File) getIntent().getSerializableExtra(EXTRA_CREDENTIALS_FILE);
        File appInfoFile = (File) getIntent().getSerializableExtra(EXTRA_APP_INFO_FILE);

        mWebView = (WebView) findViewById(R.id.webView);
        prepareWebView();

        try {
            // Configure repositories
            mAppRepo = new AppInfoFileRepository(appInfoFile);
            mCredentialsRepo = new UserCredentialsFileRepository(credentialsFile);

            mAppInfo = (OAuth2AppInfo) mAppRepo.get(providerName, null);

            mProvider = StorageFacade.forProvider(providerName)
                    .setAppInfoRepository(mAppRepo, mAppInfo.getAppName())
                    .setUserCredentialsRepository(mCredentialsRepo, null)
                    .setHttpClient(AndroidHttpClient.newInstance("pcs-api-android-test", this))
                    .setForBootstrapping(true)
                    .build();

            mBootstrapper = new OAuth2Bootstrapper(mProvider);

            URI authorizeWebUrl = mBootstrapper.getAuthorizeBrowserUrl();

            mWebView.loadUrl(authorizeWebUrl.toString());

        } catch (IOException ex) {
            LOGGER.error("Error configuring PCS_API", ex);
            finish();
        }
    }

    /**
     * Adds a callback on the web view to catch the redirect URL
     */
    private void prepareWebView() {
        mWebView.getSettings().setJavaScriptEnabled(true);
        mWebView.setVisibility(View.VISIBLE);
        mWebView.setWebViewClient(new WebViewClient() {

            private boolean done = false;

            @Override
            public void onPageFinished(WebView view, String url) {
                // Catch the redirect url with the parameters added by the provider auth web page
                if (!done && url.startsWith(mAppInfo.getRedirectUrl())) {
                    done = true;
                    getCredentials(url);
                    mWebView.setVisibility(View.INVISIBLE);
                }
            }
        });
    }


    /**
     * Gets user credentials and save them
     * If OK, set result OK and come back to calling activity
     *
     * @param code The code used to retrieve the OAuth access token
     */
    private void getCredentials(final String code) {
        new AsyncTask<Void, Void, Boolean>() {
            @Override
            protected Boolean doInBackground(Void... params) {
                try {
                    mBootstrapper.getUserCredentials(code);
                    return true;

                } catch (IOException ex) {
                    LOGGER.error("Error getting oauth credentials", ex);
                    setResult(RESULT_CANCELED);
                    finish();

                } catch (CStorageException ex) {
                    LOGGER.error("Error getting oauth credentials", ex);
                    setResult(RESULT_CANCELED);
                    finish();
                }
                // Failure
                return false;
            }

            @Override
            protected void onPostExecute(Boolean result) {
                if(result) {
                    setResult(RESULT_OK);
                } else {
                    setResult(RESULT_CANCELED);
                }
                finish();
            }
        }.execute();
    }

}
