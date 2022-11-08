#! https://zhuanlan.zhihu.com/p/581488255
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