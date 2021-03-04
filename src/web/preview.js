videoqueue = [];

window.onload = async function(e){
    video = document.getElementById('video');
    video.latencyHint = 0;
    if (window.MediaSource) {
	var mediaSourceVideo = new MediaSource();
        mediaSourceVideo.addEventListener('sourceopen', sourceOpenVideo);
        video.src = URL.createObjectURL(mediaSourceVideo);
	video.load();
    } else {
        console.log("The Media Source Extensions API is not supported.")
    }
}

var ws;

function set_scene(name) {
    ws.send("scene " + name );
}

function sourceOpenVideo(e) {
    const audio = document.getElementById('video');
    var mime = 'video/mp4; codecs="avc1.64001E"';
    var mediaSourceVideo = this;
    var firstplay = true;
    var videoSourceBuffer = mediaSourceVideo.addSourceBuffer(mime);

    if ( !mediaSourceVideo ) {
	console.log( "no MediaSource" );
	return;
    }
    
    videoqueue.push = function( buffer ) { 
        Array.prototype.push.call( this, buffer ) 
	videobump();
    }

    var playing = false;

    var resets = 0;
    document.getElementById('videoresets').innerHTML = "resets: " + resets;

    setInterval(function(){
	if ( videoSourceBuffer.buffered.length > 0 ) {
	    var buffer_duration = videoSourceBuffer.buffered.end(0) - video.currentTime;
	    ws.send("buffer " + buffer_duration.toFixed(3));
	}
    }, 50);
    
    var videobump = function() {
        if (videoqueue.length > 0 && !videoSourceBuffer.updating) {
	    videoSourceBuffer.appendBuffer(videoqueue.shift());

	    if ( videoSourceBuffer.buffered.length > 0 ) {
		var buffer_duration = (videoSourceBuffer.buffered.end(0) - video.currentTime);
		document.getElementById('videobuffer').innerHTML = "buffer: " + (24*buffer_duration).toFixed(0) + " frames";

		if ( playing && (buffer_duration > 0.5) ) {
		    video.currentTime = videoSourceBuffer.buffered.end(0) - 0.25;
		    resets++;
		    document.getElementById('videoresets').innerHTML = "resets: " + resets;
		}
	    }
	}
    }

    ws = new WebSocket("wss://stagecast.org:8401");
    ws.binaryType = 'arraybuffer';
    var decoder = new TextDecoder("utf-8");
    ws.onmessage = function( e ) {
	var type_byte = new DataView(e.data, 0, 1).getUint8(0);
	var rest = e.data.slice(1);
	if ( type_byte == 1 ) {
	    document.getElementById('message').innerHTML = new TextDecoder("utf-8").decode(rest);
	    return;
	}
	if ( type_byte == 2 ) {
	    add_control( decoder.decode( rest ) );
	    return;
	}
	if ( type_byte == 3 ) {
	    
	    return;
	}

	videoqueue.push(rest);

	if ( firstplay && videoSourceBuffer.buffered.length > 0 ) {
            video.currentTime = videoSourceBuffer.buffered.start(0);
	    firstplay = false;
	}
    }

    var add_control = function(name) {
	var div = document.getElementById('buttons');
	div.innerHTML += "<button onclick='set_scene(this.id)' type='button' id='" + name + "'>" + name + "</button>";
    }
    
    videoSourceBuffer.addEventListener('abort', function(e) {
	document.getElementById('status').innerHTML = "playback abort";
        console.log('video source buffer abort:', e);
    });
    videoSourceBuffer.addEventListener('error', function(e) {
	document.getElementById('status').innerHTML = "playback error";
	ws.onmessage = function() {}
    });
    videoSourceBuffer.addEventListener('updateend', videobump);

    video.oncanplay = function() {
	var play_promise = video.play();

	if (play_promise !== undefined) {
	    play_promise.then(function() {
		// playback started; only render UI here
		document.getElementById('status').innerHTML = 'Playing...';
		playing = true;
	    }).catch(function(error) {
		// playback failed
		document.getElementById('status').innerHTML = 'Play failed. Please click play to begin playing.';
		playing = false;
	    });
	}
    };
}

