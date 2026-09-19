// main.js
let rabbits = [];
let filteredRabbits = [];
let currentPage = 1;
const pageSize = 10;
let currentRabbitView = "active";
let parentCandidates = [];

// =====================
// 加载兔子列表
// =====================
async function loadRabbits() {
    try {
        const res = await fetch(
            `/api/rabbits?view=${currentRabbitView}`
        );
        rabbits = await res.json();
        filteredRabbits = rabbits;
        currentPage = 1;
        renderPage();
    } catch (err) {
        console.error(err);
        alert("加载兔子数据失败！");
    }
}

// =====================
// 计算年龄（年月）
// =====================
function calculateAge(birthDateStr) {
    if (!birthDateStr) return "";
    const birth = new Date(birthDateStr);
    const today = new Date();

    let years = today.getFullYear() - birth.getFullYear();
    let months = today.getMonth() - birth.getMonth();
    if (today.getDate() < birth.getDate()) months -= 1;
    if (months < 0) {
        years -= 1;
        months += 12;
    }
    return `${years}岁${months}个月`;
}

// =====================
// 渲染分页表格
// =====================
function renderPage() {
    const start = (currentPage - 1) * pageSize;
    const end = start + pageSize;
    const pageData = filteredRabbits.slice(start, end);
    renderRabbits(pageData);
    renderPagination();
}

function renderRabbits(data) {
    const tbody = document.querySelector("#rabbitTable tbody");
    tbody.innerHTML = "";

    data.forEach(r => {
        const age = calculateAge(r.BirthDate);

        const tr = document.createElement("tr");

        tr.innerHTML = `
            <td>
                <span class="rabbit-link"
                      onclick="openRabbitPage('${r.RabbitID}')">
                    ${r.RabbitID}
                </span>
            </td>

            <td>
                ${r.Gender === "Male"
                    ? "公兔"
                    : r.Gender === "Female"
                    ? "母兔"
                    : ""}
            </td>

            <td>${r.BirthDate || ""}</td>

            <td>${age}</td>

            <td>${r.Bloodline || ""}</td>

            <td>${r.Home || ""}</td>

            <td>${r.Status || ""}</td>

            <td>${r.Source || ""}</td>

            <td class="action-cell">

                ${String(r.Status || "").toLowerCase() !== "dead"
                    ? `
                        <button
                            class="death-btn"
                            onclick="markRabbitDead('${r.RabbitID}')">
                            死亡
                        </button>
                      `
                    : `
                        <span class="dead-label">
                            已死亡
                        </span>
                      `
                }

                <button
                    class="delete-btn"
                    onclick="deleteRabbit('${r.RabbitID}')">
                    删除
                </button>

            </td>
        `;

        tbody.appendChild(tr);
    });
}
// =====================
// 打开兔子主页弹窗
// =====================
// =====================
// 打开兔子主页弹窗（带谱系树）
// =====================
async function openRabbitPage(rabbitID) {
    try {
        const [resBasic, resTree] = await Promise.all([
            fetch(`/api/rabbits/${rabbitID}`),
            fetch(`/api/lineage/${rabbitID}?depth=2`)
        ]);

        
        const lineage = await resTree.json();

        const result = await resBasic.json();
        const r = result.data;

        if (!result.success || !r) {
            alert("未找到该兔子信息");
            return;
        }

        const age = calculateAge(r.BirthDate);
        const modal = document.getElementById("rabbitPageModal");

        // 关键：给弹窗本体加上遮罩层样式，同时确保内容区域能点击
        modal.style.cssText = `
            display: flex;
            position: fixed;
            inset: 0;
            background: rgba(0,0,0,0.5);
            z-index: 9999;
            align-items: center;
            justify-content: center;
            padding: 20px;
        `;

        // 弹窗内容：白色卡片
        modal.innerHTML = `
            <div style="
                background: white;
                border-radius: 12px;
                max-width: 850px;
                width: 100%;
                max-height: 90vh;
                overflow-y: auto;
                position: relative;
                padding: 24px;
                box-shadow: 0 20px 60px rgba(0,0,0,0.3);
                pointer-events: auto;
            " onclick="event.stopPropagation()">
                
                <!-- 右上角 X 关闭按钮（绝对定位，确保在最上层） -->
                <button onclick="closeRabbitPage()" style="
                    position: absolute;
                    top: 12px;
                    right: 16px;
                    font-size: 28px;
                    line-height: 1;
                    background: none;
                    border: none;
                    cursor: pointer;
                    color: #999;
                    z-index: 100;
                    padding: 0;
                    width: 36px;
                    height: 36px;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    border-radius: 50%;
                " onmouseover="this.style.background='#f0f0f0'" onmouseout="this.style.background='none'">&times;</button>

                <!-- 内容区：左右分栏 -->
                <div style="display: flex; gap: 24px; margin-top: 8px;">
                    
                    <!-- 左侧：基本信息 -->
                    <div style="flex: 1; min-width: 240px;">
                        <h3 style="margin-top: 0; color: #2c3e50;">🐰 ${r.RabbitID}</h3>
                        <p><b>性别：</b>${r.Gender === "Male" ? "公兔 ♂" : r.Gender === "Female" ? "母兔 ♀" : "-"}</p>
                        <p><b>出生日期：</b>${r.BirthDate || "-"}（${age}）</p>
                        <p><b>血统：</b><span style="color:#e67e22; font-weight:600;">${r.Bloodline || "-"}</span></p>
                        <p><b>父兔：</b>${r.FatherID || "-"}</p>
                        <p><b>母兔：</b>${r.MotherID || "-"}</p>
                        <hr style="border:none; border-top:1px solid #eee; margin:12px 0;">
                        <p><b>胎次：</b>${r.Parity !== null ? r.Parity : "-"}</p>
                        <p><b>同窝仔数：</b>${r.LitterSize !== null ? r.LitterSize : "-"}</p>
                        <p><b>近亲系数：</b>${r.InbreedingCoeff !== null ? r.InbreedingCoeff : "-"}</p>
                        <p><b>舍：</b>${r.Home || "-"}</p>
                        <p><b>状态：</b>${r.Status || "-"}</p>
                        <p><b>来源：</b>${r.Source || "-"}</p>
                        
                        <div style="margin-top: 16px;">
                            <button onclick="closeRabbitPage()" style="padding:8px 16px; margin-right:8px; cursor:pointer;">关闭</button>
                            <button onclick="closeRabbitPage(); openEditModal('${r.RabbitID}')" style="padding:8px 16px; cursor:pointer;">编辑</button>
                            <button onclick="startFeedingDemo('${r.RabbitID}', '${r.Home || ""}')"
                                style="
                                    padding:8px 16px;
                                    margin-left:8px;
                                    cursor:pointer;
                                    background:#e67e22;
                                    color:white;
                                    border:none;
                                    border-radius:8px;
                                ">
                                🍼 开始喂养
                            </button>
                        </div>
                    </div>

                    <!-- 右侧：谱系树 -->
                    <div style="flex: 1; min-width: 240px; background:#f8f9fa; border-radius:8px; padding:16px;">
                        <h3 style="margin-top:0; color:#2c3e50; border-bottom:2px solid #e67e22; padding-bottom:6px; font-size:16px;">谱系树（三代）</h3>
                        <div id="lineageTreeContainer" style="font-size:13px;"></div>
                    </div>
                </div>
            </div>
        `;

        // 渲染谱系树
        if (lineage.success && lineage.data) {
            const container = document.getElementById("lineageTreeContainer");
            renderTreeNode(lineage.data, container, "", true);
        } else {
            document.getElementById("lineageTreeContainer").innerHTML = 
                '<p style="color:#999;">未录入父母信息，无法显示谱系</p>';
        }

        // 保险1：点击黑色背景也能关闭
        modal.onclick = function(e) {
            if (e.target === modal) closeRabbitPage();
        };

        // 保险2：按 ESC 键关闭
        document.addEventListener('keydown', handleEsc);

    } catch (err) {
        console.error(err);
        alert("加载兔子主页失败！");
    }
}

// =====================
// 关闭弹窗
// =====================
function closeRabbitPage() {
    const modal = document.getElementById("rabbitPageModal");
    if (modal) {
        modal.style.display = "none";
        modal.innerHTML = "";
        modal.onclick = null;  // 清理事件，防止内存泄漏
    }
    document.removeEventListener('keydown', handleEsc);  // 移除ESC监听
}

// ESC 键处理
function handleEsc(e) {
    if (e.key === 'Escape') closeRabbitPage();
}

// =====================
// 分页按钮
// =====================
function renderPagination() {
    const totalPages = Math.ceil(filteredRabbits.length / pageSize);
    const container = document.getElementById("pagination");
    container.innerHTML = "";

    for (let i = 1; i <= totalPages; i++) {
        const btn = document.createElement("button");
        btn.textContent = i;
        btn.className = (i === currentPage) ? "active" : "";
        btn.addEventListener("click", () => {
            currentPage = i;
            renderPage();
        });
        container.appendChild(btn);
    }
}

// =====================
// 搜索功能
// =====================
function searchRabbits() {
    const keyword = document.getElementById("searchInput").value.trim().toLowerCase();
    if (!keyword) {
        filteredRabbits = rabbits;
    } else {
        filteredRabbits = rabbits.filter(r =>
            (r.RabbitID && r.RabbitID.toLowerCase().includes(keyword)) ||
            (r.Bloodline && r.Bloodline.toLowerCase().includes(keyword))
        );
    }
    currentPage = 1;
    renderPage();
}

// =====================
// 新增、编辑、删除（保持原样，可用）
// =====================
async function addRabbit() {
    const data = {
        RabbitID: document.getElementById("RabbitID").value.trim(),
        Gender: document.getElementById("Gender").value,
        BirthDate: document.getElementById("BirthDate").value,
        Bloodline: document.getElementById("Bloodline").value.trim(),
        FatherID: document.getElementById("FatherID").value.trim() || null,
        MotherID: document.getElementById("MotherID").value.trim() || null,
        Home: document.getElementById("Home").value.trim(),
        Status: document.getElementById("Status").value.trim(),
        Source: document.getElementById("Source").value.trim(),
        Parity: document.getElementById("Parity").value,
        LitterSize: document.getElementById("LitterSize").value,
        InbreedingCoeff: document.getElementById("InbreedingCoeff").value
    };
    try {
        const res = await fetch("/api/rabbits", {
            method: "POST",
            headers: {"Content-Type": "application/json"},
            body: JSON.stringify(data)
        });
        const result = await res.json();
        if (result.success) {
            alert("添加成功！");
            loadRabbits();
            loadParentCandidates();
        } else {
            alert("添加失败：" + result.message);
        }
    } catch (err) {
        console.error(err);
        alert("添加失败！");
    }
}

function openEditModal(rabbitID) {
    const r = rabbits.find(r => r.RabbitID === rabbitID);
    if (!r) return;
    document.getElementById("editRabbitID").value = r.RabbitID;
    document.getElementById("editGender").value = r.Gender || "";
    document.getElementById("editBirthDate").value = r.BirthDate || "";
    document.getElementById("editBloodline").value = r.Bloodline || "";
    document.getElementById("editFatherID").value = r.FatherID || "";
    document.getElementById("editMotherID").value = r.MotherID || "";
    document.getElementById("editHome").value = r.Home || "";
    document.getElementById("editStatus").value = r.Status || "";
    document.getElementById("editSource").value = r.Source || "";
    document.getElementById("editModal").style.display = "block";

    const modal = document.getElementById("editModal");
        modal.style.display = "flex";
}

function closeEditModal() {
    document.getElementById("editModal").style.display = "none";
}

async function saveEdit() {
    const rabbitID = document.getElementById("editRabbitID").value;
    const data = {
        Gender: document.getElementById("editGender").value,
        BirthDate: document.getElementById("editBirthDate").value,
        Bloodline: document.getElementById("editBloodline").value.trim(),
        FatherID: document.getElementById("editFatherID").value.trim() || null,
        MotherID: document.getElementById("editMotherID").value.trim() || null,
        Home: document.getElementById("editHome").value.trim(),
        Status: document.getElementById("editStatus").value.trim(),
        Source: document.getElementById("editSource").value.trim()
    };
    try {
        const res = await fetch(`/api/rabbits/${rabbitID}`, {
            method: "PUT",
            headers: {"Content-Type": "application/json"},
            body: JSON.stringify(data)
        });
        const result = await res.json();
        if (result.success) {
            alert("修改成功！");
            closeEditModal();
            loadRabbits();
            loadParentCandidates();
        } else {
            alert("修改失败：" + result.message);
        }
    } catch (err) {
        console.error(err);
        alert("修改失败！");
    }
}

async function deleteRabbit(rabbitID) {
    if (!confirm(
        `确定永久删除兔子 ${rabbitID} 吗？\n\n仅建议用于录入错误或测试数据。此操作无法恢复。`
    )) {
        return;
    }

    try {
        const res = await fetch(
            `/api/delete/${rabbitID}`,
            {
                method: "DELETE"
            }
        );

        const result = await res.json();

        if (result.success) {
            alert("记录已删除");
            loadRabbits();
            loadParentCandidates();
        } else {
            alert(result.message || "删除失败");
        }

    } catch (err) {
        console.error(err);
        alert("删除失败");
    }
}

// =====================
// 查询谱系
// =====================
async function queryLineage() {
    const rabbitID = document.getElementById("queryRabbitID").value.trim();
    const depth = document.getElementById("lineageDepth").value;
    if(!rabbitID){ alert("请输入RabbitID"); return; }
    const res = await fetch(`/api/lineage/${rabbitID}?depth=${depth}`);
    const result = await res.json();
    const box = document.getElementById("lineageResult");
    if(!result.success){ box.innerHTML = result.message; return; }
    box.innerHTML = renderLineage(result.data,0,"当前兔");
}

// 递归渲染谱系树
function renderLineage(node, level, role) {
    if(!node) return "";
    const indent = "&nbsp;".repeat(level*6);
    let html = `<div class="node">${indent}<span class="node-title">${role}</span>：${node.RabbitID}, 性别：${node.Gender || "无"}, 血统：${node.Bloodline || "无"}, 笼舍：${node.Home || "无"}</div>`;
    if(node.Father) html += renderLineage(node.Father,level+1,"父亲");
    if(node.Mother) html += renderLineage(node.Mother,level+1,"母亲");
    return html;
}

// =====================
// 筛选纯种
// =====================
async function queryPurebred() {
    const depth = document.getElementById("pureDepth").value;
    const bloodline = document.getElementById("pureBloodline").value.trim();
    let url = `/api/purebred?depth=${depth}`;
    if(bloodline) url += `&bloodline=${encodeURIComponent(bloodline)}`;
    const res = await fetch(url);
    const data = await res.json();
    const tbody = document.getElementById("pureTable");
    tbody.innerHTML = "";
    if(data.length===0){
        tbody.innerHTML = `<tr><td colspan="8">没有符合条件的纯种兔子</td></tr>`;
        return;
    }
    data.forEach(r=>{
        const age = calculateAge(r.BirthDate);
        const tr = document.createElement("tr");
        tr.innerHTML = `
            <td><span class="rabbit-link" onclick="openRabbitPage('${r.RabbitID}')">${r.RabbitID}</span></td>
            <td>${r.Gender === "Male" ? "公兔" : r.Gender === "Female" ? "母兔" : ""}</td>
            <td>${r.BirthDate}</td>
            <td>${age}</td>
            <td>${r.Bloodline}</td>
            <td>${r.FatherID || ""}</td>
            <td>${r.MotherID || ""}</td>
            <td>${r.Home || ""}</td>
        `;
        tbody.appendChild(tr);
    });
}

// =====================
// 页面初始化
// =====================
window.onload = () => {
    loadRabbits();
    loadParentCandidates();
};

// =====================
// 递归渲染谱系树节点
// =====================
function renderTreeNode(node, container, relation = "", isRoot = false) {
    if (!node || !node.RabbitID) return;

    const wrapper = document.createElement("div");
    wrapper.style.cssText = isRoot 
        ? "position:relative; margin:6px 0;" 
        : "position:relative; padding-left:24px; margin:4px 0;";

    // 连线（竖线）
    if (!isRoot) {
        const line = document.createElement("div");
        line.style.cssText = "position:absolute; left:10px; top:0; bottom:0; width:2px; background:#ddd;";
        wrapper.appendChild(line);
    }

    // 关系标签颜色：父系蓝色，母系红色/粉色
    const relationColor = relation === "父亲" ? "#3498db" : relation === "母亲" ? "#e74c3c" : "#666";
    const relationBadge = relation 
        ? `<span style="font-size:10px; padding:1px 5px; border-radius:3px; background:${relationColor}15; color:${relationColor}; margin-right:6px; font-weight:600;">${relation}</span>` 
        : "";

    // 节点卡片
    const genderColor = node.Gender === "Male" ? "#3498db" : node.Gender === "Female" ? "#e74c3c" : "#999";
    const genderText = node.Gender === "Male" ? "公" : node.Gender === "Female" ? "母" : "?";
    
    const card = document.createElement("div");
    card.style.cssText = `
        position: relative;
        background: white;
        border: 1px solid #e0e0e0;
        border-radius: 6px;
        padding: 8px 12px;
        margin-bottom: 4px;
        display: flex;
        align-items: center;
        gap: 6px;
        box-shadow: 0 1px 3px rgba(0,0,0,0.06);
    `;

    // 横线连接到父节点
    if (!isRoot) {
        const hLine = document.createElement("div");
        hLine.style.cssText = "position:absolute; left:-14px; top:50%; width:14px; height:2px; background:#ddd;";
        card.appendChild(hLine);
    }

    card.innerHTML += `
        ${relationBadge}
        <span style="font-weight:700; font-family:monospace; font-size:13px; color:#2c3e50;">${node.RabbitID}</span>
        <span style="font-size:11px; padding:2px 8px; border-radius:4px; background:${genderColor}15; color:${genderColor}; font-weight:600;">${genderText}</span>
        <span style="color:#e67e22; font-weight:600; font-size:12px;">${node.Bloodline || "无血统"}</span>
        ${node.BirthDate ? `<span style="color:#bbb; font-size:11px;">${node.BirthDate}</span>` : ""}
    `;

    wrapper.appendChild(card);

    // 递归渲染父母，带上关系标签
    const childrenBox = document.createElement("div");
    childrenBox.style.marginTop = "2px";
    
    if (node.Father && node.Father.RabbitID) {
        renderTreeNode(node.Father, childrenBox, "父亲");
    }
    if (node.Mother && node.Mother.RabbitID) {
        renderTreeNode(node.Mother, childrenBox, "母亲");
    }
    
    if (node.Father || node.Mother) {
        wrapper.appendChild(childrenBox);
    }

    container.appendChild(wrapper);
}

// =====================
// 初始化：只绑定一次事件
// =====================
document.addEventListener("DOMContentLoaded", () => {
    const btn = document.getElementById("quickSubmitBtn");
    if (btn) {
        btn.addEventListener("click", quickAddRabbit);
    }
    const normalFather =
        document.getElementById("FatherID");

    const normalMother =
        document.getElementById("MotherID");
    
    const runNormalMatingCheck = () => {

        checkMating(
            normalFather.value.trim(),
            normalMother.value.trim(),
            document.getElementById("InbreedingCoeff"),
            null
        );
    };

    normalFather.addEventListener(
        "change",
        runNormalMatingCheck
    );

    normalMother.addEventListener(
        "change",
        runNormalMatingCheck
    );

});

// =====================
// 打开弹窗
// =====================
function openQuickAddModal() {

    const modal =
        document.getElementById("quickAddModal");

    if (!modal) return;

    modal.style.display = "flex";

    // 聚焦到性别
    setTimeout(() => {

        const input =
            document.getElementById("quickGender");

        if (input) input.focus();

    }, 100);
}

// =====================
// 关闭弹窗
// =====================
function closeQuickAddModal() {

    const modal =
        document.getElementById("quickAddModal");

    if (modal) {

        modal.style.display = "none";
    }

    clearQuickForm();
}
// =====================
// 提交函数（核心）
// =====================
async function quickAddRabbit() {

    // 安全获取 DOM（避免 null.value 崩溃）
    const getVal = (id) => {
        const el = document.getElementById(id);
        return el ? el.value.trim() : "";
    };

    const data = {
        RabbitID: getVal("quickRabbitID"),
        Name: getVal("quickName"),
        Gender: document.getElementById("quickGender")?.value || "",
        BirthDate: document.getElementById("quickBirthDate")?.value || "",

        FatherID: getVal("quickFatherID") || null,
        MotherID: getVal("quickMotherID") || null,

        Bloodline: getVal("quickBloodline"),

        Parity: getVal("quickParity") ? parseInt(getVal("quickParity")) : null,
        LitterSize: getVal("quickLitterSize") ? parseInt(getVal("quickLitterSize")) : null,
        InbreedingCoeff: getVal("quickInbreedingCoeff") ? parseFloat(getVal("quickInbreedingCoeff")) : null,

        Home: getVal("quickHome"),
        Status: document.getElementById("quickStatus")?.value || "",
        Source: "耳标录入"
    };

    console.log("提交数据:", data);

    try {
        const res = await fetch("/api/rabbits", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(data)
        });

        const result = await res.json();

        if (result.success) {
            closeQuickAddModal();
            loadRabbits();
            loadParentCandidates();
        } else {
            alert(result.message || "录入失败");
        }

    } catch (err) {
        console.error(err);
        alert("网络错误或后端异常");
    }
}
// =====================
// 清空表单
// =====================
function clearQuickForm() {

    document.getElementById("quickRabbitID").value = "";
    document.getElementById("quickBirthDate").value = "";
    document.getElementById("quickFatherID").value = "";
    document.getElementById("quickMotherID").value = "";
    document.getElementById("quickBloodline").value = "";
    document.getElementById("quickParity").value = "";
    document.getElementById("quickLitterSize").value = "";
    document.getElementById("quickInbreedingCoeff").value = "";
    document.getElementById("quickHome").value = "";
}

// =====================
// F8 打开快速录入
// =====================
document.addEventListener("keydown", function (e) {

    // F8
    if (e.key === "F8") {

        e.preventDefault();

        // 打开弹窗
        openQuickAddModal();

        // 清空旧数据
        clearQuickForm();

        // 自动聚焦耳标输入框
        const input = document.getElementById("quickRabbitID");

        if (input) {
            input.focus();
        }
    }
});



// =====================================================
// MVP：自动喂养演示
// =====================================================

async function startFeedingDemo(rabbitID, home) {

    // 先通知后端创建任务
    try {
        const res = await fetch("/api/feed/start", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({
                RabbitID: rabbitID
            })
        });

        const result = await res.json();

        if (!result.success) {
            alert(result.message || "任务创建失败");
            return;
        }

    } catch (err) {
        console.error(err);
        alert("无法连接后端");
        return;
    }

    // 创建演示弹窗
    let modal = document.getElementById("feedingDemoModal");

    if (!modal) {
        modal = document.createElement("div");
        modal.id = "feedingDemoModal";
        modal.className = "modal";
        document.body.appendChild(modal);
    }

    modal.style.display = "flex";

    modal.innerHTML = `
        <div class="modal-content"
             style="max-width:600px; text-align:center;">

            <h2>🐰 自动喂养任务</h2>

            <div style="
                display:inline-block;
                padding:5px 12px;
                margin-bottom:14px;
                border-radius:20px;
                background:#fff3cd;
                color:#856404;
                font-size:13px;
            ">
                ⚠ 演示模式 Simulation
            </div>

            <p>
                <b>RabbitID：</b>${rabbitID}
            </p>

            <p>
                <b>目标笼舍：</b>${home || "未设置"}
            </p>

            <hr>

            <div id="feedingStatus"
                 style="
                    font-size:20px;
                    font-weight:bold;
                    margin:25px 0;
                 ">
                正在创建任务...
            </div>

            <div style="
                width:100%;
                background:#eee;
                height:18px;
                border-radius:10px;
                overflow:hidden;
            ">
                <div id="feedingProgress"
                     style="
                        width:0%;
                        height:100%;
                        background:#1f7a4d;
                        transition:width .5s;
                     ">
                </div>
            </div>

            <div id="feedingLog"
                 style="
                    text-align:left;
                    margin-top:20px;
                    padding:15px;
                    background:#f8f9fa;
                    border-radius:8px;
                    min-height:150px;
                    font-family:monospace;
                    line-height:1.8;
                 ">
            </div>

            <button id="feedingCloseBtn"
                    onclick="closeFeedingDemo()"
                    style="margin-top:20px;"
                    disabled>
                任务执行中
            </button>

        </div>
    `;

    const steps = [

        {
            text: "正在定位兔笼...",
            log: "工位定位 → " + (home || "目标位置"),
            progress: 15
        },

        {
            text: "✓ 定位完成",
            log: "X/Z 轴定位完成",
            progress: 30
        },

        {
            text: "正在下降奶嘴...",
            log: "执行机构下降",
            progress: 40
        },

        {
            text: "✓ 接触检测完成",
            log: "检测接触 → 回抬 10 mm",
            progress: 50
        },

        {
            text: "🍼 正在喂养...",
            log: "FEED：奶源阀开启",
            progress: 65
        },

        {
            text: "正在排出余液...",
            log: "DRAIN：排出主管残奶",
            progress: 75
        },

        {
            text: "正在清洗管路...",
            log: "CLEAN：清洁液冲洗",
            progress: 85
        },

        {
            text: "正在清水冲洗...",
            log: "RINSE：清水冲洗",
            progress: 95
        },

        {
            text: "✓ 自动喂养任务完成",
            log: "DONE：任务完成并回写状态",
            progress: 100
        }
    ];

    const status = document.getElementById("feedingStatus");
    const progress = document.getElementById("feedingProgress");
    const log = document.getElementById("feedingLog");

    for (let i = 0; i < steps.length; i++) {

        status.innerHTML = steps[i].text;

        progress.style.width =
            steps[i].progress + "%";

        log.innerHTML +=
            `✓ ${steps[i].log}<br>`;

        await sleep(800);
    }

    // 通知后端任务完成
    try {

        await fetch("/api/feed/complete", {

            method: "POST",

            headers: {
                "Content-Type": "application/json"
            },

            body: JSON.stringify({
                RabbitID: rabbitID
            })
        });

    } catch (err) {
        console.error(err);
    }

    const btn =
        document.getElementById("feedingCloseBtn");

    btn.disabled = false;
    btn.innerText = "关闭";

    status.innerHTML =
        "✅ 自动喂养流程完成";
}


function sleep(ms) {

    return new Promise(resolve =>
        setTimeout(resolve, ms)
    );
}


function closeFeedingDemo() {

    const modal =
        document.getElementById("feedingDemoModal");

    if (modal) {
        modal.style.display = "none";
    }
}


// ============================================================
// Ear Tag Scanner Interface
// Hardware side should call this function after a successful scan.
// Example:
//     window.onEarTagScanned("E200001234567890");
// ============================================================

window.onEarTagScanned = function (tagId) {
    console.log("Ear tag received:", tagId);

    if (!tagId) {
        console.error("Empty ear tag ID received.");
        return;
    }

    // Convert to string and remove spaces
    const rabbitId = String(tagId).trim();

    if (!rabbitId) {
        console.error("Invalid ear tag ID.");
        return;
    }

    // Find the RabbitID input in quick-add modal
    const rabbitIdInput = document.getElementById("quickRabbitID");

    if (!rabbitIdInput) {
        console.error("RabbitID input not found.");
        return;
    }

    // Fill RabbitID automatically
    rabbitIdInput.value = rabbitId;

    // Trigger input/change events if other code listens to this field
    rabbitIdInput.dispatchEvent(
        new Event("input", { bubbles: true })
    );

    rabbitIdInput.dispatchEvent(
        new Event("change", { bubbles: true })
    );

    console.log("RabbitID filled successfully:", rabbitId);
};


// ============================================================
// Ear Tag Polling
// Get the latest RabbitID received by Flask from PDA
// ============================================================

async function pollEarTag() {
    try {
        const res = await fetch("/api/ear-tag");
        const data = await res.json();

        if (!data.success || !data.RabbitID) {
            return;
        }

        const rabbitId = String(data.RabbitID).trim();

        // Avoid processing the same scan repeatedly
        if (!rabbitId) {
            return;
        }

        lastEarTagId = rabbitId;

        console.log("New ear tag received from server:", rabbitId);

        // Open quick-add modal automatically
        openQuickAddModal();

        // Reuse the function you already have
        window.onEarTagScanned(rabbitId);

        // Focus the next field
        const genderInput = document.getElementById("quickGender");
        if (genderInput) {
            genderInput.focus();
        }

    } catch (err) {
        console.error("Ear tag polling failed:", err);
    }
}


// Start polling after the page is loaded
document.addEventListener("DOMContentLoaded", () => {
    setInterval(pollEarTag, 800);
});

document.addEventListener("DOMContentLoaded", () => {
    const father = document.getElementById("quickFatherID");
    const mother = document.getElementById("quickMotherID");

    const runCheck = () => {
        checkMating(
            father.value.trim(),
            mother.value.trim(),
            document.getElementById("quickInbreedingCoeff"),
            document.getElementById("quickMatingResult")
        );
    };

    father.addEventListener("change", runCheck);
    mother.addEventListener("change", runCheck);
});

async function markRabbitDead(rabbitID) {
    if (!confirm(
        `确定将兔子 ${rabbitID} 标记为死亡吗？\n\n谱系记录将继续保留。`
    )) {
        return;
    }

    try {
        const res = await fetch(
            `/api/rabbits/${rabbitID}/death`,
            {
                method: "PATCH"
            }
        );

        const result = await res.json();

        if (result.success) {
            alert("已标记为死亡");
            loadRabbits();
            loadParentCandidates();
        } else {
            alert(result.message || "操作失败");
        }

    } catch (err) {
        console.error(err);
        alert("操作失败");
    }
}

async function checkMating(fatherID, motherID, coeffInput, resultBox) {
    if (!fatherID || !motherID) {
        if (coeffInput) coeffInput.value = "";
        if (resultBox) resultBox.innerHTML = "";
        return;
    }

    try {
        const res = await fetch(
            `/api/mating-check?father_id=${encodeURIComponent(fatherID)}&mother_id=${encodeURIComponent(motherID)}`
        );

        const data = await res.json();

        if (!data.success) {
            if (coeffInput) coeffInput.value = "";
            if (resultBox) {
                resultBox.innerHTML =
                    `<span style="color:#d9534f;">${data.message}</span>`;
            }
            return;
        }

        if (coeffInput) {
            coeffInput.value = data.InbreedingCoeff;
        }

        if (!resultBox) return;

        let ancestors = "无已知共同祖先";

        if (data.CommonAncestors.length > 0) {
            ancestors = data.CommonAncestors
                .map(a =>
                    `${a.RabbitID}（父系${a.FatherDistance}代 / 母系${a.MotherDistance}代）`
                )
                .join("、");
        }

        resultBox.innerHTML = `
            <div>
                <b>预计后代近交系数：</b>
                ${data.InbreedingPercent}%
            </div>

            <div>
                <b>共同祖先：</b>
                ${ancestors}
            </div>

            <div>
                <b>血统：</b>
                ${data.FatherBloodline || "-"}
                ×
                ${data.MotherBloodline || "-"}
                ${data.SameBloodline ? "（同血统）" : ""}
            </div>
        `;

    } catch (err) {
        console.error("Mating check failed:", err);
    }
}

function changeRabbitView(view) {
    currentRabbitView = view;

    document.querySelectorAll(".status-tab")
        .forEach(btn => btn.classList.remove("active"));

    const target = document.getElementById(
        view === "active"
            ? "activeTab"
            : view === "dead"
            ? "deadTab"
            : "allTab"
    );

    if (target) {
        target.classList.add("active");
    }

    loadRabbits();
    loadParentCandidates();
}

async function runMatingCheck() {

    const fatherID =
        document.getElementById("matingFatherID")
            .value.trim();

    const motherID =
        document.getElementById("matingMotherID")
            .value.trim();

    const resultBox =
        document.getElementById("matingCheckResult");

    if (!fatherID || !motherID) {
        resultBox.innerHTML =
            `<div class="mating-error">
                请选择公兔和母兔
             </div>`;
        return;
    }

    resultBox.innerHTML = "正在检查...";

    await checkMating(
        fatherID,
        motherID,
        null,
        resultBox
    );
}

async function loadParentCandidates() {

    try {

        const res = await fetch("/api/rabbits?view=all");
        parentCandidates = await res.json();

        renderParentCandidates();

    } catch (err) {

        console.error(
            "加载父母候选失败：",
            err
        );
    }
}
function renderParentCandidates() {

    const maleList =
        document.getElementById("maleRabbitList");

    const femaleList =
        document.getElementById("femaleRabbitList");

    if (!maleList || !femaleList) return;

    maleList.innerHTML = "";
    femaleList.innerHTML = "";

    parentCandidates.forEach(r => {

        const option =
            document.createElement("option");

        option.value = r.RabbitID;

        option.label =
            `${r.RabbitID} | ${r.Bloodline || "无血统"} | ${r.BirthDate || "未知日期"}`;

        if (r.Gender === "Male") {
            maleList.appendChild(option);
        }

        if (r.Gender === "Female") {
            femaleList.appendChild(option);
        }
    });
}