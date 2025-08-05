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
};

ws.onclose = () => {
    console.log('WebSocket disconnected');
    // Attempt to reconnect after a delay
    setTimeout(() => window.location.reload(), 1000);
};

ws.onerror = (error) => {
    console.error('WebSocket error:', error);
};

// Update button display
function updateButtonState(pressed) {
    const fakeBtn = document.getElementById('fakeBtn');
    fakeBtn.innerHTML = `<img src="img/${pressed ? 'pressed' : 'released'}.png" 
                            alt="${pressed ? 'Pressed' : 'Released'}">`;
}

// LED Control remains the same
const ledToggle = document.getElementById('ledToggle');
ledToggle.addEventListener('click', async () => {
    const isOn = ledToggle.classList.toggle('on');
    ledToggle.textContent = isOn ? 'ON' : 'OFF';
    
    try {
        await fetch('/led', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ state: isOn ? 1 : 0 })
        });
    } catch (error) {
        console.error('Error:', error);
    }
});