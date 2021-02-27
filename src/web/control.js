ws = new WebSocket("wss://stagecast.org:8400");
ws.binaryType = 'arraybuffer';
ws.onmessage = function( e ) {
    document.getElementById('controls').innerHTML = e.data;
}
