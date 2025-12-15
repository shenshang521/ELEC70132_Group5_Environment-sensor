#ifndef INDEX_HTML_H
#define INDEX_HTML_H
#include <Arduino.h>

// ==========================================
// MODULE : Web Interface (HTML/CSS/JS)
// This string contains the entire frontend website stored in Flash memory (PROGMEM).
// Features:
// 1. CSS for styling cards, grid layout, and colors.
// 2. HTML structure for displaying sensor data (Temp, Hum, GPS, etc.).
// 3. JavaScript to handle Server-Sent Events (SSE) for live data updates 
//    without refreshing the page, and logic for downloading/deleting data.
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Sensor Hub</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin:0; background-color: #f4f4f4; }
    h2 { background-color: #0c69d6; color: white; padding: 15px; margin: 0; }
    .content { padding: 20px; max-width: 600px; margin: 0 auto; }
    .card { background: white; padding: 20px; margin: 10px; border-radius: 10px; box-shadow: 2px 2px 10px rgba(0,0,0,0.1); }
    .unit { font-size: 14px; color: #888; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .btn { display: inline-block; padding: 10px 20px; margin: 5px; text-decoration: none; color: white; border-radius: 5px; cursor: pointer; border: none; font-size: 16px; }
    .btn-down { background-color: #28a745; }
    .btn-del { background-color: #dc3545; }
    .icon { font-size: 20px; margin-right: 5px; }
    #clock { font-size: 32px; font-weight: bold; color: #333; margin-bottom: 5px; }
    #date { font-size: 16px; color: #666; margin-bottom: 20px; }
  </style>
</head>
<body>
  <h2>ESP32 Sensor Dashboard</h2>
  <div class="content">
    <div class="card">
       <div id="clock">--:--:--</div>
       <div id="date">Loading Time...</div>
    </div>
    <div class="card">
      <p><span class="icon">&#128190;</span> SD Card Data</p>
      <a href="/download" class="btn btn-down">&#128229; Download CSV</a>
      <button onclick="deleteData()" class="btn btn-del">&#128465; Clear Data</button>
      <p id="sd_status" style="font-size:12px; color:#666; margin-top:5px;">State: Waiting...</p>
    </div>
    <div class="grid">
      <div class="card">
        <p><span class="icon">&#127777;</span> Temperature</p>
        <p><span id="temp">--</span> <span class="unit">°C</span></p>
      </div>
      <div class="card">
        <p><span class="icon">&#128167;</span> Humidity</p>
        <p><span id="hum">--</span> <span class="unit">%</span></p>
      </div>
      <div class="card">
        <p><span class="icon">&#128336;</span> Pressure</p>
        <p><span id="press">--</span> <span class="unit">hPa</span></p>
      </div>
      <div class="card">
        <p><span class="icon">&#128168;</span> Gas Res</p>
        <p><span id="gas">--</span> <span class="unit">KΩ</span></p>
      </div>
      <!-- 新增 Air Quality 卡片 -->
      <div class="card">
        <p><span class="icon">&#127795;</span> Air Quality</p>
        <p><span id="aq">--</span> <span class="unit">%</span></p>
      </div>
    </div>
    <div class="card">
      <p><span class="icon">&#128207;</span> ToF Distance</p>
      <p><span id="dist">--</span> <span class="unit">mm</span></p>
    </div>
    <div class="card">
      <p><span class="icon">&#127757;</span> GPS Location</p>
      <p>Lat: <span id="lat">--</span> | Lon: <span id="lon">--</span></p>
      <p>Alt: <span id="alt">--</span> <span class="unit">m</span></p>
      <a id="maps" href="#" target="_blank" style="display:inline-block;margin-top:10px;text-decoration:none;color:#0c69d6;border:1px solid #0c69d6;padding:5px 10px;border-radius:5px;">View on Google Maps</a>
    </div>
  </div>
<script>
function deleteData() {
  if (confirm("Are you sure?")) {
    fetch('/delete').then(r => r.ok ? alert("Deleted!") : alert("Failed"));
  }
}
if (!!window.EventSource) {
 var source = new EventSource('/events');
 source.addEventListener('update', function(e) {
  var obj = JSON.parse(e.data);
  document.getElementById("temp").innerHTML = obj.temp;
  document.getElementById("hum").innerHTML = obj.hum;
  document.getElementById("press").innerHTML = obj.press;
  document.getElementById("gas").innerHTML = obj.gas;
  document.getElementById("aq").innerHTML = obj.aq; 
  document.getElementById("dist").innerHTML = obj.dist;
  document.getElementById("lat").innerHTML = obj.lat;
  document.getElementById("lon").innerHTML = obj.lon;
  document.getElementById("alt").innerHTML = obj.alt;
  document.getElementById("sd_status").innerHTML = "SD Status: " + (obj.sd ? "Writing OK" : "Error/Missing");
  document.getElementById("clock").innerHTML = obj.time.split(" ")[1]; 
  document.getElementById("date").innerHTML = obj.time.split(" ")[0]; 
  if(obj.lat != 0) {
      document.getElementById("maps").href = "https://www.google.com/maps/search/?api=1&query=" + obj.lat + "," + obj.lon;
  }
 }, false);
}
</script>
</body>
</html>
)rawliteral";


#endif