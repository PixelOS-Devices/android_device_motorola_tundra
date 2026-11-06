/*
 * SPDX-FileCopyrightText: The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.pixelos.flingboost;

import android.app.ActivityManager.RunningTaskInfo;
import android.app.ActivityTaskManager;
import android.app.TaskStackListener;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.BatteryManager;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemProperties;
import android.util.Log;

import java.util.List;

/**
 * Runs thermal-engine on Motorola's game-perf profile while a game or a
 * benchmark is in the foreground, on our cooler charging profile while plugged
 * in, and on its default profile otherwise. A game wins over charging.
 *
 * Each change restarts thermal-engine, so leaving a game only goes back after
 * GAME_HOLD_MS, in case the game comes straight back, and the power state only
 * counts once it has held for POWER_HOLD_MS, in case of a loose cable.
 *
 * system_app can't set the vendor.thermal.mode prop itself; init.qcom.rc maps
 * this prop to it.
 */
final class ThermalModeController {
    private static final String TAG = "ThermalMode";

    private static final String PROP = "sys.tundra.thermal_mode";
    private static final String MODE_GAME = "game";
    private static final String MODE_CHARGING = "charging";
    private static final String MODE_DEFAULT = "default";

    private static final long GAME_HOLD_MS = 45_000;
    private static final long POWER_HOLD_MS = 30_000;

    // Benchmarks don't all declare the game category
    private static final String[] BENCHMARK_PREFIXES = {
        "com.antutu.",
        "com.futuremark.",
        "com.primatelabs.geekbench",
        "net.kishonti.gfxbench",
    };

    private final Context mContext;
    private final PackageManager mPackageManager;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private String mMode;
    private boolean mInGame;
    private boolean mPluggedIn;
    private boolean mCharging;

    private final Runnable mLeaveGame = () -> {
        mInGame = false;
        applyMode();
    };
    private final Runnable mApplyPower = () -> {
        mCharging = mPluggedIn;
        applyMode();
    };

    private final TaskStackListener mListener = new TaskStackListener() {
        @Override
        public void onTaskMovedToFront(RunningTaskInfo taskInfo) {
            mHandler.post(() -> onForegroundTask(taskInfo));
        }
    };

    private final BroadcastReceiver mPowerReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            mPluggedIn = Intent.ACTION_POWER_CONNECTED.equals(intent.getAction());
            mHandler.removeCallbacks(mApplyPower);
            mHandler.postDelayed(mApplyPower, POWER_HOLD_MS);
        }
    };

    ThermalModeController(Context context) {
        mContext = context;
        mPackageManager = context.getPackageManager();
    }

    void start() {
        // The prop outlives this process if it crashes
        String mode = SystemProperties.get(PROP);
        mMode = mode.isEmpty() ? MODE_DEFAULT : mode;
        mInGame = MODE_GAME.equals(mMode);

        IntentFilter filter = new IntentFilter(Intent.ACTION_POWER_CONNECTED);
        filter.addAction(Intent.ACTION_POWER_DISCONNECTED);
        mContext.registerReceiver(mPowerReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
        Intent battery = mContext.registerReceiver(null,
                new IntentFilter(Intent.ACTION_BATTERY_CHANGED), Context.RECEIVER_NOT_EXPORTED);
        mPluggedIn = battery != null
                && battery.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0) != 0;
        mCharging = mPluggedIn;

        ActivityTaskManager atm = ActivityTaskManager.getInstance();
        atm.registerTaskStackListener(mListener);

        List<RunningTaskInfo> tasks = atm.getTasks(1);
        if (!tasks.isEmpty()) {
            onForegroundTask(tasks.get(0));
        } else {
            mInGame = false;
        }
        applyMode();
    }

    private void onForegroundTask(RunningTaskInfo taskInfo) {
        if (isGameOrBenchmark(taskInfo)) {
            mHandler.removeCallbacks(mLeaveGame);
            mInGame = true;
            applyMode();
        } else if (mInGame && !mHandler.hasCallbacks(mLeaveGame)) {
            mHandler.postDelayed(mLeaveGame, GAME_HOLD_MS);
        }
    }

    private boolean isGameOrBenchmark(RunningTaskInfo taskInfo) {
        ComponentName component =
                taskInfo.baseActivity != null ? taskInfo.baseActivity : taskInfo.topActivity;
        if (component == null) {
            return false;
        }
        String packageName = component.getPackageName();
        for (String prefix : BENCHMARK_PREFIXES) {
            if (packageName.startsWith(prefix)) {
                return true;
            }
        }
        try {
            ApplicationInfo info =
                    mPackageManager.getApplicationInfoAsUser(packageName, 0, taskInfo.userId);
            return info.category == ApplicationInfo.CATEGORY_GAME
                    || (info.flags & ApplicationInfo.FLAG_IS_GAME) != 0;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    private void applyMode() {
        String mode = mInGame ? MODE_GAME : mCharging ? MODE_CHARGING : MODE_DEFAULT;
        if (mode.equals(mMode)) {
            return;
        }
        mMode = mode;
        Log.i(TAG, "Thermal profile: " + mode);
        SystemProperties.set(PROP, mode);
    }
}
