package com.example.dimmer;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.widget.SeekBar;
import android.widget.TextView;

import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.Response;
import com.android.volley.VolleyError;
import com.android.volley.toolbox.StringRequest;
import com.android.volley.toolbox.Volley;

public class MainActivity extends AppCompatActivity {
    SeekBar seekBar;
    TextView brightness;
    RequestQueue requestQueue;
    String url;
    Integer progress;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Creates the components interfaces.
        seekBar = (SeekBar) findViewById(R.id.seekBarID);
        brightness = (TextView) findViewById(R.id.brightnessTextID);

        // Creates a new request Queue.
        requestQueue = Volley.newRequestQueue(this);

        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                progress = i;
                brightness.setText(String.valueOf(progress));
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                // Adds URL parameter
                url = "http://192.168.1.1/?brightness=" + String.valueOf(progress);

                // Requests a string response from the provided URL
                StringRequest stringRequest = new StringRequest(
                        Request.Method.GET, 
                        url,
                        new Response.Listener<String>() {
                            @Override
                            public void onResponse(String response) {
                                // Displays the response string
                                brightness.setText(response);
                            }
                        }, 
                        new Response.ErrorListener() {
                            @Override
                            public void onErrorResponse(VolleyError error) {
                                brightness.setText("Connection error!");
                            }
                        }
                );

                requestQueue.add(stringRequest);
            }
        });
    }
}