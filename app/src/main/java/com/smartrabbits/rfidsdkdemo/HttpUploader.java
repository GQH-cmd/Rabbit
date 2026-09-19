package com.smartrabbits.rfidsdkdemo;

import android.util.Log;
import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

public class HttpUploader {
    private static final String TAG = "HttpUploader";
    private static final String API_URL = "http://192.168.43.110:5000/api/ear-tag";

    public static boolean uploadEarTag(String rabbitId) {
        HttpURLConnection conn = null;
        try {
            URL url = new URL(API_URL);
            conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("POST");
            conn.setRequestProperty("Content-Type", "application/json; charset=UTF-8");
            conn.setDoOutput(true);
            conn.setConnectTimeout(5000);
            conn.setReadTimeout(5000);

            JSONObject body = new JSONObject();
            body.put("RabbitID", rabbitId);

            OutputStream os = conn.getOutputStream();
            os.write(body.toString().getBytes("UTF-8"));
            os.flush();
            os.close();

            int code = conn.getResponseCode();
            Log.d(TAG, "响应码: " + code);
            if (code == 200) {
                BufferedReader reader = new BufferedReader(
                        new InputStreamReader(conn.getInputStream(), "UTF-8"));
                StringBuilder sb = new StringBuilder();
                String line;
                while ((line = reader.readLine()) != null) sb.append(line);
                reader.close();
                String resp = sb.toString();
                Log.d(TAG, "响应: " + resp);
                JSONObject json = new JSONObject(resp);
                return json.optBoolean("success", false);
            }
        } catch (Exception e) {
            Log.e(TAG, "上传失败: " + e.getMessage());
        } finally {
            if (conn != null) conn.disconnect();
        }
        return false;
    }
}