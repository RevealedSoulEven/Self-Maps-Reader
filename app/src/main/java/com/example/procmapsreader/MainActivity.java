package com.example.selfmapsreader;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Button;
import android.widget.ScrollView;
import android.widget.TextView;
import android.view.View;
import android.content.Intent;
import android.net.Uri;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.core.content.FileProvider;

import java.io.File;

public class MainActivity extends Activity {

    static {
        System.loadLibrary("native-lib");
    }

    private TextView outputText;
    private File statusFile;
    private File libartHashFile;

    // Native methods
    public native String readProcSelfStatus();
    public native String getLibArtHash();
    public native boolean exportProcSelfStatus(String path);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        outputText = findViewById(R.id.outputText);
        Button readStatusBtn = findViewById(R.id.readButton);
        Button exportBtn = findViewById(R.id.shareButton);
        Button hashBtn = findViewById(R.id.hashButton);
        Button toggleThemeBtn = findViewById(R.id.toggleThemeButton);

        // Toggle theme (light/dark)
        toggleThemeBtn.setOnClickListener(v -> {
            int mode = AppCompatDelegate.getDefaultNightMode();
            if (mode == AppCompatDelegate.MODE_NIGHT_YES)
                AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO);
            else
                AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES);
        });

        readStatusBtn.setOnClickListener(v -> {
            String text = readProcSelfStatus();
            outputText.setText(text);
        });

        exportBtn.setOnClickListener(v -> {
            File outFile = new File(getFilesDir(), "proc_self_status.txt");
            boolean ok = exportProcSelfStatus(outFile.getAbsolutePath());
            if (ok) {
                shareFile(outFile);
            } else {
                outputText.setText("Export failed");
            }
        });

        hashBtn.setOnClickListener(v -> {
            String hash = getLibArtHash();
            outputText.setText("libart.so checksum:\n" + hash);
        });
    }

    private void shareFile(File file) {
        Uri uri = FileProvider.getUriForFile(this, getPackageName() + ".provider", file);
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_STREAM, uri);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivity(Intent.createChooser(intent, "Share file"));
    }
}
