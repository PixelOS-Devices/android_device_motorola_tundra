/*
 * SPDX-License-Identifier: Apache-2.0
 */

package com.motorola.android.checkin.provider;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** No-op stand-in for Motorola's check-in segment wrapper. */
public class CheckinSegmentWrapper {
    private final String mName;

    public CheckinSegmentWrapper(String name) {
        mName = name;
    }

    public Object getSegment() {
        return null;
    }

    public String getSegmentName() {
        return mName;
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
