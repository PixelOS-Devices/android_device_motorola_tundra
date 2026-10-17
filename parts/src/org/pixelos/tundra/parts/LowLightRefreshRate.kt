/*
 * SPDX-FileCopyrightText: 2026 The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.pixelos.tundra.parts

import android.content.Context
import android.content.res.Resources
import android.hardware.display.DisplayManager
import android.provider.DeviceConfig
import android.util.Log

/**
 * Refresh rate DisplayModeDirector locks the panel to inside the low brightness zone. The choice
 * is kept in our preferences and written to the display_manager DeviceConfig override, which
 * DisplayModeDirector applies at runtime. It is written again on boot in case DeviceConfig was
 * reset.
 */
object LowLightRefreshRate {
    private const val TAG = "LowLightRefreshRate"
    private const val PREF_KEY = "low_light_refresh_rate"

    val RATES = intArrayOf(60, 90, 120, 144)

    fun get(context: Context): Int =
        prefs(context).getInt(PREF_KEY, -1).takeIf { it > 0 } ?: defaultRate()

    fun set(context: Context, rate: Int) {
        prefs(context).edit().putInt(PREF_KEY, rate).apply()
        write(rate)
        context.contentResolver.notifyChange(SummaryProvider.SUMMARY_URI, null)
    }

    fun restore(context: Context) {
        val rate = prefs(context).getInt(PREF_KEY, -1)
        if (rate > 0 && read() != rate) {
            write(rate)
        }
    }

    private fun defaultRate() =
        Resources.getSystem().getInteger(com.android.internal.R.integer.config_defaultRefreshRateInZone)

    private fun read() =
        DeviceConfig.getInt(
            DeviceConfig.NAMESPACE_DISPLAY_MANAGER,
            DisplayManager.DeviceConfig.KEY_REFRESH_RATE_IN_LOW_ZONE,
            -1,
        )

    private fun write(rate: Int) {
        Log.i(TAG, "Setting low light refresh rate to $rate Hz")
        DeviceConfig.setProperty(
            DeviceConfig.NAMESPACE_DISPLAY_MANAGER,
            DisplayManager.DeviceConfig.KEY_REFRESH_RATE_IN_LOW_ZONE,
            rate.toString(),
            /* makeDefault= */ false,
        )
    }

    private fun prefs(context: Context) =
        context.createDeviceProtectedStorageContext()
            .getSharedPreferences("settings", Context.MODE_PRIVATE)
}
