// WebSocket Client
const ws = new WebSocket(`ws://${window.location.hostname}/ws`);

ws.onopen = () => {
    console.log('WebSocket connected');
};

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    if (data.type === 'button') {
        console.log(`Button state changed: ${data.pressed ? 'PRESSED' : 'RELEASED'}`);
        updateButtonState(data.pressed);
    }
    else if (data.type === 'led') {
        console.log(`LED state changed: ${data.state ? 'ON' : 'OFF'}`);
        updateLedUI(data.state);
    }
};

ws.onclose = () => {
    console.log('WebSocket disconnected');
    // Attempt to reconnect after a delay
    setTimeout(() => window.location.reload(), 1000);
};

ws.onerror = (error) => {
    console.error('WebSocket error:', error);
};

async function fetchInitialState() {
    try {
        const response = await fetch('/led-state');
        const data = await response.json();
        console.log(`Initial LED state fetched: ${data.state ? 'ON' : 'OFF'}`);
        return data.state;
    } catch (error) {
        console.error('Failed to fetch LED state:', error);
        return 0;
    }
}

const ledToggle = document.getElementById('ledToggle');

// Initialize LED state on page load
window.addEventListener('DOMContentLoaded', async () => {
    const ledState = await fetchInitialState();
    updateLedUI(ledState);
});

function updateLedUI(isOn) {
    ledToggle.classList.toggle('on', isOn);
    ledToggle.textContent = isOn ? 'ON' : 'OFF';
}

ledToggle.addEventListener('click', async () => {
    try {
        const currentState = await fetch('/led-state').then(res => res.json());
        const newState = currentState.state ? 0 : 1;
        console.log(`User toggled LED to: ${newState ? 'ON' : 'OFF'}`);
        
        updateLedUI(newState);
        
        const response = await fetch('/led', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ state: newState })
        });
        
        if (!response.ok) {
            throw new Error('Server rejected the change');
        }
    } catch (error) {
        console.error('LED toggle failed:', error);
        const actualState = await fetch('/led-state').then(res => res.json());
        updateLedUI(actualState.state);
    }
});

// Update button display
function updateButtonState(pressed) {
    const imgName = pressed ? 'pressed' : 'released';
    console.log(`Requesting button image: img/${imgName}.png`);
    
    const fakeBtn = document.getElementById('fakeBtn');
    fakeBtn.innerHTML = `<img src="img/${imgName}.png" 
                            alt="${pressed ? 'Pressed' : 'Released'}">`;
}