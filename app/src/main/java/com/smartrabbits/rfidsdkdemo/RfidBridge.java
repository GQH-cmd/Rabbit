package com.smartrabbits.rfidsdkdemo;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.webkit.JavascriptInterface;
import android.webkit.WebView;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class RfidBridge {
    private static final String TAG = "RfidBridge";

    private final WebView mWebView;
    private final RfidManager mRfidManager;
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());
    private final ExecutorService mExecutor = Executors.newSingleThreadExecutor();

    // 当前等待单次扫描结果的回调函数名
    private String mPendingScanCallback;

    public RfidBridge(WebView webView, RfidManager rfidManager) {
        this.mWebView = webView;
        this.mRfidManager = rfidManager;

        // 设置连续扫描监听器，收到耳标ID后自动处理
        rfidManager.setScanListener(new RfidManager.ScanListener() {
            @Override
            public void onScanned(String cardId) {
                Log.d(TAG, "连续扫描收到ID: " + cardId);
                // 如果是单次扫描模式，回调网页并停止扫描
                if (mPendingScanCallback != null) {
                    String callback = mPendingScanCallback;
                    mPendingScanCallback = null;
                    mRfidManager.stopContinuousScan();
                    callJS(callback, cardId);
                }
            }

            @Override
            public void onRawData(String hexData) {
                // 原始数据可用于调试，暂不处理
            }
        });
    }

    /**
     * 单次扫描：启动连续扫描，收到第一个ID后自动停止并回调网页
     */
    @JavascriptInterface
    public void scan(String callbackFuncName) {
        Log.d(TAG, "JS调用scan，回调函数: " + callbackFuncName);
        if (callbackFuncName == null || callbackFuncName.isEmpty()) return;

        mPendingScanCallback = callbackFuncName;
        mExecutor.execute(() -> {
            boolean ok = mRfidManager.isOpen() || mRfidManager.open();
            if (!ok) {
                Log.e(TAG, "扫描失败：串口未打开");
                mMainHandler.post(() -> callJS(callbackFuncName, null, "串口打开失败"));
                mPendingScanCallback = null;
                return;
            }
            mRfidManager.startContinuousScan();
        });
    }

    /**
     * 停止扫描（网页主动停止）
     */
    @JavascriptInterface
    public void stopScan() {
        Log.d(TAG, "JS调用stopScan");
        mPendingScanCallback = null;
        mExecutor.execute(() -> mRfidManager.stopContinuousScan());
    }

    /**
     * 获取设备状态（示例：返回JSON字符串）
     */
    @JavascriptInterface
    public String getStatus() {
        boolean isOpen = mRfidManager != null && mRfidManager.isOpen();
        return "{\"rfidOpen\":" + isOpen + "}";
    }

    /**
     * 释放资源
     */
    public void destroy() {
        mPendingScanCallback = null;
        mExecutor.shutdown();
    }

    // 内部方法：回调 JavaScript 函数
    private void callJS(String funcName, String result) {
        callJS(funcName, result, null);
    }

    private void callJS(String funcName, String result, String error) {
        StringBuilder sb = new StringBuilder("javascript:");
        sb.append(funcName).append("(");
        if (result != null) {
            // 简单转义单引号
            sb.append("'").append(result.replace("'", "\\'")).append("'");
        } else {
            sb.append("null");
        }
        if (error != null) {
            sb.append(", '").append(error.replace("'", "\\'")).append("'");
        }
        sb.append(")");
        String js = sb.toString();
        Log.d(TAG, "回调JS: " + js);
        mWebView.post(() -> mWebView.loadUrl(js));
    }
}