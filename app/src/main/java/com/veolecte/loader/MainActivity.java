package com.veolecte.loader;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.Button;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public class MainActivity extends Activity {

    private static final String GAME_PKG = "com.catsbit.oxidesurvivalisland";
    private static final String LIB_NAME = "liboxide_cheat.so";

    private TextView statusText;
    private Button btnLaunch;
    private final Handler handler = new Handler(Looper.getMainLooper());

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        statusText = findViewById(R.id.statusText);
        btnLaunch = findViewById(R.id.btnLaunch);

        btnLaunch.setOnClickListener(v -> launchCheat());
    }

    private void setStatus(String msg) {
        handler.post(() -> statusText.setText(msg));
    }

    private void launchCheat() {
        btnLaunch.setEnabled(false);
        setStatus("Подготовка...");

        new Thread(() -> {
            try {
                File libFile = new File(getFilesDir(), LIB_NAME);
                copyAsset(LIB_NAME, libFile);
                libFile.setReadable(true, false);
                libFile.setExecutable(true, false);
                setStatus("Библиотека подготовлена");

                System.load(libFile.getAbsolutePath());
                setStatus("Библиотека загружена");

                handler.post(() -> {
                    setStatus("Запуск Oxide...");
                    launchGame();
                });

            } catch (Exception e) {
                setStatus("Ошибка: " + e.getMessage());
                handler.post(() -> btnLaunch.setEnabled(true));
            }
        }).start();
    }

    private void launchGame() {
        try {
            PackageManager pm = getPackageManager();
            Intent intent = pm.getLaunchIntentForPackage(GAME_PKG);
            if (intent != null) {
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                startActivity(intent);
                setStatus("Игра запущена! Подожди 5-10 сек...");
                handler.postDelayed(() -> {
                    setStatus("Veolecte активен. Меню в игре.");
                    btnLaunch.setEnabled(true);
                }, 8000);
            } else {
                setStatus("Oxide не найден! Клонируй игру в LuluBox.");
                btnLaunch.setEnabled(true);
            }
        } catch (Exception e) {
            setStatus("Ошибка запуска: " + e.getMessage());
            btnLaunch.setEnabled(true);
        }
    }

    private void copyAsset(String name, File dest) throws Exception {
        try (InputStream is = getAssets().open(name);
             FileOutputStream fos = new FileOutputStream(dest)) {
            byte[] buf = new byte[8192];
            int len;
            while ((len = is.read(buf)) > 0) {
                fos.write(buf, 0, len);
            }
        }
    }
                  }
