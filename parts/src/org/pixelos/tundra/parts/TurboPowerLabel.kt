/*
 * SPDX-FileCopyrightText: 2026 The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.pixelos.tundra.parts

import android.content.Context
import android.content.om.OverlayManager
import android.os.UEventObserver
import android.os.UserHandle
import android.util.Log

/**
 * Shows the TurboPower 68W charging label the way stock does: Moto's SystemUI shows
 * "68W" when the mmi_charger POWER_SUPPLY_POWER_WATT uevent on the battery supply is 68.
 */
object TurboPowerLabel : UEventObserver() {
    private const val TAG = "TurboPowerLabel"
    private const val STOCK_CHARGER_WATT = "68"
    private val OVERLAYS = listOf(
        "org.pixelos.tundra.turbopower.systemui",
        "org.pixelos.tundra.turbopower.settings",
    )

    private lateinit var overlayManager: OverlayManager
    private var shown: Boolean? = null

    fun start(context: Context) {
        overlayManager = context.getSystemService(OverlayManager::class.java)
        // Overlay state survives reboots; the boot-time state_sync resends the wattage.
        setShown(false)
        startObserving("POWER_SUPPLY_NAME=battery")
    }

    override fun onUEvent(event: UEvent) {
        event.get("POWER_SUPPLY_POWER_WATT")?.let { setShown(it == STOCK_CHARGER_WATT) }
    }

    @Synchronized
    private fun setShown(show: Boolean) {
        if (show == shown) return
        shown = show
        OVERLAYS.forEach {
            try {
                overlayManager.setEnabled(it, show, UserHandle.CURRENT)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to set $it to $show", e)
            }
        }
    }
}
