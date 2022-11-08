#! https://zhuanlan.zhihu.com/p/581434984
# InnoDB锁问题
## 背景
1. 支持事务、行级锁。
事务及ACID属性
- 原子性
- 一致性
- 隔离性
- 持久性
2. 并发事务处理带来的问题
- 更新丢失
- 脏读
- 不可重复读
- 幻读
3. 事务隔离级别
更新丢失是应用的责任
脏读、不可重复读、幻读是数据库读一致性的问题，必须由数据库提供一定的事务隔离机制来解决。
- 读取数据前，加锁，阻止其他事务对数据进行修改。

- 生成一个数据请求时间点的一致性数据快照，并用这个快照提供一定级别的一致性读取。从用户角度看，好像是数据库可以提供一个数据的多个版本，所以，这种技术叫做数据多版本并发控制（多版本数据库）。

- 4种隔离级别

  | 读数据一致性及允许的并发副作用 | 读数据一致性                         | 脏读 | 不可重复读 | 幻读 |
  | ------------------------------ | ------------------------------------ | ---- | ---------- | ---- |
  | 未提交读                       | 最低级别，只保证不读物理上损坏的数据 | 是   | 是         | 是   |
  | 已提交读                       | 语句级                               | 否   | 是         | 是   |
  | 可重复读                       | 事务级                               | 否   | 否         | 是   |
  | 可序列化                       | 最高级别，事务级                     | 否   | 否         | 否   |

## 获取InnoDB行锁争用情况
```
show status like 'innodb_row_lock%'
+-------------------------------+-------+
| Variable_name                 | Value |
+-------------------------------+-------+
| Innodb_row_lock_current_waits | 0     |
| Innodb_row_lock_time          | 0     |
| Innodb_row_lock_time_avg      | 0     |
| Innodb_row_lock_time_max      | 0     |
| Innodb_row_lock_waits         | 0     |
+-------------------------------+-------+
```
若发生行锁争用比较严重`Innodb_row_lock_waits`和`Innodb_row_lock_time_avg`的值比较高，可以查询information_schema相关表来查看表情况，或者设置InnoDB Monitors来进一步观察发生锁冲突的表、数据行等，并分析。
```
use information_schema
show * from innodb_locks \G;
show * from innodb_locks_waits \G;
```
```
CREATE TABLE innodb_monitor(a INT) ENGINE=INNODB;
show engine innodb status \G;
```
监视器可以用下列语句来停止
```
DROP TABLE innodb_monitor;
```
## InnoDB的行锁模式及加锁方法
类型
- 共享锁（S）
```
SELECT * FROM table_name WHERE .. LOCK IN SHARE MODE
```
- 排它锁（X）
```
SELEXT * FROM table_name WHERE .. FOR UPDATE
```
- 意向共享锁（IS）：必须先获得该表的IX锁

- 意向排它锁（IX）：必须先获得该表的IS锁

  |      | X    | IX   | S    | IS   |
  | ---- | ---- | ---- | ---- | ---- |
  | X    | 冲突 | 冲突 | 冲突 | 冲突 |
  | IX   | 冲突 | 兼容 | 冲突 | 兼容 |
  | S    | 冲突 | 冲突 | 兼容 | 兼容 |
  | IS   | 冲突 | 兼容 | 兼容 | 兼容 |

意向锁是InnoDB自动加的，不需要干预。对insert、update、delete，InnoDB会自动为相关数据集加排它锁；对于普通的select，不加锁；事务可以通过下面语句加锁。

```
S : select * from table_name where... lock in share mode
X : select * from table_name where... for update
```

## 实现方式

通过给索引上的索引项加锁实现的，如果没有索引，InnoDB会通过隐藏的聚簇索引来对记录进行加锁。

3种形式

- Record lock:对索引项加锁
- Gap lock :对索引项之间的`间隙`，第一条记录前的间隙或最后一条记录后的`间隙`进行加锁。
- Next-key lock:前两种的组合。对记录及其前面的间隙加锁。


行锁的特性会出现锁冲突，影响并发性能
- 通过InnoDB的行锁实现特点，如果不通过索引条件检索数据，那么InnoDB将对表中所有记录加锁，效果和表锁一样。
```
mysql> select * from tab_no_index;
+------+------+
| id   | name |
+------+------+
|    1 | 1    |
|    2 | 2    |
|    3 | 3    |
|    4 | 4    |
+------+------+
4 rows in set (0.00 sec)

mysql> set autocommit=0;
Query OK, 0 rows affected (0.00 sec)

mysql> select * from tab_no_index where id=1;
+------+------+
| id   | name |
+------+------+
|    1 | 1    |
+------+------+
1 row in set (0.00 sec)

mysql> select * from tab_no_index where id=1 for update;
+------+------+
| id   | name |
+------+------+
|    1 | 1    |
+------+------+
1 row in set (0.00 sec)
```
在另一个终端里就会出现锁等待
```
mysql> select * from tab_no_index where id=2 for update;
ERROR 1205 (HY000): Lock wait timeout exceeded; try restarting transaction
```
- 因为InnoDB是针对索引加锁，索引访问不同行时也可能出现锁冲突。应用设计的时候要注意这一点。
```
mysql> select * from table_with_index;
+------+------+
| id   | name |
+------+------+
|    1 | 1    |
|    2 | 2    |
|    3 | 3    |
|    4 | 4    |
|    1 | 4    |
+------+------+
5 rows in set (0.00 sec)

mysql> set autocommit=0;
Query OK, 0 rows affected (0.00 sec)

mysql> select * from table_with_index where id=1 and name='1' for update;
+------+------+
| id   | name |
+------+------+
|    1 | 1    |
+------+------+
1 row in set (0.00 sec)
```
另一个终端里就会锁等待，因为用了同一个索引`id=1`
```
mysql> select * from table_with_index where id=1 and name='4' for update;
ERROR 1205 (HY000): Lock wait timeout exceeded; try restarting transaction
```
- 不同的索引可以锁定不同的行，但是不能锁定同一行。
- 不使用索引的情况会对表加锁。（P272索引问题，会用trace文件中的属性对比选择，如果全表扫描的代价更小就不用索引）
- Next-Key锁
当我们使用的条件是范围条件，并请求共享或排它锁时，InnoDB会给复合条件的已有数据的索引项加锁；对于键值在范围条件内，但是并不存在的记录叫做间隙，InnoDB也会给间隙加锁，这种锁机制就是Next-Key锁。
```
select * from emp where empid > 100 for update;
```
  	 InnoDB不仅会给empid值为101的记录加锁，也会对大于101但是记录不存在的“记录”加锁。InnoDB加入这种机制是为了防止幻读（A事务更新了**整个表**，B向表中加入一条，A事务的用户发现没整理好，就像幻觉一样）。
 	  这里的幻读值的是如果不使用间隙锁，如果其他事务插入了一条大于100的记录，再次执行上述语句，就会发生幻读；另一方面是为了满足其恢复和复制的需要。
   	在使用范围条件检索并锁定数据时，InnoDB这种机制会阻塞符合条件范围内键值的并发插入，这会造成锁等待。所以，为了避免间隙加锁，设计应用时，要注意优化业务逻辑，尽量用相等条件来访问更新数据。
	如过给一个不存在的相等条件加锁时也使用Next-Key锁。

## 恢复和复制的需要，对InnoDB锁机制的影响

Binlog的三种格式对应三种复制方式。

| Binlog格式 | 复制方式                                                     |                   |
| ---------- | ------------------------------------------------------------ | ----------------- |
| Statement  | binlog_format=Statement(Statement_Base Replication)   (SBR)  | 基于SQL语句的复制 |
| Row        | binlog_format=Row(Row_Base Replication)                         (RBR) | 基于行的复制      |
| Mixed      | binlog_format=Mixed                                          | 混合复制          |

混合的方法是：对于安全的SQL语句采用基于SQL语句的复制方式；否则采用基于行的复制模式。

还有第四种复制方式：使用全局事务ID的复制（GTIDs）：主要是解决主从自动同步一致问题。

- 对于基于SQL语句的恢复和复制来说，因为BINLOG是根据事务的提交顺序记录的，所以正确恢复和复制的前提条件是：在一个事务未提交前，其他并行事务不能插入满足其锁定条件的任何记录，即不能产生幻读。实际就是要求串行化，但是这超过了IOS/ANSI SQL92"可重复读"隔离级别的要求。这也是为什么许多情况下采用Next-Key锁的原因
使用`intsert into .. select..`和`create table .. select .. `语句时，会对select后的源表加锁。如果查询比较复杂，冰法的事务无法执行，会引发严重的性能能问题。所以要尽量避免使用，MySQL成之为不确定SQL（Unsfe SQL）。如果一定要使用，又不希望对源表的并发的更新产生影响，可以采用3种措施：
- 将`innodb_locks_unsafe_for_binlog`的值设置为“on”，强制mysql使用多版本一致性读。但是此时产生的binlog是不正确的，所以不推荐使用。
- 通过`select * from source_tab ... Into outfile`和`load data infile..`语句组合实现。
- 采用基于行的binlog日志格式和基于行数据的复制。
## InnoDB在不同隔离级别下的一致性读及锁的差异
对于很多SQL，隔离级别越高，InnoDB给记录加的锁就越严格（尤其是使用范围条件的时候），产生锁冲突的可能性就越高，从而对并发性事务处理性能的影响就越大。
结论是在实际中，要尽量降低隔离级别，以减少锁争用的几率。
实际中，通过优化事务的逻辑，大部分应用的隔离级别使用Read Committed就足够了。对于必须使用高的隔离级别的事务，在程序中执行下列语句动态改变隔离级别的方式满足需求。

```
SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ
或者 SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE
```
## 何时使用表锁
- 事务需要更新大部分或全部数据，表有比较大
- 事务涉及多个表，比较复杂，可能发生死锁
行锁是我们选择InnoDB表的原因，所以上面两种情况不能太多，否则就要选择MyISAM表了。

在InnoDB下，使用表锁要注意以下两点：
- 表锁不是InnoDB引擎管理的，而是其由上一层的MySQL Server负责的，仅当autocommit=0、InnoDB_table_locks=1(默认设置)时，InnoDB层才能知道MySQL加的表锁，MySQL Server也才能感知到InnoDB加的行锁，这种情况下，InnoDB才能自动识别涉及表级锁的死锁。
- 在用LOCK TABLE 对表进行加锁时，要设置autocommit=0，否则MySQL不会给表加锁；事务结束前，不要用UNLOCK TABLES释放表锁，因为UNLOCK TABLES会隐式地提交事务；COMMIT或者ROLLBACK并不能释放用LOCK TABLES加的表锁，必须用UNLOCK TABLES释放表锁。
```
SET AUTOCOMMIT=0;
LOCK TABLES T1 WRITE, T2 READ, ...;
...
COMMIT;
UNLOCK TABLES;
```
## 死锁
一般情况下，InnoDB会发现死锁，并使一个事务释放锁并回退，另一个事务获得锁，继续完成事务。但是，涉及外部锁或表级锁的情况下，InnoDB并不能完全检测到死锁，这是需要设置锁等待超时参数`innodb_lock_wait_timeout`来解决。
`innodb_lock_wait_timeout`并不只是用来解决死锁问题，在并发访问比较高的情况下，如果大量事务没有立即获得所需的锁而挂起，会占用大量的计算机资源，造成严重的性能问题，甚至拖垮数据库。通过设置合适的数值来避免这种情况的发生。
通常来说，死锁都是应用设计的问题，通过调整业务流程、数据库对象设计、事务大小，以及访问数据库的SQL语句，绝大部分死锁都可以避免。
### 避免死锁的常用方法
1. 在应用中，如果不同的程序会并发存取多个表，应尽量约定以相同的顺序来访问表，这样可以大大降低发生死锁的机会。
第一个终端
```
mysql> set autocommit=0
    -> ;
Query OK, 0 rows affected (0.00 sec)

mysql> select first_name,last_name from actor where actor_id=1 for update;
+------------+-----------+
| first_name | last_name |
+------------+-----------+
| PENELOPE   | GUINESS   |
+------------+-----------+
1 row in set (0.00 sec)

mysql> insert into country(country_id,country) values(110,'test');

ERROR 1213 (40001): Deadlock found when trying to get lock; try restarting transaction
mysql> 
mysql> insert into country(country_id,country) values(110,'test');


ERROR 1205 (HY000): Lock wait timeout exceeded; try restarting transaction
```
第二个终端
```
mysql> set autocommit=0;
Query OK, 0 rows affected (0.00 sec)

mysql> insert into country(country_id,country) values(110,'test');
Query OK, 1 row affected (0.00 sec)

mysql> select first_name,last_name from actor where actor_id=1 for update;
+------------+-----------+
| first_name | last_name |
+------------+-----------+
| PENELOPE   | GUINESS   |
+------------+-----------+
1 row in set (0.00 sec)
```
2. 在程序以批量方式处理数据的时候，如果事先对数据排序，保证每个线程用固定的顺序来处理记录，也可以大大降低死锁的概率。
3. 在事务中，如果要更新记录，应该申请足够级别的锁，即排它锁，而不应先申请共享锁，更新时在申请排它锁，防止其他用户也申请了共享锁，从而产生锁冲突甚至死锁。
4. 在`REPEATABLE-READ`隔离级别下，如果两个线程同时对相同条件记录用`SELECT ... FOR UPDATE`加排他锁，在没有符合条件的记录的情况下，两个线程都会加锁成功。程序发现记录不存在，如果师徒家兔一天记录，如果两个线程都这么做，就会发生死锁。这种情况下，把隔离级别改成`READ COMMITTED`就可以避免死锁。
5. 当隔离级别是`READ COMMITTED`时，如果两个线程都先执行`SELECT .. FOR UPDATE`，判断是否存在符合条件的记录，如果没有即插入记录。此时只有一个线程能插入成功，另一个陷入锁等待，当第一个线程提交成功后，第二发个线程也会因为主键重而出错，**但是**，虽然这个线程出错了，却会获得一个排他锁。这时如果有第三个线程又来申请排他锁，也会出现死锁。
	解决办法是，可以直接做插入操作，然后再捕获主键重异常，或者在遇到主键重错误时，总是执行ROLLBACK释放获得的排他锁。

### 措施

死锁不可避免，在编程中总是捕获并处理死锁异常是很好的习惯。如果出现死锁，可以用`SHOW INNODB STATUS`命令来确定最后一个死锁产生的原因。返回结果中包括死锁相关事务的详细信息，如引发死锁的SQL语句，事务已经获得的锁，正在等待什么锁，以及被回滚的事务等。据此可以分析原因和改进措施。

## 总结

- InnoDB的行锁是基于索引实现的，若没有通过索引访问数据，就会使用表锁
- Next-Key锁机制，使用的原因
- 不同隔离级别下，InnoDB的一致性读策略和锁机制不同
- MySQL恢复和复制对InnoDB锁机制和一致性读策略也有较大影响
- 锁冲突甚至死锁很难避免，在编程是捕捉并处理锁异常是很好的编程习惯

通过设计和调整SQL减少锁冲突或死锁
- 尽量使用较低的隔离级别（一般`READ COMMITTED`就够用了）
- 设计恰当的索引，尽量使用索引访问数据
- 选择合理的事务大小，小事务发生锁冲突的概率也小
- 给记录级显式加锁时，尽量一次性申请足够级别的锁（排他锁）。
- 不同的程序访问一组表时，尽量约定以相同的顺序访问各表，对一个表而言，尽可能以固定的顺序存取表中的行。
- 尽量用相等条件访问数据，避免Next-Key锁对并发插入的影响。
- 不要申请超过实际需要的锁级别
- 除非必须，否则查询时不要显式加锁
- 对于特定的场景，表锁的效率比行锁高