package com.example.procmapsreader

import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {

    // load native lib
    companion object {
        init {
            System.loadLibrary("native-lib")
        }
    }

    // native function declaration
    external fun readMaps(): String

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val tv = findViewById<TextView>(R.id.mapsText)
        val btn = findViewById<Button>(R.id.readBtn)
        val share = findViewById<Button>(R.id.shareBtn)

        btn.setOnClickListener {
            try {
                val out = readMaps()
                tv.text = out
                // scroll to top
                val sv = findViewById<ScrollView>(R.id.scrollView)
                sv.post { sv.fullScroll(ScrollView.FOCUS_UP) }
            } catch (e: Throwable) {
                tv.text = "Error reading maps: ${'$'}{e.message}"
            }
        }

        share.setOnClickListener {
            val content = tv.text.toString()
            val intent = Intent(Intent.ACTION_SEND)
            intent.type = "text/plain"
            intent.putExtra(Intent.EXTRA_SUBJECT, "proc_self_maps.txt")
            intent.putExtra(Intent.EXTRA_TEXT, content)
            startActivity(Intent.createChooser(intent, "Share proc maps"))
        }
    }
}
