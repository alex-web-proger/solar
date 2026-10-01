#ifndef BMS_PAGE_H
#define BMS_PAGE_H

#include <Arduino.h>

const char bmsPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>JK-BMS — обмен данных по UART</title>
<style>
  body {
    font-family: Arial, sans-serif;
    background: #f0f2f5;
    display: flex;
    justify-content: center;
    padding: 24px 15px;
    box-sizing: border-box;
  }
  .box {
    background: #fff;
    border-radius: 10px;
    padding: 22px 26px;
    max-width: 480px;
    width: 100%;
    box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    box-sizing: border-box;
  }
  h1 { font-size: 19px; margin-top: 0; margin-bottom: 4px; }
  a.back { display: inline-block; margin-bottom: 14px; font-size: 13px; color: #2b92cd; text-decoration: none; }

  .status-badge {
    display: inline-block;
    padding: 3px 10px;
    border-radius: 12px;
    font-size: 13px;
    font-weight: bold;
    margin-bottom: 16px;
  }
  .status-badge.ok { background: #e4f7ea; color: #2ba85e; }
  .status-badge.bad { background: #fdeaea; color: #cd2b4a; }
  .status-badge.charge { background: #e4f7ea; color: #2ba85e; }
  .status-badge.discharge { background: #fff2e4; color: #cd6b2b; }
  .status-badge.idle { background: #e6f0fa; color: #2b7fcd; }

  .alarm-banner {
    display: none;
    background: #fdeaea;
    color: #cd2b4a;
    border: 1px solid #f3b8c0;
    border-radius: 8px;
    padding: 8px 12px;
    font-size: 13px;
    font-weight: bold;
    margin-bottom: 16px;
  }

  .grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
    margin-bottom: 18px;
  }
  .metric {
    border: 1px solid #eee;
    border-radius: 8px;
    padding: 10px 12px;
  }
  .metric .label { font-size: 12px; color: #888; margin-bottom: 2px; }
  .metric .value { font-size: 20px; font-weight: bold; color: #222; }
  .metric .sub { font-size: 12px; color: #aaa; margin-top: 2px; }

  .section-title { font-size: 13px; text-transform: uppercase; letter-spacing: 0.5px; color: #aaa; margin: 18px 0 10px; border-top: 1px solid #eee; padding-top: 14px; }
  .section-title:first-of-type { border-top: none; padding-top: 0; margin-top: 0; }

  .mos-row { display: flex; gap: 10px; margin-bottom: 4px; }
  .mos-chip {
    flex: 1;
    text-align: center;
    padding: 8px;
    border-radius: 6px;
    font-size: 13px;
    font-weight: bold;
    border: 2px solid #ccc;
    color: #888;
  }
  .mos-chip.on { border-color: #2ba85e; color: #2ba85e; }
  .mos-chip.off { border-color: #cd2b4a; color: #cd2b4a; }
  button.mos-chip {
    background: #fff;
    font-family: inherit;
    cursor: pointer;
    transition: transform 0.15s ease;
  }
  button.mos-chip:hover:not(:disabled) { transform: scale(1.04); }
  button.mos-chip:active:not(:disabled) { transform: scale(0.97); }
  button.mos-chip:disabled { cursor: default; opacity: 0.6; }

  table.cells { width: 100%; border-collapse: collapse; font-size: 15px; }
  table.cells th, table.cells td { padding: 4px 6px; text-align: right; border-bottom: 1px solid #f0f0f0; }
  table.cells th:first-child, table.cells td:first-child { text-align: left; }
  table.cells td.max { color: #cd6b2b; font-weight: bold; }
  table.cells td.min { color: #2b92cd; font-weight: bold; }

  .info-line { font-size: 12px; color: #999; margin-top: 4px; }
</style>
</head>
<body>
<div class="box">
  <a class="back" href="/">&larr; На главную</a>
  <h1>JK-BMS — обмен данных по UART</h1>
  <div id="statusBadge" class="status-badge bad">Нет данных</div>
  <div id="stateBadge" class="status-badge idle">—</div>
  <div id="alarmBanner" class="alarm-banner">⚠ Активна тревога</div>

  <div class="grid">
    <div class="metric">
      <div class="label">Напряжение</div>
      <div class="value"><span id="voltage">--</span> В</div>
    </div>
    <div class="metric">
      <div class="label">Ток</div>
      <div class="value"><span id="current">--</span> А</div>
      <div class="sub" id="currentState">—</div>
    </div>
    <div class="metric">
      <div class="label">SOC (по BMS)</div>
      <div class="value"><span id="soc">--</span> %</div>
    </div>
    <div class="metric">
      <div class="label">Циклы заряда</div>
      <div class="value"><span id="cycles">--</span></div>
    </div>
    <div class="metric">
      <div class="label">Остаток ёмкости</div>
      <div class="value"><span id="remainingAh">--</span> Ач</div>
      <div class="sub" id="capacitySub">—</div>
    </div>
    <div class="metric">
      <div class="label">Мощность</div>
      <div class="value"><span id="power">--</span> Вт</div>
    </div>
  </div>

  <div class="section-title">MOS-ключи и балансировка</div>
  <div class="mos-row">
    <button id="chargeMos" class="mos-chip" disabled title="Нажмите, чтобы разрешить или запретить заряд в BMS">Заряд: --</button>
    <div id="dischargeMos" class="mos-chip">Разряд: --</div>
    <div id="balanceChip" class="mos-chip">Баланс: --</div>
  </div>

  <div class="section-title">Температуры</div>
  <div class="grid">
    <div class="metric">
      <div class="label">Силовые ключи</div>
      <div class="value"><span id="powerTemp">--</span> °C</div>
    </div>
    <div class="metric">
      <div class="label">Плата (box)</div>
      <div class="value"><span id="boxTemp">--</span> °C</div>
    </div>
    <div class="metric">
      <div class="label">Батарея</div>
      <div class="value"><span id="batteryTemp">--</span> °C</div>
    </div>
    <div class="metric">
      <div class="label">Delta по ячейкам</div>
      <div class="value"><span id="cellDelta">--</span> В</div>
    </div>
  </div>

  <div class="section-title">Напряжения ячеек (<span id="numCells">0</span> шт.)</div>
  <table class="cells">
    <thead><tr><th>Ячейка</th><th>Напряжение, В</th></tr></thead>
    <tbody id="cellsBody"></tbody>
  </table>

  <div class="info-line" id="infoLine">—</div>
</div>

<script>
function fmt(v, digits) {
  if (v === null || v === undefined || isNaN(v)) return '--';
  return Number(v).toFixed(digits);
}

function loadData() {
  fetch('/api/bms-data')
    .then(r => r.json())
    .then(d => {
      const badge = document.getElementById('statusBadge');
      if (d.valid) {
        badge.innerText = 'Данные актуальны';
        badge.className = 'status-badge ok';
      } else {
        badge.innerText = 'Нет валидных данных (проверьте подключение UART)';
        badge.className = 'status-badge bad';
      }

      document.getElementById('voltage').innerText = fmt(d.voltage, 2);
      document.getElementById('current').innerText = fmt(d.current, 2);
      document.getElementById('currentState').innerText =
        d.isCharging ? 'Заряжается' : (d.isDischarging ? 'Разряжается' : 'Простой');
      document.getElementById('soc').innerText = d.soc ?? '--';
      document.getElementById('cycles').innerText = d.cycles ?? '--';

      const stateBadge = document.getElementById('stateBadge');
      if (d.status === 'charge') {
        stateBadge.innerText = '⚡ Charge';
        stateBadge.className = 'status-badge charge';
      } else if (d.status === 'discharge') {
        stateBadge.innerText = '🔋 Discharge';
        stateBadge.className = 'status-badge discharge';
      } else {
        stateBadge.innerText = '⏸ Idle';
        stateBadge.className = 'status-badge idle';
      }

      const alarmBanner = document.getElementById('alarmBanner');
      if (d.hasAlarm) {
        alarmBanner.style.display = 'block';
        alarmBanner.innerText = '⚠ Активна тревога (код: 0x' + Number(d.alarmRaw).toString(16).padStart(4, '0') + ')';
      } else {
        alarmBanner.style.display = 'none';
      }

      document.getElementById('power').innerText = fmt(d.power, 1);
      document.getElementById('remainingAh').innerText = fmt(d.remainingAh, 1);
      document.getElementById('capacitySub').innerHTML = 'из ' + fmt(d.capacityAh, 0) + ' Ач (<a href="/settings" style="color:inherit;text-decoration:underline;">задано вручную</a>)';

      document.getElementById('powerTemp').innerText = fmt(d.powerTemp, 1);
      document.getElementById('boxTemp').innerText = fmt(d.boxTemp, 1);
      document.getElementById('batteryTemp').innerText = fmt(d.batteryTemp, 1);
      document.getElementById('cellDelta').innerText = fmt(d.cellDelta, 3);

      const chargeMos = document.getElementById('chargeMos');
      chargeState = d.valid ? !!d.chargingEnabled : null;
      // Пока ждём подтверждения от BMS — показываем «...»; снимаем ожидание, когда статус совпал с запрошенным или вышел таймаут
      if (chargePending !== null && (!d.valid || d.chargingEnabled === chargePending || Date.now() > chargeDeadline)) {
        chargePending = null;
      }
      if (chargePending !== null) {
        chargeMos.innerText = 'Заряд: ...';
        chargeMos.disabled = true;
      } else {
        chargeMos.innerText = 'Заряд: ' + (d.chargingEnabled ? 'ON' : 'OFF');
        chargeMos.className = 'mos-chip ' + (d.chargingEnabled ? 'on' : 'off');
        chargeMos.disabled = !d.valid;
      }

      const dischargeMos = document.getElementById('dischargeMos');
      dischargeMos.innerText = 'Разряд: ' + (d.dischargingEnabled ? 'ON' : 'OFF');
      dischargeMos.className = 'mos-chip ' + (d.dischargingEnabled ? 'on' : 'off');

      const balanceChip = document.getElementById('balanceChip');
      balanceChip.innerText = 'Баланс: ' + (d.isBalancing ? 'ON' : 'OFF');
      balanceChip.className = 'mos-chip ' + (d.isBalancing ? 'on' : 'off');

      document.getElementById('numCells').innerText = d.numCells ?? 0;

      const tbody = document.getElementById('cellsBody');
      tbody.innerHTML = '';
      if (d.cells && d.cells.length > 0) {
        d.cells.forEach((v, i) => {
          const tr = document.createElement('tr');
          const cls = (v === d.highestCell) ? 'max' : (v === d.lowestCell) ? 'min' : '';
          tr.innerHTML = '<td>#' + (i + 1) + '</td><td class="' + cls + '">' + Number(v).toFixed(3) + '</td>';
          tbody.appendChild(tr);
        });
      }

      document.getElementById('infoLine').innerText =
        (d.swVersion ? ('ПО: ' + d.swVersion + '  ') : '') + (d.deviceInfo ? ('· ' + d.deviceInfo) : '');
    })
    .catch(() => {
      document.getElementById('statusBadge').innerText = 'Ошибка запроса к устройству';
      document.getElementById('statusBadge').className = 'status-badge bad';
    });
}

// Управление разрешением заряда в BMS: кнопка «Заряд: ON/OFF»
let chargeState = null;      // текущее состояние MOS заряда (null = нет валидных данных)
let chargePending = null;    // запрошенное состояние, пока BMS не подтвердил
let chargeDeadline = 0;

document.getElementById('chargeMos').onclick = () => {
  if (chargeState === null || chargePending !== null) return;
  const want = !chargeState;
  if (!want && !confirm('Запретить заряд в BMS?\nПока заряд запрещён, батарея не будет заряжаться ни от MPPT, ни от сети.')) return;
  chargePending = want;
  chargeDeadline = Date.now() + 12000;
  const chip = document.getElementById('chargeMos');
  chip.innerText = 'Заряд: ...';
  chip.disabled = true;
  fetch('/api/bms/charge?enable=' + (want ? 1 : 0))
    .then(r => { if (!r.ok) throw new Error('HTTP ' + r.status); })
    .catch(() => {
      chargePending = null;
      alert('Не удалось отправить команду в BMS');
      loadData();
    });
};

loadData();
setInterval(loadData, 2000);
</script>
</body>
</html>
 )rawliteral";

#endif
