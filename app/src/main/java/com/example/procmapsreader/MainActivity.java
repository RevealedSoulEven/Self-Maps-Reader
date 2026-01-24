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

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("native-lib");
    }

    private static final int PREVIEW_LIMIT = 200 * 1024;

    private TextView outputText;
    private ImageButton shareButton;

    private File activeFile;

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
                handleProcText("status", readProcSelfStatus()));

        mapsBtn.setOnClickListener(v ->
                handleProcText("maps", readProcSelfMaps()));

        smapsBtn.setOnClickListener(v ->
                handleProcText("smaps", readProcSelfSmaps()));

        hashBtn.setOnClickListener(v -> {
            activeFile = null;
            outputText.setText(getLibArtHash());
            shareButton.setVisibility(View.GONE);
        });

        shareButton.setOnClickListener(v -> {
            if (activeFile != null) {
                shareFile(activeFile);
            }
        });
    }

    private void handleProcText(String name, String text) {
        try {
            activeFile = new File(getFilesDir(), "self_" + name + ".txt");
            BufferedWriter w = new BufferedWriter(new FileWriter(activeFile, false));
            w.write(text);
            w.close();
        } catch (Exception e) {
            outputText.setText("Save failed: " + e.getMessage());
            shareButton.setVisibility(View.GONE);
            return;
        }

        if (text.length() > PREVIEW_LIMIT) {
            outputText.setText(
                    text.substring(0, PREVIEW_LIMIT) +
                    "\n\n[TRUNCATED – full file available via Share]"
            );
        } else {
            outputText.setText(text);
        }

        shareButton.setVisibility(View.VISIBLE);
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
