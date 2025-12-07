package com.example.selfmapsreader;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.widget.Button;
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
    private File outputFile;

    // Native methods (C side)
    public native String readProcSelfStatus();
    public native String readProcSelfMaps();
    public native String getLibArtHash();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Force app to always use dark mode
        AppCompatDelegate.setDefaultNightMode(AppCompatDelegate.MODE_NIGHT_YES);

        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        outputText = findViewById(R.id.outputText);
        Button readButton = findViewById(R.id.readButton);
        Button mapsButton = findViewById(R.id.mapsButton);
        Button hashButton = findViewById(R.id.hashButton);
        Button shareButton = findViewById(R.id.shareButton);

        // Read /proc/self/status (native)
        readButton.setOnClickListener(v -> {
            String result = readProcSelfStatus();
            outputText.setText(result);
        });

        // Read /proc/self/maps (native)
        mapsButton.setOnClickListener(v -> {
            String result = readProcSelfMaps();
            outputText.setText(result);
        });

        // Get libart.so hash (native)
        hashButton.setOnClickListener(v -> {
            String hashResult = getLibArtHash();
            outputText.setText(hashResult);
        });

        // Export whatever is shown in the TextView
        shareButton.setOnClickListener(v -> exportCurrentOutput());
    }

    private void exportCurrentOutput() {
        try {
            String text = outputText.getText().toString();
            if (text == null || text.isEmpty()) {
                outputText.setText("Nothing to export yet.");
                return;
            }

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
