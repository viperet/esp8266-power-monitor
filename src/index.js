import './style.scss';

function $(selector) {
    let el = document.querySelector(selector);
    if (!el) {
        let empty = () => {};
        el = { remove: empty, text: empty, removeClass: empty };
    } else {
        el.text = text => el.innerText = text;
        el.removeClass = (className) => el.classList.remove(className);
    }
    return el;
}

function updateDisplay(data) {
    $('#container').removeClass('hidden');
    $('#loader').remove();
    $('#voltage').text(data.voltage.toFixed(0));
    $('#frequency').text(data.frequency.toFixed(0));
    $('#current').text(data.current);
    $('#power').text(data.power);
    $('#energy').text(data.energy);
    $('#pf').text(data.pf);
}

if (process.env.NODE_ENV == 'production') {
    var ws = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);
    ws.onopen = function () { ws.send('Connected ' + new Date()); };
    ws.onerror = function (error) { console.log('WebSocket Error ', error); };
    ws.onmessage = (e) => {
        console.log('Received: ', e.data);
        let data = JSON.parse(e.data);
        updateDisplay(data);
    };
} else {
    setTimeout(() => updateDisplay({"voltage":222.9,"current":0.035,"power":0.9,"energy":0.003,"frequency":50.0,"pf":0.12}), 2000);
}