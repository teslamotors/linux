import subprocess
import os,re
import time
from datetime import datetime


cycle1 =[]
str1 = ""
cycle=0;
flag = 0;
COUNT = 13;

date = datetime.now()
fp = open('res.txt','w')

#open file for reading suspend-resume details
with open("ftrace.txt", "r") as file:
    for line in file:
        if line.startswith('#'):
            continue

        if "suspend_enter[3] begin" in line:
             cycle = cycle + 1
             fp.write("*******************Cycle " +str(cycle) +" starts******************\n")
             cycle_start_time = float(line.split(" ...1")[1].replace(" ","").split(":")[0])
        if "suspend_enter[3] end" in line:
             suspend_enter_end_time = float(line.split(" ...1")[1].replace(" ","").split(":")[0])
             fp.write("     Suspend enter took       "+str((suspend_enter_end_time-cycle_start_time)*1000000) +" us\n")
             str1=str1 + "SUSPEND_ENTER took " + str(suspend_enter_end_time-cycle_start_time) +" secs\n"

        if "suspend_resume: dpm" in line:
            if "begin" in line:
                try:
                    statestart= float(line.split(" ...1")[1].replace(" ","").split(":")[0])
                    statestartname=line.split("suspend_resume: ")[1].split('[')[0]
                except IndexError:
                    statestart= float(line.split("N.1")[1].replace(" ","").split(":")[0])
                    statestartname=line.split("suspend_resume: ")[1].split('[')[0]

            elif "end" in line:
                try :
                     stateend = float(line.split(" ...1")[1].replace(" ","").split(":")[0])
                     stateendname=line.split("suspend_resume: ")[1].split('[')[0]
                except IndexError:
                     stateend = float(line.split("N.1")[1].replace(" ","").split(":")[0])
                     stateendname=line.split("suspend_resume: ")[1].split('[')[0]
                if stateendname == statestartname:
                   str1 = str1 + stateendname.upper() + " took "+str(stateend - statestart) + " sec \n";
            try:
                fp.write(line.split("..1")[1].upper() + " \n")
            except IndexError:
                fp.write(line.split("N.1")[1].upper() + " \n")
        #elif "suspend_resume:" in line:
        #    try:
        #        fp.write(line.split("..1")[1] + "\n")
        #    except IndexError:
        #        fp.write(line.split("N.1")[1].upper() + " \n")

        if "device_pm_callback_start:" in line:
            try:
                 dpm_start = float(line.split(" ...1")[1].replace(" ","").split(":")[0])
            except IndexError:
                 dpm_start = float(line.split(" .N.1")[1].replace(" ","").split(":")[0])
            driver_sname = line.split("device_pm_callback_start: ")[1].split(",")[0]
        if "device_pm_callback_end:" in line:
            try:
                 dpm_end = float(line.split(" ...1")[1].replace(" ","").split(":")[0])
            except:
                 dpm_end = float(line.split(" .N.1")[1].replace(" ","").split(":")[0])
            driver_ename = line.split("device_pm_callback_end: ")[1].split(",")[0]
            if driver_ename == driver_sname:
                fp.write("      " +driver_ename.ljust(50) + " ------>       " + str(int((dpm_end - dpm_start)*1000000)) + " us \n")

        if "thaw_processes[0] end" in line:
            try:
                  cycle_end_time = float(line.split(" ...1")[1].replace(" ","").split(":")[0])
            except IndexError:
                  cycle_end_time = float(line.split(" .N.1")[1].replace(" ","").split(":")[0])
            fp.write("********************* Cycle " +str(cycle) +" ends***************\n")
            fp.write("Cycle " + str(cycle) + " took " + str(cycle_end_time - cycle_start_time) + " sec \n")
            fp.write(str1+"\n\n\n\n")
            str1 = ""
        if "CPU_OFF"  in line:
            if "begin" in line:
                 try:
                     cpuoffstart=  float(line.split("..1")[1].replace(" ","").split(":")[0])
                 except IndexError:
                     cpuoffstart =float(line.split("N.1")[1].replace(" ","").split(":")[0])
            elif "end" in line:
                 try:
                    cpuoffend= float(line.split("..1")[1].replace(" ","").split(":")[0])
                 except IndexError:
                     cpuoffend =float(line.split("N.1")[1].replace(" ","").split(":")[0])
                 str1 = str1 + "CPUOFF took " +str(cpuoffend-cpuoffstart) +" secs\n"

        if "machine_suspend"  in line:
            if "begin" in line:
                 try:
                     mcsusstart=  float(line.split("..1")[1].replace(" ","").split(":")[0])
                 except IndexError:
                     mcsusstart =float(line.split("N.1")[1].replace(" ","").split(":")[0])
            elif "end" in line:
                 try:
                    mcsusend= float(line.split("..1")[1].replace(" ","").split(":")[0])
                 except IndexError:
                     mcsusend =float(line.split("N.1")[1].replace(" ","").split(":")[0])
                 str1 = str1 + "MACHINE SUSPEND  took " +str(mcsusend-mcsusstart) +" secs\n"

        if "CPU_ON" in line:
            if "begin" in line:
                 try:
                     cpuonstart=  float(line.split("..1")[1].replace(" ","").split(":")[0])
                 except IndexError:
                     cpuonstart =float(line.split("N.1")[1].replace(" ","").split(":")[0])
            elif "end" in line:
                 try:
                    cpuonend= float(line.split("..1")[1].replace(" ","").split(":")[0])
                 except IndexError:
                     cpuonend =float(line.split("N.1")[1].replace(" ","").split(":")[0])
                 str1 = str1 + "CPU_ON took " +str(cpuonend-cpuonstart) +" secs\n"

        if "syscore_resume"  in line:
            if "begin" in line:
                 try:
                     sysresstart=  float(line.split("..1")[1].replace(" ","").split(":")[0])
                 except IndexError:
                     sysresstart =float(line.split("N.1")[1].replace(" ","").split(":")[0])
            elif "end" in line:
                 try:
                    sysresend= float(line.split("..1")[1].replace(" ","").split(":")[0])
                 except IndexError:
                     sysresend =float(line.split("N.1")[1].replace(" ","").split(":")[0])
                 str1 = str1 + "SYSTEM CORE RESUME took " +str(sysresend-sysresstart) +" secs\n"
        if "syscore_suspend"  in line:
            if "begin" in line:
                 try:
                     syssusstart=  float(line.split("..1")[1].replace(" ","").split(":")[0])
                 except IndexError:
                     syssusstart =float(line.split("N.1")[1].replace(" ","").split(":")[0])
            elif "end" in line:
                 try:
                    syssusend= float(line.split("..1")[1].replace(" ","").split(":")[0])
                 except IndexError:
                     syssusend =float(line.split("N.1")[1].replace(" ","").split(":")[0])
                 str1 = str1 + "SYSTEM CORE SUSPEND took " +str(syssusend-syssusstart) +" secs\n"
fp.close();

lines=0;

with open('res.txt', "r") as file:
     length = 0
     for line in file:
          lines +=1;
          matchObj = re.match( r'Cycle (.*?) took (.*?) sec.*', line, re.M|re.I)
          if matchObj:
              digit = re.findall(r'\d+', line)
              flag = 1;
          #matchObj1 = re.match( r'\n', line, re.M|re.I)
          if line == "\n" and flag:
              flag = 0;
              if length >= COUNT:
                   cycle1.append(int(digit[0]))
              length = 0;

          if flag:
               matchObj1 = re.match( r'(.*)\n', line, re.M|re.I)
               if matchObj1:
                   length = length + 1;

fp = open('/var/www/suspend-resume/result' + str(date) + '.txt','w')
for i in range(len(cycle1)):
    with open('res.txt', "r") as file:
        for line in file:
             matchObj1 = re.match( r'(.*)Cycle ' + str(cycle1[i]) + ' starts(.*)', line, re.M|re.I)
             if matchObj1:
                flag = 1;
             matchObj = re.match( r'(.*)Cycle ' + str(cycle1[i] + 1) + ' starts(.*)',  line, re.M|re.I)
             if matchObj:
                 flag = 0;
             if flag:
                 fp.write(line);
