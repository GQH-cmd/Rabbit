package com.smartrabbits.rfidsdkdemo;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

import java.util.ArrayList;
import java.util.List;

public class DatabaseHelper extends SQLiteOpenHelper {
    private static final String DB_NAME = "rabbit_records.db";
    private static final int DB_VERSION = 1;
    private static final String TABLE_NAME = "rabbit_record";

    private static final String COL_ID = "id";
    private static final String COL_EAR_TAG_ID = "ear_tag_id";
    private static final String COL_INTERNAL_ID = "internal_id";
    private static final String COL_BREED = "breed";
    private static final String COL_GENDER = "gender";
    private static final String COL_BIRTH_DATE = "birth_date";
    private static final String COL_HOUSE_PEN = "house_pen";
    private static final String COL_STATUS = "status";
    private static final String COL_SOURCE = "source";
    private static final String COL_ENTRY_DATE = "entry_date";
    private static final String COL_FATHER_ID = "father_id";
    private static final String COL_MOTHER_ID = "mother_id";
    private static final String COL_GRAND_FATHER_ID = "grand_father_id";
    private static final String COL_GRAND_MOTHER_ID = "grand_mother_id";
    private static final String COL_MATERNAL_GRAND_FATHER_ID = "maternal_grand_father_id";
    private static final String COL_MATERNAL_GRAND_MOTHER_ID = "maternal_grand_mother_id";
    private static final String COL_PARITY = "parity";
    private static final String COL_LITTER_SIZE = "litter_size";
    private static final String COL_INBREEDING_COEFF = "inbreeding_coeff";
    private static final String COL_SYNC_STATUS = "sync_status";
    private static final String COL_CREATE_TIME = "create_time";

    public DatabaseHelper(Context context) {
        super(context, DB_NAME, null, DB_VERSION);
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
        String sql = "CREATE TABLE " + TABLE_NAME + " (" +
                COL_ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                COL_EAR_TAG_ID + " TEXT," +
                COL_INTERNAL_ID + " TEXT," +
                COL_BREED + " TEXT," +
                COL_GENDER + " TEXT," +
                COL_BIRTH_DATE + " TEXT," +
                COL_HOUSE_PEN + " TEXT," +
                COL_STATUS + " TEXT," +
                COL_SOURCE + " TEXT," +
                COL_ENTRY_DATE + " TEXT," +
                COL_FATHER_ID + " TEXT," +
                COL_MOTHER_ID + " TEXT," +
                COL_GRAND_FATHER_ID + " TEXT," +
                COL_GRAND_MOTHER_ID + " TEXT," +
                COL_MATERNAL_GRAND_FATHER_ID + " TEXT," +
                COL_MATERNAL_GRAND_MOTHER_ID + " TEXT," +
                COL_PARITY + " INTEGER," +
                COL_LITTER_SIZE + " INTEGER," +
                COL_INBREEDING_COEFF + " REAL," +
                COL_SYNC_STATUS + " INTEGER DEFAULT 0," +
                COL_CREATE_TIME + " TEXT DEFAULT (datetime('now','localtime'))" +
                ")";
        db.execSQL(sql);
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        db.execSQL("DROP TABLE IF EXISTS " + TABLE_NAME);
        onCreate(db);
    }

    /**
     * 保存一条记录（插入）
     */
    public long insertRecord(LocalRecord record) {
        SQLiteDatabase db = getWritableDatabase();
        ContentValues values = recordToContentValues(record);
        return db.insert(TABLE_NAME, null, values);
    }

    /**
     * 获取所有未同步的记录
     */
    public List<LocalRecord> getPendingRecords() {
        List<LocalRecord> list = new ArrayList<>();
        SQLiteDatabase db = getReadableDatabase();
        Cursor cursor = db.query(TABLE_NAME, null,
                COL_SYNC_STATUS + "=?", new String[]{"0"},
                null, null, COL_CREATE_TIME + " ASC");
        while (cursor.moveToNext()) {
            list.add(cursorToRecord(cursor));
        }
        cursor.close();
        return list;
    }

    /**
     * 根据ID将记录标记为已同步
     */
    public void markAsSynced(long id) {
        SQLiteDatabase db = getWritableDatabase();
        ContentValues values = new ContentValues();
        values.put(COL_SYNC_STATUS, 1);
        db.update(TABLE_NAME, values, COL_ID + "=?", new String[]{String.valueOf(id)});
    }

    /**
     * 删除已同步的记录（清理）
     */
    public int deleteSyncedRecords() {
        SQLiteDatabase db = getWritableDatabase();
        return db.delete(TABLE_NAME, COL_SYNC_STATUS + "=?", new String[]{"1"});
    }

    /**
     * 将Cursor一行转成LocalRecord对象
     */
    private LocalRecord cursorToRecord(Cursor cursor) {
        LocalRecord r = new LocalRecord();
        r.id = cursor.getLong(cursor.getColumnIndex(COL_ID));
        r.earTagId = cursor.getString(cursor.getColumnIndex(COL_EAR_TAG_ID));
        r.internalId = cursor.getString(cursor.getColumnIndex(COL_INTERNAL_ID));
        r.breed = cursor.getString(cursor.getColumnIndex(COL_BREED));
        r.gender = cursor.getString(cursor.getColumnIndex(COL_GENDER));
        r.birthDate = cursor.getString(cursor.getColumnIndex(COL_BIRTH_DATE));
        r.housePen = cursor.getString(cursor.getColumnIndex(COL_HOUSE_PEN));
        r.status = cursor.getString(cursor.getColumnIndex(COL_STATUS));
        r.source = cursor.getString(cursor.getColumnIndex(COL_SOURCE));
        r.entryDate = cursor.getString(cursor.getColumnIndex(COL_ENTRY_DATE));
        r.fatherId = cursor.getString(cursor.getColumnIndex(COL_FATHER_ID));
        r.motherId = cursor.getString(cursor.getColumnIndex(COL_MOTHER_ID));
        r.grandFatherId = cursor.getString(cursor.getColumnIndex(COL_GRAND_FATHER_ID));
        r.grandMotherId = cursor.getString(cursor.getColumnIndex(COL_GRAND_MOTHER_ID));
        r.maternalGrandFatherId = cursor.getString(cursor.getColumnIndex(COL_MATERNAL_GRAND_FATHER_ID));
        r.maternalGrandMotherId = cursor.getString(cursor.getColumnIndex(COL_MATERNAL_GRAND_MOTHER_ID));
        r.parity = cursor.getInt(cursor.getColumnIndex(COL_PARITY));
        r.litterSize = cursor.getInt(cursor.getColumnIndex(COL_LITTER_SIZE));
        r.inbreedingCoeff = cursor.getDouble(cursor.getColumnIndex(COL_INBREEDING_COEFF));
        r.syncStatus = cursor.getInt(cursor.getColumnIndex(COL_SYNC_STATUS));
        r.createTime = cursor.getString(cursor.getColumnIndex(COL_CREATE_TIME));
        return r;
    }

    /**
     * 将LocalRecord转为ContentValues
     */
    private ContentValues recordToContentValues(LocalRecord r) {
        ContentValues values = new ContentValues();
        values.put(COL_EAR_TAG_ID, r.earTagId);
        values.put(COL_INTERNAL_ID, r.internalId);
        values.put(COL_BREED, r.breed);
        values.put(COL_GENDER, r.gender);
        values.put(COL_BIRTH_DATE, r.birthDate);
        values.put(COL_HOUSE_PEN, r.housePen);
        values.put(COL_STATUS, r.status);
        values.put(COL_SOURCE, r.source);
        values.put(COL_ENTRY_DATE, r.entryDate);
        values.put(COL_FATHER_ID, r.fatherId);
        values.put(COL_MOTHER_ID, r.motherId);
        values.put(COL_GRAND_FATHER_ID, r.grandFatherId);
        values.put(COL_GRAND_MOTHER_ID, r.grandMotherId);
        values.put(COL_MATERNAL_GRAND_FATHER_ID, r.maternalGrandFatherId);
        values.put(COL_MATERNAL_GRAND_MOTHER_ID, r.maternalGrandMotherId);
        values.put(COL_PARITY, r.parity);
        values.put(COL_LITTER_SIZE, r.litterSize);
        values.put(COL_INBREEDING_COEFF, r.inbreedingCoeff);
        values.put(COL_SYNC_STATUS, r.syncStatus);
        return values;
    }
}