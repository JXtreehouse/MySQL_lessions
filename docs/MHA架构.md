## MHA架构（了解）

### 概述

一套优秀的作为MySQL高可用环境下**故障切换**和**主从提升**的高可用软件。
故障切换过程中，MHA能自动完成数据库的故障切换操作，并且能最大程度保证数据的一致性。

### 组成

MHA Manager（管理节点）和MHA Node（数据节点）。

- MHA Manager可以单独部署在一台独立的机器上管理多个主从集群，也可以部署在一台slave上。
- MHA Node运行在每台MySQL服务器上，MHA Manager会定时探测集群中的master节点，当master发生故障时，它自动将最新数据的slave提升为master，然后将其他slave重新指向新的master。整个过程对应用是透明的。
- 目前MHA主要支持一主多从的架构，要搭建MHA至少需要3台数据库服务器。一台master，一台备用master，一台slave。

### 工作原理

- 从宕机崩溃的master保存二进制日志事件（binlog events）
- 识别最新更新的slave
- 应用差异的中继日志到其他slave
- 应用从master保存的二进制日志事件
- 提升一个slave为新master
- 使其他的slave连接新的master复制