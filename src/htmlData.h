#ifndef HTML
#define HTML

#include <Arduino.h>

const char JSbehaviors[] PROGMEM = R"rawliteral(
<script>
$(document).ready(function(){  
    $("#pumpLink").click(function(){
        $.getJSON("pumptoggle", function(result){
            console.log(result);
            if(result.pump) { $("#pumpStatus").html("ON"); } else { $("#pumpStatus").html("OFF"); }
            if(result.heat) { $("#heatStatus").html("ON"); } else if (result.pendingheat) { $("#heatStatus").html("PENDING"); } else { $("#heatStatus").html("OFF"); }
            $("#datetime").html(result.datetime);
        });
    });
});
</script>
)rawliteral";

const char htmlMain[] PROGMEM = R"rawliteral(
<!DOCTYPE html> <html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <title>TipsyBrew Kettle</title>
    <style>
        html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}
        body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}
        .button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}
        .button-on {background-color: #3498db;}
        .button-on:active {background-color: #2980b9;}
        .button-off {background-color: #34495e;}
        .button-off:active {background-color: #2c3e50;}
        .bold {font-weight: bold;}
        .red {color: #FF0000;}
        .green {color: #27AE60;}
        p {font-size: 14px;color: #888;margin-bottom: 10px;}
        .leftCol {display:inline-block; padding-right:10px;}
        .rightCol {display:inline-block; padding-left:10px;}
        #tbkVersion { font-size: 12px; color: #888; text-decoration: none;}
    </style>
    <script src="https://code.jquery.com/jquery-3.6.4.min.js" integrity="sha256-oP6HI9z1XaZNBrJURtCoUT5SUnxFr8s3BzRl+cbzUq8=" crossorigin="anonymous"></script>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
    <h1>TipsyBrew Kettle</h1>
    <span><canvas id="tempChart"></canvas></span>
    <script>
        %JSON_DATA%
        var labels = temps.jsonarray.map(function(e) {
            return e.timeLabel;
        });
        var data = temps.jsonarray.map(function(e) {
            return e.temp;
        });
        const ctx = document.getElementById('tempChart');
        new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [{
                    label: 'degrees C',
                    data: data,
                    borderWidth: 3,
                    backgroundColor: "rgba(255,99,132,0.2)",
                    borderColor: "rgba(255,99,132,1)",
                    pointRadius: 0
                }]
            },
            options: {
                scales: {
                    y: {
                        beginAtZero: true
                    }
                },
                plugins: {
                    legend: {
                        display: false
                    }
                }
            }
        });
    </script>
    <h1 id="currentTemp">&#176;C</h1>
    <span class="leftCol">
        <p>Pump Status: <span id="pumpStatus">OFF</span></p><a id="pumpLink" class="button button-on" nohref>ON</a>
    </span>
    <span class="rightCol">
        <p>Heat Status: <span id="heatStatus">OFF</span></p><a id="heatLink" class="button button-on" nohref>ON</a>
    </span>
    <p id="datetime"></p>
    <a id="tbkVersion" href="version">TBK version </a>
    <div id="heap" style="display: none;"></div>
    <script>
        var gateway = `ws://${window.location.hostname}/ws`;
        var websocket;
        function initWebSocket() {
            console.log('Trying to open a WebSocket connection...');
            websocket = new WebSocket(gateway);
            websocket.onopen    = onOpen;
            websocket.onclose   = onClose;
            websocket.onmessage = onMessage;
        }
        function onOpen(event) {
            console.log('Connection opened');
        }

        function onClose(event) {
            console.log('Connection closed');
            setTimeout(initWebSocket, 2000);
        }
        function onMessage(event) {
            var state;
            if (event.data == "1"){
            state = "ON";
            }
            else{
            state = "OFF";
            }
            document.getElementById('state').innerHTML = state;
        }
        $(document).ready(function(){
            $("#currentTemp").html(status.tempreading + "&#176;C");
            if(status.pump) {
                $("#pumpStatus").html("ON");
                $("#pumpLink").removeClass("button-on").addClass("button-off").html("OFF");
            } else {
                $("#pumpStatus").html("OFF");
                $("#pumpLink").removeClass("button-off").addClass("button-on").html("ON");
            }
            $("#datetime").html(status.datetime);
            $("#version").html("TBK version " + status.version);
            $("#pumpLink").click(function(){
                $.getJSON("pumptoggle", function(result){
                    console.log(result);
                    if(result.pump) { $("#pumpStatus").html("ON"); } else { $("#pumpStatus").html("OFF"); }
                    if(result.heat) { $("#heatStatus").html("ON"); } else if (result.pendingheat) { $("#heatStatus").html("PENDING"); } else { $("#heatStatus").html("OFF"); }
                    $("#datetime").html(result.datetime);
                });
            });
        });
</script>
</body>
</html>
)rawliteral";

#endif