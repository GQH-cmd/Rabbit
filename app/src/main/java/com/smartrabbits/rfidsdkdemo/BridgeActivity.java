package com.smartrabbits.rfidsdkdemo;

import android.os.Bundle;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class BridgeActivity extends AppCompatActivity {
    private WebView mWebView;
    private RfidManager mRfidManager;
    private RfidBridge mRfidBridge;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_bridge);

        mWebView = findViewById(R.id.webview);
        // 1. 开启 JavaScript
        mWebView.getSettings().setJavaScriptEnabled(true);
        // 2. 允许混合内容（HTTP 资源在 HTTPS 或不安全网络下也能加载）
        mWebView.getSettings().setMixedContentMode(WebSettings.MIXED_CONTENT_ALWAYS_ALLOW);

        // 初始化 RfidManager（打开串口）
        mRfidManager = new RfidManager();
        new Thread(() -> {
            boolean ok = mRfidManager.open();
            runOnUiThread(() -> {
                if (ok) {
                    Toast.makeText(this, "RFID 初始化成功", Toast.LENGTH_SHORT).show();
                } else {
                    Toast.makeText(this, "RFID 初始化失败", Toast.LENGTH_SHORT).show();
                }
            });
        }).start();

        // 注入桥接对象（"RfidBridge" 对应网页中的 window.RfidBridge）
        mRfidBridge = new RfidBridge(mWebView, mRfidManager);
        mWebView.addJavascriptInterface(mRfidBridge, "RfidBridge");

        // 加载前端远程网页（你的目标地址）
        mWebView.setWebViewClient(new WebViewClient());
        mWebView.loadUrl("http://10.198.39.253:5000/");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mRfidBridge != null) {
            mRfidBridge.destroy();
        }
        if (mRfidManager != null) {
            mRfidManager.close();
        }
    }
}