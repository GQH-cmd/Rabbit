package com.smartrabbits.rfidsdkdemo;

import android.util.Log;
import android.zyapi.CommonApi;
import android.zyapi.Conversion;

public class RfidManager {
    private static final String TAG = "RfidManager";

    private static final String COM_PORT = "/dev/ttyMT2";
    private static final int BAUD_RATE = 9600;
    private static final int DATA_BITS = 8;
    private static final char PARITY = 'N';
    private static final int STOP_BITS = 1;

    private static final byte CMD_READ_CARD = (byte) 0xA1;
    private static final byte CMD_REPEAT_READ = (byte) 0xAA;
    private static final byte CMD_CLOSE_ANTENNA = (byte) 0xBB;

    private CommonApi mApi;
    private int mComFd = -1;
    private boolean mIsOpen = false;

    private Thread mReadThread;
    private volatile boolean mReading = false;
    private ScanListener mScanListener;

    /**
     * 扫描回调接口
     */
    public interface ScanListener {
        void onScanned(String cardId);
        void onRawData(String hexData);
    }

    public void setScanListener(ScanListener listener) {
        this.mScanListener = listener;
    }

    public boolean open() {
        if (mIsOpen) return true;
        try {
            mApi = new CommonApi();

            // ======== 低频 RFID 模块 GPIO 上电（来自低功耗 Demo）========
            // 引脚 13
            mApi.setGpioMode(13, 0);      // 设置模式
            mApi.setGpioDir(13, 1);       // 输出方向
            mApi.setGpioOut(13, 1);       // 高电平

            // 引脚 15
            mApi.setGpioMode(15, 0);
            mApi.setGpioDir(15, 1);
            mApi.setGpioOut(15, 1);

            // 等待模块稳定（Demo 中 postDelayed 了 1000ms，我们这里用 500ms 足够）
            try { Thread.sleep(500); } catch (InterruptedException e) { e.printStackTrace(); }
            // =============================================================

            // 打开串口
            mComFd = mApi.openComEx(COM_PORT, BAUD_RATE, DATA_BITS, PARITY, STOP_BITS, 2);
            if (mComFd > 0) {
                mIsOpen = true;
                Log.d(TAG, "串口打开成功");
                return true;
            }
        } catch (Exception e) {
            Log.e(TAG, "打开串口异常: " + e.getMessage());
        }
        return false;
    }

    public void close() {
        stopReadThread();
        if (mComFd > 0) {
            mApi.closeCom(mComFd);
            mComFd = -1;
            mIsOpen = false;
        }
        if (mApi != null) {
            mApi.setGpioDir(64, 1);
            mApi.setGpioOut(64, 1);
            mApi.setGpioDir(86, 1);
            mApi.setGpioOut(86, 0);
        }
    }

    private boolean sendCommand(byte cmd) {
        return sendCommand(new byte[]{cmd});
    }

    private boolean sendCommand(byte[] cmd) {
        if (!mIsOpen || mComFd <= 0) return false;
        int ret = mApi.writeCom(mComFd, cmd, cmd.length);
        Log.d(TAG, "发送: " + Conversion.Bytes2HexString(cmd));
        return ret >= 0;
    }

    /**
     * 开启连续读取（发送读卡指令并启动读取线程）
     */
    public void startContinuousScan() {
        if (!mIsOpen) return;
        // 发送 0xA1 打开天线并开始读卡
       // sendCommand(CMD_READ_CARD);
        // 启动读取线程
        startReadThread();
    }

    /**
     * 停止连续读取
     */
    public void stopContinuousScan() {
        stopReadThread();
        //sendCommand(CMD_CLOSE_ANTENNA);
    }

    private void startReadThread() {
        if (mReading) return;
        mReading = true;
        mReadThread = new Thread(() -> {
            Log.d(TAG, "连续读取线程开始");
            while (mReading && mIsOpen) {
                byte[] buf = new byte[256];
                int ret = mApi.readComEx(mComFd, buf, 256, 0, 0); // 500ms 超时
                if (ret > 0) {
                    byte[] data = new byte[ret];
                    System.arraycopy(buf, 0, data, 0, ret);
                    Log.d(TAG, "收到原始数据: " + Conversion.Bytes2HexString(data));
                    // 通知原始数据
                    if (mScanListener != null) {
                        mScanListener.onRawData(Conversion.Bytes2HexString(data));
                    }
                    // 尝试解析每一帧：可能有多帧，每帧至少8字节
                    int offset = 0;
                    while (offset <= data.length - 8) {
                        byte[] frame = new byte[8];
                        System.arraycopy(data, offset, frame, 0, 8);
                        // 异或校验
                        byte xor = 0;
                        for (int i = 0; i < 7; i++) {
                            xor ^= frame[i];
                        }
                        if (xor == frame[7]) {
                            // 校验通过，提取卡号
                            long cardId = ((long)(frame[5] & 0xFF) << 24) |
                                    ((long)(frame[4] & 0xFF) << 16) |
                                    ((long)(frame[3] & 0xFF) << 8) |
                                    ((long)(frame[2] & 0xFF));
                            String idStr = String.valueOf(cardId);
                            Log.d(TAG, "解析出耳标ID: " + idStr);
                            if (mScanListener != null) {
                                mScanListener.onScanned(idStr);
                            }
                            offset += 8; // 跳到下一帧
                        } else {
                            // 校验失败，可能不是完整帧，尝试移动一个字节继续找帧头
                            offset++;
                        }
                    }
                }
            }
            Log.d(TAG, "连续读取线程结束");
        });
        mReadThread.start();
    }

    private void stopReadThread() {
        mReading = false;
        if (mReadThread != null) {
            mReadThread.interrupt();
            mReadThread = null;
        }
    }

    public boolean isOpen() { return mIsOpen; }

    // 调试用公开方法
    public boolean sendCommandPublic(byte[] cmd) {
        return sendCommand(cmd);
    }

    public byte[] readResponsePublic(int expectedLen, int timeoutMs) {
        // 不再使用旧的 readResponse，但保留空实现或简单读取
        if (!mIsOpen || mComFd <= 0) return null;
        byte[] buf = new byte[expectedLen + 10];
        int ret = mApi.readComEx(mComFd, buf, expectedLen, 0, timeoutMs * 1000);
        if (ret > 0) {
            byte[] result = new byte[ret];
            System.arraycopy(buf, 0, result, 0, ret);
            return result;
        }
        return null;
    }
}