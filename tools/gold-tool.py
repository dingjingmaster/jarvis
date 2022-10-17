#!/usr/bin/env python3

import re
import os
import sys


exe = "/usr/local/jarvis/bin/gold-tool"
if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: gold-tool.py option(Au/Ag) area(USD/CNY) <.csv file>\n"
              ".csv文件格式: 日期,价格\n")
    with open(sys.argv[2]) as fr:
        for line in fr.readlines():
            line = line.strip()
            arr = line.split(',')
            if len(arr) != 2:
                print("error line:" + line)
                continue
            date = arr[0]
            price = arr[1]
            if len(date) == 8 and price.isdigit():
                os.system("%s i %s %s %s %s" % (exe, date, sys.argv[1], sys.argv[2], price))
            else:
                print("error line:" + line)
        pass

    pass

