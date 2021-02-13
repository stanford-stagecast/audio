
queue = [];

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

function sourceOpen(e) {
    const audio = document.getElementById('audio');
    var mime = "audio/webm; codecs=\"opus\"";
    var mediaSource = this;
    var firstplay = true;
    var sourceBuffer = mediaSource.addSourceBuffer(mime);
    var firstmessage = true;

    queue.push = function( buffer ) { 
        // in case the message comes late
        if ( !sourceBuffer.updating && this.length == 0) { 
            sourceBuffer.appendBuffer( buffer ) 
        } else { 
            Array.prototype.push.call( this, buffer ) 
        } 
    }

    ws = new WebSocket("wss://stagecast.org:8081")
    ws.onmessage = function( e ) {
        if(sourceBuffer.buffered.length > 0){
            if(firstplay && audio.played.length == 0){ // if you start after the start
                firstplay = false;
                audio.currentTime = sourceBuffer.buffered.start(0);
            }
            console.log("buffered", sourceBuffer.buffered.start(0), sourceBuffer.buffered.end(0));
        }
        if(audio.played.length > 0){
            console.log("played", audio.played.start(0), audio.played.end(0));
        }
        if(!firstmessage){
            queue.push(e);
        } else {
            firstmessage = false;
            sourceBuffer.appendBuffer(e);
        }
    }

    sourceBuffer.addEventListener('abort', function(e) {
        console.log('audio source buffer abort:', e);
    });
    sourceBuffer.addEventListener('error', function(e) {
        console.log('audio source buffer error:', e);
    });
    sourceBuffer.addEventListener('updateend', function(e) {
        if (queue.length) {
            var data = queue.shift();
            console.log('data', data);
            sourceBuffer.appendBuffer(data);
        }
      });
}
