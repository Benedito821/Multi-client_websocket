// WebSocket Client
const ws = new WebSocket(`ws://${window.location.hostname}/ws`);

ws.onopen = () => {
    console.log('WebSocket connected');
};

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    if (data.type === 'button') {
        updateButtonState(data.pressed);
    }
    else if (data.type === 'led') {
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
        return data.state; // Should return 0 or 1
    } catch (error) {
        console.error('Failed to fetch LED state:', error);
        return 0; // Default to OFF if fetch fails
    }
}

// Modify your existing LED toggle setup
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
        // Get the CURRENT hardware state first (to avoid UI/hardware desync)
        const currentState = await fetch('/led-state').then(res => res.json());
        const newState = currentState.state ? 0 : 1; // Toggle the actual state
        
        // Optimistic UI update (will be corrected by WebSocket if wrong)
        updateLedUI(newState);
        
        // Send the command to server
        const response = await fetch('/led', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ state: newState })
        });
        
        // Verify the change was accepted
        if (!response.ok) {
            throw new Error('Server rejected the change');
        }
    } catch (error) {
        console.error('LED toggle failed:', error);
        // Revert UI to actual state
        const actualState = await fetch('/led-state').then(res => res.json());
        updateLedUI(actualState.state);
    }
});

// Update button display
function updateButtonState(pressed) {
    const fakeBtn = document.getElementById('fakeBtn');
    fakeBtn.innerHTML = `<img src="img/${pressed ? 'pressed' : 'released'}.png" 
                            alt="${pressed ? 'Pressed' : 'Released'}">`;
}