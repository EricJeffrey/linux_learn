# 线程池实现

实现一个简单的线程池

## 原理
- 线程同步
  - 互斥量：通过加锁的方式保护对共享数据的访问
  - 条件变量：阻塞线程，并在某个收到通知（条件满足）后执行任务

## 线程池设计

### 程序逻辑

功能：用户输入休眠时间t，线程唤醒后执行如下任务，循环5次，每次循环休眠t秒并输出一条消息
- 等待用户输入时间t
- 生成任务job并提交
- 将job添加到队列
- 唤醒一个线程
  - 子线程从队列取得job
  - 执行job
  - 继续等待
- 继续

### 数据结构
- 条件变量：所有线程共用
- 线程任务互斥量：与条件变量共用，同步任务
- 任务队列：包含待执行任务的队列
- 任务：由待执行函数和参数组成的逻辑序列



