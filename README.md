MorseGate
=========

Wemos D1 mini based Morse code message converter.

How to use
----------

### curl

Sending message:

```
curl -i -v 'http://192.168.96.1/msg/send?msg=sms'
```

Reading current status:

```
curl -i -v 'http://192.168.96.1/msg/status'
```

Required Libraries
------------------

### Thread

```
#include <Thread.h>
```

* https://github.com/ivanseidel/ArduinoThread

### Morse

```
#include <Morse.h>
```

* https://github.com/etherkit/MorseArduino
