# Windows 归档恢复说明

远端唯一源码目录为 /home/yttan/project/zkGPT/zkGPT-main/zkGPT-work。不要在旧 zkGPT 或新 sibling 树恢复源码；不要覆盖历史 proof/statement/scope。归档包含修改前完整源码、最终完整源码、源补丁、before/after 二进制和库、配置/参数身份、120 份参数字节、原始测试/负向/测量日志、最小 statement/proof 及旧真实 oneblock statement/proof。

用 EVIDENCE_MANIFEST.json 逐文件 SHA 校验；before-source.tar.gz 的逐文件身份见 BEFORE_SOURCE_SHA.json，after-source.tar.gz 见 AFTER_SOURCE_SHA.json。解压做 restore-read 不代表批准运行；新二进制没有继承旧 scope，未生成 permit。所有路径均为归档相对路径，不含 SSH 配置、私钥或凭据。远端证据没有删除。本地 WINDOWS_ARCHIVE_VERIFICATION.json 单独记录 SHA/成员恢复读取结果，不能把它当真实 oneblock 验收。
