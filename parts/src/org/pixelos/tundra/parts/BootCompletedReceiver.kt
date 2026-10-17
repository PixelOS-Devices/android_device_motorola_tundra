/*
 * SPDX-FileCopyrightText: 2026 The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.pixelos.tundra.parts

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

class BootCompletedReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        LowLightRefreshRate.restore(context)
    }
}
