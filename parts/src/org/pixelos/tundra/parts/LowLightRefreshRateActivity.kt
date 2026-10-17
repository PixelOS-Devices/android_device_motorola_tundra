/*
 * SPDX-FileCopyrightText: 2026 The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.pixelos.tundra.parts

import android.os.Bundle
import com.android.settingslib.collapsingtoolbar.CollapsingToolbarBaseActivity
import com.android.settingslib.collapsingtoolbar.R

class LowLightRefreshRateActivity : CollapsingToolbarBaseActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        supportFragmentManager
            .beginTransaction()
            .replace(R.id.content_frame, LowLightRefreshRateFragment(), TAG)
            .commit()
    }

    companion object {
        private const val TAG = "low_light_refresh_rate"
    }
}
