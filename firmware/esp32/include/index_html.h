#ifndef INDEX_HTML_H
#define INDEX_HTML_H

#include <Arduino.h>

    const char INDEX_HTML[] PROGMEM = R"=====(
    <!DOCTYPE html>
    <html lang="en">

    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
        <title>Mainframe Cellular Engine</title>
        <style>
            :root {
                --bg-dark: #0a0c10;
                --panel-bg: #121620;
                --border-glow: #00ff66;
                --text-main: #e2e8f0;
                --accent-red: #ff3366;
                --accent-blue: #00d2ff;
                --accent-green: #00ff66;
                --accent-yellow: #ffcc00;

                /* Dynamic target variable for game view tinting */
                --dynamic-team-bg: #05070a;
            }

            * {
                box-sizing: border-box;
                margin: 0;
                padding: 0;
                font-family: 'Courier New', monospace;
                user-select: none;
            }

            body {
                background-color: var(--bg-dark);
                color: var(--text-main);
                overflow: hidden;
                height: 100vh;
                display: flex;
                flex-direction: column;
            }

            /* Layout Containers */
            .screen {
                display: flex;
                flex-direction: column;
                align-items: center;
                justify-content: center;
                flex: 1;
                padding: 20px;
                box-sizing: border-box;
            }

            .hidden {
                display: none !important;
            }

            .panel {
                background: var(--panel-bg);
                border: 2px solid #202b3c;
                border-radius: 8px;
                padding: 25px;
                width: 100%;
                max-width: 500px;
                box-shadow: 0 0 15px rgba(0, 0, 0, 0.5);
                text-align: center;
            }

            h1 {
                font-size: 1.6rem;
                color: var(--border-glow);
                margin-bottom: 20px;
                text-shadow: 0 0 8px rgba(0, 255, 102, 0.3);
            }

            h2 {
                font-size: 1.1rem;
                margin-bottom: 15px;
                color: #8fa0b5;
            }

            /* Cyberpunk Dropdown Styling */
            select {
                width: 100%;
                background: #1b2330;
                border: 1px solid #3b4e68;
                color: var(--text-main);
                padding: 12px;
                font-weight: bold;
                border-radius: 4px;
                font-family: 'Courier New', monospace;
                outline: none;
                cursor: pointer;
                transition: all 0.2s;
                appearance: none;
                /* Strip native OS styling */
                -webkit-appearance: none;
                -moz-appearance: none;
                background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10' viewBox='0 0 10 10'><path d='M0,3 L5,8 L10,3 Z' fill='%238fa0b5'/></svg>");
                background-repeat: no-repeat;
                background-position: calc(100% - 15px) center;
            }

            select:hover:not(:disabled) {
                background-color: #283549;
                border-color: #536e92;
            }

            select:focus {
                border-color: var(--border-glow);
                color: var(--border-glow);
                box-shadow: 0 0 10px rgba(0, 255, 102, 0.2);
                background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10' viewBox='0 0 10 10'><path d='M0,3 L5,8 L10,3 Z' fill='%2300ff66'/></svg>");
            }

            select:disabled {
                opacity: 0.3;
                cursor: not-allowed;
            }

            select option {
                background: var(--panel-bg);
                color: var(--text-main);
            }

            button {
                background: #1b2330;
                border: 1px solid #3b4e68;
                color: var(--text-main);
                padding: 12px;
                font-weight: bold;
                border-radius: 4px;
                cursor: pointer;
                transition: all 0.2s;
            }

            button:hover:not(:disabled) {
                background: #283549;
                border-color: #536e92;
            }

            button:disabled {
                opacity: 0.3;
                cursor: not-allowed;
            }

            /* Color Quadrant Logic */
            .color-picker {
                display: grid;
                grid-template-columns: repeat(2, 1fr);
                gap: 12px;
                margin-bottom: 25px;
            }

            .color-btn {
                position: relative;
                border-width: 2px;
            }

            .color-btn[data-color="1"] {
                border-color: var(--accent-red);
                color: var(--accent-red);
            }

            .color-btn[data-color="2"] {
                border-color: var(--accent-blue);
                color: var(--accent-blue);
            }

            .color-btn[data-color="3"] {
                border-color: var(--accent-green);
                color: var(--accent-green);
            }

            .color-btn[data-color="4"] {
                border-color: var(--accent-yellow);
                color: var(--accent-yellow);
            }

            .color-btn[data-color="1"].selected {
                background: var(--accent-red);
                color: #000;
            }

            .color-btn[data-color="2"].selected {
                background: var(--accent-blue);
                color: #000;
            }

            .color-btn[data-color="3"].selected {
                background: var(--accent-green);
                color: #000;
            }

            .color-btn[data-color="4"].selected {
                background: var(--accent-yellow);
                color: #000;
            }

            /* Taken/Occupied Overlay State */
            .color-btn.taken {
                opacity: 0.15;
                border-style: dashed;
                background: transparent !important;
                cursor: not-allowed;
            }

            .terminal-list {
                text-align: left;
                background: #080a0e;
                padding: 12px;
                border-radius: 4px;
                height: 110px;
                overflow-y: auto;
                margin-bottom: 20px;
                border: 1px solid #1a2230;
            }

            .terminal-item {
                font-size: 0.85rem;
                margin-bottom: 6px;
                display: flex;
                justify-content: space-between;
            }

            /* Deploy Action Controls */
            #start-btn {
                width: 100%;
                background: #004d1a;
                border-color: var(--accent-green);
                color: var(--accent-green);
                font-size: 1.1rem;
            }

            #start-btn:hover:not(:disabled) {
                background: var(--accent-green);
                color: #000;
                box-shadow: 0 0 15px rgba(0, 255, 102, 0.4);
            }

            /* Countdown Overlay Viewport */
            #countdown-screen h2 {
                font-size: 5rem;
                color: var(--accent-blue);
                text-shadow: 0 0 20px rgba(0, 210, 255, 0.4);
                margin: 20px 0;
            }

            /* Tactical Arena Map Viewport */
            #game-screen {
                padding: 10px;
                justify-content: flex-start;
                background-color: var(--dynamic-team-bg);
                transition: background-color 0.4s ease;
            }

            .canvas-container {
                position: relative;
                width: 100%;
                max-width: 900px;
                aspect-ratio: 100 / 56;
                background: #05070a;
                border: 2px solid #1f293d;
                border-radius: 6px;
                box-shadow: 0 0 20px rgba(0, 0, 0, 0.7);
            }

            canvas {
                width: 100%;
                height: 100%;
                display: block;
                image-rendering: pixelated;
            }

            /* Seed Controller Selection Deck */
            .control-deck {
                display: flex;
                width: 100%;
                max-width: 900px;
                gap: 10px;
                margin-top: 15px;
                justify-content: space-between;
            }

            .macro-btn {
                flex: 1;
                font-size: 0.75rem;
                padding: 10px 5px;
                text-transform: uppercase;
                display: flex;
                flex-direction: column;
                gap: 4px;
                background: rgba(27, 35, 48, 0.85);
            }

            .macro-btn span {
                font-size: 0.9rem;
                color: #fff;
            }

            .macro-btn.selected-macro {
                border-color: var(--border-glow);
                color: var(--border-glow);
                background: rgba(0, 255, 102, 0.15);
            }

            /* Engine Telemetry HUD */
            .hud-overlay {
                display: grid;
                grid-template-columns: repeat(4, 1fr);
                width: 100%;
                max-width: 900px;
                gap: 10px;
                margin-top: 12px;
            }

            .hud-card {
                background: rgba(18, 22, 32, 0.85);
                border: 1px solid #1f293d;
                padding: 8px;
                border-radius: 4px;
                font-size: 0.75rem;
                text-align: left;
                opacity: 0.4;
                backdrop-filter: blur(4px);
            }

            .hud-card.active-node {
                opacity: 1;
                border-color: #3b4e68;
            }

            .hud-card .title {
                font-weight: bold;
                margin-bottom: 4px;
                white-space: nowrap;
                overflow: hidden;
                text-overflow: ellipsis;
            }

            .hud-card .stat {
                font-size: 0.85rem;
                color: #fff;
                font-family: monospace;
            }

            /* Master Game Over Screen Overlay */
            #game-over-screen h2 {
                font-size: 2.2rem;
                color: var(--accent-red);
                margin-bottom: 20px;
            }

            .win-banner {
                font-size: 1.3rem;
                margin-bottom: 30px;
                text-transform: uppercase;
                letter-spacing: 2px;
            }
        </style>
    </head>

    <body>

        <div id="lobby-screen" class="screen">
            <div class="panel">
                <h1>CYBER MAINFRAME</h1>

                <h2>CRITICAL CONFIGURATION MODE</h2>
                <div style="margin-bottom: 25px;">
                    <select id="mode-dropdown" onchange="changeMode(this.value)">
                        <option value="0">CORE MELTDOWN</option>
                        <option value="1">GENETIC MUTATION</option>
                        <option value="2">CRITICAL MASS</option>
                    </select>
                </div>

                <h2>SELECT QUANTUM IDENTITY</h2>
                <div class="color-picker">
                    <button class="color-btn" data-color="1" onclick="claimColor(1)">RED_ROOTKIT</button>
                    <button class="color-btn" data-color="2" onclick="claimColor(2)">BLUE_BLUEPRINT</button>
                    <button class="color-btn" data-color="3" onclick="claimColor(3)">GREEN_GHOST</button>
                    <button class="color-btn" data-color="4" onclick="claimColor(4)">YELLOW_AGENT</button>
                </div>

                <h2>CONNECTED CORE TERMINALS</h2>
                <div id="terminal-display" class="terminal-list"></div>

                <button id="start-btn" class="hidden" onclick="requestLaunch()" disabled>LAUNCH SIMULATION [REQ
                    2P]</button>
            </div>
        </div>

        <div id="countdown-screen" class="screen hidden">
            <div class="panel">
                <h1>COMMENCING INJECTION</h1>
                <h2 id="countdown-timer">3</h2>
                <p style="color: #627d98; font-size: 0.85rem;" id="countdown-mode-label">PREPARING MEMORY REALLOCATION
                </p>
            </div>
        </div>

        <div id="game-screen" class="screen hidden">
            <div class="canvas-container">
                <canvas id="arena"></canvas>
            </div>

            <div class="control-deck">
                <button class="macro-btn selected-macro" data-seed="1" onclick="selectSeed(1)">
                    <div>GLIDER</div><span>15 EP</span>
                </button>
                <button class="macro-btn" data-seed="2" onclick="selectSeed(2)">
                    <div>BLOCK</div><span>25 EP</span>
                </button>
                <button class="macro-btn" data-seed="3" onclick="selectSeed(3)">
                    <div>BLINKER</div><span>30 EP</span>
                </button>
                <button class="macro-btn" data-seed="4" onclick="selectSeed(4)">
                    <div>GUN [COMPACT]</div><span>75 EP</span>
                </button>
            </div>

            <div id="hud-deck" class="hud-overlay">
                <div class="hud-card" id="card-1">
                    <div class="title" style="color:var(--accent-red)">RED_ROOT</div>
                    <div class="stat" id="stat-1">OFFLINE</div>
                </div>
                <div class="hud-card" id="card-2">
                    <div class="title" style="color:var(--accent-blue)">BLUE_BLUE</div>
                    <div class="stat" id="stat-2">OFFLINE</div>
                </div>
                <div class="hud-card" id="card-3">
                    <div class="title" style="color:var(--accent-green)">GREEN_GHST</div>
                    <div class="stat" id="stat-3">OFFLINE</div>
                </div>
                <div class="hud-card" id="card-4">
                    <div class="title" style="color:var(--accent-yellow)">YEL_AGENT</div>
                    <div class="stat" id="stat-4">OFFLINE</div>
                </div>
            </div>
        </div>

        <div id="game-over-screen" class="screen hidden">
            <div class="panel">
                <h2>SIMULATION HALTED</h2>
                <div id="winner-announcement" class="win-banner">NEUTRALIZED MATRIX</div>
                <button id="return-btn" class="hidden" onclick="requestLobbyReturn()"
                    style="width:100%; border-color:var(--accent-blue); color:var(--accent-blue);">RE-INITIALIZE MAIN
                    LOBBY</button>
            </div>
        </div>

        <script>
            let ws;
            let myId = null;
            let activeSeed = 1;
            let localCountdownVal = 3;
            let countdownTicker = null;
            let myColorInt = 0; // Tracks player identity locally

            const canvas = document.getElementById('arena');
            const ctx = canvas.getContext('2d');
            canvas.width = 100;
            canvas.height = 56;

            const PALETTE = {
                0: '#05070a', // NONE
                1: '#ff3366', // RED
                2: '#00d2ff', // BLUE
                3: '#00ff66', // GREEN
                4: '#ffcc00'  // YELLOW
            };

            // Deep desaturated/low-opacity palette for screen backgrounds (maintains UI contrast)
            const BG_PALETTE = {
                0: '#05070a',
                1: 'rgba(255, 51, 102, 0.08)',  // Muted Red
                2: 'rgba(0, 210, 255, 0.08)',  // Muted Blue
                3: 'rgba(0, 255, 102, 0.06)',  // Muted Green
                4: 'rgba(255, 204, 0, 0.06)'   // Muted Yellow
            };

            function initWebSocket() {
                ws = new WebSocket('ws://' + window.location.hostname + '/ws');
                ws.binaryType = "arraybuffer";

                ws.onopen = () => { console.log("System Engine Link Active."); };
                ws.onclose = () => { setTimeout(initWebSocket, 2000); };

                ws.onmessage = (event) => {
                    if (event.data instanceof ArrayBuffer) {
                        renderBinaryGrid(new Uint8Array(event.data));
                        return;
                    }

                    const data = JSON.parse(event.data);

                    if (data.status === "IAM") {
                        myId = data.id;
                        ws.send(JSON.stringify({ action: "join_lobby" }));
                    }
                    else if (data.status === "LOBBY_UPDATE") {
                        processLobbyState(data);
                    }
                    else if (data.status === "START_COUNTDOWN") {
                        switchView('countdown-screen');
                        triggerVisualCountdown(data.mode);
                    }
                    else if (data.status === "LAUNCH_GAME") {
                        clearInterval(countdownTicker);

                        // Apply dynamic background tint based on player's assigned team color index
                        const targetBgColor = BG_PALETTE[myColorInt] || BG_PALETTE[0];
                        document.documentElement.style.setProperty('--dynamic-team-bg', targetBgColor);

                        switchView('game-screen');
                    }
                    else if (data.status === "GAME_TICK") {
                        updateTelemetryHUD(data.nodes);
                    }
                    else if (data.status === "GAME_OVER") {
                        processGameOver(data);
                    }
                    else if (data.status === "LOBBY_ERROR") {
                        alert(data.message);
                    }
                };
            }

            function switchView(screenId) {
                document.getElementById('lobby-screen').classList.add('hidden');
                document.getElementById('countdown-screen').classList.add('hidden');
                document.getElementById('game-screen').classList.add('hidden');
                document.getElementById('game-over-screen').classList.add('hidden');
                document.getElementById(screenId).classList.remove('hidden');
            }

            function changeMode(modeInt) {
                ws.send(JSON.stringify({ action: "select_mode", mode: parseInt(modeInt) }));
            }

            function claimColor(colorInt) {
                ws.send(JSON.stringify({ action: "select_color", color: colorInt }));
            }

            function requestLaunch() {
                ws.send(JSON.stringify({ action: "start_match" }));
            }

            function requestLobbyReturn() {
                ws.send(JSON.stringify({ action: "return_to_lobby" }));
            }

            function selectSeed(seedId) {
                activeSeed = seedId;
                document.querySelectorAll('.macro-btn').forEach(btn => {
                    btn.classList.toggle('selected-macro', parseInt(btn.getAttribute('data-seed')) === seedId);
                });
            }

            function processLobbyState(data) {
                switchView('lobby-screen');

                const modeDropdown = document.getElementById('mode-dropdown');
                if (modeDropdown) {
                    modeDropdown.value = data.currentMode;
                    modeDropdown.disabled = (myId !== data.hostId);
                }

                const colorButtons = document.querySelectorAll('.color-btn');
                colorButtons.forEach(btn => {
                    btn.disabled = false;
                    btn.classList.remove('selected', 'taken');
                });

                let readyPlayersCount = 0;
                const terminalDisplay = document.getElementById('terminal-display');
                terminalDisplay.innerHTML = '';

                data.terminals.forEach(t => {
                    const isMe = (t.id === myId);
                    const isHost = (t.id === data.hostId);

                    if (isMe) {
                        myColorInt = t.color; // Save local client color identity
                    }

                    if (t.color > 0) {
                        readyPlayersCount++;
                        const btn = document.querySelector(`.color-btn[data-color="${t.color}"]`);
                        if (btn) {
                            if (isMe) {
                                btn.classList.add('selected');
                            } else {
                                btn.classList.add('taken');
                                btn.disabled = true;
                            }
                        }
                    }

                    const line = document.createElement('div');
                    line.className = 'terminal-item';
                    line.innerHTML = `<span>Terminal ID: ${t.id} ${isMe ? '(YOU)' : ''} ${isHost ? '[HOST]' : ''}</span>
                                  <span style="color:${PALETTE[t.color] || '#8fa0b5'}">${t.color > 0 ? 'TEAM_ASSIGNED' : 'UNASSIGNED'}</span>`;
                    terminalDisplay.appendChild(line);
                });

                const startBtn = document.getElementById('start-btn');
                if (myId === data.hostId) {
                    startBtn.classList.remove('hidden');
                    startBtn.disabled = (readyPlayersCount < 2);
                    startBtn.innerText = readyPlayersCount >= 2 ? "LAUNCH SIMULATION" : `WAITING FOR TEAMS [${readyPlayersCount}/2]`;
                } else {
                    startBtn.classList.add('hidden');
                }
            }

            function triggerVisualCountdown(modeInt) {
                clearInterval(countdownTicker);
                localCountdownVal = 3;
                document.getElementById('countdown-timer').innerText = localCountdownVal;

                const modes = ["INITIALIZING CORE MELTDOWN ROUTINE", "STABILIZING GENETIC TAKEOVER PROTOCOL", "CRITICAL MASS RATIO ASSIGNMENT"];
                document.getElementById('countdown-mode-label').innerText = modes[modeInt] || "LOADING ENGINE";

                countdownTicker = setInterval(() => {
                    localCountdownVal--;
                    if (localCountdownVal > 0) {
                        document.getElementById('countdown-timer').innerText = localCountdownVal;
                    } else {
                        document.getElementById('countdown-timer').innerText = "INJECTING";
                        clearInterval(countdownTicker);
                    }
                }, 1000);
            }

            function updateTelemetryHUD(nodes) {
                for (let i = 1; i <= 4; i++) {
                    document.getElementById(`card-${i}`).classList.remove('active-node');
                    document.getElementById(`stat-${i}`).innerText = "OFFLINE";
                }

                nodes.forEach(n => {
                    const card = document.getElementById(`card-${n.color}`);
                    const stat = document.getElementById(`stat-${n.color}`);
                    if (!card || !stat) return;

                    card.classList.add('active-node');
                    if (n.dead) {
                        stat.innerHTML = `<span style="color:var(--accent-red)">MUTED / DEAD</span>`;
                    } else {
                        stat.innerHTML = `EP:${n.ep} HP:${n.hp}%<br>DNA:${n.dna}% DOM:${n.eq}%`;
                    }
                });
            }

            function renderBinaryGrid(buf) {
                if (buf.length < 102 || buf[0] !== 0xAA || buf[buf.length - 1] !== 0xBB) return;

                const gridCells = buf.subarray(2, buf.length - 1);
                ctx.clearRect(0, 0, canvas.width, canvas.height);

                for (let y = 0; y < 56; y++) {
                    for (let x = 0; x < 100; x++) {
                        const cellVal = gridCells[y * 100 + x];
                        if (cellVal > 0) {
                            ctx.fillStyle = PALETTE[cellVal] || '#ffffff';
                            ctx.fillRect(x, y, 1, 1);
                        }
                    }
                }
            }

            function processGameOver(data) {
                switchView('game-over-screen');
                const announcement = document.getElementById('winner-announcement');

                // Dictionary mapping the numerical team ID to its clear-text identity string
                const TEAM_NAMES = {
                    1: "RED_ROOTKIT",
                    2: "BLUE_BLUEPRINT",
                    3: "GREEN_GHOST",
                    4: "YELLOW_AGENT"
                };

                if (data.winner === 0) {
                    announcement.innerText = "MUTUAL ANNIHILATION: SYSTEM CORES DECAYED";
                    announcement.style.color = "var(--text-main)";
                    announcement.style.textShadow = "none";
                } else {
                    const teamName = TEAM_NAMES[data.winner] || `VECTOR_ID_${data.winner}`;
                    const teamColor = PALETTE[data.winner] || '#ffffff';

                    // Update the screen text with the clear-text team name
                    announcement.innerText = `VICTORY SECURED BY VECTOR: ${teamName}`;

                    // Dynamically style the text color and add a clean cyberpunk color-glow
                    announcement.style.color = teamColor;
                    announcement.style.textShadow = `0 0 15px ${teamColor}80`; // 80 adds 50% transparency to the glow glow
                }

                const returnBtn = document.getElementById('return-btn');
                if (myId === data.hostId) {
                    returnBtn.classList.remove('hidden');
                } else {
                    returnBtn.classList.add('hidden');
                }
            }

            canvas.addEventListener('click', (e) => {
                const rect = canvas.getBoundingClientRect();
                const scaleX = canvas.width / rect.width;
                const scaleY = canvas.height / rect.height;

                const x = Math.floor((e.clientX - rect.left) * scaleX);
                const y = Math.floor((e.clientY - rect.top) * scaleY);

                if (x >= 0 && x < 100 && y >= 0 && y < 56) {
                    const payload = new Uint8Array([activeSeed, x, y]);
                    if (ws && ws.readyState === WebSocket.OPEN) {
                        ws.send(payload);
                    }
                }
            });

            window.onload = initWebSocket;
        </script>
    </body>

    </html>
    )=====";

    #endif