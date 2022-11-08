## replication

### 概述

将主数据库的DDL和DML操作通过二进制日志传到从库上，然后在从库上对这些日志重新执行，从而达到从库和主库数据的同步。
MySQL支持一主库向多从库复制，从库也可以作为其他服务器的主库，实现链状复制。
通过show processlist在主库查看Binlog Dump线程，MySQL复制是主库主动推送日志到从库去的，属于推日志的方式来做同步。
通过show processlist在从库查看IO和SQL线程，IO等待主库上的Binlog Dump线程发送事件并更新到中继日志，SQL线程读取中继日志并应用到数据库。
所以复制是异步的，有延时。

### 优点

- 主库和从库可以切换，主库出问题时，从库继续服务。
- 在从库上执行查询，降低主库压力。
- 从库上备份，避免影响主库的服务。
  **MySQL是异步复制，主从库有差异。从库上查询的数据要有选择性（实时性不高的）。**

### 原理与相关文件

#### 概述

1. 主库在事务提交时会把数据变更作为事件Events记录在二进制日志文件`Binlog`中；MySQL主库上的sync_binlog参数控制Binlog日志刷新到磁盘。
2. 主库推送二进制日志文件Binlog中的事件到从库中继日志`Rely Log`中，之后从库根据中继日志重做数据变更操作，通过逻辑复制达到主从数据一致。

#### 3个线程

`Binlog Dump`（主库）、`I/O`（从库）、`SQL`（从库）

1. 从库连接主库
2. `IO`线程向主库要求数据
3. `Binlog Dump`线程读取数据库时间并把数据发给IO线程
4. `SQL`线程应用中继日志中的事件

#### Binlog

create、drop、insert、update、delete。

#### Relay Log

格式内容于Binlog一致，但是SQL线程会自动删除。

#### master.info

IO读取主库二进制日志Bin两个的进度。

#### relay-log.info

SQL线程应用中继日志的进度。

### 复制方式

Binlog的三种格式对应三种复制方式。

| Binlog格式 | 复制方式                                                 |                   |
| ---------- | -------------------------------------------------------- | ----------------- |
| Statement  | binlog_format=Statement(Statement_Base Replication)(SBR) | 基于SQL语句的复制 |
| Row        | binlog_format=Row(Row_Base Replication)(RBR)             | 基于行的复制      |
| Mixed      | binlog_format=Mixed                                      | 混合复制          |

还有第四种复制方式：使用全局事务ID的复制（GTIDs）：主要是解决主从自动同步一致问题。
Row 更能保证数据的一致性，但Binlog日志量大。

Bilog日志格式通过全局变量binlog-format设置，也可以在当前Session动态设置。

```
set global binlog_format='Row'
```

### 架构

#### 一主多从

读写分离。将实时性不高的数据的查询工作给从库。主库宕机可以切换从库进行服务。

![](D:\资料\pictures\一主多从复制框架图.jpg)

#### 多级复制架构

一主多从可以解决大部分读请求压力过大的场景需求，考虑到主库”推送“Binlog日志到从库，主库的IO压力和网络压力会随着从库数量的增加而增长（从库Binlog Dump线程独立）。多级复制架构解决IO和网络压力问题。

![](D:\资料\pictures\多级复制架构图.jpg)

一级主库只给二级主库推送Binlog，二级主库并不承担读写请求，只负责推送日志。缺点是：因为是异步复制，所以延时大。

#### 双主复制/Dual Master框架

![](D:\资料\pictures\双主复制框架.jpg)

### 搭建过程

#### 异步复制

#### 半同步复制

解决主从不一致问题。异步复制时，主库写入一个事务并提交成功，在从库没有得到主库推送的Binlog日志时，主库宕机了，从库就会丢失这个事务，导致主从不一致。
解决方法：事务提交成功时，至少有两份日志记录，一份在binlog，另一份在至少一个从库的relay log上。

- 半同步的体现
  虽然主库和从库的binlog日志是同步的，但是主库不会等待从库应用完binlog就会返回提交结果，这个操作是异步的。是半同步不是完全同步。
- 步骤

1. 判断MySQL服务器是否支持动态增加插件

```
select @@have_dynamic_loading;
```

1. 检查是否有插件。

```
MYSQL_HOME/lib/plugin/semisync_master.so
MYSQL_HOME/lib/plugin/semisync_slave.so
```

1. 主库安装插件semisync_master.so

```
install plugin rpl_semi_sync_master SONAME 'semisync_master.so'；
```

1. 从库安装插件semisync_slave.so

```
install plugin rpl_semi_sync_master SONAME 'semisync_slave.so'；
```

1. 查看安装情况，记录在表里，下次会自动加载插件。

```
select * from mysql.plugin;
```

1. 在主库和从库上配置参数打开半同步semi-sync。

- 主库

```
set global rpl_semi_sync_master_enabled=1;
set global rpl_semi_sync_master_timeout=30000;
```

- 从库

```
set global rpl_semi_sync_master_enabled=1;
```

- 然后重启从库的IO线程。（如果是全新配置的半自动同步则不需要）

```
STOP SLAVE IO_THREAD; START SLAVE IO_THREAD;
```

- 验证。

```
show status like '%semi_sync%';
```

需要着重关注以下3个值。

- Rpl_semi_sync_master_status:值为on，表示半同步复制打开。
- Rpl_semi_sync_master_yes_tx：事务通过半同步复制到从库对的量。
- Rpl_semi_sync_master_no_tx：不是半同步模式下从库及时响应的。（网络故障或者是从库宕机等）

### 复制启动选项

#### 概述

安装配置时有几个启动时的复制参数，包括MASTER_HOST , MASTER_PORT , MASTER_USER , MASTER_PASSWORD , MASTER_LOG_FILE , MASTER_LOG_POS，这几个参数需要在从库上配置。其他启动选项还有`log-slace-updates`、`master-connect-retry`、`read-only`等。

#### log-slace-updates

是否写从库更新的二进制文件，若该从库是其他从库的主库，则需要打开。需要和--log-bin参数一起用。

#### master-connect-retry

主从连接丢失时重连的时间间隔。

#### read-only

只读模式打开数据库。

```
mysqladmin -uroot -p shutdown
./bin/mysqld_safe --read-only&
starting mysqld daemon with databases from /home/mysql/sysdb/data
```

#### 指定复制的数据库或表

-replicate-do-table
在主数据库中建两个表t1和t2，t1有数据。这时主从两个表的记录是相同的。关闭从数据库，重新启动输入参数指定复制其中一个表。

```
mysqladmin -uroot -p shutdown
./bin/mysqld_safe --replicate-do- table=t1. t2&
starting mysqld daemon with databases from /home/mysql/sysdb/data
```

此时，从主库上更新t1和t2表，只有t1在从库上更新了。
类似的参数还有`--replicate-do-db`、`--replicate-wild-do-table`、`--replicate-ignore-table`、`--replicate-ignore-db`

#### slave-skip-errors

跳过复制过程中的一些错误，例如主键冲突。

```
--slave-skip-errors=[err_code1,err_code2...|all]
```

#### 使用场景

从数据库仅仅是为了分担主数据库的查询压力，且对数据的完整性要求不是很严格，则这个选项可以减轻管理员对从数据库的工作量。
若从数据库作为主数据库的备份，则不应该启用。

### 管理维护

环境配置完成后，还要进行一些日常监控和管理维护工作。

#### 查看从库状态

```
  show slave status \G
```

显示的信息中，较为重要的是`Slave_IO_Running`（读取binlog日志）和`Slave_SQL_Running`（读取和执行中继日志中的binlog日志）是不是为yes。

#### 主从库同步维护

- 场景：主库更新频繁，从库由于各种原因更新较慢，到最后主从数据差距越来越大，最终对对某些应用产生影响。我们需要定期进行主从数据同步，使差距变小。
- 方法：在负载较低时，阻塞主数据库的更新，强制主从数据库更新同步。
- 步骤

1. 阻塞主库的更新操作

   ```
   FLUSH TABLES WITH READ LOCK;
   SHOW MASTER STATUS;
   ```

   记录show语句输出的日志名和偏移量，这些是从库复制的目标地址。

2. 在从库中执行下面的语句,其中MASTER_POS_WAIT()函数的参数是前面步骤中得到的复制坐标值。

   ```
   select MASTER_POS_WAIT('日志名','偏移量');
   ```

   这个select语句会阻塞直到从库到达指定的日志文件和偏移量，返回0。如果返回-1，则超时退出。如果返回值为0，则从库与主库同步。

3. 在主库上，执行下面语句，允许主库更新工作。

```
  UNLOCK TABLES;
```

#### 从库复制出错处理

- 步骤

1. 首先要确定表结构是不是相同。
2. 确定手动更新是否安全，然后忽视来自主库的更新失败语句。使用SET GLOBAL SQL_SLAVE_SKIP_COUNTER=n,n的取值可以使1、2。如果来自主库的更新中使用了AUTO_INCREAMEENT或LAST_INSERT_ID()的语句，n值应为2，否则为1。原因是AUTO_INCREAMEENT或LAST_INSERT_ID()语句要调用两个二进制日志中的事件。

- 实例

1. 从库停止复制进程，并设置跳过两个语句。假设有表t1，只有一列AUTO_INCREAMEENT的ID字段。

```
  stop slave;
  SET GLOBAL SQL_slave_SKIP_COUNTER=2;
```

1. 在主库插入3条数据。

```
  insert into t1 values(1),(2),(3);
```

此时主库中此表中多了3条数据。

1. 在从库中启动渎职进程，查询t1表中数据更新的情况,已跳过两条更新语句，只插入一条。

```
  start slave;
  select * from t1;
```

#### log event entry wxceeded max_allowed_packet的处理

如果应用中使用大的BLOG列或者长字符串，那么，在从库上恢复时，可能会出现这个错误，这时因为含有大文本的记录无法通过网络进行传输。解决办法是增加max_allowed_packet参数的大小，默认是1MB，可以按照需要更改。

```
  show variables like 'max_allowed_packet';
  SET @@global.max_allowed_packet=??????;
```

更改配置文件，以便后续的工作。

#### 查看复制进度

- 实现
  判断是否需要手工进行主从复制。show processlist中的`Slave_SQL_Running`线程Time值得到，他记录了从库当前执行的SQL时间戳与系统时间的差距（s）。
- 实例：

1. 首先在主库中插入一个包含当前时间戳的记录。

```
alter table t1 add column createtime datetime;
```

1. 为了方便模拟时间，，先将从库中复制的IO进程Slave_IO_Running停下来，使得从库暂时不写中继日志，也就是最后执行的SQL就是中继日志的最后一个SQL。

```
shop slave IO_THREAD;
```

1. 一段时间过后（保证最后执行的SQL就是中继日志的最后一个SQL），在从库端查询复制情况。

```
selece * from t1;
select now();    ---------获取当前时间

```

1. 查询SQL线程的时间，显示的Time值就是最后执行的复制操作是主库之前多少分钟前的更新。

```
show processlist \G;

```

**MySQL复制的机制是执行主库传输过来的二进制日志，二进制日志中的每个语句通过设置时间戳来保证执行时间和顺序的正确性，所以每个语句执行前都会设置时间戳，而通过查询这个Time就可以知道最后设置的时间戳和当前时间的差距。**

### 提高复制性能

#### 如何查看性能

主库是多线程并发写入数据，而从库只有一个SQL进程在应用日志，所以会出现从库追不上主库的现象。用`SHOW SLAVE STATUS`来查看从库落后主库的时间。

```
SHOW SLAVE STATUS \G

```

Seconds_Behind_Master显示了**预估**从库落后主库的时间（s）。

#### 提高性能方案

##### 提高硬件性能

##### 采用多级复制架构

##### MySQL 5.6提供了基于Schema(库)的多线程复制，允许从库进行更新。

例如，主库上存在两个Schema，即demo和user。MySQL 5.6的从库在同步主库时，通过设置参数`slave_parallel_workers`为2，让从库在复制时启动两个SQL线程。

### 切换主从库

#### 操作步骤

1. 确保每个从库已经执行了relay log中的全部更新,通过show Processlist看到**has read all relay log**。

```
STOP SLAVE IO_THREAD;
SHOW PROCESSLIST \G

```

1. 在要切换的从库上执行`STOP SLAVE`来停止复制服务，然后执行RESET MASTER以设置成主数据库。

```
STOP SLAVE;
reset master;

```

1. 在其他从库上修改配置，使其指向新的主数据库

```
stop slave;
change master to master_host='IPIPIPIPIPIP';
start slave;

```

1. 通知所有的客户端将应用指向新的主数据库，这样客户端发送的所有更新语法写入到S1的二进制日志。
2. 删除新的主数据库服务器上的master.info和relay-log.info文件，否则下次重启时，还会按照从库启动。
3. 若旧的主数据库可以修复，可以将旧的主数据库配置成新的主数据库的从库。
   **以上测试默认新的主数据库是打开`log-bin`选项的，这样重置成主数据库后可以将二进制日志传输到其他库。其次，`log-slave-updates`参数没有打开，否则重置成主数据库后，可能会将已经执行过的二进制日志重复传输给其他从库，导致其他从库同步错误。**

### 小结

- 复制很常用，可以保证主库数据安全，减轻备份压力，分担查询压力。
- 环境搭建简单，建议给重要的数据库配置复制。如果没有足够的服务器，也可以在一个主数据库上启动另一个MySQL服务作为另一个MySQL服务的从数据库。