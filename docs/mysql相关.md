# MySQL学习

## mysqldump 

备份的是创建表或者装载表的SQL语句。
### 连接选项
```
-u：用户
-p：密码
-h：域名
-P：端口
```
### 输出内容选项
在每个创建数据库/表的语句前加上DROP DATABASE/DROP TABLE语句。导入时，会自动删除旧的数据库，不用手动删除。
```
--add-drop-database
--add-drop-table
```
```
-n	不包含数据库的创建语句
-t	不包含表的创建语句
-d	不包含数据
```
```
只包含标的创建语句：mysqldump -uroot --compact -d test table_name >a
```
### 输出格式选项
```
--compact使输出结果简洁，不包含注释。
-c 使输出文件中的insert语句包括字段名称，默认是不包括字段名称的。
-T 将指定数据表中的数据备份为单纯的数据文本和建表SQL两个文件。导出目的地是目录，一个文件是.sql，一个是.txt
```
### 字符集选项
```
--default-character-set=name
```
### 其他选项
```
-F 备份前刷新日志，关掉旧日志，生成新日志。使恢复时直接从新日志开始，方便了恢复过程。
-l 给表加读锁。保持数据一致性。
```
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
2. 检查是否有插件。
```
MYSQL_HOME/lib/plugin/semisync_master.so
MYSQL_HOME/lib/plugin/semisync_slave.so
```
3. 主库安装插件semisync_master.so

```
install plugin rpl_semi_sync_master SONAME 'semisync_master.so'；
```
4. 从库安装插件semisync_slave.so

```
install plugin rpl_semi_sync_master SONAME 'semisync_slave.so'；
```
5. 查看安装情况，记录在表里，下次会自动加载插件。

```
select * from mysql.plugin;
```
6. 在主库和从库上配置参数打开半同步semi-sync。

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
2. 在主库插入3条数据。
 ```
  insert into t1 values(1),(2),(3);
 ```
此时主库中此表中多了3条数据。
3. 在从库中启动渎职进程，查询t1表中数据更新的情况,已跳过两条更新语句，只插入一条。
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
2. 为了方便模拟时间，，先将从库中复制的IO进程Slave_IO_Running停下来，使得从库暂时不写中继日志，也就是最后执行的SQL就是中继日志的最后一个SQL。

```
shop slave IO_THREAD;
```
3. 一段时间过后（保证最后执行的SQL就是中继日志的最后一个SQL），在从库端查询复制情况。
```
selece * from t1;
select now();    ---------获取当前时间
```
4. 查询SQL线程的时间，显示的Time值就是最后执行的复制操作是主库之前多少分钟前的更新。
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
2. 在要切换的从库上执行`STOP SLAVE`来停止复制服务，然后执行RESET MASTER以设置成主数据库。
```
STOP SLAVE;
reset master;
```
3. 在其他从库上修改配置，使其指向新的主数据库
```
stop slave;
change master to master_host='IPIPIPIPIPIP';
start slave;
```
4. 通知所有的客户端将应用指向新的主数据库，这样客户端发送的所有更新语法写入到S1的二进制日志。
5. 删除新的主数据库服务器上的master.info和relay-log.info文件，否则下次重启时，还会按照从库启动。
6. 若旧的主数据库可以修复，可以将旧的主数据库配置成新的主数据库的从库。
**以上测试默认新的主数据库是打开`log-bin`选项的，这样重置成主数据库后可以将二进制日志传输到其他库。其次，`log-slave-updates`参数没有打开，否则重置成主数据库后，可能会将已经执行过的二进制日志重复传输给其他从库，导致其他从库同步错误。**

### 小结

- 复制很常用，可以保证主库数据安全，减轻备份压力，分担查询压力。
- 环境搭建简单，建议给重要的数据库配置复制。如果没有足够的服务器，也可以在一个主数据库上启动另一个MySQL服务作为另一个MySQL服务的从数据库。

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
## mysql日志
### 错误日志
记录档MySQL启动和停止时，以及服务器在运行过程中发生任何严重错误时的相关信息。当数据库出现任何故障导致无法正常使用时，可以首先查看此日志。
- 选项 用--log-error[=file name]来指定错误日志的位置（日志名）。默认的位置是在参数DATADIR指定的目录中写入日志文件。
### 二进制日志
binlog记录了所有的DDL（数据定义）和DML（数据操纵）语句。语句以”事件“的形式保存，该日志对灾难时的数据恢复起着重要作用。
#### 位置格式
##### 位置
当--log-bin[=file name]选项启动时，mysqld开始将数据变更情况写入日志文件。默认名为主机名后边加“-bin”。默认的位置是在参数DATADIR指定的目录中写入日志文件。
##### 格式
1. STATEMENT
日志中的记录是SQL语句，通过mysqlbinlog工具可以看到每条语句的文本。
2. ROW
记录每一行的变更，不是SQL语句。优点是记录每一行变化的细节，不会出现无法复制的情况。缺点是日志量大，对IO影响大。
3. MIXED
目前MySQL默认的日志格式，混合了STATEMENT和ROW两种日志。默认情况采用STATEMENT，但在一些特殊情况下采用ROW进行记录。例如：采用NDB存储引擎，此时对表的DML语句全部采用ROW；客户端采用了临时表；客户端采用了不确定函数，比如current_user()（因为主从获得的值可能不同，导致主从数据不一致）。
**可以在`global`和`session`级别对对`binlog_format`进行日志格式的设置，但一定要谨慎操作，确保从库的复制能够正常进行。**
#### 日志的读取
使用`msyqlbinlog`查看二进制日志。
```
mysqlbinlog log-file
```
如果日志是ROW格式，则mysqlbinlog解析后是一堆无法理解的字符，可以加上-v或者-vv参数进行读取。
#### 日志删除
对于比较繁忙的OLTP（在线事务处理）系统，每天生成的日志量大，占用磁盘，要定期清理日志维护MySQL数据库。
##### 方法1
删除所有binlog日志，新日志编号从”000001“开始。
```
reset master；
system ls -ltr localhost-bin*；
```
##### 方法2
执行**PURGE MASTER LOGS TO 'mysql-bin.******'**命令，该命令删除“******”编号之前的所有日志。
```
PURGE MASTER LOGS TO 'localhost-bin.000006'；
system ls -ltr localhost-bin*；
```
##### 方法3
执行**PURGE MASTER LOGS before 'yyyy-mm-dd hh24:mi:ss'**命令，该命令删除日期为yyyy-mm-dd hh24:mi:ss之前的所有日志。
```
PURGE MASTER LOGS before '2007-08-10 04:07:00'；
system ls -ltr localhost-bin*；
```
##### 方法4
在配置文件my.cnf中设置参数--expire_logs_days=#，设置日志过期天数，过了指定天数后日志就自动删除。
#### 其他选项
- --binlog-do-db=db_name告诉主服务器，如果当前的数据库是db_name，则将所有的更新信息写入二进制日志，其他没有显式定义的数据库的更新将被忽略。
- --binlog-ignore-db=db_name告诉主服务器，如果当前的数据库是db_name，则数据库的更新将被忽略，其他没有显式定义的数据库的更新将信息写入二进制日志。
- --innodb-safe-binlog此选项经常和--sync-binlog=N（每写N次日志同步到磁盘）配合使用，使事务在日志中的记录更加安全。
- SET SQL_LOG_BIN=0 具有supper权限的用户可以用这个选项设置禁止自己的SQL语句记录到日志。使用要小心，容易造成主从数据不一致。

### 查询日志
查询日志包含了客户端的所有语句，而二进制日志步包含查询数据的语句。
#### 位置和格式
#### 读取
### 慢查询日志
记录了执行时间超过参数`long_query_time`(单位s)值并且扫描记录数不小于`min_examined_row_limit`的所有SQL语句（表锁定的时间不算执行时间）。
**两类语句不会记录到慢查询日志：管理语句和不使用索引的查询语句。**
- 默认会写到DATADIR路径下的指定位置，host-slow.log。
#### 参数输出
使用两个参数启动慢查询日志：`--slow_query_log[={0|1}]`、`--slow_query_log_file[=file_name]`。如果不指定`--slow_query_log[={0|1}]`的值或者值为1，都会打开慢查询。
- 可以选择日志的输出方式，`--log-output`默认输出到文件，也可以选择输出到表，表中精确到秒，文件中精确到微秒。
#### 工具
如果慢查询中的内容很多，可以使用mysqldumpslow工具对慢查询日志进行分类汇总。
```
data目录下执行：mysqldumpslow bj37-slow.log
mysql目录下执行：mysqldumpslo localhost-slow.log
```
mysqldumpslow是将SQL文本一致，变量不同的语句视为同一个语句进行统计，变量值用N代替。大大加快了用具阅读慢查询日志的效率，快速定位SQL瓶颈。
### mysqlsla
分析日志的第三方工具。
- 解析查询日志和慢查询日志
```
mysqlsla --log-type slow LOG
mysqlsla --log-type  general LOG
```
- 解析二进制日志，需要线通过mysqlbinlog进行转换
```
mysqlbinlog LOG | mysqlsla --log-type binary -
```
- 解析微秒日志
```
mysqlsla --log-type msl LOG 
```
- 解析用户自定义日志 
```
mysqlsla --log-type udl --udl-format FILE
```

## SQL 优化

### 场景

应用开发过程中，初期数据量少，开发人员更重视功能上的实现。应用上线以后，随着数据量急剧增长，一些SQL语句暴露出性能问题，这时系统性能的瓶颈就是这些有问题的SQL语句。
- 加载了案例库sakila（电影出租厅管理系统）
### 优化的步骤
#### 通过show status查看语句的使用频率
```
show status like 'Com_%';
```
##### 所有存储引擎
主要关注的几个参数：

```
Com_select

Com_update

Com_insert

Com_delete
```


##### innodb存储引擎
```
Innodb_rows_read			    select查询返回的行数
Innodb_rows_inserted			执行insert插入的行数
Innodb_rows_updated				执行update操作更新的行数
Innodb_rows_deleteed			执行delete操作删除的行数
```
****
##### 目的

通过以上参数，可以了解到当前数据库的应用是以插入更新为主还是查询为主，以及各种SQL大致的执行比例。对于更新操作的计数，是对执行次数的计数，不论提交还是回滚都会进行累加。

对于事务型的应用，通过Com_commit和Com_rollback可以了解事务提交和回滚的情况，对于回滚操作非常频繁的数据库，可能意味着应用编写存在问题。
##### 基本情况
```
Connections:试图连接MySQL的次数
Uptime:服务器工作时间
Slow_queries:慢查询次数
```
#### 定位执行效率低的SQL语句
##### 两种方式
- 慢查询日志
- show processlist
**慢查询日志在查询结束以后才记录，所以在应用反映执行效率出现问题时查询慢查询日志并不能定位问题，可以使用show processlist查看当前MySQL在进行的线程，包括线程的状态、是否锁表等，可以实时地查看SQL的执行情况，同时对一些锁表操作进行优化。**
#### 通过EXPLAIN分析低效SQL的执行计划
通过以上步骤查询到低效的SQL语句后，可以用EXPLAIN或者DESC命令获取MySQL如何执行select语句的信息，包括在select语句执行过程中表如何连接和连接的顺序。例如要查询某个email为租赁电影拷贝所支付的总金额，需要关联顾客表customer和付款表payment，并且对金额amount求和。
```
explain select sum(amount) from customer a, payment b where 1=1 and a.customer_id=b.customer_id and email='MARY.SMITH@sakilacustomer.org'\G
```
```
*************************** 1. row ***************************
           id: 1
  select_type: SIMPLE
        table: a
         type: ALL
possible_keys: PRIMARY
          key: NULL
      key_len: NULL
          ref: NULL
         rows: 599
        Extra: Using where
*************************** 2. row ***************************
           id: 1
  select_type: SIMPLE
        table: b
         type: ref
possible_keys: idx_fk_customer_id
          key: idx_fk_customer_id
      key_len: 2
          ref: sakila.a.customer_id
         rows: 13
        Extra: NULL
2 rows in set (0.00 sec)
```

| 参数          | 值                                                           | 概述                                                         |
| ------------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| select_type   | `simple`  `primary`  `union` `subquery`                      | `select`的类型，有`simple`（简单表（不用表连接或者子查询））,`primary`（主查询（外层的查询）），`union`（UNION中的第二个或者后面的查询语句），`subquery`（子查询中的第一个select） |
| table         |                                                              | 输出结果集的表                                               |
| type          | `all` `index` `range` `ref` `eq_ref` `const` `system` `null` | 从左至右性能由最差到最好                                     |
| possible_keys |                                                              | 查询时可能用到的索引                                         |
| key           |                                                              | 实际使用的索引                                               |
| key_len       |                                                              | 使用到的索引字段的长度                                       |
| rows          |                                                              | 扫描行的数量                                                 |
| Extra         |                                                              | 执行情况的说明和描述，包括不适合在其他列中显示但是对执行计划非常重要的额外信息 |

#### 通过show profile 分析SQL
默认profiling是关闭的，可以通过set语句在Session级别开启profiling
```
select @@profiling;
set profiling =1
```
##### 举例
对于MyISAM表有表的元数据缓存（例如行数count（*）），但是InnoDB没有，count（*），执行很慢。
1. 这是innodb引擎上的payment表执行count（*）查询。
```
mysql> select count(*) from payment;
+----------+
| count(*) |
+----------+
|    16049 |
+----------+
1 row in set (0.01 sec)

mysql> show profiles;
+----------+------------+------------------------------+
| Query_ID | Duration   | Query                        |
+----------+------------+------------------------------+
|        1 | 0.00007075 | select @@profiling           |
|        2 | 0.00514425 | select count(*) from payment |
+----------+------------+------------------------------+
2 rows in set, 1 warning (0.00 sec)
```
2. 通过`show profile for query 2;`看到执行过程中线程对的每个状态和消耗的时间。
```
show profile for query 2;
```
发现时间主要消耗在sending data上了，这个状态是访问数据返回结果。
3. 为了更仔细地观察排序结果，可以查询information_schema.profiling表，并按照时间做个DESC排序。
```
set @query_id := 4;
SELECT STATE, SUM(DURATION) AS Total_R,
	ROUND(	100*SUM(DURATION) /
			(SELECT SUM(DURATION)
			FROM INFORMATION_SCHEMA.PROFILING
			WHERE QUERY_ID = @query_id
		),2) AS Pct_R,
	COUNT(*) AS Calls,
	SUM(DURATION) / COUNT(*) AS "R/Call"
FROM INFORMATION_SCHEMA.PROFILING
WHERE QUERY_ID = @query_id
GROUP BY STATE
ORDER BY Total_R DESC;
```
```
+----------------------+----------+-------+-------+--------------+
| STATE                | Total_R  | Pct_R | Calls | R/Call       |
+----------------------+----------+-------+-------+--------------+
| Sending data         | 0.004972 | 96.62 |     1 | 0.0049720000 |
| starting             | 0.000045 |  0.87 |     1 | 0.0000450000 |
| freeing items        | 0.000022 |  0.43 |     1 | 0.0000220000 |
| Opening tables       | 0.000019 |  0.37 |     1 | 0.0000190000 |
| cleaning up          | 0.000013 |  0.25 |     1 | 0.0000130000 |
| statistics           | 0.000011 |  0.21 |     1 | 0.0000110000 |
| init                 | 0.000011 |  0.21 |     1 | 0.0000110000 |
| preparing            | 0.000010 |  0.19 |     1 | 0.0000100000 |
| closing tables       | 0.000010 |  0.19 |     1 | 0.0000100000 |
| end                  | 0.000008 |  0.16 |     1 | 0.0000080000 |
| System lock          | 0.000007 |  0.14 |     1 | 0.0000070000 |
| checking permissions | 0.000006 |  0.12 |     1 | 0.0000060000 |
| query end            | 0.000006 |  0.12 |     1 | 0.0000060000 |
| optimizing           | 0.000004 |  0.08 |     1 | 0.0000040000 |
| executing            | 0.000002 |  0.04 |     1 | 0.0000020000 |
+----------------------+----------+-------+-------+--------------+
```

4. 在获得了最消耗时间的线程状态后，MySQL还支持进一步选择all、cpu、block、io、context、switch、page faults等明细类型查看MySQL在使用什么资源上耗费了过高的时间。
```
show profile cpu for query 2;
```
5. 对比MyISAM的count（*）
```
create table payment_myisam like payment;
alter table payment_myisam engine=myisam;
insert into payment_myisam select * from payment;
select count(*) from payment_myisam;
show profiles;
show profile for query N;
```
```
+----------------------+----------+
| Status               | Duration |
+----------------------+----------+
| starting             | 0.000048 |
| checking permissions | 0.000006 |
| Opening tables       | 0.000020 |
| init                 | 0.000013 |
| System lock          | 0.000007 |
| optimizing           | 0.000006 |
| executing            | 0.000009 |
| end                  | 0.000004 |
| query end            | 0.000002 |
| closing tables       | 0.000008 |
| freeing items        | 0.000009 |
| cleaning up          | 0.000015 |
+----------------------+----------+
```

**show profile可以告诉我们时间都耗费在哪了。MySQL 5.6通过trace文件进一步向我们展示了优化器是如何选择执行计划的。**

#### 通过trace文件分析优化器如何选择执行计划

1. 打开trace并设置格式为JSON，设置trace最大能够使用的内存。

```
SET OPTOMOZER_TRACE='enabled=on',END_MARKERS_IN_JSON=on;
SET OPTOMOZER_TRACE_MAX_MEN_SIZE=1000000;
```

2. 执行想做trace的SQL语句。
3. 检查INFORMATION_SCHEMA.OPTIMIZER_TRACE就可以知道MySQL是如何执行SQL的。
```
SELECT * FROM INFORMATION_SCHEMA.OPTIMIZER_TRACE \G
```
#### 确定问题并采取相应的优化措施
##### 场景

例如要查询某个email为租赁电影拷贝所支付的总金额，需要关联顾客表customer和付款表payment，并且对金额amount求和。

```
explain select sum(amount) from customer a, payment b where 1=1 and a.customer_id=b.customer_id and email='MARY.SMITH@sakilacustomer.org'\G
```
```
*************************** 1. row ***************************
           id: 1
  select_type: SIMPLE
        table: a
         type: ALL
possible_keys: PRIMARY
          key: NULL
      key_len: NULL
          ref: NULL
         rows: 599
        Extra: Using where
*************************** 2. row ***************************
           id: 1
  select_type: SIMPLE
        table: b
         type: ref
possible_keys: idx_fk_customer_id
          key: idx_fk_customer_id
      key_len: 2
          ref: sakila.a.customer_id
         rows: 13
        Extra: NULL
2 rows in set (0.00 sec)
```
我们可以确认对客户表customer的全盘扫描导致效率不理想，那么对客户表customer的email字段创建索引

```
create index idx_email on customer(email);
explain select sum(amount) from customer a, payment b where 1=1 and a.customer_id=b.customer_id and email='MARY.SMITH@sakilacustomer.org'\G
```

```
*************************** 1. row ***************************
           id: 1
  select_type: SIMPLE
        table: a
         type: ref
possible_keys: PRIMARY,idx_email
          key: idx_email
      key_len: 153
          ref: const
         rows: 1
        Extra: Using where; Using index
*************************** 2. row ***************************
           id: 1
  select_type: SIMPLE
        table: b
         type: ref
possible_keys: idx_fk_customer_id
          key: idx_fk_customer_id
      key_len: 2
          ref: sakila.a.customer_id
         rows: 13
        Extra: NULL
2 rows in set (0.00 sec)
```

可以看到我们检索的行数从599行变成了1行。

### 索引问题

#### 索引存储的分类
MySQL索引的分类、存储、使用方法。
##### 分类
| 索引                      | 概述                                                         |
| ------------------------- | ------------------------------------------------------------ |
| B-Tree索引                | 最常见的索引，大部分引擎都支持B数索引                        |
| HASH索引                  | 只有Memory引擎支持，场景简单                                 |
| R-Tree索引（空间索引）    | 空间索引是MyISAM的一个特殊索引类型，主要用于地理空间数据类型，使用较少 |
| Full-text索引（全文索引） | 空间索引也是MyISAM的一个特殊索引类型，主要用于全文索引，MySQL5.6版本开始支持全文索引 |

MySQL暂时不支持函数索引，但是能对前面某一部分进行索引，例如标题title字段可以只取title的前十个字符进行索引，这个特性可以大大缩小索引的大小。但是前缀索引有个缺点，在排序Order By和分组Group by时无法使用。前缀索引：

```
create index idx_title on film(title(10));
```

##### 常用引擎支持的索引类型

| 索引      | MyISAM | Innodb | memory |
| --------- | ------ | ------ | ------ |
| B-Tree    | √      | √      | √      |
| HASH      | ×      | ×      | √      |
| R-Tree    | √      | ×      | ×      |
| Full-text | √      | √      | ×      |

- Innodb自动生成哈希索引但是不支持用户干预。

- **最常用的就是B树和哈希索引。哈希索引相对简单，适用于Key-Value查询，通过Hash索引要比通过B-Tree索引查询更快速；哈希索引不适用范围查询，例如< , > , <= , >= 这类操作。**
- **如果使用memory/Heap引擎并且where条件中不使用‘’=‘’进行索引列，那么不会用到索引。**

#### 如何使用索引（B-Tree）

B-Tree索引中的B不是代表二叉树（binary），而是代表平衡树（balanced）。

##### 应用场景

可以利用B-Tree索引进行全关键字、关键字范围和关键字前缀查询。

- 匹配全值
- 匹配值的范围查询
- 匹配最左前缀
- 仅仅对索引进行查询
- 匹配列前缀
- 能够实现索引匹配部分精确而其他部分进行范围匹配
- 如果列名是索引，那么使用column_name is null 就会使用索引。

##### 有索引但是不能使用的场景

- 以%开头的LIKE查询

- 数据类型出现隐式转换时

  特别是当列是字符串，那么一定记得在where条件中把字符常量值用引号引起来，因为mysql默认把输入的常量值进行转换以后才进行检索。

- 不满足最左原则

- 若MySQL估计使用索引比全表扫描更慢

  trace中看优化去选择的过程。会发现选择的代价cost，比较选择。

- 用or分割开的条件

  前边条件有索引但是后边的条件不包含索引则涉及的索引都不会用到。

#### 查看索引使用情况

`Handler_read_key`如果索引正在工作，值会变很高，代表了一个行被索引值读的次数，很低的值代表增加的索引性能改善不高，因为索引不经常使用。

`Handler_read_rnd_next`数据文件中读下一行的请求数。值高意味着查询运行低效，应建立索引补救。如果有大量的表扫描，`Handler_read_rnd_next`值高通常意味着索引不正确或者写入的查询没有利用索引。

```
show status like 'Handler_read%';
```

### 两个简单的优化方法

#### 定期分析表和检查表

```
analyze table payment;
check table payment_myisam;
```

#### 定期优化表

```
optimize table payment_myisam;
```

对于Innodb表来说，设置`innodb_fil_per_table`参数，设置Innodb为独立表空间模式，这样每个库的每个表都会生成一个独立的ibd文件，用于存储表的数据和索引，这样可以一定程度上实现InnoDB表的空间回收问题。另外，在删除大量数据后，InnoDB表可以通过alter table但是不修改引擎的方式来回收不用的空间。

```
alter table payment engine=innodb;
```

**ANALYZE , CHECK , OPTIMIZE , ALTER TABLE执行期间都会对表进行锁定，因此一定要注意在数据库不繁忙的时候执行这些操作。**

### 常用SQL的优化（insert、group by等）

对于InnoDB
#### 大批量插入数据
1. 将导入的数据按照主键的顺序排列可以提高导入数据的效率。

2. 导入数据前，执行`SET UNIQUE_CHECKS=0`。关闭唯一性校验，导入结束后执行`SET UNIQUE_CHECKS=1`恢复唯一性校验。

3. 如果应用采用自动提交方式，建议在导入前执行`SET AUTOCOMMIT=0`关闭自动提交，导入结束后执行`SET AUTOCOMMIT=0`恢复。
#### 优化insert语句
1. 对于同意客户，尽量使用多个值的insert语句
2. 对于从不同客户插入很多行，可以使用INSERT DELAYED语句得到更高的速度。DELAYED语句的含义是让insert语句立即执行，其实数据都被放在内存队列中，并没有写入磁盘，这比每条语句分别插入要快得多。LOW_PRIORITY正好相反，对所有用户对表的读写完成之后才进行插入。
3. 将索引文件和数据文件在不同的磁盘上存放。（利于建表中的选项）
4. MyISAM适用：批量插入时，增加`bulk_insert_buffer_size`变量值的方法来提高速度。
5. 当一个文本文件装载一个表时，使用LOAD DATA INFILE,比使用insert速度会提高20倍。
#### 优化order by语句

1. 排序方式

- 通过有序索引顺序扫描返回有序数据，使用explain时Extra的值为Using index。
- 对返回数据排序，Filesort排序。
**尽量减少额外的排序，通过索引直接返回有序数据。**
order by和where使用相同的索引，并且order by 的顺序和索引对的顺序相同。
**不使用索引的情况**
- order by字段中混合含有DESC、ASC。
- 用于查询行的关键字与order by中所使用的不同。
```
select * from 1 where col1='a' order by col2;
```
- 对不同的关键字使用order by
2. Filesort的优化
- 两次扫描法
- 一次扫描法
#### GROUP BY语句的优化
##### 优化嵌套查询
将嵌套查询改为更有效率的连接
```
select * from customer where customer_id not in (select cutomer_id from payment);
select * from customer a left join payment on customer.customer_ip=payment.customer_id where payment.customer_id is null;
```

#### MySQL优化OR条件

对于含有OR条件的查询语句，要想利用索引，则每个条件列都必须用到索引；如果没有，可以考虑增加索引。

#### 优化分页查询

一般分页查询时，通过创建覆盖索引可以提高性能。
##### 第一种优化思路
在索引上完成排序分页的操作，最后根据主键关联回源表查询所需要的其他列的内容。
- 对标题排序后，取某一页数据，从结果看进行了全表扫描，效率不高。
```
mysql> explain select film_id, description from film order by title limit 50, 5\G
*************************** 1. row ***************************
           id: 1
  select_type: SIMPLE
        table: film
         type: ALL
possible_keys: NULL
          key: NULL
      key_len: NULL
          ref: NULL
         rows: 1000
        Extra: Using filesort
1 row in set (0.00 sec)

```
- 按照索引分页后回表方式改写SQL后结果中已经看不出全表扫描了。
```
mysql> explain select a.film_id, a.description from film a  inner join (select film_id from film order by title limit 50, 5)b on a.film_id\G
*************************** 1. row ***************************
           id: 1
  select_type: PRIMARY
        table: <derived2>
         type: ALL
possible_keys: NULL
          key: NULL
      key_len: NULL
          ref: NULL
         rows: 55
        Extra: NULL
*************************** 2. row ***************************
           id: 1
  select_type: PRIMARY
        table: a
         type: ALL
possible_keys: NULL
          key: NULL
      key_len: NULL
          ref: NULL
         rows: 1000
        Extra: Using where; Using join buffer (Block Nested Loop)
*************************** 3. row ***************************
           id: 2
  select_type: DERIVED
        table: film
         type: index
possible_keys: NULL
          key: idx_title
      key_len: 767
          ref: NULL
         rows: 1000
        Extra: Using index
```
**这种方式让mysql扫描尽可能少的页面来提高分页效率**
https://segmentfault.com/a/1190000008131735
##### 第二种优化思路
把limit查询转换成某个位置的查询
提前确定位置，将limit m,n 的查询编程limit n的查询。这种情况只适合在排序字段不会出现重复值的特定环境，能够减少分页带来的压力；如果排序字段出现大量的重复，仍进行这种优化那么分页结果可能会丢失部分记录。

#### 使用SQL提示
SQL提示（SQL HINT）。简单的说就是在SQL语句中加入一些人为的提示来达到优化操作的目的。
```
SELCT SQL_BUFFER_RESULT * FROM ... 
```
这个语句将强制MySQL生成一个临时的结果集。只要临时结果集生成后，所有表上的锁定均被释放。这能在遇到表锁定问题时或要花很长时间将结果发给客户端时有用，因为可以尽快释放锁资源。
- USE INDEX
提供MySQL期望的索引，不再考虑其他可用索引。
```
mysql> explain select count(*) from rental use index (rental_date)\G
*************************** 1. row ***************************
           id: 1
  select_type: SIMPLE
        table: rental
         type: index
possible_keys: NULL
          key: rental_date
      key_len: 10
          ref: NULL
         rows: 16005
        Extra: Using index
******************************************************
mysql> explain select count(*) from rental\G
           id: 1
  select_type: SIMPLE
        table: rental
         type: index
possible_keys: NULL
          key: idx_fk_staff_id
      key_len: 1
          ref: NULL
         rows: 16005
        Extra: Using index
```
- IGNORE INDEX
忽略一个或者多个索引。
- FORCE INDEX
强制使用一个特定的索引。
```
mysql> explain select * from rental where inventory_id>1\G
*************************** 1. row ***************************
           id: 1
  select_type: SIMPLE
        table: rental
         type: ALL
possible_keys: idx_fk_inventory_id
          key: NULL
      key_len: NULL
          ref: NULL
         rows: 16005
        Extra: Using where
1 row in set (0.01 sec)
```
因为大部分的id都大于1，所以MySQL会默认使用全表扫描。
```
mysql> explain select * from rental FORCE INDEX(idx_fk_inventory_id)  where inventory_id>1\G
*************************** 1. row ***************************
           id: 1
  select_type: SIMPLE
        table: rental
         type: range
possible_keys: idx_fk_inventory_id
          key: idx_fk_inventory_id
      key_len: 3
          ref: NULL
         rows: 8002
        Extra: Using index condition
1 row in set (0.00 sec)
```
这种情况使用use index 是不能指定索引的。
#### 常用的SQL技巧
##### 正则表达式

| 模式 | 描述 |
| ---- | ---- |
|      |      |
|      |      |
|      |      |
|      |      |
|      |      |
|      |      |
|      |      |
|      |      |
|      |      |



| 模式       | 描述                                                         |
| ---------- | ------------------------------------------------------------ |
| ^          | 匹配输入字符串的开始位置。如果设置了 RegExp 对象的 Multiline 属性，^ 也匹配 '\n' 或 '\r' 之后的位置。 |
| $          | 匹配输入字符串的结束位置。如果设置了RegExp 对象的 Multiline 属性，$ 也匹配 '\n' 或 '\r' 之前的位置。 |
| .          | 匹配除 "\n" 之外的任何单个字符。要匹配包括 '\n' 在内的任何字符，请使用象 '[.\n]' 的模式。 |
| [...]      | 字符集合。匹配所包含的任意一个字符。例如， '[abc]' 可以匹配 "plain" 中的 'a'。 |
| [^...]     | 负值字符集合。匹配未包含的任意字符。例如， '[^abc]' 可以匹配 "plain" 中的'p'。 |
| p1\|p2\|p3 | 匹配 p1 或 p2 或 p3。例如，'z\|food' 能匹配 "z" 或 "food"。'(z\|f)ood' 则匹配 "zood" 或 "food"。 |
| *          | 匹配前面的子表达式零次或多次。例如，zo* 能匹配 "z" 以及 "zoo"。* 等价于{0,}。 |
| +          | 匹配前面的子表达式一次或多次。例如，'zo+' 能匹配 "zo" 以及 "zoo"，但不能匹配 "z"。+ 等价于 {1,}。 |
| {n}        | n 是一个非负整数。匹配确定的 n 次。例如，'o{2}' 不能匹配 "Bob" 中的 'o'，但是能匹配 "food" 中的两个 o。 |
| {n,m}      | m 和 n 均为非负整数，其中n <= m。最少匹配 n 次且最多匹配 m 次。 |

##### 使用RAND()随机提取行

抽样分析统计时很有用。

```
select * from category by rand() limit 5;
```

##### 利用GROUP BY的WITH ROLLUP子句

使用这个子句可以检索出更多的分组聚合信息，它不仅仅像一般的GROUP BY 语句那样检索出各组的聚合信息，还能检索出本组类的整体聚合信息。

- 在payment表中，按照支付时间的年月、经手员工的编号列分组对支付金额amount列进行聚合计算如下：
```
mysql> select date_format(payment_date, '%Y-%m'),staff_id, sum(amount) from payment group by date_format(payment_date, '%Y-%m'), staff_id;
+------------------------------------+----------+-------------+
| date_format(payment_date, '%Y-%m') | staff_id | sum(amount) |
+------------------------------------+----------+-------------+
| 2005-05                            |        1 |     2621.83 |
| 2005-05                            |        2 |     2202.60 |
| 2005-06                            |        1 |     4776.36 |
| 2005-06                            |        2 |     4855.52 |
| 2005-07                            |        1 |    14003.54 |
| 2005-07                            |        2 |    14370.35 |
| 2005-08                            |        1 |    11853.65 |
| 2005-08                            |        2 |    12218.48 |
| 2006-02                            |        1 |      234.09 |
| 2006-02                            |        2 |      280.09 |
+------------------------------------+----------+-------------+
10 rows in set (0.04 sec)
```
**WITH ROLLUP**

```
mysql> select date_format(payment_date, '%Y-%m'),IFNULL(staff_id, ''), sum(amount) from payment group by date_format(payment_date, '%Y-%m'), staff_id with rollup;
+------------------------------------+----------------------+-------------+
| date_format(payment_date, '%Y-%m') | IFNULL(staff_id, '') | sum(amount) |
+------------------------------------+----------------------+-------------+
| 2005-05                            | 1                    |     2621.83 |
| 2005-05                            | 2                    |     2202.60 |
| 2005-05                            |                      |     4824.43 |
| 2005-06                            | 1                    |     4776.36 |
| 2005-06                            | 2                    |     4855.52 |
| 2005-06                            |                      |     9631.88 |
| 2005-07                            | 1                    |    14003.54 |
| 2005-07                            | 2                    |    14370.35 |
| 2005-07                            |                      |    28373.89 |
| 2005-08                            | 1                    |    11853.65 |
| 2005-08                            | 2                    |    12218.48 |
| 2005-08                            |                      |    24072.13 |
| 2006-02                            | 1                    |      234.09 |
| 2006-02                            | 2                    |      280.09 |
| 2006-02                            |                      |      514.18 |
| NULL                               |                      |    67416.51 |
+------------------------------------+----------------------+-------------+
16 rows in set (0.02 sec)
```











































































## MySQL其他

- where 1=1 和where0=1

如果用户在多条件查询页面中，不选择任何字段、不输入任何关键词，那么，必将返回表中所有数据；如果用户在页面中，选择了部分字段并且输入了部分查询关键词，那么，就按用户设置的条件进行查询。where 1=1的应用，不是什么高级的应用，也不是所谓的智能化的构造，仅仅只是为了满足多条件查询页面中不确定的各种因素而采用的一种构造一条正确能运行的动态SQL语句的一种方法。
where 1=0; 这个条件始终为false，结果不会返回任何数据，只有表结构，可用于快速建表。该select语句主要用于读取表的结构而不考虑表中的数据，这样节省了内存，因为可以不用保存结果集。
```
SELECT * FROM strName WHERE 1 = 0; 
```
创建一个新表，而新表的结构与查询的表的结构是一样的。
```
create table newtable as select * from oldtable where 1=0;  
```










