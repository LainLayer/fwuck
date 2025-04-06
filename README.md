# Fwuck

Small curl wrapper meant for handling URLs in shell scripts. Work in progress.

# Compile

```bash
cc -o fwuck fwuck.c
```

# Usage examples


## Get the port form a URL
```console
$ ./fwuck "http://google.com:443/" -t | grep PORT | cut -d ':' -f 2
443
```

## Assemble URL from parts
```console
$ ./fwuck -p <<EOL
SCHEME:https
HOST:google.com
EOL
https://google.com
```

## Replace the host
```console
$ ./fwuck "https://example.com/" -t | sed 's/^HOST:.*$/HOST:google.com/' | ./fwuck -p
https://google.com
```

