# 超简易容器

参考[busybox简单容器](https://segmentfault.com/a/1190000006913509)，在新的Namespace中运行busybox。

## 容器创建步骤
1. 创建根目录、bin目录等
2. 配置文件系统目录/root
3. 挂载文件系统procfs等
4. 