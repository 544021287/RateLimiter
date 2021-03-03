@author wyt

Rate limiter based on tokens.

rate_limiter is a preemptive class. If the number of tokens one thread is acquiring is big enough (no more than the capacity, and there are many threads which want to acquire the tokens), it may be stuck for very long period or can't even acquire the tokens permanently.

rate_limiter_e is a first-in-first-get class. No matter how many tokens one thread wants to acquire (no more than the capacity), it would always acquire the tokens successfully (may wait for a specific peroid).

These codes can be compiled both in windows and linux.
If you use windows, visual studio is recommanded.

If you use linux, you can type the following command to compile the code:

    cd rate_limiter
    make
    ./test
    
    (or)
    
    cd rate_limiter_e
    make
    ./test
    
If you have any issue, please feel free to contact me. My email : 544021287@qq.com, thank you.
