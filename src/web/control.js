var ws = new WebSocket("wss://stagecast.org:8500");
ws.binaryType = 'arraybuffer';

var decoder = new TextDecoder("utf-8");

var controls_created = false;
ws.onmessage = function( e ) {  
    doc = JSON.parse( decoder.decode( e.data ) );
    if (!controls_created) {
	for ( x in doc["client"] ) {
	    make_client( x, doc["client"][x] );
	}
	for ( x in doc["board"] ) {
	    make_board( x, doc["board"][x] );
	}
	controls_created = true;
    }

    for ( client_name in doc["client"] ) {
	var client_area = document.getElementById('client-' + client_name);
	if ( doc["client"][client_name]["client"]["max_lag"] ) {
	    client_area.style.display = "block";
	    var id = client_name + ":" + "earpiece";
	    show_info( id, doc["client"][client_name]["client"] )
	    update_sliders( id, doc["client"][client_name]["client"] );
	
	    for ( feed_name in doc["client"][client_name]["feed"] ) {
		var id = client_name + ":" + feed_name;
		show_info( id, doc["client"][client_name]["feed"][ feed_name ] );
		update_sliders( id, doc["client"][client_name]["feed"][feed_name] );
	    }
	} else {
	    client_area.style.display = "none";
	}
    }

    for ( board_name in doc["board"] ) {
	for ( channel_name in doc["board"][board_name]["channels"] ) {
	    document.getElementById( `amplitude:${board_name}:${channel_name}` ).value = -to_dbfs( doc["board"][board_name]["channels"][channel_name][ "amplitude" ] );

	    var gain_elem = document.getElementById( `gain:${board_name}:${channel_name}` );
	    var db = to_dbfs( doc["board"][board_name]["channels"][channel_name][ "gain" ] );
	    if ( ! gain_elem.ignoring ) {
		document.getElementById( `gain:${board_name}:${channel_name}` ).value = -db;
	    }
	    if ( db > -99 ) {
		document.getElementById( `text:gain:${board_name}:${channel_name}` ).innerHTML = db.toFixed(0) + " dB gain";
	    } else {
		document.getElementById( `text:gain:${board_name}:${channel_name}` ).innerHTML = "[mute]";
	    }
	}
    }
}

to_dbfs = function( val ) {
    if ( val <= 0.00001 ) {
    return -100;
  }

  return 20 * Math.log10( val );
}

show_info = function( id, structure ) {
    id = "info:" + id;
    if ( typeof( structure.compressions ) != "undefined" ) {
	document.getElementById( id ).innerHTML = "resets: " + structure.resets + ", compressions: " + structure.compressions + ", expansions: " + structure.expansions + ", quality=" + structure.quality.toFixed(4);
    } else {
	document.getElementById( id ).innerHTML = "resets: " + structure.resets + ", quality=" + structure.quality.toFixed(4);
    }
}

update_sliders = function( id, structure ) {
    //    console.log( structure );
    update_slider( "lag:" + id, structure[ "actual_lag" ] ); 
    update_slider( "min_lag:" + id, structure[ "min_lag" ] ); 
    update_slider( "max_lag:" + id, structure[ "max_lag" ] ); 
    update_slider( "target_lag:" + id, structure[ "target_lag" ] ); 
}

update_slider = function( id, val ) {
    document.getElementById( id ).value = val;
    document.getElementById( "text:" + id ).innerHTML = " " + (val / 48.0).toFixed(0) + " ms";  
}

make_lagdisplay = function( client_name, feed_name )
{
    var ret = "";
    ret += `<span style="float: left; width: 150px;">${feed_name}:lag</span><input type="range" min="0" max="7200" id=${"lag:"+client_name+":"+feed_name} style="appearance:none; background: #D0D0D0; width: 500px;"><span id="${"text:lag:"+client_name+":"+feed_name}"></span><br>`;
    ret += `<span style="float: left; width: 150px;">${feed_name}:target_lag</span><input type="range" min="0" max="7200" id=${"target_lag:"+client_name+":"+feed_name} style="appearance:none; background: #80D080; width: 500px;"><span id="${"text:target_lag:"+client_name+":"+feed_name}"></span><br>`;
    ret += `<span style="float: left; width: 150px;">${feed_name}:min_lag</span><input type="range" min="0" max="7200" id=${"min_lag:"+client_name+":"+feed_name} style="appearance:none; background: #F0D0D0; width: 500px;"><span id="${"text:min_lag:"+client_name+":"+feed_name}"></span><br>`;
    ret += `<span style="float: left; width: 150px;">${feed_name}:max_lag</span><input type="range" min="0" max="7200" id=${"max_lag:"+client_name+":"+feed_name} style="appearance:none; background: #F0D0D0; width: 500px;"><span id="${"text:max_lag:"+client_name+":"+feed_name}"></span><br>`;

    ret += `<span style="float: left; width: 150px;">${feed_name}:info</span><span id=${"info:"+client_name+":"+feed_name}>info</span><br>`;

    ret += `<p>`;
    
    return ret;
}

var make_client = function(name, val) {
    document.getElementById('clients').innerHTML += '<div id="client-' + name + '"></div>';

    var client_area = document.getElementById('client-' + name);
    client_area.innerHTML += `<hr><b>${name}</b><br>`;
    client_area.innerHTML += make_lagdisplay( name, "earpiece" );
    for ( feed_name in val["feed"] ) {
	client_area.innerHTML += make_lagdisplay( name, feed_name );
    }
}

var make_board = function(name, val) {
    document.getElementById('boards').innerHTML += `<div id="board-${name}"></div>`;

    var board_area = document.getElementById("board-" + name);
    board_area.innerHTML += `<hr><b>${name}</b> mixing board<br>`;
    for ( channel_name in val["channels"] ) {
	board_area.innerHTML += make_channel( name, channel_name )
    }
}

var send_control = function(id, value )
{
    ws.send( id + ":" + -value );
}

var make_channel = function(name, channel_name) {
    var ret = "";
    ret += `<div style="display: inline-block; width: 100px;">
<br>${channel_name}<br>
<div style="display: inline-block; width: 10px; height: 300px;">
<input id="amplitude:${name}:${channel_name}" type="range" min="-13" max="100" style="width: 300px; height: 10px; appearance: none; background-color: #FFFFFF; transform: rotate(90deg); transform-origin: 20px 20px">
</div>
<div style="display: inline-block; width: 10px; height: 300px;">
<input id="gain:${name}:${channel_name}" type="range" min="-13" max="100" style="width: 300px; height: 10px; appearance: none; background-color: #DDDDDD; transform: rotate(90deg); transform-origin: 20px 20px"
onmousedown="this.ignoring = true;" onmouseup="this.ignoring = false;" oninput="send_control(this.id, value);">
</div>
<p>

<span style="width:100px;" id="text:gain:${name}:${channel_name}"></span>

</div>`;
    return ret;
}

