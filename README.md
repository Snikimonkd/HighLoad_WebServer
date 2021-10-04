# HighLoad_WebServer
Лукаш Сергей АПО-31

# Nginx
ab -n 100000 -c 8 "http://127.0.0.1/wikipedia_russia.html"\

This is ApacheBench, Version 2.3 <$Revision: 1843412 $>

Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/

Licensed to The Apache Software Foundation, http://www.apache.org/

Benchmarking 127.0.0.1 (be patient)

Completed 10000 requests

Completed 20000 requests

Completed 30000 requests

Completed 40000 requests

Completed 50000 requests

Completed 60000 requests

Completed 70000 requests

Completed 80000 requests

Completed 90000 requests

Completed 100000 requests

Finished 100000 requests


Server Software:        nginx/1.18.0

Server Hostname:        127.0.0.1

Server Port:            80

Document Path:          /wikipedia_russia.html

Document Length:        954824 bytes

Concurrency Level:      8

Time taken for tests:   19.476 seconds

Complete requests:      100000

Failed requests:        0

Total transferred:      95507100000 bytes

HTML transferred:       95482400000 bytes

Requests per second:    5134.44 [#/sec] (mean)

Time per request:       1.558 [ms] (mean)

Time per request:       0.195 [ms] (mean, across all concurrent requests)
Transfer rate:          4788818.98 [Kbytes/sec] received

Connection Times (ms)
min           |mean | [+/-sd] |       | median |  max
--------------|-----|---------|-------|--------|--------
Connect:      | 0   |  0      | 0.0   |  0     |  0
Processing:   | 1   |  2      | 0.1   |  2     |  6
Waiting:      | 0   |  0      | 0.0   |  0     |  5
Total:        | 1   |  2      | 0.1   |  2     |  6

Percentage of the requests served within a certain time (ms)

  50%      2

  66%      2

  75%      2

  80%      2

  90%      2

  95%      2

  98%      2

  99%      2

 100%      6 (longest request)
