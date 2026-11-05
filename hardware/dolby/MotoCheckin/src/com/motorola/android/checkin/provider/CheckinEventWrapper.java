/*
 * SPDX-License-Identifier: Apache-2.0
 */

package com.motorola.android.checkin.provider;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** No-op stand-in for Motorola's check-in event wrapper. */
public class CheckinEventWrapper {
    private final String mTag;
    private final String mEventName;
    private final String mVersion;
    private final long mTimestamp;

    public CheckinEventWrapper(String tag, String eventName, String version) {
        this(tag, eventName, version, System.currentTimeMillis());
    }

    public CheckinEventWrapper(String tag, String eventName, String version, long timestamp) {
        mTag = tag;
        mEventName = eventName;
        mVersion = version;
        mTimestamp = timestamp;
    }

    public void addSegment(CheckinSegmentWrapper segment) {}

    public void publish(Object contentResolver) {}

    public void setHeaderAttribute(String key, long value) {}
    public void setHeaderAttribute(String key, String value) {}

    public String getApkName() {
        return null;
    }

    public String getApkVerCode() {
        return null;
    }

    public String getEventName() {
        return mEventName;
    }

    public Map getHeaderAttributes() {
        return Collections.emptyMap();
    }

    public List getSegments() {
        return Collections.emptyList();
    }

    public String getTagName() {
        return mTag;
    }

    public long getTimestamp() {
        return mTimestamp;
    }

    public long getTimezoneOffset() {
        return 0;
    }

    public String getUserid() {
        return null;
    }

    public String getVersion() {
        return mVersion;
    }

    public void setValue(int index, double value) {}
    public void setValue(int index, int value) {}
    public void setValue(int index, long value) {}
    public void setValue(int index, String value) {}
    public void setValue(int index, ArrayList value) {}
    public void setValue(int index, HashMap value) {}
    public void setValue(int index, boolean value) {}
    public void setValue(String key, double value) {}
    public void setValue(String key, int value) {}
    public void setValue(String key, long value) {}
    public void setValue(String key, String value) {}
    public void setValue(String key, ArrayList value) {}
    public void setValue(String key, HashMap value) {}
    public void setValue(String key, boolean value) {}

    public Map getNVAttributes() {
        return Collections.emptyMap();
    }

    public List getPositionBasedAttributes() {
        return Collections.emptyList();
    }
}
