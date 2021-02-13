
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

async function sourceOpen(e) {
    const audio = document.getElementById('audio');
    var mime = "audio/webm; codecs=\"opus\"";
    var mediaSource = this;
    var firstplay = true;
    queue.push = function( buffer ) { 
        // in case the message comes late
        if ( !sourceBuffer.updating && this.length == 0) { 
            sourceBuffer.appendBuffer( buffer ) 
        } else { 
            Array.prototype.push.call( this, buffer ) 
        } 
    }

    var firstmessage = true;
    var sourceBuffer = mediaSource.addSourceBuffer(mime);
    const response = await fetch('http://127.0.0.1:8080');
    const reader = response.body.getReader();
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
    while(true){
        const {value, done} = await reader.read();
        if(done){
            break;
        }
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
            queue.push(value);
        } else {
            firstmessage = false;
            sourceBuffer.appendBuffer(value);
        }   
    }
}
