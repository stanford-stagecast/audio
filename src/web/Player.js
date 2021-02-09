

window.onload = async function(e){
    audio = document.getElementById('audio');
    if (window.MediaSource) {
        var mediaSource = new MediaSource();
        mediaSource.addEventListener('sourceopen', sourceOpen);
        audio.src = URL.createObjectURL(mediaSource);
        audio.load();
        audio.play();
    } else {
        console.log("The Media Source Extensions API is not supported.")
    }
}

async function sourceOpen(e) {
    const audio = document.getElementById('audio');
    var mime = "audio/webm; codecs=\"opus\"";
    var mediaSource = this;
    var first = true;
    var sourceBuffer = mediaSource.addSourceBuffer(mime);
    const response = await fetch('http://127.0.0.1:8080');
    const reader = response.body.getReader();
    sourceBuffer.addEventListener('abort', function(e) {
        console.log('audio source buffer abort:', e);
    });
    sourceBuffer.addEventListener('error', function(e) {
        console.log('audio source buffer error:', e);
    });
    while(true){
        const {value, done} = await reader.read();
        if(done){
            break;
        }
        if(sourceBuffer.buffered.length > 0){
            if(first && audio.played.length == 0){ // if you start after the start
                first = false;
                audio.currentTime = sourceBuffer.buffered.start(0);
            }
            console.log("buffered", sourceBuffer.buffered.start(0), sourceBuffer.buffered.end(0));
        }
        if(audio.played.length > 0){
            console.log("played", audio.played.start(0), audio.played.end(0));
        }
        sourceBuffer.appendBuffer(value);
    }
}
