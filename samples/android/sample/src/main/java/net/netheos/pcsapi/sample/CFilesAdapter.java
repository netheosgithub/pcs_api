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

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;

import net.netheos.pcsapi.models.CFile;

import java.util.List;

/**
 *
 */
public class CFilesAdapter extends BaseAdapter {

    private final List<CFile> mFiles;
    private final Context mContext;

    public CFilesAdapter(List<CFile> files, Context context) {
        mFiles = files;
        mContext = context;
    }

    @Override
    public int getCount() {
        return mFiles.size();
    }

    @Override
    public CFile getItem(int position) {
        return mFiles.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup group) {
        if(convertView == null) {
            convertView = LayoutInflater.from(mContext).inflate(R.layout.item_file, group, false);
        }
        TextView textView = (TextView) convertView.findViewById(R.id.fileTextView);

        CFile file = mFiles.get(position);
        textView.setText(file.getPath().getPathName());
        if(file.isBlob()) {
            textView.setCompoundDrawablesWithIntrinsicBounds(mContext.getResources().getDrawable(R.drawable.ic_file), null, null, null);
        } else {
            textView.setCompoundDrawablesWithIntrinsicBounds(mContext.getResources().getDrawable(R.drawable.ic_folder), null, null, null);
        }
        return convertView;
    }
}
