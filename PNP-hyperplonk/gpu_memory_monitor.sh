#!/bin/bash

# 输出文件名
output_file="gpu_memory_usage.log"

# 如果文件存在则删除它
if [ -f $output_file ]; then
    rm $output_file
fi

# 无限循环，每隔0.1秒记录一次GPU显存使用情况
while true; do
    # 获取当前时间
    current_time=$(date +"%Y-%m-%d %H:%M:%S.%3N")

    # 获取显存使用情况
    gpu_memory=$(nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits)

    # 将当前时间和显存使用情况写入文件
    echo "$current_time - GPU Memory Usage: $gpu_memory MB" >> $output_file

    # 等待0.1秒
    sleep 0.1
done
