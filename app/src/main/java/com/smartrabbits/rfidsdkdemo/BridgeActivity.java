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

        // ======== 开启 WebView 远程调试（Chrome inspect）========
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.KITKAT) {
            WebView.setWebContentsDebuggingEnabled(true);
        }
        // =========================================================

        mWebView = findViewById(R.id.webview);
        mWebView.getSettings().setJavaScriptEnabled(true);
        // 允许混合内容（HTTP 资源在 WebView 中正常加载）
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

        // 加载前端网页，并检测桥接注入状态
        mWebView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageFinished(WebView view, String url) {
                super.onPageFinished(view, url);
                // 检测 RfidBridge 是否注入成功
                view.evaluateJavascript("typeof window.RfidBridge !== 'undefined'", value -> {
                    runOnUiThread(() -> {
                        if ("true".equals(value)) {
                            Toast.makeText(BridgeActivity.this, "✅ RfidBridge 已注入", Toast.LENGTH_SHORT).show();
                        } else {
                            Toast.makeText(BridgeActivity.this, "❌ RfidBridge 未注入！", Toast.LENGTH_SHORT).show();
                        }
                    });
                });
            }
        });
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