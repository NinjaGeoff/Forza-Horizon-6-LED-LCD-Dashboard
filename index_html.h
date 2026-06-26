#pragma once

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>FH6 Shift Light Config</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #1c1c1f; color: #e0e0e6; }
        .container { max-width: 500px; margin: 0 auto; background: #2a2a2e; padding: 20px; border-radius: 8px; box-shadow: 0 4px 10px rgba(0,0,0,0.3); }
        h1 { text-align: center; color: #ff4757; margin-bottom: 20px; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 5px; font-weight: bold; font-size: 14px;}
        input[type="number"] { width: 100%; box-sizing: border-box; padding: 8px; background: #3a3a3e; border: 1px solid #555; color: #fff; border-radius: 4px; }
        select { width: 100%; padding: 8px; background: #3a3a3e; border: 1px solid #555; color: #fff; border-radius: 4px; }
        button { width: 100%; padding: 12px; background: #ff4757; border: none; color: white; font-size: 16px; border-radius: 4px; cursor: pointer; font-weight: bold; }
        button:hover { background: #ff6b81; }
        .row { display: flex; gap: 10px; }
        .row .form-group { flex: 1; }
    </style>
</head>
<body>
<div class="container">
    <h1>FH6 Tachometer Config</h1>
    <form action="/update" method="POST">
        <h3>LED Behavior</h3>
        <div class="row">
            <div class="form-group">
                <label>Brightness (1-255)</label>
                <input type="number" id="scaleBrightness" name="scaleBrightness" min="1" max="255" value="20">
            </div>
            <div class="form-group">
                <label>White BG Factor</label>
                <input type="number" id="whiteBrightnessFactor" step="0.01" name="whiteBrightnessFactor" min="0" max="1" value="0.75">
            </div>
        </div>
        <div class="row">
            <div class="form-group">
                <label>LED Offset</label>
                <input type="number" id="ledOffset" name="ledOffset" value="0">
            </div>
            <div class="form-group">
                <label>Direction</label>
                <select id="ledReversed" name="ledReversed">
                    <option value="0">Normal</option>
                    <option value="1">Reversed</option>
                </select>
            </div>
        </div>

        <h3>RPM Thresholds (0.0 - 1.0)</h3>
        <div class="row">
            <div class="form-group"><label>Green</label><input type="number" step="0.01" id="rpmGreenStart" name="rpmGreenStart" value="0.10"></div>
            <div class="form-group"><label>Yellow</label><input type="number" step="0.01" id="rpmYellowStart" name="rpmYellowStart" value="0.55"></div>
            <div class="form-group"><label>Red</label><input type="number" step="0.01" id="rpmRedStart" name="rpmRedStart" value="0.75"></div>
            <div class="form-group"><label>Flash</label><input type="number" step="0.01" id="rpmFlashStart" name="rpmFlashStart" value="0.90"></div>
        </div>

        <h3>LED Counts per Zone</h3>
        <div class="row">
            <div class="form-group"><label>Green</label><input type="number" id="zoneGreenCount" name="zoneGreenCount" value="23"></div>
            <div class="form-group"><label>Yellow</label><input type="number" id="zoneYellowCount" name="zoneYellowCount" value="11"></div>
            <div class="form-group"><label>Red</label><input type="number" id="zoneRedCount" name="zoneRedCount" value="11"></div>
        </div>

        <button type="submit">Apply Settings</button>
    </form>
</div>

<script>
    // Fetch current settings as json from the ESP32 dynamically after page loads
    window.addEventListener('DOMContentLoaded', () => {
        fetch('/config')
            .then(res => res.json())
            .then(data => {
                document.getElementById('scaleBrightness').value = data.scaleBrightness;
                document.getElementById('whiteBrightnessFactor').value = data.whiteBrightnessFactor;
                document.getElementById('ledOffset').value = data.ledOffset;
                document.getElementById('ledReversed').value = data.ledReversed ? "1" : "0";
                document.getElementById('rpmGreenStart').value = data.rpmGreenStart;
                document.getElementById('rpmYellowStart').value = data.rpmYellowStart;
                document.getElementById('rpmRedStart').value = data.rpmRedStart;
                document.getElementById('rpmFlashStart').value = data.rpmFlashStart;
                document.getElementById('zoneGreenCount').value = data.zoneGreenCount;
                document.getElementById('zoneYellowCount').value = data.zoneYellowCount;
                document.getElementById('zoneRedCount').value = data.zoneRedCount;
            })
            .catch(err => console.error("Error fetching live config:", err));
    });
</script>
</body>
</html>
)rawliteral";