// 初始化地图
var map = new BMap.Map("map");
var point = new BMap.Point(116.404, 39.915);  // 初始中心点
map.centerAndZoom(point, 15);

// 创建一个空的折线对象
var polyline = new BMap.Polyline([], {
    strokeColor: "blue",
    strokeWeight: 2,
    strokeOpacity: 0.5
});
map.addOverlay(polyline);

// 初始化 QWebChannel
new QWebChannel(qt.webChannelTransport, function(channel) {
    var locationProvider = channel.objects.locationProvider;

    // 监听位置变化
    locationProvider.locationChanged.connect(function(location) {
		console.log("recev msg from qt");
        var lng = location.x;
        var lat = location.y;
        var point = new BMap.Point(lng, lat);

        // 更新轨迹
        var path = polyline.getPath();
        path.push(point);
        polyline.setPath(path);

        // 将地图中心移动到最新点
        map.panTo(point);
    });
});