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

function send_control(id, value)
{
    ws.send( "control " + id + " " + value );
}

function reset_controls()
{
    ws.send( "control zoom:zoom 1.00" );
    ws.send( "control crop:left 0" );
    ws.send( "control crop:right 0" );
    ws.send( "control crop:top 0" );
    ws.send( "control crop:bottom 0" );
}

function set_live(name) {
    ws.send("live " + name );
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

    var add_control = function(name) {
	var div = document.getElementById('buttons');
	div.innerHTML += "<button onclick='set_live(this.id)' type='button' id='" + name + "'>" + name + "</button>";
    }

    document.getElementById('zooms').innerHTML = 'Camera control for: <b><span id="text:live"></span></b><br><button onclick="reset_controls();" type="button">Reset controls</button><br><span style="float: left; width: 200px;" id="text:zoom:x">x</span><input type="range" min="0" max="3840" step="1" id="zoom:x" style="width: 1280px;" onmousedown="this.ignoring = true;" onmouseup="this.ignoring = false;" oninput="send_control(this.id, value);"><br><span style="float: left; width: 200px;" id="text:zoom:y">y</span><input type="range" min="0" max="3840" step="1" id="zoom:y" style="width: 1280px;" onmousedown="this.ignoring = true;" onmouseup="this.ignoring = false;" oninput="send_control(this.id, value);"><br><span style="float: left; width: 200px;" id="text:zoom:zoom">zoom</span><input type="range" min="1" max="3" step="0.001" id="zoom:zoom" style="width: 1280px;" onmousedown="this.ignoring = true;" onmouseup="this.ignoring = false;" oninput="send_control(this.id, value);"><br><span style="float: left; width: 200px;" id="text:crop:left">crop</span><input type="range" min="0" max="3840" step="1" id="crop:left" style="width: 1280px;" onmousedown="this.ignoring = true;" onmouseup="this.ignoring = false;" oninput="send_control(this.id, value);"><br><span style="float: left; width: 200px;" id="text:crop:right">crop</span><input type="range" min="0" max="3840" step="1" id="crop:right" style="width: 1280px;" onmousedown="this.ignoring = true;" onmouseup="this.ignoring = false;" oninput="send_control(this.id, value);"><br><span style="float: left; width: 200px;" id="text:crop:top">crop</span><input type="range" min="0" max="2160" step="1" id="crop:top" style="width: 1280px;" onmousedown="this.ignoring = true;" onmouseup="this.ignoring = false;" oninput="send_control(this.id, value);"><br><span style="float: left; width: 200px;" id="text:crop:bottom">crop</span><input type="range" min="0" max="2160" step="1" id="crop:bottom" style="width: 1280px;" onmousedown="this.ignoring = true;" onmouseup="this.ignoring = false;" oninput="send_control(this.id, value);"><br>';
    
    ws = new WebSocket("wss://stagecast.org:8400");
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
	    add_control( decoder.decode(rest) );
	    return;
	}
	if ( type_byte == 3 ) {
	    var doc = JSON.parse( decoder.decode( rest ) );
	    var x_elem = document.getElementById("zoom:x");
	    var y_elem = document.getElementById("zoom:y");
	    var zoom_elem = document.getElementById("zoom:zoom");

	    if ( !x_elem.ignoring ) {
		x_elem.value = doc["zoom"]["x"];
	    }

	    if ( !y_elem.ignoring ) {
		y_elem.value = doc["zoom"]["y"];
	    }
	    
	    if ( !zoom_elem.ignoring ) {
		document.getElementById("zoom:zoom").value = doc["zoom"]["zoom"];
	    }
	    
	    document.getElementById( "text:live" ).innerHTML = doc["live"];
	    document.getElementById( "text:zoom:x" ).innerHTML = "x: " + doc["zoom"]["x"];
	    document.getElementById( "text:zoom:y" ).innerHTML = "y: " + doc["zoom"]["y"];
	    document.getElementById( "text:zoom:zoom" ).innerHTML = "zoom: " + doc["zoom"]["zoom"].toFixed(2);

	    var left_elem = document.getElementById("crop:left");
	    var right_elem = document.getElementById("crop:right");
	    var top_elem = document.getElementById("crop:top");
	    var bottom_elem = document.getElementById("crop:bottom");

	    if ( !left_elem.ignoring ) {
		left_elem.value = doc["crop"]["left"];
	    }
	    if ( !right_elem.ignoring ) {
		right_elem.value = doc["crop"]["right"];
	    }
	    if ( !top_elem.ignoring ) {
		top_elem.value = doc["crop"]["top"];
	    }
	    if ( !bottom_elem.ignoring ) {
		bottom_elem.value = doc["crop"]["bottom"];
	    }

	    document.getElementById( "text:crop:left" ).innerHTML = "crop left: " + doc["crop"]["left"];
	    document.getElementById( "text:crop:right" ).innerHTML = "crop right: " + doc["crop"]["right"];
	    document.getElementById( "text:crop:top" ).innerHTML = "crop top: " + doc["crop"]["top"];
	    document.getElementById( "text:crop:bottom" ).innerHTML = "crop bottom: " + doc["crop"]["bottom"];
	    
	    return;
	}

	videoqueue.push(rest);

	if ( firstplay && videoSourceBuffer.buffered.length > 0 ) {
            video.currentTime = videoSourceBuffer.buffered.start(0);
	    firstplay = false;
	}
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

