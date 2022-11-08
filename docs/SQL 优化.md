#! https://zhuanlan.zhihu.com/p/581490399
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

------

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

1. 通过`show profile for query 2;`看到执行过程中线程对的每个状态和消耗的时间。

```
show profile for query 2;

```

发现时间主要消耗在sending data上了，这个状态是访问数据返回结果。

1. 为了更仔细地观察排序结果，可以查询information_schema.profiling表，并按照时间做个DESC排序。

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

1. 在获得了最消耗时间的线程状态后，MySQL还支持进一步选择all、cpu、block、io、context、switch、page faults等明细类型查看MySQL在使用什么资源上耗费了过高的时间。

```
show profile cpu for query 2;

```

1. 对比MyISAM的count（*）

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

1. 执行想做trace的SQL语句。
2. 检查INFORMATION_SCHEMA.OPTIMIZER_TRACE就可以知道MySQL是如何执行SQL的。

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

1. Filesort的优化

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

##### 使用BIT GROUP FUNCTIONS做统计
如何使用GROUP BY语句和BIT_AND、BIT_OR函数完成统计工作。这两个函数的用途就是做数值之间的逻辑位运算，但是当它们与GROUP BY 联合使用就可以完成一些其他操作。
- 场景
超市要记录每个顾客来超市都购买了哪些商品。（面包、牛奶、饼干、啤酒）
1. 通常的处理方法是，建立购物单表，记录时间、顾客；再建立一个购物单明细，记录购买的商品。这样设计的优点是可以记录商品的详细信息（数量价格种类）。但是如果我们只需要知道顾客购买的商品的种类和总价格，那么这个数据结构就复杂。
2. 一个表实现这个功能。并且用一个字段用字符串 的方式记录顾客购买的所有商品的商品号。但是如果顾客一次购买的商品很多，需要很大的存储空间，则做统计的时候也会捉襟见肘。
3. 最好的解决办法是：类似与第二种方法，仍用一个字段表示顾客购买的商品信息，但是是数值类型而不是字符串类型，这个字段存储一个十进制的数字，当她转换成二进制时，每一位二进制数字代表一个商品该位为1，则顾客购买了该商品；否则，没有购买。例如面包牛奶面包啤酒代表4位二进制。在数据库中用BIT_OR()操作就可以知道这个用户购买过什么商品（或操作）;BIT_AND()可以知道这个用户每次来超市都买的东西（与操作）。
```
mysql> create table order_rab(id int, customer_id int, kind int);
Query OK, 0 rows affected (0.01 sec)

mysql> insert into order_rab values(1,1,5),(4,2,4);
Query OK, 2 rows affected (0.01 sec)
Records: 2  Duplicates: 0  Warnings: 0

mysql> insert into order_rab values(3,2,3),(2,1,4);
Query OK, 2 rows affected (0.00 sec)
Records: 2  Duplicates: 0  Warnings: 0

mysql> select * from order_rab;
+------+-------------+------+
| id   | customer_id | kind |
+------+-------------+------+
|    1 |           1 |    5 |
|    4 |           2 |    4 |
|    3 |           2 |    3 |
|    2 |           1 |    4 |
+------+-------------+------+
4 rows in set (0.00 sec)

mysql> select customer_id,bit_or(kind) from order_rab group by customer_id;
+-------------+--------------+
| customer_id | bit_or(kind) |
+-------------+--------------+
|           1 |            5 |
|           2 |            7 |
+-------------+--------------+
2 rows in set (0.00 sec)

mysql> select customer_id,bit_and(kind) from order_rab group by customer_id;
+-------------+---------------+
| customer_id | bit_and(kind) |
+-------------+---------------+
|           1 |             4 |
|           2 |             0 |
+-------------+---------------+
2 rows in set (0.00 sec)
```
##### 数据库名、表名大小写问题
UNIX对大小写敏感，windows对大小写不敏感。所以在两种系统中，会因为名字的问题产生冲突。最好采用一致的转换，例如总是用小写创建并引用数据库名和表名。
使用`lower_case_tables_name`来选择如何在硬盘上保存和使用表名、数据库名。

| 值   | 含义                                                         |
| ---- | ------------------------------------------------------------ |
| 0    | 对大小写敏感，怎么创建怎么保存。（UNIX默认值）               |
| 1    | 表名在硬盘上用小写保存，名称对大小写敏感。（Windows默认值）  |
| 2    | 怎么创建怎么保存在硬盘上，但是MySQL将其转换成小写以便查询使用。在对大小写不敏感的系统上使用。 |

只在一个平台上使用MySQL是不用设置这个变量的。

##### 使用外键需要注意的问题
```
mysql> create table user2(id int, bookname varchar(20),userid int, primary key(id)) engine=innodb;
Query OK, 0 rows affected (0.01 sec)

mysql> create table book2(id int, bookname varchar(10),userid int, primary key(id),constraint fk_user_id foreign key(userid) references user2(id))engine=innodb;
Query OK, 0 rows affected (0.01 sec)

mysql> inser into book2 values(1,"book",1);
ERROR 1452 (23000): Cannot add or update a child row: a foreign key constraint fails (`sakila`.`book2`, CONSTRAINT `fk_user_id` FOREIGN KEY (`userid`) REFERENCES `user2` (`id`))

mysql> show create table book2;
| Table | Create Table
| book2 | CREATE TABLE `book2` (
  `id` int(11) NOT NULL DEFAULT '0',
  `bookname` varchar(10) DEFAULT NULL,
  `userid` int(11) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `fk_user_id` (`userid`),
  CONSTRAINT `fk_user_id` FOREIGN KEY (`userid`) REFERENCES `user2` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 |
```

