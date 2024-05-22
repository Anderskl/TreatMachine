const char HTML_webPage[] PROGMEM = R"=====(
<!DOCTYPE HTML>
<html>
	<head>
		<title>Ollie Treat Machine</title>
		<!-- <link rel="shortcut icon" href="#"> -->
		<link rel="icon" href="data:,">
		<meta name="viewport" content="width=device-width, initial-scale=1">
		<style>
			body {
				font-size: 20px;
				background-color: #9c9f84
			}
			.centerbox {
				display: inline-block;
				text-align: center;
			}
			#treatButton {
				color: #090909;
				padding: 0.7em 1.7em;
				font-size: 30px;
				border-radius: 0.5em;
				background: #e8e8e8;
				border: 1px solid #e8e8e8;
				box-shadow: 0px 0px 0px rgba(0,0,0,0),
							0px 0px 0px rgba(0,0,0,0);
				cursor: pointer;
			}
			#treatButton:active {
				color: #666;
				box-shadow: inset 4px 4px 12px #c5c5c5,
							inset -4px -4px 12px #ffffff;
			}
			#treatButton:disabled,
			#treatButton[disabled]{
				background-color: #cccccc;
				color: #666666;
				pointer-events:none;
			}
			#validOrNot {
				padding-left:5px;
			}
			input[type='number']{
				width: 100px;
			} 
		</style>
	</head>
	<body>
		<div style="text-align: center;"> 							 
			<div class="centerbox"> 

				<div style="display: flex; align-items: center; justify-content: center;">
					<label for="reloadmachine"> Reload machine with this amount of treats: </label>
					<!-- <input id="reloadmachine" type="number" min="1" max="24" placeholder="1 <= x <= 24" onchange="HandleReloadInput()"> -->
					<input id="reloadmachine" type="number" min="1" onchange="HandleReloadInput()">
					<span id="validOrNot" padding-left="100"></span>
				</div>
				
				<p> Number of treats left: <span id="numtreats">-</span> </p>
				
				<button type="button" id="treatButton" onclick="HandleTreatButton()" disabled>Give Ollie a treat!</button>
				
				<p> Clients connected: <span id="nrclients">-</span> </p>

        <button type="button" id="resetWifiSettings" onclick="HandleResetWifiSettings()">Reset Wifi settings and reboot ESP8266!</button>

        <button type="button" id="restartESP" onclick="HandleRestartESP()">Reboot ESP8266!</button>
			</div>
		</div>
		
		
		<script>
		{
			let reloadStatement = "0 - Please reload the machine";
			let gateway = "ws://" + window.location.hostname + "/ws";
			let Socket;
		
			let maxMachineCapacity;
		
			let id_numtreats = document.getElementById("numtreats");
			let id_validOrNot = document.getElementById("validOrNot");
			let id_treatButton = document.getElementById("treatButton");
			let id_reloadmachine = document.getElementById("reloadmachine");
			let id_nrclients = document.getElementById("nrclients");
		
			function Init() {
				Socket = new WebSocket(gateway);
				Socket.onmessage = function(event) {
					processCommand(event);
				}
			}
		
			function HandleReloadInput() {
				let inputValue = id_reloadmachine.value;
				id_reloadmachine.value = [];
				if (inputValue < 1 || inputValue > maxMachineCapacity) {
					id_validOrNot.innerHTML = "&#x2717";
					if (isNaN(id_numtreats.innerHTML)) {  
						id_numtreats.innerHTML = reloadStatement;
						id_treatButton.disabled = true;
					}
				}
				else {
					id_validOrNot.innerHTML = "&#10003";
					let message = {
						type: "reloadmachine",
						value: inputValue
					}
					console.log("sent message: " + JSON.stringify(message));
					Socket.send(JSON.stringify(message));
				}
			}

			function HandleTreatButton() {
				console.log("treatButton");
				Socket.send("treatButton");
			}

			function HandleResetWifiSettings() {
				console.log("ResetWifi");
				Socket.send("ResetWifi");
			}

			function HandleRestartESP() {
				console.log("RestartESP");
				Socket.send("RestartESP");
			}
		
			function processCommand(event) {
				console.log(event.data);
				let obj = JSON.parse(event.data);
				let type = obj.type;
				if (type.localeCompare("machineCapacity") == 0) {
					maxMachineCapacity = obj.value;
					id_reloadmachine.setAttribute("max", maxMachineCapacity);
					id_reloadmachine.setAttribute("placeholder", "1 <= x <= " + maxMachineCapacity);
				}
				else if (type.localeCompare("nrClients") == 0) {
					id_nrclients.innerHTML = obj.value;
				}
				else if (type.localeCompare("treatsRemaining") == 0) {
					receivedValue = obj.value;
					if (receivedValue <= 0) {
						id_numtreats.innerHTML = reloadStatement;
						id_treatButton.disabled = true;
					}
					else {
						id_numtreats.innerHTML = receivedValue;
						if (id_treatButton.disabled) {id_treatButton.disabled = false;}
					}
				}
			}
		
			window.onload = function() {
				Init();
			}
		}
		</script>
	</body>
</html>
)=====";