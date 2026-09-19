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
    "password": "123456"
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

        Status VARCHAR(50),
        Source VARCHAR(100),

        FatherID VARCHAR(50),
        MotherID VARCHAR(50),
        Home VARCHAR(100),

        Parity INTEGER,
        LitterSize INTEGER,
        InbreedingCoeff DOUBLE PRECISION,

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
    if not row:
        return None

    data = dict(row)

    # Convert PostgreSQL DATE to YYYY-MM-DD
    if data.get("BirthDate"):
        data["BirthDate"] = data["BirthDate"].strftime("%Y-%m-%d")

    return data

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
        Status AS "Status",
        Source AS "Source",
        FatherID AS "FatherID",
        MotherID AS "MotherID",
        Home AS "Home",
        Parity AS "Parity",
        LitterSize AS "LitterSize",
        InbreedingCoeff AS "InbreedingCoeff"
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

def get_all_rabbits_for_pedigree():
    """Load pedigree data for inbreeding calculation."""
    conn = get_conn()
    cursor = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

    cursor.execute("""
        SELECT
            RabbitID AS "RabbitID",
            FatherID AS "FatherID",
            MotherID AS "MotherID"
        FROM Rabbit
    """)

    rows = cursor.fetchall()
    cursor.close()
    conn.close()

    return {
        row["RabbitID"]: {
            "FatherID": row["FatherID"],
            "MotherID": row["MotherID"]
        }
        for row in rows
    }


def calculate_inbreeding_coefficient(father_id, mother_id):
    """
    Calculate expected inbreeding coefficient of offspring.

    F_child = A(father, mother) / 2

    Unknown founders are assumed unrelated and non-inbred.
    """

    if not father_id or not mother_id:
        return 0.0, []

    pedigree = get_all_rabbits_for_pedigree()

    if father_id not in pedigree or mother_id not in pedigree:
        return 0.0, []

    # -------------------------------------------------
    # Collect all ancestors needed for the calculation
    # -------------------------------------------------
    needed = set()

    def collect(rabbit_id):
        if not rabbit_id or rabbit_id in needed:
            return

        needed.add(rabbit_id)

        rabbit = pedigree.get(rabbit_id)
        if not rabbit:
            return

        collect(rabbit["FatherID"])
        collect(rabbit["MotherID"])

    collect(father_id)
    collect(mother_id)

    # -------------------------------------------------
    # Topological order: parents before offspring
    # -------------------------------------------------
    order = []
    visited = set()

    def visit(rabbit_id):
        if not rabbit_id or rabbit_id in visited:
            return

        rabbit = pedigree.get(rabbit_id)

        if rabbit:
            visit(rabbit["FatherID"])
            visit(rabbit["MotherID"])

        visited.add(rabbit_id)
        order.append(rabbit_id)

    visit(father_id)
    visit(mother_id)

    index = {rabbit_id: i for i, rabbit_id in enumerate(order)}
    n = len(order)

    A = [[0.0 for _ in range(n)] for _ in range(n)]

    # -------------------------------------------------
    # Build additive relationship matrix
    # -------------------------------------------------
    for i, rabbit_id in enumerate(order):
        rabbit = pedigree.get(rabbit_id, {})

        father = rabbit.get("FatherID")
        mother = rabbit.get("MotherID")

        fi = index.get(father)
        mi = index.get(mother)

        # Off-diagonal relationships
        for j in range(i):
            father_rel = A[fi][j] if fi is not None else 0.0
            mother_rel = A[mi][j] if mi is not None else 0.0

            A[i][j] = A[j][i] = (
                father_rel + mother_rel
            ) / 2.0

        # Diagonal
        if fi is not None and mi is not None:
            A[i][i] = 1.0 + A[fi][mi] / 2.0
        else:
            # Founder / one unknown parent
            A[i][i] = 1.0

    father_index = index[father_id]
    mother_index = index[mother_id]

    coefficient = A[father_index][mother_index] / 2.0

    # -------------------------------------------------
    # Find common ancestors for explanation
    # -------------------------------------------------
    def ancestor_distances(start_id):
        result = {}
        queue = [(start_id, 0)]

        while queue:
            current, distance = queue.pop(0)

            rabbit = pedigree.get(current)
            if not rabbit:
                continue

            for parent in [rabbit["FatherID"], rabbit["MotherID"]]:
                if not parent:
                    continue

                new_distance = distance + 1

                if parent not in result or new_distance < result[parent]:
                    result[parent] = new_distance
                    queue.append((parent, new_distance))

        return result

    father_ancestors = ancestor_distances(father_id)
    mother_ancestors = ancestor_distances(mother_id)

    common_ids = (
        set(father_ancestors.keys())
        & set(mother_ancestors.keys())
    )

    common_ancestors = []

    for ancestor_id in common_ids:
        common_ancestors.append({
            "RabbitID": ancestor_id,
            "FatherDistance": father_ancestors[ancestor_id],
            "MotherDistance": mother_ancestors[ancestor_id]
        })

    common_ancestors.sort(
        key=lambda x:
        x["FatherDistance"] + x["MotherDistance"]
    )

    return round(coefficient, 6), common_ancestors

@app.route("/api/mating-check", methods=["GET"])
def mating_check():
    father_id = request.args.get("father_id", "").strip()
    mother_id = request.args.get("mother_id", "").strip()

    if not father_id or not mother_id:
        return jsonify({
            "success": False,
            "message": "请输入父兔和母兔"
        }), 400

    valid, msg = validate_parents(father_id, mother_id)

    if not valid:
        return jsonify({
            "success": False,
            "message": msg
        }), 400

    coefficient, common_ancestors = \
        calculate_inbreeding_coefficient(
            father_id,
            mother_id
        )

    father = get_rabbit_by_id(father_id)
    mother = get_rabbit_by_id(mother_id)

    return jsonify({
        "success": True,
        "FatherID": father_id,
        "MotherID": mother_id,
        "FatherBloodline": father.get("Bloodline"),
        "MotherBloodline": mother.get("Bloodline"),
        "SameBloodline":
            father.get("Bloodline") == mother.get("Bloodline")
            if father.get("Bloodline") and mother.get("Bloodline")
            else False,
        "InbreedingCoeff": coefficient,
        "InbreedingPercent": round(coefficient * 100, 2),
        "CommonAncestors": common_ancestors
    })

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


def check_purebred_recursive(rabbit_id, bloodline, depth):
    """
    Check whether a rabbit is purebred within the specified generations.

    Rules:
    1. The rabbit itself must belong to the target bloodline.
    2. For every generation being checked, both parents must be known.
    3. All ancestors within the requested depth must belong
       to the same bloodline.
    """

    rabbit = get_rabbit_by_id(rabbit_id)

    if not rabbit:
        return False

    # Current rabbit must match the target bloodline
    if rabbit.get("Bloodline") != bloodline:
        return False

    # Required generations have been checked
    if depth <= 0:
        return True

    father_id = rabbit.get("FatherID")
    mother_id = rabbit.get("MotherID")

    # Incomplete pedigree -> cannot confirm purebred
    if not father_id or not mother_id:
        return False

    return (
        check_purebred_recursive(
            father_id,
            bloodline,
            depth - 1
        )
        and
        check_purebred_recursive(
            mother_id,
            bloodline,
            depth - 1
        )
    )


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/rabbits", methods=["GET"])
def get_rabbits():
    view = request.args.get("view", "active").lower()

    conn = get_conn()
    cursor = conn.cursor(
        cursor_factory=psycopg2.extras.RealDictCursor
    )

    base_sql = """
        SELECT
            RabbitID AS "RabbitID",
            Name AS "Name",
            Gender AS "Gender",
            BirthDate AS "BirthDate",
            Bloodline AS "Bloodline",
            Status AS "Status",
            Source AS "Source",
            FatherID AS "FatherID",
            MotherID AS "MotherID",
            Home AS "Home",
            Parity AS "Parity",
            LitterSize AS "LitterSize",
            InbreedingCoeff AS "InbreedingCoeff"
        FROM Rabbit
    """

    if view == "dead":
        base_sql += """
            WHERE LOWER(COALESCE(Status, '')) = 'dead'
        """

    elif view == "all":
        pass

    else:
        base_sql += """
            WHERE LOWER(COALESCE(Status, '')) <> 'dead'
        """

    base_sql += " ORDER BY RabbitID"

    cursor.execute(base_sql)

    rows = cursor.fetchall()

    cursor.close()
    conn.close()

    return jsonify([
        row_to_dict(row)
        for row in rows
    ])

@app.route("/api/rabbits/<rabbit_id>", methods=["GET"])
def get_single_rabbit(rabbit_id):
    rabbit = get_rabbit_by_id(rabbit_id)

    if not rabbit:
        return jsonify({
            "success": False,
            "message": "未找到该兔子"
        }), 404

    return jsonify({
        "success": True,
        "data": rabbit
    })

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
    status = data.get("Status")
    source = data.get("Source")
    parity = data.get("Parity")
    litter_size = data.get("LitterSize")
    # Automatically calculate inbreeding coefficient
    if father_id and mother_id:
        inbreeding_coeff, _ = calculate_inbreeding_coefficient(
            father_id,
            mother_id
        )
    else:
        inbreeding_coeff = 0.0

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
            RabbitID,
            Name,
            Gender,
            BirthDate,
            Bloodline,
            FatherID,
            MotherID,
            Home,
            Status,
            Source,
            Parity,
            LitterSize,
            InbreedingCoeff
        )
        VALUES (
            %s, %s, %s, %s, %s,
            %s, %s, %s, %s, %s,
            %s, %s, %s
        )
        """, (
            rabbit_id,
            name,
            gender,
            birth_date,
            bloodline,
            father_id,
            mother_id,
            home,
            status,
            source,
            parity,
            litter_size,
            inbreeding_coeff
        ))

        conn.commit()
        conn.close()

        return jsonify({"success": True, "message": "兔子信息添加成功"})

#修改
    except psycopg2.IntegrityError as e:
        conn.rollback()
        conn.close()

        return jsonify({
            "success": False,
            "message": f"添加失败：{str(e)}"
        }), 400
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


@app.route("/api/purebred")
def get_purebred_rabbits():
    bloodline = request.args.get("bloodline", "").strip()

    try:
        depth = int(request.args.get("depth", 1))
    except ValueError:
        return jsonify({
            "error": "depth 必须是整数"
        }), 400

    if not bloodline:
        return jsonify({
            "error": "缺少 bloodline 参数"
        }), 400

    if depth < 1:
        return jsonify({
            "error": "depth 必须大于等于 1"
        }), 400

    conn = get_conn()
    cursor = conn.cursor(
        cursor_factory=psycopg2.extras.RealDictCursor
    )

    cursor.execute("""
        SELECT
            RabbitID AS "RabbitID",
            Name AS "Name",
            Gender AS "Gender",
            BirthDate AS "BirthDate",
            Bloodline AS "Bloodline",
            FatherID AS "FatherID",
            MotherID AS "MotherID",
            Home AS "Home",
            Status AS "Status",
            Source AS "Source",
            Parity AS "Parity",
            LitterSize AS "LitterSize",
            InbreedingCoeff AS "InbreedingCoeff"
        FROM Rabbit
        WHERE Bloodline = %s
          AND (
              Status IS NULL
              OR LOWER(Status) <> 'dead'
          )
        ORDER BY RabbitID
    """, (bloodline,))

    rabbits = cursor.fetchall()
    cursor.close()
    conn.close()

    result = []

    for rabbit in rabbits:
        if check_purebred_recursive(
            rabbit["RabbitID"],
            bloodline,
            depth
        ):
            result.append(row_to_dict(rabbit))

    return jsonify(result)


@app.route("/api/rabbits/<rabbit_id>/death", methods=["PATCH"])
def mark_rabbit_dead(rabbit_id):
    conn = get_conn()
    cursor = conn.cursor()

    try:
        cursor.execute("""
            UPDATE Rabbit
            SET Status = 'Dead'
            WHERE RabbitID = %s
        """, (rabbit_id,))

        if cursor.rowcount == 0:
            conn.rollback()
            return jsonify({
                "success": False,
                "message": "未找到该兔子"
            }), 404

        conn.commit()

        return jsonify({
            "success": True,
            "message": "兔子已标记为死亡"
        })

    except Exception as e:
        conn.rollback()

        return jsonify({
            "success": False,
            "message": str(e)
        }), 500

    finally:
        cursor.close()
        conn.close()

@app.route("/api/delete/<rabbit_id>", methods=["DELETE"])
def delete_rabbit(rabbit_id):
    conn = get_conn()
    cursor = conn.cursor()

    try:
        # Check whether this rabbit is referenced by descendants
        cursor.execute("""
            SELECT RabbitID
            FROM Rabbit
            WHERE FatherID = %s
               OR MotherID = %s
            LIMIT 1
        """, (rabbit_id, rabbit_id))

        child = cursor.fetchone()

        if child:
            return jsonify({
                "success": False,
                "message": "该兔子已存在后代谱系引用，不能永久删除。可以将其标记为死亡。"
            }), 400

        cursor.execute("""
            DELETE FROM Rabbit
            WHERE RabbitID = %s
        """, (rabbit_id,))

        if cursor.rowcount == 0:
            conn.rollback()
            return jsonify({
                "success": False,
                "message": "未找到该兔子"
            }), 404

        conn.commit()

        return jsonify({
            "success": True,
            "message": "兔子记录已永久删除"
        })

    except Exception as e:
        conn.rollback()

        return jsonify({
            "success": False,
            "message": str(e)
        }), 500

    finally:
        cursor.close()
        conn.close()

@app.route("/api/feed/start", methods=["POST"])
def start_feed_task():
    data = request.json or {}
    rabbit_id = data.get("RabbitID")

    if not rabbit_id:
        return jsonify({
            "success": False,
            "message": "RabbitID 不能为空"
        }), 400

    rabbit = get_rabbit_by_id(rabbit_id)

    if not rabbit:
        return jsonify({
            "success": False,
            "message": "未找到该兔子"
        }), 404

    return jsonify({
        "success": True,
        "message": "模拟喂养任务已创建",
        "RabbitID": rabbit_id,
        "Home": rabbit.get("Home"),
        "mode": "simulation"
    })

@app.route("/api/feed/complete", methods=["POST"])
def complete_feed_task():
    data = request.json or {}

    rabbit_id = data.get("RabbitID")

    if not rabbit_id:
        return jsonify({
            "success": False,
            "message": "RabbitID 不能为空"
        }), 400

    return jsonify({
        "success": True,
        "RabbitID": rabbit_id,
        "message": "模拟喂养任务完成",
        "mode": "simulation"
    })


# ============================================================
# Ear Tag Scanner API
# ============================================================

latest_ear_tag = {
    "RabbitID": None
}


@app.route("/api/ear-tag", methods=["GET", "POST"])
def receive_ear_tag():

    # Frontend gets and consumes the latest scanned tag
    if request.method == "GET":
        rabbit_id = latest_ear_tag["RabbitID"]

        # Consume the event
        latest_ear_tag["RabbitID"] = None

        return jsonify({
            "success": True,
            "RabbitID": rabbit_id,
            "message": "Ear tag API is running"
        })

    # PDA uploads scanned tag
    data = request.get_json(silent=True) or {}

    rabbit_id = str(data.get("RabbitID", "")).strip()

    if not rabbit_id:
        return jsonify({
            "success": False,
            "message": "RabbitID cannot be empty"
        }), 400

    latest_ear_tag["RabbitID"] = rabbit_id

    print(f"[EarTag] Received: {rabbit_id}")

    return jsonify({
        "success": True,
        "RabbitID": rabbit_id,
        "message": "Ear tag received successfully"
    }), 200

if __name__ == "__main__":
    init_db()
    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True
    )
