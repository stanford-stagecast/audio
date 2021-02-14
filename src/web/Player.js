
queue = [];

window.onload = async function(e){
    audio = document.getElementById('audio');
    if (window.MediaSource) {
        var mediaSource = new MediaSource();
        mediaSource.addEventListener('sourceopen', sourceOpen);
        audio.src = URL.createObjectURL(mediaSource);
        audio.load();
//	URL.revokeObjectURL(audio.src);
    } else {
        console.log("The Media Source Extensions API is not supported.")
    }
}

function sourceOpen(e) {
    const audio = document.getElementById('audio');
    var mime = 'audio/webm; codecs="opus"';
    var mediaSource = this;
    var firstplay = true;
    var sourceBuffer = mediaSource.addSourceBuffer(mime);

    if ( !mediaSource ) {
	console.log( "no MediaSource" );
	return;
    }
    
    queue.push = function( buffer ) { 
        Array.prototype.push.call( this, buffer ) 
	bump();
    }

    var playing = false;
    
    bump = function() {
        if (queue.length > 0 && !sourceBuffer.updating) {
            sourceBuffer.appendBuffer(queue.shift());

	    document.getElementById('buffer').innerHTML = sourceBuffer.buffered.end(0) - audio.currentTime;
	    
	    if ( sourceBuffer.buffered.length > 0 ) {
		if ( sourceBuffer.buffered.end(0) > (audio.currentTime + 0.3)
		     && sourceBuffer.buffered.start(0) <= (sourceBuffer.buffered.end(0) - 0.05) ) {
		    audio.currentTime = sourceBuffer.buffered.end(0) - 0.05;
		    console.log( "adjustment, curtime now " + audio.currentTime + " on " + sourceBuffer.buffered.start(0) + " .. " + sourceBuffer.buffered.end(0) );
		}
	    }
	}
    }

    ws = new WebSocket("wss://stagecast.org:8081");
    ws.binaryType = 'arraybuffer';
    ws.onmessage = function( e ) {
        queue.push(e.data);

//	console.log("buffered: " + sourceBuffer.buffered.start(0) + " to " + sourceBuffer.buffered.end(0) );

	if ( firstplay && sourceBuffer.buffered.length > 0 ) {
            audio.currentTime = sourceBuffer.buffered.start(0);
	    firstplay = false;
	}
    }

    sourceBuffer.addEventListener('abort', function(e) {
        console.log('audio source buffer abort:', e);
    });
    sourceBuffer.addEventListener('error', function(e) {
        console.log('audio source buffer error:', e);
    });
    sourceBuffer.addEventListener('updateend', bump);

    audio.oncanplay = function() {
	var play_promise = audio.play();

	if ( play_promise == undefined ) {
	    console.log( "undefined promise" );
	} else {
	    play_promise.then(function() {
		playing = true;
		audio.oncanplay = function() {};
	    }).catch(function(error) {
		console.log( "play error: " + error );
	    });
	} 
    }
}
