#!/bin/bash

# 定义 TLE 数据的保存文件
TLE_FILE="./TLE.txt"

# 日志文件
LOG_FILE="./Tle_log.txt"

# 获取当前时间
TIMESTAMP=$(date +"%Y-%m-%d %H:%M:%S")

# 记录日志（追加模式）
echo "[$TIMESTAMP] Retrieving TLE Data..." | tee -a "$LOG_FILE"

# 使用 curl 获取 TLE 数据，并保存到 TLE_FILE
curl -s "https://celestrak.org/NORAD/elements/gp.php?GROUP=gnss&FORMAT=tle" -o "$TLE_FILE"

# 检查文件是否成功获取
if [ $? -eq 0 ]; then
    echo "[$TIMESTAMP] TLE data retrieved successfully; saved to $TLE_FILE" | tee -a "$LOG_FILE"
else
    echo "[$TIMESTAMP] TLE data retrieval failed. Check the network connection or URL." | tee -a "$LOG_FILE"
fi

# 退出脚本
exit 0
