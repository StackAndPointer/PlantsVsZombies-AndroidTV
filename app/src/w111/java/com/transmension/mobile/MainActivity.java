/*
 * Copyright (C) 2023-2026  PvZ TV Touch Team
 *
 * This file is part of PlantsVsZombies-AndroidTV.
 *
 * PlantsVsZombies-AndroidTV is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * PlantsVsZombies-AndroidTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * PlantsVsZombies-AndroidTV.  If not, see <https://www.gnu.org/licenses/>.
 */

package com.transmension.mobile;

import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.WindowManager;

public class MainActivity extends NativeActivity {
    private static final String TAG = "MainActivity";
    private final AudioOutput mAudioOutput = new AudioOutput(this);
    private InputManager mInputManager;
    private String mInputManagerFactoryName = InputManagerFactory.class.getName();

    public void hideSystemNagvigationBar() {
        getWindow().getDecorView().setSystemUiVisibility(2);
    }


    @Override // com.transmension.mobile.NativeActivity, android.app.Activity
    public void onDestroy() {
        if (this.mInputManager != null) {
            this.mInputManager.onDestroy();
        }
        super.onDestroy();
    }

    @Override // com.transmension.mobile.NativeActivity, android.app.Activity
    public void onStart() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        super.onStart();
    }

    @Override // com.transmension.mobile.NativeActivity, android.app.Activity
    public void onStop() {
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        super.onStop();
    }

    public AudioOutput getAudioOutput() {
        return this.mAudioOutput;
    }

    public String getInputManagerFactoryName() {
        return this.mInputManagerFactoryName;
    }

    public void setInputManagerFactoryName(String name) {
        this.mInputManagerFactoryName = name;
    }

    public InputManager createInputManager() {
        if (this.mInputManager != null) {
            return this.mInputManager;
        }
        try {
            Class<?> clazz = Class.forName(this.mInputManagerFactoryName);
            try {
                InputManagerFactory factory = (InputManagerFactory) clazz.newInstance();
                this.mInputManager = factory.create(this);
                Log.i(TAG, "InputManager: " + this.mInputManager.getName());
                onInputManagerCreated();
                return this.mInputManager;
            } catch (IllegalAccessException | InstantiationException e) {
                e.printStackTrace();
                return null;
            }
        } catch (ClassNotFoundException e3) {
            e3.printStackTrace();
            return null;
        }
    }

    public boolean hasInputManager() {
        return this.mInputManager != null;
    }

    public InputManager getInputManager() {
        return this.mInputManager;
    }

    public void onInputManagerCreated() {
    }


    @Override // com.transmension.mobile.NativeActivity
    public void onNativeKeyEvent(KeyEvent event) {
        if (this.mInputManager == null) {
            createInputManager();
        }
        if (this.mInputManager == null) {
            super.onNativeKeyEvent(event);
        } else {
            this.mInputManager.onKeyEvent(event);
        }
    }

    @Override // com.transmension.mobile.NativeActivity
    public void onNativeMotionEvent(MotionEvent event) {
        if (this.mInputManager == null) {
            createInputManager();
        }
        if (this.mInputManager == null) {
            super.onNativeMotionEvent(event);
        } else {
            this.mInputManager.onMotionEvent(event);
        }
    }

}