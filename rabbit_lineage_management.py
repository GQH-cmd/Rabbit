# rabbit_lineage_purebred_recursive.py
import sqlite3
from datetime import datetime


def init_db(db_name="rabbit_lineage.db"):
    conn = sqlite3.connect(db_name)
    cursor = conn.cursor()

    cursor.execute('''
    CREATE TABLE IF NOT EXISTS Rabbit (
        RabbitID TEXT PRIMARY KEY,
        Name TEXT,
        Gender TEXT CHECK(Gender IN ('Male', 'Female')),
        BirthDate TEXT,
        Bloodline TEXT,  -- 新增血统标记
        FatherID TEXT,
        MotherID TEXT,
        Home TEXT,
        FOREIGN KEY(FatherID) REFERENCES Rabbit(RabbitID),
        FOREIGN KEY(MotherID) REFERENCES Rabbit(RabbitID)
    )
    ''')
    conn.commit()
    return conn, cursor


def add_rabbit(cursor, rabbit_id, name, gender, birth_date, **kwargs):
    # 先检查是否已存在
    cursor.execute("SELECT COUNT(*) FROM Rabbit WHERE RabbitID = ?", (rabbit_id,))
    if cursor.fetchone()[0] > 0:
        # 已存在：更新信息
        cursor.execute('''
            UPDATE Rabbit 
            SET Name = ?, Gender = ?, BirthDate = ?, Bloodline = ?, Home = ?
            WHERE RabbitID = ?
        ''', (name, gender, birth_date, kwargs.get("Bloodline"), kwargs.get("Home"), rabbit_id))
        print(f"更新兔子信息: {rabbit_id}")
    else:
        # 不存在：插入新记录
        cursor.execute('''
            INSERT INTO Rabbit (RabbitID, Name, Gender, BirthDate, Bloodline, Home)
            VALUES (?, ?, ?, ?, ?, ?)
        ''', (rabbit_id, name, gender, birth_date, kwargs.get("Bloodline"), kwargs.get("Home")))
        print(f"新增兔子: {rabbit_id}")


# -------------------------------
# 递归检查血统一致性
# -------------------------------
def check_bloodline_recursive(cursor, rabbit_id, depth=3):
    """
    检查 rabbit_id 的血统是否纯种
    depth: 递归代数（父母为1代，祖父母为2代）
    返回 True/False
    """
    if depth == 0:
        return True  # 超过递归深度，不再检查

    cursor.execute('SELECT Bloodline, FatherID, MotherID FROM Rabbit WHERE RabbitID=?', (rabbit_id,))
    row = cursor.fetchone()
    if not row:
        return False  # 无信息，无法判断
    bloodline, father_id, mother_id = row

    if not father_id or not mother_id:
        return False  # 父母缺失，则不能判定纯种

    # 获取父母血统
    cursor.execute('SELECT Bloodline FROM Rabbit WHERE RabbitID=?', (father_id,))
    father_blood = cursor.fetchone()
    cursor.execute('SELECT Bloodline FROM Rabbit WHERE RabbitID=?', (mother_id,))
    mother_blood = cursor.fetchone()
    if not father_blood or not mother_blood:
        return False
    father_blood = father_blood[0]
    mother_blood = mother_blood[0]

    # 父母血统必须一致
    if father_blood != bloodline or mother_blood != bloodline:
        return False

    # 递归检查祖父母
    return check_bloodline_recursive(cursor, father_id, depth - 1) and check_bloodline_recursive(cursor, mother_id,
                                                                                                 depth - 1)


# -------------------------------
# 获取所有纯种兔子
# -------------------------------
def get_purebred_recursive(cursor, depth=3):
    cursor.execute('SELECT RabbitID, Name FROM Rabbit')
    rabbits = cursor.fetchall()
    purebred_list = []
    for r in rabbits:
        rabbit_id, name = r
        if check_bloodline_recursive(cursor, rabbit_id, depth):
            purebred_list.append((rabbit_id, name))
    return purebred_list


# -------------------------------
# 测试/demo
# -------------------------------
def demo():
    conn, cursor = init_db()

    # 添加父母及子代，并标记血统
    add_rabbit(cursor, "R001", "Fluffy", "Male", "2024-01-01", Bloodline="A", Home="HomeA")
    add_rabbit(cursor, "R002", "Snow", "Female", "2024-01-02", Bloodline="A", Home="HomeA")
    add_rabbit(cursor, "R003", "Bunny", "Male", "2024-02-01", Bloodline="A", FatherID="R001", MotherID="R002",
               Home="HomeA")

    # 混血示例
    add_rabbit(cursor, "R004", "MixRabbit", "Female", "2024-02-02", Bloodline="B", FatherID="R001", MotherID="R002",
               Home="HomeB")

    conn.commit()

    print("=== 纯种兔子列表（递归检查血统一致） ===")
    purebred = get_purebred_recursive(cursor, depth=2)
    for r in purebred:
        print(f"RabbitID: {r[0]}, Name: {r[1]}")

    conn.close()


if __name__ == "__main__":
    demo()