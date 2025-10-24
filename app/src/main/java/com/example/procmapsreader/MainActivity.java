package com.example.selfmapsreader;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;
import androidx.core.content.FileProvider;

import java.io.*;

public class MainActivity extends Activity {
    private TextView outputText;
    private File outputFile;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        outputText = findViewById(R.id.outputText);
        Button readButton = findViewById(R.id.readButton);
        Button shareButton = findViewById(R.id.shareButton);

        readButton.setOnClickListener(v -> readProcMaps());
        shareButton.setOnClickListener(v -> shareFile());
    }

    private void readProcMaps() {
        try {
            outputFile = new File(getFilesDir(), "self_maps.txt");
            BufferedReader reader = new BufferedReader(new FileReader("/proc/self/maps"));
            BufferedWriter writer = new BufferedWriter(new FileWriter(outputFile));

            StringBuilder builder = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                builder.append(line).append("\n");
                writer.write(line);
                writer.newLine();
            }
            reader.close();
            writer.close();
            outputText.setText(builder.toString());
        } catch (Exception e) {
            outputText.setText("Error: " + e.getMessage());
        }
    }

    private void shareFile() {
        if (outputFile == null || !outputFile.exists()) return;

        Uri uri = FileProvider.getUriForFile(this,
                getPackageName() + ".provider", outputFile);

        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_STREAM, uri);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivity(Intent.createChooser(intent, "Share file"));
    }
}
