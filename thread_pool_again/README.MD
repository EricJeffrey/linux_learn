
C++线程池实现

自己写的线程池实现，不过有很多问题都没考虑到，感兴趣可以参考：[log4cplus/ThreadPool](https://github.com/log4cplus/ThreadPool/blob/master/ThreadPool.h)。思想不复杂，但是需要同步的东西比较多。

1. 任务队列
   1. 同步 `mutex` 
   2. 使用条件变量 `condition_variable` 实现生产者消费者模型
   3. 当前任务数 - 原子加/减操作`atomic_fetch_sub_explicit`
2. 线程池状态
   1. 是否已停止(析构，手动停止) - 阻塞操作后需要检查
   2. wait
      1. 等待所有任务执行完
3. 添加任务 - 生产者
   1. 生产者是可能阻塞的（队列最大长度限制）
4. 设置池大小
   1. 设置任务队列大小
   2. 设置线程池限制 - 开启新线程
5. 线程worker - 消费者
   1. 取任务，更新任务队列，通知生产者
   2. 停止或池大小已更新 --> detach并pop（线程执行完会自动结束）