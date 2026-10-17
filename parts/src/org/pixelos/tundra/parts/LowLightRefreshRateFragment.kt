/*
 * SPDX-FileCopyrightText: 2026 The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.pixelos.tundra.parts

import android.os.Bundle
import com.android.settingslib.widget.SelectorWithWidgetPreference
import com.android.settingslib.widget.SettingsBasePreferenceFragment

class LowLightRefreshRateFragment :
    SettingsBasePreferenceFragment(), SelectorWithWidgetPreference.OnClickListener {

    private val ratePreferences = mutableListOf<SelectorWithWidgetPreference>()

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.low_light_refresh_rate, rootKey)

        val current = LowLightRefreshRate.get(requireContext())
        for (rate in LowLightRefreshRate.RATES) {
            val preference =
                SelectorWithWidgetPreference(requireContext()).apply {
                    key = rate.toString()
                    title = getString(R.string.refresh_rate_hz, rate)
                    isPersistent = false
                    isChecked = rate == current
                    setOnClickListener(this@LowLightRefreshRateFragment)
                }
            ratePreferences.add(preference)
            preferenceScreen.addPreference(preference)
        }
    }

    override fun onRadioButtonClicked(emiter: SelectorWithWidgetPreference) {
        LowLightRefreshRate.set(requireContext(), emiter.key.toInt())
        ratePreferences.forEach { it.isChecked = it == emiter }
    }
}
