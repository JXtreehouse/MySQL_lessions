<!--
 * @Author: AlexZ33 775136985@qq.com
 * @Date: 2022-11-08 11:05:25
 * @LastEditors: AlexZ33 775136985@qq.com
 * @LastEditTime: 2022-11-08 14:15:54
 * @FilePath: /MySQL_lessions/docs/mysqldump.md
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
-->
#! https://zhuanlan.zhihu.com/p/581491620
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