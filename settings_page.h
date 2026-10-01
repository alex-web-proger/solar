#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <Arduino.h>

const char settingsPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Настройки автоподзарядки</title>
<style>
  body {
    font-family: Arial, sans-serif;
    background: #f0f2f5;
    display: flex;
    justify-content: center;
    padding: 30px 15px;
    box-sizing: border-box;
  }
  .settings-box {
    background: #fff;
    border-radius: 10px;
    padding: 25px 30px;
    max-width: 420px;
    width: 100%;
    box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    box-sizing: border-box;
  }
  h1 { font-size: 20px; margin-top: 0; }
  .field { margin-bottom: 16px; }
  label { display: block; font-size: 14px; margin-bottom: 4px; color: #333; font-weight: bold; }
  .hint { font-size: 12px; color: #888; margin-top: 3px; }
  input[type=number] {
    width: 100%;
    box-sizing: border-box;
    padding: 8px 10px;
    border: 1px solid #ccc;
    border-radius: 6px;
    font-size: 15px;
  }
  button {
    width: 100%;
    padding: 10px;
    background: #2b92cd;
    color: #fff;
    border: none;
    border-radius: 6px;
    font-size: 16px;
    font-weight: bold;
    cursor: pointer;
    margin-top: 8px;
  }
  button:active { background: #22729e; }
  #status { margin-top: 12px; font-size: 14px; text-align: center; min-height: 18px; }
  #status.ok { color: #2ba85e; }
  #status.err { color: #cd2b4a; }
  a.back { display: inline-block; margin-bottom: 14px; font-size: 13px; color: #2b92cd; text-decoration: none; }
  select {
    width: 100%;
    box-sizing: border-box;
    padding: 8px 10px;
    border: 1px solid #ccc;
    border-radius: 6px;
    font-size: 15px;
    background: #fff;
  }
  .switch-row { display: flex; align-items: center; justify-content: space-between; }
  .switch { position: relative; display: inline-block; width: 46px; height: 26px; }
  .switch input { opacity: 0; width: 0; height: 0; }
  .slider {
    position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
    background-color: #ccc; border-radius: 26px; transition: .2s;
  }
  .slider:before {
    position: absolute; content: ""; height: 20px; width: 20px; left: 3px; bottom: 3px;
    background-color: white; border-radius: 50%; transition: .2s;
  }
  .switch input:checked + .slider { background-color: #2ba85e; }
  .switch input:checked + .slider:before { transform: translateX(20px); }
  .section-title { font-size: 13px; text-transform: uppercase; letter-spacing: 0.5px; color: #aaa; margin: 22px 0 10px; border-top: 1px solid #eee; padding-top: 14px; }
  .section-title:first-of-type { border-top: none; padding-top: 0; margin-top: 0; }
</style>
</head>
<body>
<div class="settings-box">
  <a class="back" href="/">&larr; На главную</a>
  <h1>Настройки автоподзарядки от сети</h1>

  <div class="section-title">Автоматический режим</div>
  <div class="field">
    <div class="switch-row">
      <label style="margin-bottom:0;">Автоподзарядка активна</label>
      <label class="switch">
        <input type="checkbox" id="autoEnabled" onchange="toggleAutomation()">
        <span class="slider"></span>
      </label>
    </div>
    <div class="hint">При выключенном переключателе автоматика (по разряду и по высокому току нагрузки) не запускает подзарядку от сети. Ручное управление кнопкой на главной странице продолжает работать всегда.</div>
  </div>

  <div class="field">
    <div class="switch-row">
      <label style="margin-bottom:0;">Приоритет сети</label>
      <label class="switch">
        <input type="checkbox" id="mainsPrio" onchange="toggleMainsPriority()">
        <span class="slider"></span>
      </label>
    </div>
    <div class="hint">После каждого пропадания сети на заданное время включается повышенный порог подзарядки — чтобы к следующему блекауту батарея была максимально заряжена.</div>
  </div>
  <div class="field" id="mainsPrioParams" style="display:none;">
    <div id="mainsPrioStatus" style="font-size:13px;font-weight:bold;margin-bottom:10px;"></div>
    <label>Целевой SOC в окне приоритета (%)</label>
    <input type="number" id="mainsPrioTargetSoc" step="0.1" min="0" max="100">
    <div class="hint">Порог активации подзарядки на это время заменяется этим значением (обычно выше обычного экономного порога)</div>
    <label style="margin-top:12px;">Память о блекауте (часов)</label>
    <input type="number" id="mainsPrioMemoryH" step="1" min="0">
    <div class="hint">На сколько долго после пропадания сети держится повышенный порог. Если сеть стояла стабильно дольше этого времени — порог сам вернётся к обычному.</div>
  </div>

  <div class="section-title">Тип аккумулятора</div>
  <div class="field">
    <label>Аккумулятор</label>
    <select id="chemistry" onchange="switchChemistry()">
      <option value="gel">Гель/свинец (через INA3221 + кулонометр)</option>
      <option value="lfp">LiFePO4 (напрямую с JK-BMS)</option>
    </select>
    <div class="hint">Каждый тип хранит свои настройки ниже отдельно — переключение применяется сразу, без перезагрузки страницы</div>
  </div>
  <div class="field" id="lfpCapField" style="display:none;">
    <label>Е́мкость LFP-пакета (Ач)</label>
    <input type="number" id="lfpCapAh" step="1" min="1">
    <div class="hint">Номинальная ёмкость вашего пакета — используется для расчёта остатка в Ач и оставшегося времени на странице /bms (в самом протоколе JK нет надёжного поля для этого)</div>
  </div>
  <div class="field" id="cellImbalanceField" style="display:none;">
    <label>Порог разбалансировки ячеек — жёлтый (мВ)</label>
    <input type="number" id="cellWarnMv" step="1" min="0">
    <label style="margin-top:12px;">Порог разбалансировки ячеек — красный (мВ)</label>
    <input type="number" id="cellCritMv" step="1" min="0">
    <div class="hint">Разница между максимальным и минимальным напряжением ячеек. При достижении этих порогов на главной странице рядом с батареей мигает индикатор.</div>
  </div>

  <div class="section-title">Разряд батареи</div>
  <div class="field">
    <label>Порог разряда батареи (%)</label>
    <input type="number" id="socLow" step="0.1" min="0" max="100">
    <div class="hint">Ниже этого SOC (или соответствующего напряжения по таблице) начинается подзарядка от сети</div>
  </div>

  <div class="section-title">Сеть и солнце</div>
  <div class="field">
    <label>Порог наличия сети (В)</label>
    <input type="number" id="mainsV" step="1" min="0" max="400">
    <div class="hint">Выше этого переменного напряжения считаем, что сеть есть</div>
  </div>
  <div class="field">
    <label>Мизерная солнечная мощность (Вт)</label>
    <input type="number" id="solarNeg" step="0.1" min="0">
    <div class="hint">Суммарная мощность всех 4 MPPT ниже этого — солнце считается «мизерным»</div>
  </div>

  <div class="section-title">Ток нагрузки</div>
  <div class="field">
    <label>Порог SOC для подзарядки по току (%)</label>
    <input type="number" id="loadSoc" step="0.1" min="0" max="100">
    <div class="hint">Подзарядка по высокому току срабатывает только если SOC ниже этого значения (независимо от порога разряда выше)</div>
  </div>
  <div class="field">
    <label>Включение подзарядки по току (А)</label>
    <input type="number" id="loadOn" step="0.1" min="0">
  </div>
  <div class="field">
    <label>Выключение подзарядки по току (А)</label>
    <input type="number" id="loadOff" step="0.1" min="0">
    <div class="hint">Гистерезис — должен быть меньше порога включения</div>
  </div>
  <div class="field">
    <label>Задержка перед выключением по току (сек)</label>
    <input type="number" id="loadOffDelay" step="1" min="0">
    <div class="hint">Ток должен пробыть ниже порога выключения не менее этого времени подряд, чтобы не было ложных срабатываний</div>
  </div>

  <div class="section-title">Ручное управление</div>
  <div class="field">
    <label>Пауза автоматики после ручного выключения (мин)</label>
    <input type="number" id="offSuppMin" step="1" min="0">
  </div>

  <button onclick="saveSettings()">Сохранить</button>
  <div id="status"></div>
</div>

<script>
function loadCurrent() {
  fetch('/api/settings')
    .then(r => r.json())
    .then(s => {
      document.getElementById('autoEnabled').checked = !!s.autoEnabled;
      document.getElementById('mainsPrio').checked = !!s.mainsPrio;
      document.getElementById('mainsPrioParams').style.display = s.mainsPrio ? 'block' : 'none';
      document.getElementById('mainsPrioTargetSoc').value = s.mainsPrioTargetSoc;
      document.getElementById('mainsPrioMemoryH').value = s.mainsPrioMemoryH;
      const mpStatus = document.getElementById('mainsPrioStatus');
      if (s.mainsPrioActive) {
        mpStatus.innerText = '⚡ Сейчас активно окно повышенного приоритета (недавно был блекаут)';
        mpStatus.style.color = '#cd6b2b';
      } else {
        mpStatus.innerText = 'Сейчас действует обычный экономный порог';
        mpStatus.style.color = '#888';
      }
      document.getElementById('chemistry').value = s.chemistry;
      document.getElementById('lfpCapField').style.display = (s.chemistry === 'lfp') ? 'block' : 'none';
      document.getElementById('cellImbalanceField').style.display = (s.chemistry === 'lfp') ? 'block' : 'none';
      document.getElementById('lfpCapAh').value = s.lfpCapAh;
      document.getElementById('cellWarnMv').value = s.cellWarnMv;
      document.getElementById('cellCritMv').value = s.cellCritMv;
      document.getElementById('socLow').value = s.socLow;
      document.getElementById('loadSoc').value = s.loadSoc;
      document.getElementById('mainsV').value = s.mainsV;
      document.getElementById('solarNeg').value = s.solarNeg;
      document.getElementById('loadOn').value = s.loadOn;
      document.getElementById('loadOff').value = s.loadOff;
      document.getElementById('loadOffDelay').value = s.loadOffDelay;
      document.getElementById('offSuppMin').value = s.offSuppMin;
    })
    .catch(() => {
      const el = document.getElementById('status');
      el.innerText = 'Не удалось загрузить настройки';
      el.className = 'err';
    });
}

function toggleAutomation() {
  const on = document.getElementById('autoEnabled').checked;
  fetch('/api/settings?autoEnabled=' + (on ? '1' : '0'))
    .then(() => {
      const el = document.getElementById('status');
      el.innerText = 'Автоподзарядка ' + (on ? 'включена' : 'выключена');
      el.className = 'ok';
    })
    .catch(() => {
      const el = document.getElementById('status');
      el.innerText = 'Не удалось переключить автоматику';
      el.className = 'err';
      document.getElementById('autoEnabled').checked = !on; // откатываем переключатель, если запрос не прошёл
    });
}

function toggleMainsPriority() {
  const on = document.getElementById('mainsPrio').checked;
  document.getElementById('mainsPrioParams').style.display = on ? 'block' : 'none';
  fetch('/api/settings?mainsPrio=' + (on ? '1' : '0'))
    .then(() => {
      loadCurrent();
      const el = document.getElementById('status');
      el.innerText = 'Приоритет сети ' + (on ? 'включён' : 'выключен');
      el.className = 'ok';
    })
    .catch(() => {
      const el = document.getElementById('status');
      el.innerText = 'Не удалось переключить приоритет сети';
      el.className = 'err';
      document.getElementById('mainsPrio').checked = !on;
      document.getElementById('mainsPrioParams').style.display = !on ? 'block' : 'none';
    });
}

function switchChemistry() {
  const chem = document.getElementById('chemistry').value;
  fetch('/api/settings?chemistry=' + encodeURIComponent(chem))
    .then(() => {
      loadCurrent(); // подтягиваем значения, относящиеся уже к новой химии
      const el = document.getElementById('status');
      el.innerText = 'Переключено на: ' + (chem === 'lfp' ? 'LiFePO4' : 'гель/свинец');
      el.className = 'ok';
    })
    .catch(() => {
      const el = document.getElementById('status');
      el.innerText = 'Не удалось переключить тип аккумулятора';
      el.className = 'err';
    });
}

function saveSettings() {
  const params = new URLSearchParams({
    chemistry: document.getElementById('chemistry').value,
    lfpCapAh: document.getElementById('lfpCapAh').value,
    cellWarnMv: document.getElementById('cellWarnMv').value,
    cellCritMv: document.getElementById('cellCritMv').value,
    mainsPrioTargetSoc: document.getElementById('mainsPrioTargetSoc').value,
    mainsPrioMemoryH: document.getElementById('mainsPrioMemoryH').value,
    socLow: document.getElementById('socLow').value,
    loadSoc: document.getElementById('loadSoc').value,
    mainsV: document.getElementById('mainsV').value,
    solarNeg: document.getElementById('solarNeg').value,
    loadOn: document.getElementById('loadOn').value,
    loadOff: document.getElementById('loadOff').value,
    loadOffDelay: document.getElementById('loadOffDelay').value,
    offSuppMin: document.getElementById('offSuppMin').value,
  });
  fetch('/api/settings?' + params.toString())
    .then(r => r.json().then(data => ({ok: r.ok, data})))
    .then(({ok, data}) => {
      const el = document.getElementById('status');
      if (ok) {
        el.innerText = data.socLowVoltage !== undefined
          ? 'Сохранено (порог по напряжению: ' + Number(data.socLowVoltage).toFixed(2) + ' В)'
          : 'Сохранено';
        el.className = 'ok';
        loadCurrent();
      } else {
        el.innerText = 'Ошибка: ' + (data.error || 'неизвестная');
        el.className = 'err';
      }
    })
    .catch(() => {
      const el = document.getElementById('status');
      el.innerText = 'Ошибка сохранения';
      el.className = 'err';
    });
}

loadCurrent();
</script>
</body>
</html>
 )rawliteral";

#endif
