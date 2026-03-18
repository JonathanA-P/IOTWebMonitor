// Cek config
if (typeof firebaseConfig === 'undefined' || firebaseConfig.projectId === "YOUR_PROJECT_ID" || firebaseConfig.projectId === "") {
  const n = document.getElementById('notice');
  if(n) {
    n.textContent = '⚠ Isi firebaseConfig di dalam file js/config.js terlebih dahulu!';
    n.classList.add('show');
  }
}

try {
  firebase.initializeApp(firebaseConfig);
} catch (e) {
  console.error("Firebase init failed", e);
}

const db = firebase.firestore();
let allData = [];
let shMode = 'suhu', elMode = 'daya';

// ===== CHARTS =====
const cOpt = {
  responsive:true, animation:false,
  plugins:{legend:{display:false}},
  scales:{
    x:{ticks:{color:'#6b7280',font:{family:'JetBrains Mono',size:9},maxTicksLimit:6},grid:{color:'#1a1e27'}},
    y:{ticks:{color:'#6b7280',font:{family:'JetBrains Mono',size:9}},grid:{color:'#1a1e27'}}
  }
};
function ds(label,data,color){
  return{label,data,borderColor:color,backgroundColor:color+'18',borderWidth:1.5,
         pointRadius:2,pointHoverRadius:4,fill:true,tension:0.3};
}
const csh = new Chart(document.getElementById('csh').getContext('2d'),
  {type:'line',options:cOpt,data:{labels:[],datasets:[ds('Suhu',[],  '#f87171')]}});
const cel = new Chart(document.getElementById('cel').getContext('2d'),
  {type:'line',options:cOpt,data:{labels:[],datasets:[ds('Daya',[],  '#a78bfa')]}});

window.sw = function(id, btn, mode) {
  document.querySelectorAll('#t-'+id+' .tab').forEach(b=>b.classList.remove('active'));
  btn.classList.add('active');
  if(id==='sh') shMode=mode; else elMode=mode;
  renderCharts();
}

function renderCharts() {
  const pts = allData.slice(0,30).reverse();
  const lbl = pts.map(d=>fmtTime(d.timestamp));

  // SH
  if(shMode==='both'){
    csh.data.labels=lbl;
    csh.data.datasets=[ds('Suhu',pts.map(d=>d.suhu),'#f87171'),ds('Humid',pts.map(d=>d.humid),'#60a5fa')];
    csh.options.plugins.legend.display=true;
  } else {
    const m={suhu:['Suhu','#f87171'],humid:['Humid','#60a5fa']};
    const[l,c]=m[shMode]||m.suhu;
    csh.data.labels=lbl;
    csh.data.datasets=[ds(l,pts.map(d=>d[shMode]),c)];
    csh.options.plugins.legend.display=false;
  }
  csh.update();

  // EL
  const em={daya:['Daya','#a78bfa','daya'],tegangan:['Tegangan','#fbbf24','tegangan'],arus:['Arus','#34d399','arus']};
  const[el,ec,ek]=em[elMode]||em.daya;
  cel.data.labels=lbl;
  cel.data.datasets=[ds(el,pts.map(d=>d[ek]),ec)];
  cel.update();
}

// ===== CARDS =====
function setVal(id,v,dec){
  const e=document.getElementById(id);
  if(e) e.textContent=isNaN(v)?'—':parseFloat(v).toFixed(dec);
}
function updateCards(d){
  setVal('v-suhu', d.suhu,  1); setVal('v-humid', d.humid, 1);
  setVal('v-teg',  d.tegangan, 1); setVal('v-arus', d.arus, 2);
  setVal('v-daya', d.daya,  1); setVal('v-kwhh',d.kwh_hari,3);
  setVal('v-kwhb', d.kwh_bulan, 2);

  // Network metrics
  setVal('v-delay',      d.delay_ms,    1);
  setVal('v-throughput', d.throughput,  2);
  setVal('v-packetloss', d.packet_loss, 1);

  // RSSI + kualitas sinyal
  const rssi = parseInt(d.rssi);
  const rEl  = document.getElementById('v-rssi');
  const qEl  = document.getElementById('v-rssi-quality');
  const dEl  = document.getElementById('v-rssi-desc');
  if (rEl) rEl.textContent = isNaN(rssi) ? '—' : rssi;
  let quality, desc, color;
  if      (rssi >= -50) { quality='Sangat Baik'; desc='-50 dBm ke atas';  color='#22c55e'; }
  else if (rssi >= -60) { quality='Baik';        desc='-50 ~ -60 dBm';    color='#86efac'; }
  else if (rssi >= -70) { quality='Cukup';       desc='-60 ~ -70 dBm';    color='#f59e0b'; }
  else if (rssi >= -80) { quality='Lemah';       desc='-70 ~ -80 dBm';    color='#fb923c'; }
  else                  { quality='Sangat Lemah';desc='di bawah -80 dBm'; color='#ef4444'; }
  if (qEl) { qEl.textContent=quality; qEl.style.color=color; }
  if (dEl) dEl.textContent=desc;

  [1,2,3,4].forEach(i=>{
    const c=document.getElementById('ir'+i);
    if(!c) return;
    const on = d['ir'+i]===0;
    c.className='irc '+(on?'on':'');
    c.querySelector('.irstat').textContent=on?'TERDETEKSI':'KOSONG';
  });
  document.getElementById('lupdate').textContent=fmtTimeFull(d.timestamp);
}

// ===== TABLE =====
function updateTable(data){
  const tb=document.getElementById('tbody');
  if(!data.length){tb.innerHTML='<tr><td colspan="15" class="nodata">Belum ada data.</td></tr>';return;}
  tb.innerHTML=data.map(d=>`<tr>
    <td style="color:var(--muted)">${fmtTimeFull(d.timestamp)}</td>
    <td style="color:#f87171">${f(d.suhu,1)}</td>
    <td style="color:#60a5fa">${f(d.humid,1)}</td>
    <td style="color:#fbbf24">${f(d.tegangan,1)}</td>
    <td style="color:#34d399">${f(d.arus,2)}</td>
    <td style="color:#a78bfa">${f(d.daya,1)}</td>
    <td style="color:var(--muted)">${f(d.kwh_hari,3)}</td>
    ${[1,2,3,4].map(i=>{const on=d['ir'+i]===0;return`<td><span class="ib ${on?'ion':'ioff'}"></span>${on?'ON':'OFF'}</td>`;}).join('')}
    <td style="color:#22c55e">${f(d.rssi,0)}</td>
    <td style="color:#3b82f6">${f(d.delay_ms,1)}</td>
    <td style="color:#f59e0b">${f(d.throughput,2)}</td>
    <td style="color:#ef4444">${f(d.packet_loss,1)}</td>
  </tr>`).join('');
}

// ===== HELPERS =====
function f(v,d){return isNaN(v)?'—':parseFloat(v).toFixed(d);}
function fmtTime(ts){
  try{let d=ts?.toDate?ts.toDate():new Date(ts);return d.toLocaleTimeString('id-ID',{hour:'2-digit',minute:'2-digit',second:'2-digit'});}catch{return'—';}
}
function fmtTimeFull(ts){
  try{let d=ts?.toDate?ts.toDate():new Date(ts);return d.toLocaleString('id-ID',{day:'2-digit',month:'short',year:'numeric',hour:'2-digit',minute:'2-digit',second:'2-digit'});}catch{return'—';}
}
function nowStr(){return new Date().toISOString().slice(0,16).replace(/[:-]/g,'');}

// ===== EXPORT =====
window.exportCSV = function(){
  if(!allData.length) return alert('Belum ada data!');
  const h=['Waktu','Suhu','Humid','Tegangan','Arus','Daya','KWh_hari','KWh_bulan','IR1','IR2','IR3','IR4','RSSI','Delay_ms','Throughput_kbps','Packet_Loss'];
  const rows=allData.map(d=>[fmtTimeFull(d.timestamp),f(d.suhu,2),f(d.humid,2),f(d.tegangan,2),f(d.arus,3),f(d.daya,2),f(d.kwh_hari,3),f(d.kwh_bulan,3),d.ir1,d.ir2,d.ir3,d.ir4,f(d.rssi,0),f(d.delay_ms,1),f(d.throughput,2),f(d.packet_loss,1)]);
  const csv=[h,...rows].map(r=>r.join(',')).join('\n');
  const a=document.createElement('a');
  a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv'}));
  a.download='sensor_'+nowStr()+'.csv'; a.click();
}
window.exportXLSX = function(){
  if(!allData.length) return alert('Belum ada data!');
  const h=['Waktu','Suhu (°C)','Humid (%)','Tegangan (V)','Arus (A)','Daya (W)','KWh/hari','KWh/bulan','IR1','IR2','IR3','IR4','RSSI (dBm)','Delay (ms)','Throughput (kbps)','Packet Loss (%)'];
  const rows=allData.map(d=>[fmtTimeFull(d.timestamp),+f(d.suhu,2),+f(d.humid,2),+f(d.tegangan,2),+f(d.arus,3),+f(d.daya,2),+f(d.kwh_hari,3),+f(d.kwh_bulan,3),d.ir1,d.ir2,d.ir3,d.ir4,+f(d.rssi,0),+f(d.delay_ms,1),+f(d.throughput,2),+f(d.packet_loss,1)]);
  const wb=XLSX.utils.book_new();
  const ws=XLSX.utils.aoa_to_sheet([h,...rows]);
  ws['!cols']=[{wch:22},...h.slice(1).map(()=>({wch:14}))];
  XLSX.utils.book_append_sheet(wb,ws,'Log');
  XLSX.writeFile(wb,'sensor_'+nowStr()+'.xlsx');
}

// ===== LISTENER =====
db.collection('sensor_logs').orderBy('timestamp','desc').limit(100)
  .onSnapshot(snap=>{
    allData=snap.docs.map(d=>d.data());
    if(allData.length){updateCards(allData[0]);renderCharts();updateTable(allData);}
    document.getElementById('loadscr').classList.add('gone');
  }, err=>{
    document.getElementById('loadscr').classList.add('gone');
    const n=document.getElementById('notice');
    n.textContent='❌ Error Firebase: '+err.message;
    n.classList.add('show');
  });
