class wsplayer {
	constructor(wsserver,rtspurl,mediaplayer){
		this.wsserverurl = wsserver;
		this.rtspurl = rtspurl;
		this.queue = [];
		this.mediaplayer = mediaplayer;
		this.needsend = false;
		this.openws = this.openws.bind(this);
		this.stop = this.stop.bind(this);
		this.loadvideo = this.loadvideo.bind(this);
		this.onUpdateEnd = this.onUpdateEnd.bind(this);
		this.onCanPlay = this.onCanPlay.bind(this);
		this.onMediaSourceOpen = this.onMediaSourceOpen.bind(this);
		this.onUpdateEnd = this.onUpdateEnd.bind(this);
	}
	openws() { 
        this.websocket = new WebSocket(this.wsserverurl); 
        this.websocket.onopen =  this.onOpen.bind(this); 
        this.websocket.onclose = this.onClose.bind(this); 
        this.websocket.onmessage = this.onMessage.bind(this); 
        this.websocket.onerror = this.onError.bind(this); 
    }  
	
	onOpen(evt) { 
		this.websocket.binaryType="arraybuffer";
        console.log("CONNECTED"); 
		var sendurl = "{\"url\": \"" + this.rtspurl + "\"}"
		this.doSend(sendurl); 
    }
	
	onClose(evt) { 
        console.log("DISCONNECTED"); 
		this.stop();
    } 
	
	onMessage(evt) { 
		if(typeof(evt.data)=="string")            //服务器传过来的可能是字符串，判断是不是
        {
            var str = evt.data;
			console.log(str);
			var strs = new Array(); //定义一数组
			strs = str.split(":"); //字符分割
			if (strs[0] == "open")
			{
				var mimestr = strs[1];
				this.playurl(mimestr);
			}
        }
        else
        {
			//var binaryType = websocket.binaryType;
			var result = new Uint8Array(evt.data);
			this.queue.push(result);
			//console.log("recive queue:",this.queue.length);
			if (this.needsend == true)
			{
				this.loadvideo();
			}
        }
    }
	onError(evt) { 
        console.log('ERROR:'+ evt.data); 
		this.stop();
    }  
  
    doSend(message) { 
        console.log("SENT: " + message);  
        this.websocket.send(message); 
    }
	
	playurl(mimestr){
		this.supportstr = 'video/mp4; codecs=\"' + mimestr + '\"'; 
		console.log("call playurl:", this.supportstr);
		if (MediaSource.isTypeSupported(this.supportstr)) {
			this.mediaSource = new MediaSource;
			this.mediaSource.addEventListener('sourceopen',this.onMediaSourceOpen);
			this.mediaplayer.src = URL.createObjectURL(this.mediaSource);
			this.mediaplayer.addEventListener("canplay", this.onCanPlay);
		}else{
			console.log("Unsupported MIME type or codec: ", + this.supportstr);
		}
	}
	
	onCanPlay(){
		this.mediaplayer.play();
	}
	
	onMediaSourceOpen(e){
		this.mediaSource.removeEventListener('sourceopen', this.onMediaSourceOpen);
		console.log("sourceopen" + this.supportstr);
		this.sourceBuffer = this.mediaSource.addSourceBuffer(this.supportstr);
		this.loadvideo();
		this.sourceBuffer.addEventListener('updateend', this.onUpdateEnd);
	}
	
	onUpdateEnd(e){
		this.loadvideo();
	}
	
	stop(){
		this.mediaplayer.pause();
		//this.sourceBuffer.abort();
		//this.mediaSource.endOfStream();
	}
	loadvideo(){
		if (this.queue && this.queue.length){
			if (!this.sourceBuffer.updating){
				this.needsend = false;
				var newBuffer = this.queue.shift();
				//console.log("send queue:",this.queue.length);
				this.sourceBuffer.appendBuffer(newBuffer);
				newBuffer = null;
			}else{
				this.needsend = true;
			}		
		}else{
			this.needsend = true;
		}
	}
}