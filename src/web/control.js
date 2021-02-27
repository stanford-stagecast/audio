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
	controls_created = true;
    }

    for ( client_name in doc["client"] ) {
	var id = client_name + ":" + "earpiece";
	show_info( id, doc["client"][client_name]["client"] )
	update_sliders( id, doc["client"][client_name]["client"] );
	
	for ( feed_name in doc["client"][client_name]["feed"] ) {
	    var id = client_name + ":" + feed_name;
	    show_info( id, doc["client"][client_name]["feed"][ feed_name ] );
	    update_sliders( id, doc["client"][client_name]["feed"][feed_name] );
	}
    }
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

getclient = function(name) {
    return document.getElementById('client-' + name);
}

make_lagdisplay = function( client_name, feed_name )
{
    ret = "";
    ret += `<span style="float: left; width: 150px;">${feed_name}:lag</span><input type="range" min="0" max="7200" id=${"lag:"+client_name+":"+feed_name} style="appearance:none; background: #D0D0D0; width: 500px;"><span id="${"text:lag:"+client_name+":"+feed_name}"></span><br>`;
    ret += `<span style="float: left; width: 150px;">${feed_name}:target_lag</span><input type="range" min="0" max="7200" id=${"target_lag:"+client_name+":"+feed_name} style="appearance:none; background: #80D080; width: 500px;"><span id="${"text:target_lag:"+client_name+":"+feed_name}"></span><br>`;
    ret += `<span style="float: left; width: 150px;">${feed_name}:min_lag</span><input type="range" min="0" max="7200" id=${"min_lag:"+client_name+":"+feed_name} style="appearance:none; background: #F0D0D0; width: 500px;"><span id="${"text:min_lag:"+client_name+":"+feed_name}"></span><br>`;
    ret += `<span style="float: left; width: 150px;">${feed_name}:max_lag</span><input type="range" min="0" max="7200" id=${"max_lag:"+client_name+":"+feed_name} style="appearance:none; background: #F0D0D0; width: 500px;"><span id="${"text:max_lag:"+client_name+":"+feed_name}"></span><br>`;

    ret += `<span style="float: left; width: 150px;">${feed_name}:info</span><span id=${"info:"+client_name+":"+feed_name}>info</span><br>`;

    ret += `<p>`;
    
    return ret;
}

make_client = function(name, val) {
    document.getElementById('clients').innerHTML += '<div id="client-' + name + '"></div>';

    client_area = getclient(name);
    client_area.innerHTML += `<hr><b>${name}</b><br>`;
    client_area.innerHTML += make_lagdisplay( name, "earpiece" );
    for ( feed_name in val["feed"] ) {
	client_area.innerHTML += make_lagdisplay( name, feed_name );
    }
}
