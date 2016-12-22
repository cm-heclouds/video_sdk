$(function(){
    $(".vjs-control-bar,.vjs-error-display,.vjs-caption-settings,.vjs-big-play-button").hide();
    var player = videojs("video");
    $(".url-btn").click(function(){
		alert("ok");
        player.src($(".video-url input").val());
        player.play();
    })
})