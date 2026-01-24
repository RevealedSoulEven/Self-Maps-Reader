package com.example.selfmapsreader;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.core.content.FileProvider;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("native-lib");
    }

    private static final int PREVIEW_LIMIT = 200 * 1024;

    private TextView outputText;
    private ImageButton shareButton;

    private File activeFile;
    private String activeLabel;
    private boolean previewShown = false;

    public native String readProcSelfStatus();
    public native String readProcSelfMaps();
    public native String readProcSelfSmaps();
    public native String getLibArtHash();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES);
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        outputText = findViewById(R.id.outputText);
        shareButton = findViewById(R.id.shareButton);

        Button statusBtn = findViewById(R.id.readButton);
        Button mapsBtn   = findViewById(R.id.mapsButton);
        Button smapsBtn  = findViewById(R.id.smapsButton);
        Button hashBtn   = findViewById(R.id.hashButton);

        shareButton.setVisibility(View.GONE);

        statusBtn.setOnClickListener(v ->
                handleText("status", readProcSelfStatus()));

        mapsBtn.setOnClickListener(v ->
                handleText("maps", readProcSelfMaps()));

        smapsBtn.setOnClickListener(v ->
                handleText("smaps", readProcSelfSmaps()));

        hashBtn.setOnClickListener(v ->
                handleText("native libs hash", getLibArtHash()));

        shareButton.setOnClickListener(v -> {
            if (activeFile != null)
                shareFile(activeFile);
        });
    }

    private void handleText(String label, String text) {
        previewShown = false;
        activeLabel = label;

        try {
            activeFile = new File(getFilesDir(), "self_" + label + ".txt");
            BufferedWriter w = new BufferedWriter(new FileWriter(activeFile, false));
            w.write(text);
            w.close();
        } catch (Exception e) {
            outputText.setText("Save failed: " + e.getMessage());
            return;
        }

        int size = text.length();

        if (size <= PREVIEW_LIMIT) {
            outputText.setText(text);
        } else {
            outputText.setText(
                    "/proc/self/" + label + "\n\n" +
                    "Size: " + (size / 1024) + " KB\n\n" +
                    "Tap again to preview\n" +
                    "Tap share to export full file"
            );

            outputText.setOnClickListener(v -> {
                if (!previewShown) {
                    previewShown = true;
                    showPreview();
                }
            });
        }

        shareButton.setVisibility(View.VISIBLE);
    }

    private void showPreview() {
        StringBuilder sb = new StringBuilder();

        try (BufferedReader r = new BufferedReader(new FileReader(activeFile))) {
            int c;
            while ((c = r.read()) != -1 && sb.length() < PREVIEW_LIMIT) {
                sb.append((char) c);
            }
        } catch (Exception e) {
            outputText.setText("Preview failed: " + e.getMessage());
            return;
        }

        sb.append("\n\n[TRUNCATED – full file available via Share]");
        outputText.setText(sb.toString());
    }

    private void shareFile(File file) {
        Uri uri = FileProvider.getUriForFile(
                this,
                getPackageName() + ".provider",
                file
        );

        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_STREAM, uri);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivity(Intent.createChooser(intent, "Share output"));
    }
}
