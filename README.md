# RTSP-WS-Proxy

一个RTSP转Websocket的服务端和对应的web端播放器
---
*服务端:*
	WSServer目录下
	环境基于centos7，如需在其它环境下使用，请先替换使用到的第三方库
	服务端编译：
		
		cd WSServer
		make
---		
	服务端依赖的第三方库:
		boost		版本不宜过高，否则会与websocketpp不兼容，我使用的是yum安装的，版本号1.53
		websocketpp	include中已有头文件，不需要单独编译，依赖boost
		jsoncpp		我使用的版本是1.8.4
		ffmpeg		使用的是4.2.2，早一些的版本也可使用
		如更换使用环境，请检查编译后的第三方库，更新Makefile
	服务端运行方法：
	
		WSProxy 9001
---		
		9001是服务端绑定的websocket端口
---
*web端:*
	ClientPlayer目录下
	wsplayer.js是播放器，testplayer.html是示例，需要浏览器支持html5的mse
	播放器使用方法示例：
	
		var video1 = document.getElementById('video1');
        player = new wsplayer("ws://192.168.5.133:9001","rtsp://192.168.5.1/stream1",video1);    
        player.openws();

---		
		在new wsplayer("ws://192.168.5.133:9001","rtsp://192.168.5.1/stream1",video1);中使用的三个参数，第一个是服务端的ws地址，第二个是要打开的rtsp地址，第三个是播放器绑定的htmldocument对象
---
*原理:*
	web端将要打开的rtsp地址发送给服务端，服务端打开rtsp流成功后将流的mime发送给web并开始推送fmp4数据，web利用mime初始化mse，成功后将websocket收到的二进制数据交给mse播放，程序目前只支持h264视频和aac音频，如要接入其它格式，请修改服务端，在服务端进行转码工作