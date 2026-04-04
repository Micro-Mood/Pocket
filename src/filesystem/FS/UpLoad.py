import os
import serial.tools.list_ports
os.system("cls")

spiffs="fs.bin"
print("0x130000: ",spiffs)

command1= "esptool --chip esp32c3 --port "
command2= " --baud 921600  --before default_reset --after hard_reset write_flash  -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x100000 "



while(1):
    ports_list = list(serial.tools.list_ports.comports())
    if len(ports_list) <= 0:
        print("无串口设备,请插入设备")
        ports_list = list(serial.tools.list_ports.comports())
        os.system("pause")
    else:
        print("\033[91m=================================================\033[0m")
        print("\033[91m============== 可用的串口设备如下 ===============\033[0m")
        print("\033[91m=================================================\033[0m")
        for comport in ports_list:
            print(list(comport)[0], "[",list(comport)[1],"]")
        os.system("pause")
        print("\033[92m=================================================\033[0m")
        print("\033[92m============== 正在逐个烧录程序中 ===============\033[0m")
        print("\033[92m=================================================\033[0m")
    for comport in ports_list:
        if list(comport)[0] != "COM1":  
            print("\033[94m---------------------",list(comport)[0],"----------------------\033[0m")              
            os.system(command1+list(comport)[0]+command2+spiffs+" --force")
        else:
            print("---------------识别到COM1,未烧录-----------------\033[0m")
    print("\033[92m=================================================\033[0m")
    print("\033[92m============== 本次循环烧录已结束 ===============\033[0m")
    print("\033[92m=================================================\033[0m")
    os.system("pause")
    os.system("cls")
    print("0x130000: ",spiffs)
 
 