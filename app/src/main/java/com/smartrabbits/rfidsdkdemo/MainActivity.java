package com.smartrabbits.rfidsdkdemo;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {

    private RfidManager rfidManager;
    private TextView tvResult;
    private Button btnStartScan, btnStopScan;
    private Handler mainHandler = new Handler(Looper.getMainLooper());
    private boolean isScanning = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvResult = findViewById(R.id.tvResult);
        btnStartScan = findViewById(R.id.btnStartScan);
        btnStopScan = findViewById(R.id.btnStopScan);

        rfidManager = new RfidManager();
        rfidManager.setScanListener(new RfidManager.ScanListener() {
            @Override
            public void onScanned(String cardId) {
                mainHandler.post(() -> {
                    tvResult.setText("耳标ID: " + cardId);
                    // 上传到后端
                    new Thread(() -> {
                        boolean ok = HttpUploader.uploadEarTag(cardId);
                        mainHandler.post(() -> Toast.makeText(MainActivity.this,
                                ok ? "上传成功" : "上传失败", Toast.LENGTH_SHORT).show());
                    }).start();
                });
            }
            @Override
            public void onRawData(String hexData) { }
        });

        new Thread(() -> {
            boolean ok = rfidManager.open();
            mainHandler.post(() -> {
                Toast.makeText(this, ok ? "RFID 初始化成功" : "RFID 初始化失败",
                        Toast.LENGTH_SHORT).show();
                btnStartScan.setEnabled(ok);
            });
        }).start();

        btnStartScan.setOnClickListener(v -> {
            if (!isScanning) {
                rfidManager.startContinuousScan();
                isScanning = true;
                tvResult.setText("扫描中...");
                btnStartScan.setEnabled(false);
                btnStopScan.setEnabled(true);
            }
        });

        btnStopScan.setOnClickListener(v -> {
            if (isScanning) {
                rfidManager.stopContinuousScan();
                isScanning = false;
                tvResult.setText("已停止");
                btnStartScan.setEnabled(true);
                btnStopScan.setEnabled(false);
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (rfidManager != null) rfidManager.close();
    }
}