/*
 * SPDX-FileCopyrightText: 2026 The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.pixelos.tundra.parts

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.net.Uri
import android.os.Bundle

/** Supplies the dynamic summary of the tile injected into Settings > Display. */
class SummaryProvider : ContentProvider() {

    override fun onCreate() = true

    override fun call(method: String, arg: String?, extras: Bundle?): Bundle? {
        if (method != METHOD_GET_DYNAMIC_SUMMARY) return null
        val context = context ?: return null
        val rate = LowLightRefreshRate.get(context)
        return Bundle().apply {
            putString(META_DATA_PREFERENCE_SUMMARY, context.getString(R.string.refresh_rate_hz, rate))
        }
    }

    override fun query(
        uri: Uri,
        projection: Array<String>?,
        selection: String?,
        selectionArgs: Array<String>?,
        sortOrder: String?,
    ): Cursor? = null

    override fun getType(uri: Uri): String? = null

    override fun insert(uri: Uri, values: ContentValues?): Uri? = null

    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<String>?) = 0

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        selectionArgs: Array<String>?,
    ) = 0

    companion object {
        val SUMMARY_URI: Uri =
            Uri.parse(
                "content://org.pixelos.tundra.parts.summary/getDynamicSummary/low_light_refresh_rate"
            )

        private const val METHOD_GET_DYNAMIC_SUMMARY = "getDynamicSummary"
        private const val META_DATA_PREFERENCE_SUMMARY = "com.android.settings.summary"
    }
}
