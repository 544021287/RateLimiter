@author wyt

Rate limiter based on tokens.
rate_limiter is a preemptive class. If one thread acquire the number of tokens is big enough (no more than the capacity), it may be stuck for very long time or can't acquire the tokens permanently.
rate_limiter_e is a first in first acquire class. No matter how many one thread acquire the tokens (no more than the capacity), it would always acquire the tokens successfully (may wait for a specific time).

These codes can be compiled both in windows and linux.
If you use windows, you can use visual studio.

If you use linux, you can type the following command to compile the code:

    cd rate_limiter
    make
    ./test
    
    (or)
    
    cd rate_limiter_e
    make
    ./test
    
If you have any issue, please feel free to contact me. My email : 544021287@qq.com, thank you.
