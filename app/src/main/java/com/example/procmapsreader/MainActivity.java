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

    private TextView outputText;
    private ImageButton shareButton;

    private File outputFile;
    private File smapsFile;
    private boolean smapsActive = false;

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

        Button readButton = findViewById(R.id.readButton);
        Button mapsButton = findViewById(R.id.mapsButton);
        Button smapsButton = findViewById(R.id.smapsButton);
        Button hashButton = findViewById(R.id.hashButton);

        shareButton.setVisibility(View.GONE);

        readButton.setOnClickListener(v -> {
            smapsActive = false;
            outputText.setText(readProcSelfStatus());
            shareButton.setVisibility(View.VISIBLE);
        });

        mapsButton.setOnClickListener(v -> {
            smapsActive = false;
            outputText.setText(readProcSelfMaps());
            shareButton.setVisibility(View.VISIBLE);
        });

        smapsButton.setOnClickListener(v -> {
            smapsActive = true;
            shareButton.setVisibility(View.GONE);
            outputText.setText("Reading /proc/self/smaps…");

            new Thread(() -> {
                String smaps = readProcSelfSmaps();

                try {
                    smapsFile = new File(getFilesDir(), "self_smaps.txt");
                    BufferedWriter w = new BufferedWriter(new FileWriter(smapsFile, false));
                    w.write(smaps);
                    w.close();
                } catch (Exception e) {
                    runOnUiThread(() ->
                        outputText.setText("Failed to save smaps: " + e.getMessage())
                    );
                    return;
                }

                int sizeKb = smaps.length() / 1024;

                runOnUiThread(() -> {
                    outputText.setText(
                        "/proc/self/smaps\n\nSize: " + sizeKb + " KB\n\nTap share to export full file"
                    );
                    shareButton.setVisibility(View.VISIBLE);
                });
            }).start();
        });

        hashButton.setOnClickListener(v -> {
            smapsActive = false;
            outputText.setText(getLibArtHash());
            shareButton.setVisibility(View.VISIBLE);
        });

        shareButton.setOnClickListener(v -> {
            if (smapsActive && smapsFile != null) {
                shareFile(smapsFile);
            } else {
                exportCurrentOutput();
            }
        });
    }

    private void exportCurrentOutput() {
        try {
            String text = outputText.getText().toString();
            if (text == null || text.isEmpty())
                return;

            outputFile = new File(getFilesDir(), "self_reader_output.txt");
            BufferedWriter writer = new BufferedWriter(new FileWriter(outputFile, false));
            writer.write(text);
            writer.close();

            shareFile(outputFile);
        } catch (Exception e) {
            outputText.setText("Error exporting: " + e.getMessage());
        }
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
