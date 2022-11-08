<!--
 * @Author: AlexZ33 775136985@qq.com
 * @Date: 2022-11-08 11:05:25
 * @LastEditors: AlexZ33 775136985@qq.com
 * @LastEditTime: 2022-11-08 14:04:21
 * @FilePath: /MySQL_lessions/docs/MySQL分页查询.md
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
-->
#! https://zhuanlan.zhihu.com/p/581487029
# MySQL分页查询

典型的分页查询语句

```
select .. from ..  where .. order by .. limit ..
```
在中小数据量的情况下，这样的SQL就足够了，需要注意的是确保使用索引。
例如，最好在category_id，id上建立复合索引比较好
```
SELECT * FROM articles WHERE category_id = 123 ORDER BY id LIMIT 50, 10
```
随着数据量的增多，页数越来越多，查看最后几页的SQL就可能类似这样的SQL语句
```
SELECT * FROM articles WHERE category_id = 123 ORDER BY id LIMIT 1000, 10
```
所以，分页越往后，数据量越大，偏移量就可能很大，分页的速度就会变慢。这时用子查询方式来提高分页效率
```
SELECT * FROM articles WHERE  id >=  
 (SELECT id FROM articles  WHERE category_id = 123 ORDER BY id LIMIT 10000, 1) LIMIT 10 
```
**分析：为什么子查询快？**
因为子查询是在索引上完成的，而普通的查询是在文件数据文件上完成的。通常，索引文件比数据文件小得多，所以操作更有效率。
```
不带子查询的SQL
mysql> explain select * from film order by title limit 900,10\G
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

带子查询的SQL
mysql> explain select * from film where film_id>=(select film_id from film order by title limit 900,1)limit 10\G
*************************** 1. row ***************************
           id: 1
  select_type: PRIMARY
        table: film
         type: range
possible_keys: PRIMARY
          key: PRIMARY
      key_len: 2
          ref: NULL
         rows: 99
        Extra: Using where
*************************** 2. row ***************************
           id: 2
  select_type: SUBQUERY
        table: film
         type: index
possible_keys: NULL
          key: idx_title
      key_len: 767
          ref: NULL
         rows: 1000
        Extra: Using index
2 rows in set (0.00 sec)
```
