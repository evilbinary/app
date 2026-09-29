# 视频目录

把视频/音频文件放在这里，console-os 的「播放器」磁贴会扫描本目录：

    roms/video/xxx.mp4

支持的扩展名见 emulators/player/player.json 的 exts 字段。
网络流（RTSP/HTTP）不走本目录，直接在设备 shell 里运行：

    player rtsp://user:pass@host:port/stream
