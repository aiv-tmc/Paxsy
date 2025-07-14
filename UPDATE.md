#Undate PXI 3.4 - PXI 3.4w1
List of changes:

1. The parsing module (parser) has been redesigned.
    
2. The definition of a special format has been changed, and it is now defined using the **'%'** symbol.
    
3. The definition of a preprocessor has been changed, and it is now denoted by the **'#'** symbol.
    
4. The specification of a number system for a specific number has been changed, and it is now specified in the following format: ```0 number_system_prefix number```, for example: ```0xFE30```
    
5. The ```syscall``` command has been returned.
    
6. Removed commands: ```elif```, ```do```, ```malloc``` and ```delete```
    
7. Added commands: ```stack```, ```push```, and ```pop```
    
8. Changed the format of the code file, now it is **'.px'** and **'.hp'**
    
9. Added a set of libraries, you can read about them at [this link](https://github.com/aiv-tmc/Paxsi/blob/main/examples/lib)
    
10. Data type **void** has been removed
    
11. Flags **-l** and **-p** have been removed

#Update PXI 3.4w1 - PXI 3.4w1f2
List of changes:
    
1. Removed commands: ```ralloc```, ```ealloc``` and ```this```

2. Added commands: ```realloc```

3. Critical errors have been fixed

