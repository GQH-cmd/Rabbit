// 加载兔子列表
async function loadRabbits() {
    const res = await fetch("/api/rabbits");
    const data = await res.json();
    const tbody = document.getElementById("rabbitTable");
    tbody.innerHTML = "";
    data.forEach(r => {
        const tr = document.createElement("tr");
        tr.innerHTML = `
            <td>${r.RabbitID}</td>
            <td>${r.Name}</td>
            <td>${r.Gender}</td>
            <td>${r.BirthDate}</td>
            <td>${r.Bloodline}</td>
            <td>${r.FatherID}</td>
            <td>${r.MotherID}</td>
            <td>${r.Home}</td>
            <td><button class="delete-btn" onclick="deleteRabbit('${r.RabbitID}')">删除</button></td>
        `;
        tbody.appendChild(tr);
    });
}

// 添加兔子
async function addRabbit() {
    const data = {
        RabbitID: document.getElementById("RabbitID").value.trim(),
        Name: document.getElementById("Name").value.trim(),
        Gender: document.getElementById("Gender").value,
        BirthDate: document.getElementById("BirthDate").value,
        Bloodline: document.getElementById("Bloodline").value.trim(),
        FatherID: document.getElementById("FatherID").value.trim(),
        MotherID: document.getElementById("MotherID").value.trim(),
        Home: document.getElementById("Home").value.trim()
    };
    const res = await fetch("/api/rabbits", { method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify(data) });
    const result = await res.json();
    alert(result.message);
    if(result.success) { clearForm(); loadRabbits(); }
}

function clearForm() {
    ["RabbitID","Name","Gender","BirthDate","Bloodline","FatherID","MotherID","Home"].forEach(id => document.getElementById(id).value = "");
}

// 删除兔子
async function deleteRabbit(rabbitID) {
    if(!confirm(`确定删除 ${rabbitID}?`)) return;
    const res = await fetch(`/api/delete/${rabbitID}`, { method:"DELETE" });
    const result = await res.json();
    alert(result.message);
    loadRabbits();
}

// 查询谱系
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
    let html = `<div class="node">${indent}<span class="node-title">${role}</span>：${node.RabbitID}, 名称：${node.Name || "无"}, 性别：${node.Gender || "无"}, 血统：${node.Bloodline || "无"}, 笼舍：${node.Home || "无"}</div>`;
    if(node.Father) html += renderLineage(node.Father,level+1,"父亲");
    if(node.Mother) html += renderLineage(node.Mother,level+1,"母亲");
    return html;
}

// 筛选纯种
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
        const tr = document.createElement("tr");
        tr.innerHTML = `<td>${r.RabbitID}</td><td>${r.Name}</td><td>${r.Gender}</td><td>${r.BirthDate}</td><td>${r.Bloodline}</td><td>${r.FatherID}</td><td>${r.MotherID}</td><td>${r.Home}</td>`;
        tbody.appendChild(tr);
    });
}

// 页面加载时刷新兔子列表
window.onload = loadRabbits;