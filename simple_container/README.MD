# 超简易容器

参考[busybox简单容器](https://segmentfault.com/a/1190000006913509)，在新的Namespace中运行busybox。

## 容器创建步骤
1. 创建根目录、bin目录等
   1. 创建`/`, `/bin`目录
2. 配置根文件系统目录/root
   1. `/root`, `/old_root`
3. 挂载文件系统procfs等
   1. `/proc`, `mount`, `pivot_root`, `chroot`
4. 配置网络连接bridge + NAT
5. 配置用户/组ID映射
6. 从配置文件创建容器

## TODO - 待办
1. 配置网络连接
2. 添加`LOG/DEBUG`选项机制
3. 添加包装函数

## 已解决问题
1. ~~`clone` 之后ssh连接失效/用户登出问题~~ : 需要在终止时`kill`掉子进程，即`SIGCHLD`或`kill(0, SIGKILL)`
2. ~~UID/GID映射写入失败~~: 需要由父进程向打开子进程的`/proc/PID/uid_map gid_map`文件，同时写入`gid_map`前需要向`/proc/PID/setgroups`写入字符串`"deny"`!