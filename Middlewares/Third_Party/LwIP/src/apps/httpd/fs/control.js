// LED Control
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

// Fake Button State Updates
const fakeBtn = document.getElementById('fakeBtn');
async function updateButtonState() {
    try {
        const response = await fetch('/button-state');
        const state = await response.json();
        fakeBtn.innerHTML = `<img src="img/${state.pressed ? 'pressed' : 'released'}.png" alt="${state.pressed ? 'Pressed' : 'Released'}">`;
    } catch (error) {
        console.error('Error:', error);
    }
    setTimeout(updateButtonState, 100); // Poll every 100ms
}

updateButtonState();