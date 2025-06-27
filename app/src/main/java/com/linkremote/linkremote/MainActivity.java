package com.linkremote.linkremote;

import android.os.Bundle;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.View;
import android.view.Menu;
import android.view.MenuItem;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

import org.eclipse.paho.android.service.MqttAndroidClient;
import org.eclipse.paho.client.mqttv3.DisconnectedBufferOptions;
import org.eclipse.paho.client.mqttv3.IMqttActionListener;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.IMqttToken;
import org.eclipse.paho.client.mqttv3.MqttCallback;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;

import java.io.IOException;
import java.io.InputStream;
import java.time.Duration;
import java.time.LocalDateTime;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

public class MainActivity extends AppCompatActivity {

    private MqttAndroidClient mqttAndroidClient;
    private final String serverUri = "tcp://m21.cloudmqtt.com:14273";
    private String clientId = "85e50870-9397-411e-934d-5f8e14d3c0db";
    private String boardId = null;
    private String cluster = null;

    private Map<String, LocalDateTime> lastResponse = new HashMap<>();
    private ScheduledExecutorService executorService = Executors.newScheduledThreadPool(5);

    private int padding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        padding = getResources().getDimensionPixelSize(R.dimen.switch_padding);

        setContentView(R.layout.activity_main);
        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        LinearLayout linearLayout = findViewById(R.id.content_main);

        mqttAndroidClient = new MqttAndroidClient(getApplicationContext(), serverUri, clientId);
        boardId  = "000A.07E2ALAqt.0001";
        //boardId  = "000A.07E2B5Eya.0002";
        cluster  = "FF000980/FF000980";
        //cluster  = "FF000980/FF000980";
        //cluster  = "FEDCBA98/FEDCBA98";

        final String publishTopic = cluster +"/" + boardId ;
        final String contollerTopic = cluster +"/" + "controller" ;

        mqttAndroidClient.setCallback(new MqttCallback() {
            @Override
            public void connectionLost(Throwable cause) {

            }

            @Override
            public void messageArrived(String topic, MqttMessage message) throws Exception {
                String msg = new String(message.getPayload());

                if(msg.startsWith("ACT") || msg.startsWith("PNG")){
                    int idx_board_id_start = msg.indexOf(' ');
                    int idx_board_id_end   = msg.indexOf(':');
                    String board_id = msg.substring(idx_board_id_start + 1, idx_board_id_end);
                    lastResponse.put(board_id, LocalDateTime.now());
                }

                if(msg.startsWith("SCM")){

                }
            }

            @Override
            public void deliveryComplete(IMqttDeliveryToken token) {

            }
        });

        MqttConnectOptions mqttConnectOptions = new MqttConnectOptions();
        mqttConnectOptions.setAutomaticReconnect(true);
        mqttConnectOptions.setCleanSession(false);
        mqttConnectOptions.setUserName("ztojfebt");
        mqttConnectOptions.setPassword("yFXd_a6yEnt7".toCharArray());


        try {
            //addToHistory("Connecting to " + serverUri);
            mqttAndroidClient.connect(mqttConnectOptions, null, new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    DisconnectedBufferOptions disconnectedBufferOptions = new DisconnectedBufferOptions();
                    disconnectedBufferOptions.setBufferEnabled(true);
                    disconnectedBufferOptions.setBufferSize(100);
                    disconnectedBufferOptions.setPersistBuffer(false);
                    disconnectedBufferOptions.setDeleteOldestMessages(false);
                    mqttAndroidClient.setBufferOpts(disconnectedBufferOptions);

                    try {
                        IMqttToken token = mqttAndroidClient.subscribe(contollerTopic, 1);
                        token.setActionCallback(new IMqttActionListener() {
                            @Override
                            public void onSuccess(IMqttToken asyncActionToken) {
                                asyncActionToken.getClient();
                            }

                            @Override
                            public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                                asyncActionToken.getClient();

                            }
                        });
                    } catch (MqttException e) {
                        e.printStackTrace();
                    }

                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    Snackbar.make(findViewById(android.R.id.content), "Failed to connect to: " + serverUri, Snackbar.LENGTH_LONG)
                            .setAction("Action", null).show();
                }
            });


        } catch (MqttException ex){
            ex.printStackTrace();
        }

        try {
            String controls = readStream(getApplicationContext().getAssets().open("controls.json"));
        } catch (IOException e) {
            e.printStackTrace();
        }

        addSlider(linearLayout, "Built in LED","0", publishTopic);
        for(int i = 1; i< 8; i++){
            linearLayout.addView(createSwitch("00" + i, "" + i, publishTopic, true));
        }
        addSlider(linearLayout,"008","" + 8, publishTopic);

    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }


    private Switch createSwitch(String label, final String control, final String topic, final boolean inverted){
        Switch ts = new Switch(getApplicationContext());
        ts.setText(label);
        ts.setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
        ts.setPadding(padding ,padding ,padding, padding);
        ts.setBackground(getResources().getDrawable(R.drawable.controls_border));

        ts.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, final boolean isChecked) {
                sendMQTTtoBoard("SDV " + control +"=" + (isChecked == inverted ? 0 : 1), boardId);
            }
        });
        return ts;
    }

    private SeekBar createSlider(int max, final String control,  final String topic){
        SeekBar slider = new SeekBar(getApplicationContext());
        //slider.setText(label);
        slider.setMax(max);
        //slider.setMin(1);
        slider.setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
        slider.setPadding(padding ,padding ,padding, padding);
        slider.setBackground(getResources().getDrawable(R.drawable.controls_border));

        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {

            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                sendMQTTtoBoard(("SAV " + control +"=" + seekBar.getProgress()), boardId);
            }
        });
        return slider;
    }

    private void addSlider(ViewGroup parent, String label, final String control, final String topic){
        TextView tv = new TextView(getApplicationContext());
        tv.setPadding(padding ,padding ,padding, 0);
        tv.setText(label);
        parent.addView(tv);
        parent.addView(createSlider(1023 ,control, topic));
    }

    private TextView createTextView(String text){
        TextView tv = new TextView(getApplicationContext());
        tv.setPadding(padding ,padding ,padding, padding);
        tv.setText(text);
        return tv;
    }

    private void sendMQTTtoBoard(final String message, final String boardId){

        executorService.schedule(new Runnable() {
            @Override
            public void run() {
                LocalDateTime lastResponseDate = lastResponse.get(boardId);

                while(lastResponseDate == null || Duration.between(lastResponseDate, LocalDateTime.now()).toMinutes() > 1){
                    sendMQTT("PNG",cluster + "/" + boardId);
                    try {
                        Thread.sleep(1000);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    lastResponseDate = lastResponse.get(boardId);
                }
                sendMQTT(message, cluster + "/" + boardId);
            }
        }, 0, TimeUnit.SECONDS);


    }

    private void sendMQTT(final String message, final String topic){

        MqttMessage mqqtMessage = new MqttMessage();
        mqqtMessage.setRetained(true);
        mqqtMessage.setPayload(message.getBytes());
        try {
            if(mqttAndroidClient.isConnected()){
                mqttAndroidClient.publish(topic, mqqtMessage);
            }
        } catch (MqttException e) {
            e.printStackTrace();
        }

    }

    public String readStream(InputStream is) throws IOException {
        StringBuilder sb = new StringBuilder();
        byte[] buffer = new byte [10 * 1024];
        int length = 0;
        while((length = is.read(buffer))> 0){
            sb.append(new String(buffer, 0, length));
        }
        return sb.toString();
    }
}
