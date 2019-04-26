1. CRC and NVM done > for app and flags

rectified > 
1. NVMCTRL flash size issue 64kB > 256 kB 
2. All addresses typecast changed to 32 bit
3. OTAFU file download rectified ->config for http > change port number and tls.

Slight tweaking for bootloader logic to accomodate OTAFU check and test

Issues-
1. Jump to App fail due to more peripherals involved for deinitializations (issue with optimization i guess)


To do 
1. OTAFU logic with mqtt button 
2. New App code project using fresh A7 Starter code ( findings from bootloader code can help with jump code(app > bootloader))  


OTAFU logic

All the below happens in the app
1. If SW0 button pressed (or) OTAFU button on in cloud app

2.a SW0 Button > hardware interrrupt
2.b OTAFU Button > poll in the app's while(1)  

3. Download version.txt and compare with version flag (NVM_VER)
4. If version different > 
	a. delete all previous sd card data (firmware,version,crc_new)
	b. download the new firware.bin file 
	c. download crc_otafu.txt
	d. download version.txt
	e. compare the crc of the file downloaded and crc_otafu.txt
	f. if true > write the otafu flag in nvm to be 1.

5. Jump to bootloader
6. Check the otafu flag in boot check > nvm flag written to 0xFF. if true > write firmware on nvm and update version on nvm 
7. jump to app   