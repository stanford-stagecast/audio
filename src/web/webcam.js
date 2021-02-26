videoqueue = [];

window.onload = async function(e){
    video = document.getElementById('video');
    if (window.MediaSource) {
	var mediaSourceVideo = new MediaSource();
        mediaSourceVideo.addEventListener('sourceopen', sourceOpenVideo);
        video.src = URL.createObjectURL(mediaSourceVideo);
	video.load();
    } else {
        console.log("The Media Source Extensions API is not supported.")
    }
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

    var resets = 0.0;
    document.getElementById('videobuffer').innerHTML = "resets: " + resets;

    videobump = function() {
        if (videoqueue.length > 0 && !videoSourceBuffer.updating) {
            videoSourceBuffer.appendBuffer(videoqueue.shift());

	    if ( videoSourceBuffer.buffered.length > 0 ) {
		var buffer_duration = (videoSourceBuffer.buffered.end(0) - video.currentTime);
		document.getElementById('videobuffer').innerHTML = "buffer: " + buffer_duration;

		if ( buffer_duration > 5 ) {
		    videoSourceBuffer.remove(0, videoSourceBuffer.buffered.end(0) - 5.0);
		}
		
		if ( buffer_duration > 0.5 ) {
		    video.currentTime = videoSourceBuffer.buffered.end(0) - 0.25;
		    resets++;
		    document.getElementById('videoresets').innerHTML = "resets: " + resets;
		}
	    }
	}
    }

    ws = new WebSocket("ws://localhost:8400");
    ws.binaryType = 'arraybuffer';
    ws.onmessage = function( e ) {
        videoqueue.push(e.data);

//	console.log("buffered: " + videoSourceBuffer.buffered.start(0) + " to " + videoSourceBuffer.buffered.end(0) );

	if ( firstplay && videoSourceBuffer.buffered.length > 0 ) {
            video.currentTime = videoSourceBuffer.buffered.start(0);
	    firstplay = false;
	}
    }

    videoSourceBuffer.addEventListener('abort', function(e) {
        console.log('video source buffer abort:', e);
    });
    videoSourceBuffer.addEventListener('error', function(e) {
        console.log('video source buffer error:', e);
	ws.onmessage = function() {}
    });
    videoSourceBuffer.addEventListener('updateend', videobump);

    video.oncanplay = function() {
	var play_promise = video.play();

	if ( play_promise == undefined ) {
	    console.log( "undefined promise" );
	} else {
	    play_promise.then(function() {
		playing = true;
		video.oncanplay = function() {};
	    }).catch(function(error) {
		console.log( "play error: " + error );
	    });
	} 
    }
}
