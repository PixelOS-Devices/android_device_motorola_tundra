/*
 * SPDX-FileCopyrightText: The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.pixelos.flingboost;

import android.app.ActivityManager.RunningTaskInfo;
import android.app.ActivityTaskManager;
import android.app.TaskStackListener;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemProperties;
import android.util.Log;

import java.util.List;

/**
 * Runs thermal-engine on Motorola's game-perf profile while a game or a
 * benchmark is in the foreground, and on its default profile otherwise.
 *
 * Each change restarts thermal-engine, so leaving a game only goes back to the
 * default profile after HOLD_MS, in case the game comes straight back.
 *
 * system_app can't set the vendor.thermal.mode prop itself; init.qcom.rc maps
 * this prop to it.
 */
final class ThermalModeController {
    private static final String TAG = "ThermalMode";

    private static final String PROP = "sys.tundra.thermal_mode";
    private static final String MODE_GAME = "game";
    private static final String MODE_DEFAULT = "default";

    private static final long HOLD_MS = 45_000;

    // Benchmarks don't all declare the game category
    private static final String[] BENCHMARK_PREFIXES = {
        "com.antutu.",
        "com.futuremark.",
        "com.primatelabs.geekbench",
        "net.kishonti.gfxbench",
    };

    private final PackageManager mPackageManager;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Runnable mLeaveGame = () -> setGameMode(false);

    private boolean mGameMode;

    private final TaskStackListener mListener = new TaskStackListener() {
        @Override
        public void onTaskMovedToFront(RunningTaskInfo taskInfo) {
            mHandler.post(() -> onForegroundTask(taskInfo));
        }
    };

    ThermalModeController(Context context) {
        mPackageManager = context.getPackageManager();
    }

    void start() {
        // The prop outlives this process if it crashes
        mGameMode = MODE_GAME.equals(SystemProperties.get(PROP));

        ActivityTaskManager atm = ActivityTaskManager.getInstance();
        atm.registerTaskStackListener(mListener);

        List<RunningTaskInfo> tasks = atm.getTasks(1);
        if (!tasks.isEmpty()) {
            onForegroundTask(tasks.get(0));
        } else if (mGameMode) {
            setGameMode(false);
        }
    }

    private void onForegroundTask(RunningTaskInfo taskInfo) {
        if (isGameOrBenchmark(taskInfo)) {
            mHandler.removeCallbacks(mLeaveGame);
            setGameMode(true);
        } else if (mGameMode && !mHandler.hasCallbacks(mLeaveGame)) {
            mHandler.postDelayed(mLeaveGame, HOLD_MS);
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

    private void setGameMode(boolean gameMode) {
        if (mGameMode == gameMode) {
            return;
        }
        mGameMode = gameMode;
        Log.i(TAG, gameMode ? "Game in the foreground: game-perf" : "Back to the default profile");
        SystemProperties.set(PROP, gameMode ? MODE_GAME : MODE_DEFAULT);
    }
}
