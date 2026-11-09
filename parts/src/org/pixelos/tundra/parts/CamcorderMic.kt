/*
 * SPDX-FileCopyrightText: 2026 The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.pixelos.tundra.parts

import android.content.Context
import android.hardware.display.DisplayManager
import android.media.AudioManager
import android.media.AudioRecordingConfiguration
import android.media.MediaRecorder
import android.view.Display
import android.view.Surface

/**
 * Records camera audio with the stock camcorder mics and calibration. The CAF HAL picks its
 * stereo camcorder device (mapped to camcorder-mic-bf, ACDB 50) only for back camera landscape,
 * while ro.audio.monitorRotation feeds it the display rotation, which camera apps keep in
 * portrait. Report landscape while a camcorder stream runs; setting cameraFacing makes the HAL
 * reroute streams that are already running.
 */
object CamcorderMic : AudioManager.AudioRecordingCallback() {
    private lateinit var audioManager: AudioManager
    private lateinit var displayManager: DisplayManager
    private var active = false

    fun start(context: Context) {
        audioManager = context.getSystemService(AudioManager::class.java)
        displayManager = context.getSystemService(DisplayManager::class.java)
        audioManager.registerAudioRecordingCallback(this, null)
        onRecordingConfigChanged(audioManager.activeRecordingConfigurations)
    }

    override fun onRecordingConfigChanged(configs: List<AudioRecordingConfiguration>) {
        val camcorder = configs.any { it.clientAudioSource == MediaRecorder.AudioSource.CAMCORDER }
        if (camcorder == active) return
        active = camcorder
        val rotation = if (camcorder) 90 else displayRotation()
        audioManager.setParameters("rotation=$rotation;cameraFacing=back")
    }

    private fun displayRotation() =
        when (displayManager.getDisplay(Display.DEFAULT_DISPLAY).rotation) {
            Surface.ROTATION_90 -> 90
            Surface.ROTATION_180 -> 180
            Surface.ROTATION_270 -> 270
            else -> 0
        }
}
