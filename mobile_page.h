#ifndef MOBILE_PAGE_H
#define MOBILE_PAGE_H

#include <Arduino.h>

const char mobilePage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>СЭС — мобильная</title>
<style>
  * { box-sizing: border-box; }
  body {
    margin: 0;
    padding: 12px;
    background: #f0f2f5;
    color: #222;
    font-family: -apple-system, "Segoe UI", Roboto, Arial, sans-serif;
    -webkit-text-size-adjust: 100%;
  }
  .wrap { max-width: 520px; margin: 0 auto; }

  .topbar { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }
  .topbar h1 { font-size: 19px; margin: 0; }
  .conn { font-size: 13px; color: #888; display: flex; align-items: center; gap: 6px; }
  .dot { width: 10px; height: 10px; border-radius: 50%; background: #2ba85e; }
  body.offline .dot { background: #cd2b4a; }

  .links { display: flex; gap: 8px; margin-bottom: 12px; }
  .links a {
    flex: 1; text-align: center; padding: 10px 6px; background: #fff; border-radius: 10px;
    text-decoration: none; color: #2b7fcd; font-size: 14px; font-weight: bold;
    box-shadow: 0 1px 3px rgba(0,0,0,.08);
  }

  .card {
    background: #fff; border-radius: 12px; padding: 12px 14px; margin-bottom: 12px;
    box-shadow: 0 1px 4px rgba(0,0,0,.08);
  }
  .card h2 {
    font-size: 12px; text-transform: uppercase; letter-spacing: .6px; color: #9a9a9a;
    margin: 0 0 4px; font-weight: bold;
  }
  .row {
    display: flex; justify-content: space-between; align-items: flex-start; gap: 12px;
    padding: 9px 0; border-bottom: 1px solid #f1f1f1; font-size: 16px;
  }
  .row:last-of-type { border-bottom: none; }
  .k { color: #555; }
  .v { font-weight: bold; text-align: right; }
  .sub { display: block; font-size: 12px; font-weight: normal; color: #9a9a9a; margin-top: 2px; }

  .pos { color: #2ba85e; }
  .neg { color: #cd6b2b; }
  .muted { color: #b0b0b0; }
  .on  { color: #2ba85e; }
  .off { color: #999; }
  .warn { color: #cd2b4a; }

  .btn {
    width: 100%; padding: 15px; margin-top: 10px; font-size: 16px; font-weight: bold;
    border: none; border-radius: 10px; color: #fff; background: #2b92cd;
  }
  .btn:active { opacity: .85; }
  .btn.stop { background: #cd2b4a; }
</style>
</head>
<body>
<div class="wrap">

  <div class="topbar">
    <h1>СЭС</h1>
    <div class="conn"><span class="dot"></span><span id="connText">Online</span></div>
  </div>

  <div class="links">
    <a href="/">Схема</a>
    <a href="/bms">BMS</a>
    <a href="/settings">Настройки</a>
  </div>

  <div class="card">
    <h2>Аккумулятор</h2>
    <div class="row"><span class="k">Уровень заряда</span><span class="v"><span id="bat-soc">--</span> %</span></div>
    <div class="row"><span class="k">Напряжение</span><span class="v"><span id="bat-v">--</span> В</span></div>
    <div class="row">
      <span class="k">Ток</span>
      <span class="v"><span id="bat-i">--</span> А<span class="sub" id="bat-dir">—</span></span>
    </div>
    <div class="row"><span class="k">Мощность</span><span class="v"><span id="bat-p">--</span> Вт</span></div>
  </div>

  <div class="card">
    <h2>Солнечные панели</h2>
    <div class="row"><span class="k">Суммарная мощность</span><span class="v"><span id="sun-total">--</span> Вт</span></div>
    <div class="row"><span class="k">MPPT 1</span><span class="v"><span id="mppt-1-val">—</span><span class="sub" id="mppt-1-sub">нет связи</span></span></div>
    <div class="row"><span class="k">MPPT 2</span><span class="v"><span id="mppt-2-val">—</span><span class="sub" id="mppt-2-sub">нет связи</span></span></div>
    <div class="row"><span class="k">MPPT 3</span><span class="v"><span id="mppt-3-val">—</span><span class="sub" id="mppt-3-sub">нет связи</span></span></div>
    <div class="row"><span class="k">MPPT 4</span><span class="v"><span id="mppt-4-val">—</span><span class="sub" id="mppt-4-sub">нет связи</span></span></div>
  </div>

  <div class="card">
    <h2>Шина (выход MPPT)</h2>
    <div class="row"><span class="k">Напряжение</span><span class="v"><span id="bus-v">--</span> В</span></div>
    <div class="row"><span class="k">Ток</span><span class="v"><span id="bus-i">--</span> А</span></div>
  </div>

  <div class="card">
    <h2>Нагрузка</h2>
    <div class="row"><span class="k">Основная 12 В</span><span class="v"><span id="load-main">--</span> А</span></div>
    <div class="row"><span class="k">5 В (через преобразователь)</span><span class="v"><span id="load-dc">--</span> А</span></div>
    <div class="row"><span class="k">Итого</span><span class="v"><span id="load-total">--</span> А</span></div>
  </div>

  <div class="card">
    <h2>Сеть и зарядное устройство</h2>
    <div class="row">
      <span class="k">Сеть</span>
      <span class="v"><span id="net-v">--</span> В<span class="sub" id="net-state">—</span></span>
    </div>
    <div class="row"><span class="k">Зарядное устройство</span><span class="v" id="chg-state">—</span></div>
    <div class="row"><span class="k">Режим включения</span><span class="v" id="chg-mode">—</span></div>
    <div class="row"><span class="k">Автоматика</span><span class="v" id="chg-auto">—</span></div>
    <button class="btn" id="chgBtn">Включить заряд от сети</button>
  </div>

  <div class="card">
    <h2>Энергия за сутки <span id="day" style="text-transform:none;letter-spacing:0;font-weight:normal;"></span></h2>
    <div class="row"><span class="k">Выработано</span><span class="v"><span id="e-gen">--</span> Втч</span></div>
    <div class="row"><span class="k">Потреблено</span><span class="v"><span id="e-cons">--</span> Втч</span></div>
    <div class="row"><span class="k">Батарея получила</span><span class="v pos"><span id="e-chg">--</span> Втч</span></div>
    <div class="row"><span class="k">Батарея отдала</span><span class="v neg"><span id="e-dis">--</span> Втч</span></div>
  </div>

</div>

<script>
  // Коды состояния зарядки Victron (CS)
  const MPPT_STATES = {0: 'Off', 2: 'Fault', 3: 'Bulk', 4: 'Absorption', 5: 'Float', 6: 'Storage', 7: 'Equalize'};

  function num(v) {
    const n = parseFloat(v);
    return isNaN(n) ? null : n;
  }
  function fmt(v, d) {
    const n = num(v);
    return n === null ? '--' : n.toFixed(d);
  }
  function fmtSigned(v, d) {
    const n = num(v);
    if (n === null) return '--';
    return (n > 0 ? '+' : '') + n.toFixed(d);
  }
  function setText(id, text) { document.getElementById(id).innerText = text; }
  function setClass(id, cls) { document.getElementById(id).className = cls; }

  function updateData() {
    fetch('api/solar-data')
      .then(r => r.json())
      .then(json => {
        document.body.classList.remove('offline');
        setText('connText', 'Online');

        // --- Аккумулятор ---
        const b = json.bms['bms-1'];
        const bi = num(b.current);
        const bv = num(b.voltage);
        setText('bat-soc', fmt(b.level_percent, 1));
        setText('bat-v', fmt(b.voltage, 2));
        setText('bat-i', fmtSigned(b.current, 2));
        setClass('bat-i', bi > 0.01 ? 'pos' : (bi < -0.01 ? 'neg' : ''));
        setText('bat-dir', bi > 0.01 ? 'Заряд' : (bi < -0.01 ? 'Разряд' : 'Простой'));
        if (bi !== null && bv !== null) {
          const p = bi * bv;
          setText('bat-p', (p > 0 ? '+' : '') + p.toFixed(0));
          setClass('bat-p', p > 0.5 ? 'pos' : (p < -0.5 ? 'neg' : ''));
        } else {
          setText('bat-p', '--');
        }

        // --- Солнечные панели ---
        let total = 0;
        ['mppt-1', 'mppt-2', 'mppt-3', 'mppt-4'].forEach(key => {
          const m = json.solar[key];
          if (m && m.status === 'on') {
            const pw = num(m.power) || 0;
            total += pw;
            setText(key + '-val', pw.toFixed(0) + ' Вт');
            const st = MPPT_STATES[m.state] || '—';
            setText(key + '-sub', fmt(m.voltage, 1) + ' В · ' + fmt(m.current, 2) + ' А · ' + st);
          } else {
            setText(key + '-val', '—');
            setText(key + '-sub', 'нет связи');
          }
        });
        setText('sun-total', total.toFixed(0));

        // --- Шина ---
        setText('bus-v', fmt(json.bus.voltage, 2));
        setText('bus-i', fmt(json.bus.current, 2));

        // --- Нагрузка ---
        const lm = num(json.load.main) || 0;
        const ld = num(json.load.dc) || 0;
        setText('load-main', lm.toFixed(2));
        setText('load-dc', ld.toFixed(2));
        setText('load-total', (Math.abs(lm) + Math.abs(ld)).toFixed(2));

        // --- Сеть и зарядное ---
        setText('net-v', '~' + (json.load.mainsVoltage || 0));
        const netOn = json.load.mainsStatus === 'on';
        setText('net-state', netOn ? 'Есть' : 'Нет');

        const ch = json.charger;
        const chOn = ch.state === 'on';
        setText('chg-state', chOn ? 'Включено' : 'Выключено');
        setClass('chg-state', 'v ' + (chOn ? 'on' : 'off'));

        let mode = '—';
        if (ch.source === 'manual') mode = 'Ручной';
        else if (ch.source === 'auto') mode = 'Автоматический';
        setText('chg-mode', mode);

        setText('chg-auto', ch.automationEnabled ? 'Включена' : 'Выключена');
        setClass('chg-auto', 'v ' + (ch.automationEnabled ? 'on' : 'off'));

        const btn = document.getElementById('chgBtn');
        btn.innerText = chOn ? 'Выключить заряд от сети' : 'Включить заряд от сети';
        btn.className = 'btn' + (chOn ? ' stop' : '');

        // --- Энергия за сутки ---
        const e = json.energy;
        if (e) {
          setText('e-gen', e.today_wh);
          setText('e-cons', e.consumed_today_wh);
          setText('e-chg', e.battery_charged_today_wh);
          setText('e-dis', e.battery_discharged_today_wh);
          setText('day', e.day ? '(' + e.day + ')' : '');
        }
      })
      .catch(() => {
        document.body.classList.add('offline');
        setText('connText', 'Offline');
      });
  }

  document.getElementById('chgBtn').onclick = () => {
    fetch('/api/charger')
      .then(r => r.text())
      .then(() => updateData())
      .catch(() => alert('Ошибка связи'));
  };

  updateData();
  setInterval(updateData, 1000);
</script>
</body>
</html>
 )rawliteral";

#endif
