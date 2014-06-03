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
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;


public class MenuActivity extends Activity implements View.OnClickListener
{
    private Button mBtDropbox;
    private Button mBtGoogleDrive;
    private Button mBtHubic;
    private Button mBtCloudMe;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_menu);

        mBtDropbox = (Button) findViewById(R.id.btDropbox);
        mBtDropbox.setOnClickListener(this);

        mBtGoogleDrive = (Button) findViewById(R.id.btGoogleDrive);
        mBtGoogleDrive.setOnClickListener(this);

        mBtHubic = (Button) findViewById(R.id.btHubic);
        mBtHubic.setOnClickListener(this);

        mBtCloudMe = (Button) findViewById(R.id.btCloudMe);
        mBtCloudMe.setOnClickListener(this);
    }

    @Override
    public void onClick(View v)
    {
        Intent i = new Intent(this, MainActivity.class);
        i.putExtra(MainActivity.EXTRA_PROVIDER_NAME, (String) v.getTag());
        startActivity(i);
    }
}
