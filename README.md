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
 
 # MySQL学习记录
- [数据类型与操作数据表](https://segmentfault.com/a/1190000010454836)
- [MySQL Documentation & Example Databases](https://dev.mysql.com/doc/index-other.html)
