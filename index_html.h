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
        input[type="number"], input[type="text"] { width: 100%; padding: 8px; box-sizing: border-box; background: #3a3a3e; border: 1px solid #555; color: #fff; border-radius: 4px; }
        select { width: 100%; padding: 8px; background: #3a3a3e; border: 1px solid #555; color: #fff; border-radius: 4px; }
        button { width: 100%; padding: 12px; background: #ff4757; border: none; color: white; font-size: 16px; border-radius: 4px; cursor: pointer; font-weight: bold;}
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
                <input type="number" name="scaleBrightness" min="1" max="255" value="%SCALE_BRIGHTNESS%">
            </div>
            <div class="form-group">
                <label>White BG Factor</label>
                <input type="number" step="0.01" name="whiteBrightnessFactor" min="0" max="1" value="%WHITE_BG_FACTOR%">
            </div>
        </div>
        <div class="row">
            <div class="form-group">
                <label>LED Offset</label>
                <input type="number" name="ledOffset" value="%LED_OFFSET%">
            </div>
            <div class="form-group">
                <label>Direction</label>
                <select name="ledReversed">
                    <option value="0" %REVERSED_FALSE%>Normal</option>
                    <option value="1" %REVERSED_TRUE%>Reversed</option>
                </select>
            </div>
        </div>

        <h3>RPM Thresholds (0.0 - 1.0)</h3>
        <div class="row">
            <div class="form-group"><label>Green</label><input type="number" step="0.01" name="rpmGreenStart" value="%RPM_GREEN%"></div>
            <div class="form-group"><label>Yellow</label><input type="number" step="0.01" name="rpmYellowStart" value="%RPM_YELLOW%"></div>
            <div class="form-group"><label>Red</label><input type="number" step="0.01" name="rpmRedStart" value="%RPM_RED%"></div>
            <div class="form-group"><label>Flash</label><input type="number" step="0.01" name="rpmFlashStart" value="%RPM_FLASH%"></div>
        </div>

        <h3>LED Counts per Zone</h3>
        <div class="row">
            <div class="form-group"><label>Green</label><input type="number" name="zoneGreenCount" value="%ZONE_GREEN%"></div>
            <div class="form-group"><label>Yellow</label><input type="number" name="zoneYellowCount" value="%ZONE_YELLOW%"></div>
            <div class="form-group"><label>Red</label><input type="number" name="zoneRedCount" value="%ZONE_RED%"></div>
        </div>

        <button type="submit">Apply Settings</button>
    </form>
</div>
</body>
</html>
)rawliteral";