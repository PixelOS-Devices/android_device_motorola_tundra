/*
 * SPDX-FileCopyrightText: 2026 The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.pixelos.tundra.parts

import android.app.Application

class TundraPartsApplication : Application() {

    override fun onCreate() {
        super.onCreate()
        TurboPowerLabel.start(this)
        CamcorderMic.start(this)
    }
}
