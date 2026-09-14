#ifndef WEB_PAGE_H
#define WEB_PAGE_H

#include <Arduino.h>

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<link rel="shortcut icon" href="http://kontro03.web01.net/icon.png" type="image/x-icon">
<style>
    body {
        background: #f0f0f0;
        display: flex;
        justify-content: center;
        align-items: center;
        height: 100vh;
        font-family: Arial, sans-serif;
        gap: 20px;
    }

    .hidden{
        display: none;
    }

    .accu-box, .mppt-box {
        width: 200px;
        border: 4px solid #333;
        border-radius: 8px;
        padding: 10px;
        padding-top: 20px;
        box-sizing: border-box;
        text-align: center;
        background: #0077cc;
        color: #fff;
        position: relative;
    }

    /* Батарея */
    .accu-box::before,
    .accu-box::after {
        content: "";
        position: absolute;
        top: -15px;
        width: 30px;
        height: 18px;
        border-radius: 4px;
        background: #555;
    }

    .accu-box::before { left: 25px; background: #b30000; }
    .accu-box::after { right: 25px; background: #666; }

    .battery {
        position: relative;
        width: 146px;
        height: 48px;
        border: 3px solid #333;
        border-radius: 6px;
        background: white;
        display: flex;
        align-items: center;
        padding: 4px 6px;
        box-sizing: border-box;
        margin: 0 auto 10px auto;
    }

    .level {
        display: flex;
        gap: 3px;
        flex-grow: 1;
    }

    .bar {
        width: 10px;
        height: 33px;
        background: #fbfbfb;
        border-radius: 2px;
        border: 2px solid #ddd;
        box-sizing: border-box;
    }

    .bar.filled {
        background: #3ac056;
        border-color: #2e9f46;
    }
    
    .bar.filled-warning {
        background: #FF9800;
        border-color: #EF8800;
    }
    
    .bar.filled-danger {
        background: #b30000;
        border-color: #a30000;
    }

    .text-line {
        font-size: 24px;
        font-weight: bold;
        margin: 4px 0;
        color: #fff;
    }

    .text-green { color: #3ac056; font-weight: bold; }

    /* MPPT контроллер */
   .mppt-box {
        position: relative;
        padding: 10px;
        border-radius: 8px;
        background: #2b92cd;
        width: 200px;
        padding: 10px;
        border: 3px solid #999;
   }

   .mppt-box.disabled {
        background: #a0a0a0;
        color: #bbb;
   }

   .charger {
        border-radius: 8px;
        background: #555;
        width: 175px;
        height: 140px;
        padding: 10px;
        border: 3px solid #888;
   }

   .sun-corner {
    position: absolute;
    top: 1px;
    left: 1px;
    font-size: 24px;
}

.mppt-line {
    display: flex;
    justify-content: space-between;
    font-size: 18px;
    font-weight:bold;
    margin: 4px 0;
    padding: 0 15px 0 20px;
}

.bus {
    position: absolute;
    width: 140px;
    height: 460px;
    border: 2px #dFbA99 solid;
    border-radius: 8px;
    top: 10px;
    left: 330px;
    background: #FFDAB9;
}

.load {
    position: absolute;
    width: 140px;
    height: 85px;
    border: 2px #999 solid;
    border-radius: 4px;
    background: #3ba2dd;
}

.load-switch {
    position: absolute; 
    font-size: 36px; 
    color: gray; 
    left: 8px; 
    top:-5px;
    cursor: pointer;
    transition: transform 0.3s ease;
}
 
.load-switch:hover{
    transform: scale(1.2);
}
.load-name{
    position: absolute; 
    font-size: 18px; 
    color: #eee; 
    font-weight: bold; 
    right: 5px; 
    top:60px
}

.switch-on{
    color: lime;
}
.load-switch.switch-disabled{
    cursor: default;
    transform: none;
}
.load-type{
    position: absolute; 
    font-size: 18px; 
    color: #eee; 
    font-weight: bold; 
    right: 5px; 
    top:5px
}

.accu-box {
    position: absolute;
    left:300px;
    top: 540px;
}

.caption {
    position: absolute; 
    font-size: 18px; 
    color: #888; 
    font-weight: bold;
}
.caption-busbar{
    position: absolute; 
    font-size: 18px; 
    color: #888; 
    font-weight: bold;
    width: 100%;
    text-align: center;
}
.busbar-caption-1{
    top: 160px;
    color:#999;
}
.busbar-caption-current{
    top: 190px;
    font-size: 20px;
}
.busbar-caption-2{
    top: 250px;
    color:#999;
}
.busbar-caption-voltage{
    top: 280px;
    font-size: 20px;
}
.caption-mppt-1 {
   left: 255px; 
   top: 40px;
}

.caption-mppt-2 {
   left: 255px; 
   top: 150px;
}

.caption-mppt-3 {
   left: 255px; 
   top: 275px;
}

.caption-mppt-4 {
   left: 255px; 
   top: 390px;
}

.caption-charger {
   left: 125px; 
   top: 495px;
}

.caption-bms-1 {
   left: 415px; 
   top: 495px;
}

.caption-line-load-1 {
   left: 530px; 
   top: 33px;
}

.caption-line-load-2 {
   left: 535px; 
   top: 137px;
}

.mppt-1 {
    position: absolute;
    left:10px;
    top: 10px;
}

.line-mppt-1 {
    position: absolute;
    left:210px;
    top: 60px;
    width: 120px;
}

.mppt-2 {
    position: absolute;
    left:10px;
    top: 130px;
}

.line-mppt-2 {
    position: absolute;
    left:210px;
    top: 170px;
    width: 120px;
}

.mppt-3 {
    position: absolute;
    left:10px;
    top: 250px;
}

.line-mppt-3 {
    position: absolute;
    left:210px;
    top: 280px;
    width: 120px;
}

.mppt-4 {
    position: absolute;
    left:10px;
    top: 370px;
}

.line-mppt-4 {
    position: absolute;
    left:210px;
    top: 390px;
    width: 120px;
}

.charger {
    position: absolute;
    left:10px;
    top: 540px;
    display: flex;
    justify-content: center;
}

.charger-switch{
    color: lime; 
    font-size: 80px;
    cursor: pointer;
    transition: transform 0.3s ease; 
}
.charger-switch:hover{
    transform: scale(1.13);
}
.charger-switch:active {
    transform: scale(0.981); /* Немного меньше исходного размера */
    color: color-mix(in srgb, lime, black 30%);;           /* Можно также изменить цвет на более темный */
    transition: transform 0.1s; /* Быстрая реакция на клик */
}

.charger-power{
    position: absolute;
    left: 5px; 
    top: 0px;
    font-size:42px;
    font-weight: bold;
    color: #777;
}
.charger-power-on{
    color: #eFc700;
}
.charger-label{
    position: absolute;
    left: 40px; 
    bottom: 20px;
    font-size:22px;
    color: #ccc;
    font-weight: bold;
}
.line-charger {
    position: absolute;
    left:105px;
    top: 442px;
    height: 66px;
}
.line-battery {
    position: absolute;
    left:395px;
    top: 376px;
    height: 66px;
}

.line-load-internal{
    position: absolute;
    left:473px;
    top: -110px;
    width: 172px;
}

.line-load-main{
    position: absolute;
    left:473px;
    top: -15px;
    width: 172px;
}

.line-load-usb-charger{
    position: absolute;
    left:473px;
    top: -20px;
    width: 146px;
}

.load-1{
    top: 10px;
    left: 645px;
}
.load-2{
    top: 115px;
    left: 645px;
}
.load-3{
    top: 615px;
    left: 645px;
    background: #555;
}
.load-3 .load-type{
    color: #ccc;
}
.load-3 .load-name{
    color: #ccc;
}
.load-3 .load-switch{
    font-size: 24px; 
    font-weight: bold; 
    right: 3px; 
    top: 3px;
    cursor: default;
}
.load-3 .load-switch:hover{
    transform: none;
}
        .flow-container {
            
            height: 8px;
            background-color: #ccc;
            position: relative;
            overflow: hidden;
            border-radius: 2px;
        }
        .flow-container::before {
            content: "";
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: repeating-linear-gradient(
                90deg, 
                #FFA000,
                #FFA000 10px,
                #ccc 10px,
                #ccc 20px
            );
            animation: flow 0.4s linear infinite;
        }

        @keyframes flow {
            0% {
                transform: translateX(0);
            }
            100% {
                transform: translateX(20px); 
            }
        }

        .flow-container.stopped::before {
          display: none; 
        }

        .flow-container-vertical {
            width: 10px; 
            background-color: #ccc;
            position: relative;
            overflow: hidden;
            border-radius: 2px;
        }

        .flow-container-vertical::before {
            content: "";
            position: absolute;
            top: 0;
            left: 0;
            width: 10px;
            height: calc(100% + 30px); 
            top: -20px; 

            background: repeating-linear-gradient(
                0deg,
                #FFA000, 
                #FFA000 10px,
                #ccc 10px,
                #ccc 20px
            );
            
            animation: flow-up 0.6s linear infinite;
        }

        @keyframes flow-up {
            0% {transform: translateY(20px); }
            100% {transform: translateY(0); }
        }

        .flow-container-vertical.reverse-flow::before {
            animation-name: flow-down; 
        }

        @keyframes flow-down {
             0% { transform: translateY(-20px); }
            100% { transform: translateY(0px); }
        }

        .flow-container-vertical.stopped::before {
           animation-play-state: paused; 
          display: none; 
        }
        
#status-led {
    width: 12px;
    height: 12px;
    background-color: green;
    border-radius: 50%;
    display: inline-block;
    box-shadow: 0 0 5px green;
    transition: all 0.3s ease;
    margin-right: 5px;
}

/* Состояние при потере связи */
.is-offline #status-led {
    background-color: red;
    box-shadow: none;
}
#status-text {
    color: #888;
    font-weight: bold;
}
@keyframes pulse-green {
    0% { transform: scale(1); opacity: 1; }
    50% { transform: scale(1.5); opacity: 0.5; }
    100% { transform: scale(1); opacity: 1; }
}

.ping {
    animation: pulse-green 0.4s ease-out;
}


.led-off {
  width: 15px;
  height: 15px;
  border-radius: 50%;
  background-color: #333;
  box-shadow: inset 0 0 5px rgba(0,0,0,0.5);
  transition: background 0.3s;
  position: absolute;
  left:4px;
  top:81px
}

/* Свечение для каждого цвета */
.bulk { background-color: #007bff; box-shadow: 0 0 5px #bbb; animation: none;}
.absorption { background-color: #ff8c00; box-shadow: 0 0 5px #bbb; animation: none;}
.float { background-color: lime; box-shadow: 0 0 5px #bbb; animation: none;}
.off { background-color: #a0a0a0; box-shadow: 0 0 0 #bbb; animation: none;}

/* Мигающий синий (раз в 3 секунды) */
.not-charging {
  background-color: #007bff;
  box-shadow: 0 0 3px #ccc;
  animation: blink 2s infinite;
}

@keyframes blink {
  0%, 80% { opacity: 1; }
  81% { opacity: 0; }
  100% { opacity: 1; }
}

</style>
</head>
<div style="border: 1px #ddd solid; width: 800px; height: 718px; position: relative;">

<div class="mppt-box mppt-1 disabled">
    <span class="sun-corner hidden" style='font-size:20px'>☀️</span>
    <div class="mppt-line"><span class="label">Voltage</span> <span class="value voltage">---</span></div>
    <div class="mppt-line"><span class="label">Power</span> <span class="value power">---</span></div>
    <div class="mppt-line"><span class="label">Current</span> <span class="value current">---</span></div>
    <div class="led led-off off"></div>
</div>

<div class="mppt-box mppt-2 disabled">
    <span class="sun-corner hidden" style='font-size:20px'>☀️</span>
    <div class="mppt-line"><span class="label">Voltage</span> <span class="value voltage">---</span></div>
    <div class="mppt-line"><span class="label">Power</span> <span class="value power">---</span></div>
    <div class="mppt-line"><span class="label">Current</span> <span class="value current">---</span></div>
    <div class="led led-off off"></div>
</div>

<div class="mppt-box mppt-3 disabled">
    <span class="sun-corner hidden" style='font-size:20px'>☀️</span>
    <div class="mppt-line"><span class="label">Voltage</span> <span class="value voltage">---</span></div>
    <div class="mppt-line"><span class="label">Power</span> <span class="value power">---</span></div>
    <div class="mppt-line"><span class="label">Current</span> <span class="value current">---</span></div>
    <div class="led led-off off"></div>
</div>

<div class="mppt-box mppt-4 disabled">
    <span class="sun-corner hidden" style='font-size:20px'>☀️</span>
    <div class="mppt-line"><span class="label">Voltage</span> <span class="value voltage">---</span></div>
    <div class="mppt-line"><span class="label">Power</span> <span class="value power">---</span></div>
    <div class="mppt-line"><span class="label">Current</span> <span class="value current">---</span></div>
    <div class="led led-off off hidden"></div>
</div>    

<div class="charger">
    <span class="charger-power charger-power-on">⚡&#xFE0E</span>
    <span class="charger-switch" id="chargerBtn">⏻</span>
    <span class="charger-label">CHARGER</span>
</div>

<div class="bus">
    <div id="status-container" style="position: absolute;top:12px;left:35px">
       <span id="status-led" class="ping"></span>
       <span id="status-text">Online</span>
    </div> 
    <div class="caption-busbar busbar-caption-1">Current</div>
    <div class="caption-busbar busbar-caption-current"></div>
    <div class="caption-busbar busbar-caption-2">Voltage</div>
    <div class="caption-busbar busbar-caption-voltage"></div>
</div>

<div class="accu-box lvl-green bms-1">
    <div class="battery">
        <div class="level">
            <div class="bar"></div>
            <div class="bar"></div>
            <div class="bar"></div>
            <div class="bar"></div>
            <div class="bar"></div>
            <div class="bar"></div>
            <div class="bar"></div>
            <div class="bar"></div>
            <div class="bar"></div>
            <div class="bar"></div>
        </div>
    </div>
    <div class="text-line charge-value" style='font-size:28px'>60 %</div>
    <div class="text-line" style='color:#eee'><span class="battery-voltage">0.0</span> V <span class="battery-current">0.0</span> A</div>
</div>

<div class="load load-1">
    <div class="load-switch switch-on switch-disabled">⏻</div>
    <div class="load-type">12V to 5V</div>
    <div class="load-name">5V Load</div>
</div>
<div class="load load-2">
    <div class="load-switch switch-on switch-disabled">⏻</div>
    <div class="load-type">12V</div>
    <div class="load-name">Main Load</div>
</div>
<div class="load load-3">
    <div class="load-switch mains-status">⚡︎</div>
    <div class="load-type" id="mains-input">~0 V</div>
    <div class="load-name">Mains Input</div>
</div>

<div class="flow flow-container line-mppt-1 stopped"></div>
<div class="flow flow-container line-mppt-2 stopped"></div>
<div class="flow flow-container line-mppt-3 stopped"></div>
<div class="flow flow-container line-mppt-4 stopped"></div>        
<div class="flow flow-container-vertical line-charger stopped"></div>

<div class="flow flow-container-vertical line-battery line-bms-1 stopped"></div>
<div class="flow flow-container line-load-internal"></div>
<div class="flow flow-container line-load-main"></div>

<div class="caption caption-mppt-1 hidden"></div>
<div class="caption caption-mppt-2 hidden"></div>
<div class="caption caption-mppt-3 hidden"></div>
<div class="caption caption-mppt-4 hidden"></div>        
<div class="caption caption-charger hidden">2.5A</div>
<div class="caption caption-bms-1">2.5A</div>
<div class="caption caption-line-load-1">0.09 A</div>
<div class="caption caption-line-load-2">1.29 A</div>

</div>
    
<script>
  function updateData() {
      
    fetch('api/solar-data') // Запрашиваем данные
      .then(response => response.json())
      .then(json => {
      
      const led = document.getElementById('status-led');
      document.body.classList.remove('is-offline');
      document.getElementById('status-text').innerText = 'Online';
      led.classList.add('ping');
      setTimeout(() => led.classList.remove('ping'), 400);
      
      const data = json.solar; 

      Object.keys(data).forEach(key => {
        // Ищем блок в HTML - ID должен быть "mppt-1", как в JSON)
        const container = document.querySelector('.' + key);
        
        if (container) {
          const device = data[key];
          const line = document.querySelector('.line-' + key);
          const caption = document.querySelector('.caption-' + key);

          if(device.status == 'on'){
              container.querySelector('.voltage').innerText = device.voltage + ' V';
              container.querySelector('.power').innerText = device.power + ' W';
              container.querySelector('.current').innerText = device.current + ' A';
              
              container.classList.remove('disabled');
              caption.innerText = device.current + ' A';
              
              if(device.current > 0){
                  line.classList.remove('stopped');
              }else{
                  line.classList.add('stopped');
              }
              
              if(device.generation == 'on') {
                  caption.classList.remove('hidden');
                  container.querySelector('.sun-corner').classList.remove('hidden');
              }
              else{ 
                  caption.classList.add('hidden');
                  container.querySelector('.sun-corner').classList.add('hidden');
              }
              
              updateLedStatus(device.state, key);
                 
          }else{
              container.querySelector('.voltage').innerText = '---';
              container.querySelector('.power').innerText = '---';
              container.querySelector('.current').innerText = '---';
              container.classList.add('disabled');
              line.classList.add('stopped');
              caption.classList.add('hidden');
              container.querySelector('.sun-corner').classList.add('hidden');
              updateLedStatus(255, key);
          }
        }
      });
      
      const bms = json.bms;
      Object.keys(bms).forEach(key => {
        // Ищем блок в HTML - ID должен быть "bms-1", как в JSON)
        const container = document.querySelector('.' + key);
        const line = document.querySelector('.line-' + key);
        const caption = document.querySelector('.caption-' + key);
        
        if (container) {
          const device = bms[key];
          container.querySelector('.battery-voltage').innerText = device.voltage;
          container.querySelector('.battery-current').innerText = device.current;
          container.querySelector('.charge-value').innerText = device.level_percent + ' %';
          caption.innerText = device.current + ' A';
          
          // Находим все элементы с классом bar внутри уровня заряда
          const bars = container.querySelectorAll('.bar');
          const charge = Math.round(device.level_percent / 10); 
          var filled = 'filled';
         
          bars.forEach((bar, index) => {
              if (index < charge) {
                  
                  if(charge < 3) {
                      bar.classList.add('filled-danger');
                      bar.classList.remove('filled');
                      bar.classList.remove('filled-warning');
                  }
                  else {
                      if(charge < 6) {
                         bar.classList.add('filled-warning');
                         bar.classList.remove('filled');
                         bar.classList.remove('filled-danger');
                      }
                      else{
                         bar.classList.add('filled');
                         bar.classList.remove('filled-danger');
                         bar.classList.remove('filled-warning');
                      }
                  };
                 
              } else {
                  bar.classList.remove('filled');
                  bar.classList.remove('filled-danger');
                  bar.classList.remove('filled-warning');
              }
          });
          
          if(line){
             //if(device.state == 'storage'){
             if(device.current == 0){
                 line.classList.add('stopped');
                 caption.classList.add('hidden');
             }
             //if(device.state == 'charge'){
             if(device.current > 0){
                 line.classList.add('reverse-flow');
                 line.classList.remove('stopped');
                 caption.classList.remove('hidden');
             }
             //if(device.state == 'discharge'){
             if(device.current < 0){
                 line.classList.remove('reverse-flow');
                 line.classList.remove('stopped');
                 caption.classList.remove('hidden');
             }
          }
          
        }
      });
      
      const bus = json.bus;
      document.querySelector('.busbar-caption-current').innerText = bus.current + ' A';
      document.querySelector('.busbar-caption-voltage').innerText = bus.voltage + ' V';
      
      const charger = json.charger;
      const switchCharge = document.querySelector('.charger-power');
      const chargerLine = document.querySelector('.line-charger');
      if(charger.state == 'on') {
          switchCharge.classList.add('charger-power-on');
          chargerLine.classList.remove('stopped');
      }
      else {
          switchCharge.classList.remove('charger-power-on');
          chargerLine.classList.add('stopped');
      }
      
      const load = json.load;
      document.querySelector('.caption-line-load-1').innerText = load.dc + ' A';
      document.querySelector('.caption-line-load-2').innerText = load.main + ' A';
      document.querySelector('#mains-input').innerText = '~' + load.mainsVoltage + ' V';
      if(load.mainsStatus == 'on'){
          document.querySelector('.mains-status').classList.add('charger-power-on');
      }else{
          document.querySelector('.mains-status').classList.remove('charger-power-on');
      }
      
      
    })
      .catch(err => {
          console.error('Ошибка:');
          document.body.classList.add('is-offline');
          document.getElementById('status-text').innerText = 'Offline';
          document.querySelectorAll('.flow').forEach(el => {
              el.classList.add('stopped');
          });
      });
  }

  setInterval(updateData, 1000);
  
  updateData();
  
  const chargerBtn = document.querySelector('#chargerBtn');
  chargerBtn.onclick = () => {
    fetch('/api/charger')
        .then(response => response.text()) // Дожидаемся текста ("on" или "off")
        .then(state => {
            const switchCharge = document.querySelector('.charger-power');
            if (state.trim() === 'on') {
                switchCharge.classList.add('charger-power-on');
            } else {
                switchCharge.classList.remove('charger-power-on');
            }
        })
        .catch(err => alert('Ошибка связи'));
  };


  function updateLedStatus(statusCode, num) {
      console.log(num);
      const mppt = document.querySelector('.' + num);
      const led = mppt.querySelector('.led');
  
      led.className = 'led led-off';

      switch(statusCode) {
        case 3: 
          led.classList.add('bulk');
          break;
        case 4:
          led.classList.add('absorption');
          break;
        case 5: 
          led.classList.add('float');
          break;
        case 0: 
          led.classList.add('not-charging');
          break;
        default:
          led.classList.add('off');
  }
}
</script>    

</body>
</html>


 )rawliteral";

#endif
