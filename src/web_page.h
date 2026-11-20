/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000. GPL-3.0-or-later (ver LICENSE).
 */

#pragma once
// WebUI: una sola página; /api/status cada 2 s, /api/config para leer y guardar la configuración

static const char WEB_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>APRS LoRa+ NMEA 2000</title>
<link rel="icon" href="data:image/png;base64,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">
<style>
:root{--bg:#10151c;--card:#19212b;--fg:#e6edf3;--mut:#8b98a5;--acc:#4aa3ff;--red:#ff5d5d;--ok:#3fb950}
*{box-sizing:border-box}body{margin:0;font-family:system-ui,sans-serif;background:var(--bg);color:var(--fg)}
/* Cabecera como la del tracker APRS LoRa+: logo, "APRS LoRa+" y debajo "NMEA 2000" con la version */
header{background:#131a22;border-bottom:2px solid transparent;border-image:linear-gradient(90deg,#3ce68a,#28b5e1) 1;height:56px;display:flex;align-items:center;gap:10px;padding:0 16px;position:sticky;top:0;z-index:10}
header .logo{height:40px;width:40px;flex-shrink:0;border-radius:8px}
header .brand{font-weight:600;color:#fff;font-size:1.1rem;display:flex;flex-direction:column;line-height:1.1}
header .brand-sub{font-weight:400;font-size:.68rem;opacity:.95;letter-spacing:.02em;color:#28b5e1}
header .right{margin-left:auto;color:#fff;opacity:.85;font-size:12px;text-align:right}
main{max-width:1100px;margin:auto;padding:12px;display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(320px,1fr))}
.c{background:var(--card);border-radius:10px;padding:14px}.w{grid-column:1/-1}h3{margin:0 0 10px;font-size:15px;color:var(--mut);font-weight:600}
table{width:100%;border-collapse:collapse;font-size:13px}td,th{padding:4px 6px;text-align:left;border-bottom:1px solid #26303b}th{color:var(--mut);font-weight:600}
.kv td:first-child{color:var(--mut);width:42%}.okc{color:var(--ok)}.bad{color:var(--red)}.mut{color:var(--mut);font-size:13px}
button{background:#263241;color:var(--fg);border:1px solid #33475c;border-radius:8px;padding:9px 14px;font-size:14px;cursor:pointer}
button:active{background:var(--acc)}input,select,textarea{background:#0d1218;color:var(--fg);border:1px solid #33475c;border-radius:6px;padding:7px;font-size:14px}
textarea{width:100%;height:300px;font-family:monospace;font-size:12px}.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin:6px 0}
label{font-size:13px;color:var(--mut)}label input,label select{display:block;margin-top:3px;width:100%}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:8px}
pre{white-space:pre-wrap;word-break:break-all;font-size:12px;margin:0;max-height:260px;overflow:auto}a{color:var(--acc)}
.btn-donate{display:inline-flex;align-items:center;gap:8px;background:#0070e0;color:#fff!important;border-radius:999px;padding:8px 18px;font-weight:600;text-decoration:none;font-size:14px}.btn-donate:hover{background:#003087;color:#fff}#about p{margin:6px 0;font-size:13px}
</style></head><body>
<header><img class="logo" src="data:image/png;base64,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" alt="APRS LoRa+" width="40" height="40"><span class="brand"><span class="brand-name">APRS LoRa+</span><small class="brand-sub">NMEA 2000 <span id="ver"></span></small></span><span class="right" id="hdr"></span></header>
<main>
<div class="c"><h3>Tracker</h3><table class="kv" id="lk"></table></div>
<div class="c"><h3>NMEA 2000</h3><table class="kv" id="nk"></table></div>
<div class="c"><h3>Ambiente (HAT XIAO-LOG)</h3><table class="kv" id="lg"></table><table class="kv" id="an"></table></div>
<div class="c w"><h3>Blancos AIS en el plotter <span class="mut" id="cnt"></span></h3><table id="st"><thead><tr><th>Estación</th><th>MMSI</th><th>Clase</th><th>Posición</th><th>Rumbo / vel.</th><th>Dist.</th><th>Oída</th></tr></thead><tbody></tbody></table></div>
<div class="c w"><h3>Últimos paquetes</h3><pre id="rc"></pre></div>
<div class="c w"><h3>Configuración</h3>
<div class="grid">
<label>Recibir del tracker<select id="f_src"><option value="tcp">WiFi (KISS TCP)</option><option value="ble">Bluetooth (KISS BLE)</option><option value="both">Los dos</option></select></label>
<label>Tracker por WiFi (vacío = buscarlo)<input id="f_host" placeholder="192.168.4.2"></label>
<label>Puerto KISS TCP<input id="f_port" type="number"></label>
<label>Tracker por BLE (parte del nombre)<input id="f_ble" placeholder="vacío = el primero"></label>
<label>WiFi<select id="f_wm"><option value="2">Punto de acceso</option><option value="1">Cliente de otra red</option><option value="0">Apagada</option></select></label>
<label>Red (cliente)<input id="f_ssid"></label>
<label>Clave de la red<input id="f_pass" type="password"></label>
<label>Clave del punto de acceso<input id="f_ap" type="password"></label>
<label>MID del MMSI<input id="f_mid" type="number"></label>
<label>Repetir posición (s)<input id="f_ref" type="number"></label>
<label>Olvidar tras (min)<input id="f_ttl" type="number"></label>
<label>Distancia máx. (km, 0 = todas)<input id="f_km" type="number" step="0.1"></label>
<label>Estaciones APRS en tierra<select id="f_aton"><option value="1">Ayuda a la navegación (sin alarma)</option><option value="0">Buque AIS (clase B)</option></select></label>
<label>No enviar (indicativos)<input id="f_ign" placeholder="EA7JRS-5"></label>
<label>Emitir GPS del tracker al bus<select id="f_gps"><option value="0">No (el barco ya tiene GPS)</option><option value="1">Sí, si no hay otro GPS</option></select></label>
<label>Indicativo del tracker (el barco)<input id="f_own" placeholder="EA7JRS-5"></label>
<label>Tiempo de internet de la app al bus<select id="f_wx"><option value="0">No</option><option value="1">Sí, si no hay sensores</option></select></label>
<label>Cristal del MCP2515<select id="f_mhz"><option value="16">16 MHz</option><option value="8">8 MHz</option></select></label>
<label>Interrupción del MCP2515<select id="f_int"><option value="43">D6 (kit CAN)</option><option value="-1">ninguna (consulta cada ms)</option></select></label>
<label>Ambiente al bus<select id="f_env"><option value="1">Sí</option><option value="0">No</option></select></label>
<label>Batería en D1<select id="f_bat"><option value="0">No</option><option value="1">Sí</option></select></label>
<label>Anemómetro (D3)<select id="f_an"><option value="0">No</option><option value="1">Sí</option></select></label>
<label>km/h por impulso por segundo<input id="f_kmh" type="number" step="0.01"></label>
<label>Viento<select id="f_wref"><option value="apparent">Aparente (en el barco)</option><option value="true">Verdadero (en tierra)</option></select></label>
</div>
<div class="row"><button onclick="saveForm()">Guardar y reiniciar</button><button onclick="loadCfg()">Releer</button><span class="mut" id="msg"></span></div>
<details><summary class="mut">Configuración entera (JSON)</summary><textarea id="raw"></textarea>
<div class="row"><button onclick="saveRaw()">Guardar JSON y reiniciar</button><button onclick="post('/api/reboot','')">Reiniciar</button><a href="/update">Actualizar firmware</a></div></details>
</div>
<div class="c w" id="about"><h3>Acerca de</h3>
<p>APRS LoRa+ NMEA 2000 <span id="ver2"></span> · © 2026 Manuel - EA7JRS</p>
<p><b>APRS LoRa+</b> &mdash; EA7JRS: <a href="https://ea7jrs.github.io/APRS-LoRa-Plus/" target="_blank" rel="noopener">ea7jrs.github.io/APRS-LoRa-Plus</a> (instalador y proyecto: tracker, app Android, rotor, gateway, display y náutica).</p>
<p class="mut">Las estaciones APRS que oye el tracker salen en el plotter. Por defecto, las de radioaficionado (sin perfil «AIS:») van como <b>ayuda a la navegación virtual</b> (129041): el plotter las muestra como baliza, no como buque, y <b>no disparan la alarma de abordaje</b> (no son barcos reales). Los barcos con perfil «AIS:» (MMSI real) van como buque AIS (clase B: 129039, 129809 y 129810; clase A: 129038 y 129794), con alarma de abordaje normal. Se puede cambiar en Configuración. Temperatura y humedad: 130311, 130312 y 130313. Viento del anemómetro: 130306.</p>
<p class="mut">Librería NMEA2000 y NMEA2000_mcp (Timo Lappalainen, MIT), CAN_BUS_Shield (Seeed Studio, MIT), NimBLE-Arduino, ArduinoJson. Licencia GPL-3.0-or-later. <a href="https://github.com/EA7JRS">github.com/EA7JRS</a></p>
<p><a class="btn-donate" href="https://paypal.me/ea7jrs" target="_blank" rel="noopener">♥ Donar (PayPal.me/ea7jrs)</a></p>
</div>
</main>
<script>
const $=id=>document.getElementById(id);let cfg=null;
const esc=s=>String(s??'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'})[c]);
const kv=(id,rows)=>$(id).innerHTML=rows.filter(r=>r).map(r=>`<tr><td>${r[0]}</td><td>${r[1]}</td></tr>`).join('');
const ago=s=>s==null?'—':s<60?s+' s':s<3600?Math.floor(s/60)+' min':Math.floor(s/3600)+' h';
const dm=(v,p,n)=>{const a=Math.abs(v),d=Math.floor(a);return d+'°'+((a-d)*60).toFixed(3)+"'"+(v<0?n:p)};
const yes=(b,t,f)=>b?`<span class="okc">${t}</span>`:`<span class="bad">${f}</span>`;
async function tick(){try{const r=await fetch('/api/status');const s=await r.json();
$('ver').textContent=s.ver;$('ver2').textContent=s.ver;
$('hdr').innerHTML=esc(s.name)+'<br>'+(s.wifi.ap?'AP ':'')+esc(s.wifi.ip);
const l=s.link;kv('lk',[['Estado',esc(l.state)],['Por WiFi',yes(l.tcp,'conectado '+esc(l.tcpPeer),'no')],['Por BLE',yes(l.ble,'conectado '+esc(l.blePeer)+(l.bleText?' (TNC2)':''),'no')],l.seen?['Vistos por BLE',esc(l.seen)]:null,
['Recibido',l.bytes+' bytes · '+s.gate.frames+' tramas'],['Último dato',ago(l.ageS)],['Posiciones',s.gate.positions+(s.gate.ignored?' · '+s.gate.ignored+' ignoradas':'')+(s.gate.far?' · '+s.gate.far+' lejos':'')]]);
const n=s.n2k;kv('nk',[['Bus',yes(n.open,'abierto','cerrado: '+esc(n.spi||'sin MCP2515')+' (CANSTAT '+(n.canstat??0).toString(16).toUpperCase()+', CANCTRL '+(n.canctrl??0).toString(16).toUpperCase()+')')],['Dirección',n.addr],['Enviados',n.sent+(n.errors?' · <span class="bad">'+n.errors+' fallos</span>':'')],
['Recibidos',n.rx+' · '+n.devices+' equipos'],
['Errores CAN','emisión '+n.tec+' (máx. '+n.tecMax+') · recepción '+n.rec+' (máx. '+n.recMax+')'+(n.busOffNow?' · <span class="bad">fuera del bus</span>':n.errPassive?' · <span class="bad">error pasivo</span>':'')+(n.busOff?' · '+n.busOff+' veces fuera':'')+(n.mode?' · <span class="bad">modo '+n.mode+'</span>':'')],
['Tramas perdidas',n.lost?'<span class="bad">'+n.lost+'</span>':'0'],['Interrupción',n.int>=0?'GPIO'+n.int+' · '+n.ints:'no (cada ms)'],
['Equipos del bus',(n.list||[]).map(d=>d.src+': '+esc(d.manName||('fabricante '+d.man))+(d.model?' '+esc(d.model):'')+(d.sw?' <span class="mut">'+esc(d.sw)+'</span>':'')).join('<br>')||'—'],['Barco',n.lat!=null?dm(n.lat,'N','S')+' '+dm(n.lon,'E','W')+' ('+ago(n.posAgeS)+')':'—'],['Hora del bus',n.utc?new Date(n.utc*1000).toISOString().replace('T',' ').slice(0,19)+' UTC':'—'],
['WiFi',(s.wifi.ap?'punto de acceso '+esc(s.wifi.ssid)+' · '+(s.wifi.clients||0)+' equipos':esc(s.wifi.ssid)+(s.wifi.rssi?' ('+s.wifi.rssi+' dBm)':''))]]);
const g=s.log;kv('lg',g.on?[['Temperatura',g.t!=null?g.t+' °C':(g.sht?'—':'sin SHT40')],['Humedad',g.h!=null?g.h+' %':'—'],['Luz',g.lux!=null?g.lux+' lx':(g.light?'—':'sin BH1750')],
['Reloj',g.rtc?'PCF8563':'no'],g.bat!=null?['Batería',g.bat+' V']:null,['Hora',s.time?new Date(s.time*1000).toISOString().replace('T',' ').slice(0,19)+' UTC':'—']]:[['HAT','apagado']]);
const a=s.anemo,kn=v=>v==null?'—':v+' m/s · '+(v*3.6).toFixed(1)+' km/h · '+(v*1.94384).toFixed(1)+' kn';
kv('an',a?[['Viento (3 s)',kn(a.ws)],['Medio 2 / 10 min',a.avg2!=null?a.avg2+' / '+a.avg10+' m/s'+(a.bft!=null?' · Beaufort '+a.bft:''):'—'],['Racha (10 min)',kn(a.gust)],['Impulsos',a.hz+' Hz · '+a.pulses+' en total']]:[]);
$('cnt').textContent=s.gate.active+' activos';
$('st').tBodies[0].innerHTML=s.stations.sort((a,b)=>a.ageS-b.ageS).map(o=>`<tr class="${o.live?'':'mut'}"><td>${esc(o.name)}${o.name!=o.call?' <span class="mut">'+esc(o.call)+'</span>':''}</td><td>${o.mmsi}</td><td>${o.cls}</td><td>${dm(o.lat,'N','S')} ${dm(o.lon,'E','W')}</td><td>${o.cog!=null?o.cog+'°':'—'} ${o.sog!=null?o.sog+' kn':''}</td><td>${o.km!=null?o.km+' km':'—'}</td><td>${ago(o.ageS)}</td></tr>`).join('')||'<tr><td colspan="7" class="mut">Aún no hay estaciones con posición</td></tr>';
$('rc').textContent=s.recent.slice().reverse().join('\n');
}catch(e){$('hdr').textContent='sin conexión';}}
async function loadCfg(){cfg=await (await fetch('/api/config')).json();$('raw').value=JSON.stringify(cfg,null,2);
const t=cfg.tracker,a=cfg.ais,k=cfg.n2k,w=cfg.wifi;$('f_src').value=t.src;$('f_host').value=t.tcpHost;$('f_port').value=t.tcpPort;$('f_ble').value=t.bleName;
$('f_wm').value=w.mode;$('f_ssid').value=w.ssid;$('f_pass').value=w.pass;$('f_ap').value=w.apPass;$('f_mid').value=a.mid;$('f_ref').value=a.refreshS;$('f_ttl').value=a.ttlMin;
$('f_km').value=a.maxKm;$('f_aton').value=a.aton?1:0;$('f_ign').value=a.ignore;$('f_gps').value=cfg.gps.on?1:0;$('f_own').value=cfg.gps.ownCall;$('f_wx').value=cfg.gps.wx?1:0;$('f_mhz').value=k.mhz;$('f_int').value=k.int<0?-1:43;$('f_env').value=k.env?1:0;$('f_bat').value=cfg.log.battery?1:0;$('f_an').value=cfg.anemo.on?1:0;$('f_kmh').value=cfg.anemo.kmhPerHz;$('f_wref').value=cfg.anemo.ref;}
async function post(u,b){const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:b});const j=await r.json().catch(()=>({}));
$('msg').textContent=j.ok?'Guardado; reiniciando...':('Error: '+(j.err||r.status));if(j.ok)setTimeout(()=>location.reload(),6000);}
function saveForm(){const c={tracker:{src:$('f_src').value,tcpHost:$('f_host').value.trim(),tcpPort:+$('f_port').value,bleName:$('f_ble').value.trim()},
wifi:{mode:+$('f_wm').value,ssid:$('f_ssid').value,pass:$('f_pass').value,apPass:$('f_ap').value},
ais:{mid:+$('f_mid').value,refreshS:+$('f_ref').value,ttlMin:+$('f_ttl').value,maxKm:+$('f_km').value,aton:$('f_aton').value=='1',ignore:$('f_ign').value.trim()},
gps:{on:$('f_gps').value=='1',wx:$('f_wx').value=='1',ownCall:$('f_own').value.trim()},
n2k:{mhz:+$('f_mhz').value,int:+$('f_int').value,env:$('f_env').value=='1'},log:{battery:$('f_bat').value=='1'},
anemo:{on:$('f_an').value=='1',kmhPerHz:+$('f_kmh').value,ref:$('f_wref').value}};post('/api/config',JSON.stringify(c));}
function saveRaw(){try{JSON.parse($('raw').value)}catch(e){$('msg').textContent='JSON no válido';return}post('/api/config',$('raw').value);}
tick();loadCfg();setInterval(tick,2000);
</script></body></html>)HTML";
