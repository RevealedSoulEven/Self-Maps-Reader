package com.example.selfmapsreader;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;
import android.widget.ScrollView;
import android.content.Intent;
import android.net.Uri;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.core.content.FileProvider;

import java.io.*;

public class MainActivity extends Activity {

    static {
        System.loadLibrary("native-lib");
    }

    private TextView outputText;
    private File outputFile;

    // Native methods
    public native String readProcSelfStatus();
    public native String getLibArtHash();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        outputText = findViewById(R.id.outputText);
        Button readButton = findViewById(R.id.readButton);
        Button exportButton = findViewById(R.id.shareButton);
        Button hashButton = findViewById(R.id.hashButton);
        Button themeButton = findViewById(R.id.toggleThemeButton);

        // Toggle light/dark mode
        themeButton.setOnClickListener(v -> {
            int mode = AppCompatDelegate.getDefaultNightMode();
            if (mode == AppCompatDelegate.MODE_NIGHT_YES)
                AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_NO);
            else
                AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES);
        });

        readButton.setOnClickListener(v -> {
            String result = readProcSelfStatus();
            outputText.setText(result);
        });

        exportButton.setOnClickListener(v -> exportProcStatus());

        hashButton.setOnClickListener(v -> {
            String hash = getLibArtHash();
            outputText.setText("libart.so SHA256:\n" + hash);
        });
    }

    private void exportProcStatus() {
        try {
            outputFile = new File(getFilesDir(), "proc_self_status.txt");
            BufferedReader reader = new BufferedReader(new FileReader("/proc/self/status"));
            BufferedWriter writer = new BufferedWriter(new FileWriter(outputFile));

            String line;
            while ((line = reader.readLine()) != null) {
                writer.write(line);
                writer.newLine();
            }
            reader.close();
            writer.close();

            shareFile(outputFile);
        } catch (Exception e) {
            outputText.setText("Error exporting: " + e.getMessage());
        }
    }

    private void shareFile(File file) {
        Uri uri = FileProvider.getUriForFile(this,
                getPackageName() + ".provider", file);

        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_STREAM, uri);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivity(Intent.createChooser(intent, "Share file"));
    }
}
