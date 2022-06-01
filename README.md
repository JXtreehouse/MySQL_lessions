<!--
 * @Author: your name
 * @Date: 2021-10-26 21:11:37
 * @LastEditTime: 2022-06-01 11:24:59
 * @LastEditors: zhaokang zhaokang1@xiaomi.com
 * @Description: In User Settings Edit
 * @FilePath: /MySQL_lessions/README.md
-->

# MySQL 学习记录

[数据类型与操作数据表](https://segmentfault.com/a/1190000010454836)
[MySQL Documentation & Example Databases](https://dev.mysql.com/doc/index-other.html)

# 常见命令

[mysql 命令行查看数据库所有用户](https://jingyan.baidu.com/article/fea4511aced59cf7ba91255e.html)
# 安装

<b>To install MySQL on CentOS 7:</b>
- Centos: [How To Install MySQL on CentOS 7](https://www.digitalocean.com/community/tutorials/how-to-install-mysql-on-centos-7)


<b>To install MySQL on Ubuntu 16.04:</b>
```
$ sudo apt-get install mysql-server
$ sudo apt-get isntall mysql-client
$ mysql -u root -p
mysql> create database visualization;
mysql> grant all on visualization.* to datav@localhost identified by 'datav';

```

- [How To Create a New User and Grant Permissions in MySQL](https://www.digitalocean.com/community/tutorials/how-to-create-a-new-user-and-grant-permissions-in-mysql)
 
 # MySQL学习记录
- MySQL数据类型和存储引擎
 - MySQL数据表的基本操作
   - 创建数据表（CREATE TABLE)
   - 修改数据表(ALTER TABLE)
   - 修改/删除字段
   - 删除数据表(DORP TABLE)
   - 删除被其它表关联的主表
   - 查看表结构命令
   - 数据表添加字段（三种方式）
   - SQL语句对应的文件操作

- MySQL约束、函数和运算符
 - [如何查看MySQL的版本？](https://zhuanlan.zhihu.com/p/522591626)
 - [查看mysql权限](https://zhuanlan.zhihu.com/p/522621034)
- [数据类型与操作数据表](https://segmentfault.com/a/1190000010454836)
- [MySQL Documentation & Example Databases](https://dev.mysql.com/doc/index-other.html)
- [MySQL数据类型: UNSIGNED](https://zhuanlan.zhihu.com/p/426230888)


# 最佳实践
[MySQL best practices to follow as a developer](https://wpdatatables.com/mysql-best-practices/)

[MySQL indexes - what are the best practices?](https://stackoverflow.com/questions/3049283/mysql-indexes-what-are-the-best-practices)



