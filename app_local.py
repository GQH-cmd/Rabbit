#修改导入库
import psycopg2
import psycopg2.extras
from flask import Flask, render_template, request, jsonify

app = Flask(__name__)

#改成 PostgreSQL 配置
DB_CONFIG = {
    "host": "localhost",
    "port": 5432,
    "database": "rabbit_lineage",
    "user": "postgres",
    "password": 123456
}

#核心修改
def get_conn():
    return psycopg2.connect(**DB_CONFIG)

def init_db():
    conn = get_conn()
    #修改查询方式后面的也全部修改
    cursor = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

#建表SQL改动
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS Rabbit (
        RabbitID VARCHAR(50) PRIMARY KEY,
        Name VARCHAR(100),
        Gender VARCHAR(10) CHECK (Gender IN ('Male', 'Female')),
        BirthDate DATE,
        Bloodline VARCHAR(100),
        FatherID VARCHAR(50),
        MotherID VARCHAR(50),
        Home VARCHAR(100),

        CONSTRAINT fk_father
            FOREIGN KEY (FatherID)
            REFERENCES Rabbit(RabbitID)
            ON DELETE SET NULL,

        CONSTRAINT fk_mother
            FOREIGN KEY (MotherID)
            REFERENCES Rabbit(RabbitID)
            ON DELETE SET NULL
    )
    """)

    conn.commit()
    conn.close()


def row_to_dict(row):
    return dict(row) if row else None

#所有？改成%s
def get_rabbit_by_id(rabbit_id):
    conn = get_conn()
    cursor = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

#修改：在select语句加别名
    cursor.execute("""
    SELECT 
        RabbitID AS "RabbitID",
        Name AS "Name",
        Gender AS "Gender",
        BirthDate AS "BirthDate",
        Bloodline AS "Bloodline",
        FatherID AS "FatherID",
        MotherID AS "MotherID",
        Home AS "Home"
    FROM Rabbit
    WHERE RabbitID = %s
    """, (rabbit_id,))

    row = cursor.fetchone()
    conn.close()

    return row_to_dict(row)


def validate_parents(father_id, mother_id):
    """
    校验父母合法性。
    返回 (is_valid, error_message)
    """
    # 1. 父母不能是同一只
    if father_id and mother_id and father_id == mother_id:
        return False, "父亲和母亲不能是同一只兔子"

    # 2. 检查父亲存在且性别为 Male
    if father_id:
        father = get_rabbit_by_id(father_id)
        if not father:
            return False, f"父亲 {father_id} 不存在，请先录入该兔子"
        if father.get("Gender") != "Male":
            return False, f"父亲 {father_id} 的性别不是 Male（当前为 {father.get('Gender') or '未填写'}）"

    # 3. 检查母亲存在且性别为 Female
    if mother_id:
        mother = get_rabbit_by_id(mother_id)
        if not mother:
            return False, f"母亲 {mother_id} 不存在，请先录入该兔子"
        if mother.get("Gender") != "Female":
            return False, f"母亲 {mother_id} 的性别不是 Female（当前为 {mother.get('Gender') or '未填写'}）"

    return True, ""


def build_lineage_tree(rabbit_id, depth=3):
    """
    递归生成谱系树
    depth=1：查父母
    depth=2：查祖父母
    depth=3：查曾祖父母
    """

    rabbit = get_rabbit_by_id(rabbit_id)

    if not rabbit:
        return None

    node = {
        "RabbitID": rabbit["RabbitID"],
        "Name": rabbit["Name"],
        "Gender": rabbit["Gender"],
        "BirthDate": rabbit["BirthDate"],
        "Bloodline": rabbit["Bloodline"],
        "Home": rabbit["Home"],
        "FatherID": rabbit["FatherID"],
        "MotherID": rabbit["MotherID"],
        "Father": None,
        "Mother": None
    }

    if depth <= 0:
        return node

    if rabbit["FatherID"]:
        node["Father"] = build_lineage_tree(rabbit["FatherID"], depth - 1)

    if rabbit["MotherID"]:
        node["Mother"] = build_lineage_tree(rabbit["MotherID"], depth - 1)

    return node


def check_purebred_recursive(rabbit_id, target_bloodline=None, depth=3):
    """
    递归判断是否纯种。

    判断逻辑：
    1. 当前兔子必须存在；
    2. 当前兔子必须有 Bloodline；
    3. 如果未指定 target_bloodline，则以当前兔子的 Bloodline 作为目标血统；
    4. 当前兔子的 Bloodline 必须等于目标血统；
    5. 如果 depth > 0，则继续检查父母；
    6. 父母必须存在，且父母血统也必须一致；
    7. 递归检查祖父母、曾祖父母等。
    """

    rabbit = get_rabbit_by_id(rabbit_id)

    if not rabbit:
        return False

    if not rabbit["Bloodline"]:
        return False

    if target_bloodline is None:
        target_bloodline = rabbit["Bloodline"]

    if rabbit["Bloodline"] != target_bloodline:
        return False

    if depth <= 0:
        return True

    father_id = rabbit["FatherID"]
    mother_id = rabbit["MotherID"]

    if not father_id or not mother_id:
        return False

    father_ok = check_purebred_recursive(
        father_id,
        target_bloodline=target_bloodline,
        depth=depth - 1
    )

    mother_ok = check_purebred_recursive(
        mother_id,
        target_bloodline=target_bloodline,
        depth=depth - 1
    )

    return father_ok and mother_ok


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/rabbits", methods=["GET"])
def get_rabbits():
    conn = get_conn()
    cursor = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

    cursor.execute("""
    SELECT RabbitID, Name, Gender, BirthDate, Bloodline, FatherID, MotherID, Home
    FROM Rabbit
    ORDER BY RabbitID
    """)

    rows = cursor.fetchall()
    conn.close()

    data = [row_to_dict(row) for row in rows]
    return jsonify(data)


@app.route("/api/rabbits", methods=["POST"])
def add_rabbit():
    data = request.json

    rabbit_id = data.get("RabbitID")
    name = data.get("Name")
    gender = data.get("Gender")
    birth_date = data.get("BirthDate")
    bloodline = data.get("Bloodline")
    father_id = data.get("FatherID") or None
    mother_id = data.get("MotherID") or None
    home = data.get("Home")

    if not rabbit_id:
        return jsonify({"success": False, "message": "RabbitID 不能为空"}), 400

    if gender not in ["Male", "Female"]:
        return jsonify({"success": False, "message": "Gender 必须是 Male 或 Female"}), 400

    # 校验父母合法性
    valid, msg = validate_parents(father_id, mother_id)
    if not valid:
        return jsonify({"success": False, "message": msg}), 400

    conn = get_conn()
    cursor = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

    try:
        cursor.execute("""
        INSERT INTO Rabbit (
            RabbitID, Name, Gender, BirthDate, Bloodline, FatherID, MotherID, Home
        )
        VALUES (%s, %s, %s, %s, %s, %s, %s, %s)
        """, (
            rabbit_id,
            name,
            gender,
            birth_date,
            bloodline,
            father_id,
            mother_id,
            home
        ))

        conn.commit()
        conn.close()

        return jsonify({"success": True, "message": "兔子信息添加成功"})

#修改
    except psycopg2.IntegrityError as e:
        conn.close()
        conn.rollback()
        return jsonify({
            "success": False,
            "message": f"添加失败，可能是 RabbitID 已存在或性别格式错误：{str(e)}"
        }), 400


@app.route("/api/rabbits/<rabbit_id>", methods=["PUT", "PATCH"])
def update_rabbit(rabbit_id):
    data = request.json or {}

    # 先查一下兔子在不在
    rabbit = get_rabbit_by_id(rabbit_id)
    if not rabbit:
        return jsonify({"success": False, "message": "未找到该兔子"}), 404

    # 用旧值当默认值，只覆盖前端传了的字段
    name = data.get("Name", rabbit["Name"])
    gender = data.get("Gender", rabbit["Gender"])
    birth_date = data.get("BirthDate", rabbit["BirthDate"])
    bloodline = data.get("Bloodline", rabbit["Bloodline"])
    home = data.get("Home", rabbit["Home"])

    # 父母 ID 特殊处理：前端传了空字符串就置为 None
    father_id = data.get("FatherID", rabbit["FatherID"])
    if "FatherID" in data and not data["FatherID"]:
        father_id = None

    mother_id = data.get("MotherID", rabbit["MotherID"])
    if "MotherID" in data and not data["MotherID"]:
        mother_id = None

    # 性别校验
    if gender not in ["Male", "Female"]:
        return jsonify({"success": False, "message": "Gender 必须是 Male 或 Female"}), 400

    # 校验父母合法性（编辑时也要检查）
    valid, msg = validate_parents(father_id, mother_id)
    if not valid:
        return jsonify({"success": False, "message": msg}), 400

    conn = get_conn()
    cursor = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

    try:
        cursor.execute("""
            UPDATE Rabbit
            SET Name = %s, Gender = %s, BirthDate = %s, Bloodline = %s,
                FatherID = %s, MotherID = %s, Home = %s
            WHERE RabbitID = %s
        """, (name, gender, birth_date, bloodline, father_id, mother_id, home, rabbit_id))

        conn.commit()
        conn.close()

        return jsonify({"success": True, "message": "兔子信息更新成功"})

    except psycopg2.IntegrityError as e:
        conn.close()
        return jsonify({"success": False, "message": f"更新失败：{str(e)}"}), 400


@app.route("/api/lineage/<rabbit_id>", methods=["GET"])
def get_lineage(rabbit_id):
    depth = int(request.args.get("depth", 3))

    tree = build_lineage_tree(rabbit_id, depth=depth)

    if not tree:
        return jsonify({"success": False, "message": "未找到该兔子"}), 404

    return jsonify({
        "success": True,
        "data": tree
    })


@app.route("/api/purebred", methods=["GET"])
def get_purebred():
    depth = int(request.args.get("depth", 2))
    bloodline_filter = request.args.get("bloodline", "").strip()

    conn = get_conn()
    cursor = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

    if bloodline_filter:
        cursor.execute("""
        SELECT RabbitID, Name, Gender, BirthDate, Bloodline, FatherID, MotherID, Home
        FROM Rabbit
        WHERE Bloodline = %s
        ORDER BY RabbitID
        """, (bloodline_filter,))
    else:
        cursor.execute("""
        SELECT RabbitID, Name, Gender, BirthDate, Bloodline, FatherID, MotherID, Home
        FROM Rabbit
        ORDER BY RabbitID
        """)

    rows = cursor.fetchall()
    conn.close()

    result = []

    for row in rows:
        rabbit = row_to_dict(row)

        is_pure = check_purebred_recursive(
            rabbit["RabbitID"],
            target_bloodline=rabbit["Bloodline"],
            depth=depth
        )

        if is_pure:
            result.append(rabbit)

    return jsonify(result)


@app.route("/api/delete/<rabbit_id>", methods=["DELETE"])
def delete_rabbit(rabbit_id):
    conn = get_conn()
    cursor = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

    cursor.execute("DELETE FROM Rabbit WHERE RabbitID = %s", (rabbit_id,))
    conn.commit()
    conn.close()

    return jsonify({"success": True, "message": "删除成功"})


if __name__ == "__main__":
    init_db()
    app.run(debug=True)