#!/usr/bin/env python3

import re
import os
import sys
import requests 
from bs4 import BeautifulSoup

baseURL = 'https://www.sge.com.cn'
requestURL = 'https://www.sge.com.cn/sjzx/mrhqsj'

header = {
    'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64; rv:106.0) Gecko/20100101 Firefox/106.0',
    'Pragma': 'no-cache',
    'Sec-Fetch-Dest': 'document',
}

if __name__ == '__main__':

    if len(sys.argv) != 2:
        print("请输入要保存的路径")
        exit(-1)

    endPage = 0
    beginPage = 0

    sess = requests.Session()

    doc = sess.get(requestURL, headers=header)
    if 200 != doc.status_code:
        print("request %s error, %d" % (requestURL, doc.status_code))
        exit(1)

    soup = BeautifulSoup(doc.text, 'xml')

    # 获取抓取范围
    pagination = soup.find('div', class_='pagination')
    for i in pagination.find('div', class_='paginationBar fl').find_all('li', class_='border_ea noLeft_border'):
        num = i.text.split(')>')
        if 2 != len(num):
            continue
        endPage = max(beginPage, int(num[1]))
    print("获取抓取范围 %d - %d" % (beginPage, endPage))

    fw = open(sys.argv[1], "w+")

    # 开始抓取
    for i in range(beginPage, endPage):
        url = requestURL + '?p=' + str(i)
        print("开始请求url: %s" % url)
        pageDoc = sess.get(url,headers=header)
        pageSoup = BeautifulSoup(pageDoc.text, 'xml')

        #print(pageSoup)

        pageItemDiv = pageSoup.find('div', class_='articleList border_ea mt30 mb30')
        if None is pageItemDiv:
            continue
        pageItem = pageItemDiv.find_all('a', class_='title fs14  color333 clear')
        for j in pageItem:
            date = j.find('span', class_='fr').text
            uri = baseURL + j['href']
            print('开始请求url: %s' % uri)
            doc = sess.get(uri,headers=header)
            page = BeautifulSoup(doc.text, 'xml')
            div = page.find('div', class_='content')
            if None is div:
                div = page.find('div', class_='content center1200 bgfff')

            if None is div:
                continue

            table = div.find('table')
            if None is table:
                continue
            tbody = table.find('tbody')
            if None is tbody:
                continue
            tr = tbody.find_all('tr')
            if None is tr:
                continue
            for k in tr:
                td = k.find_all('td')
                if len(td) < 5:
                    continue
                if ('Au99.99' == td[0].text.strip()) or ('Au9999' == td[0].text.strip()):
                    price = td[4].text.strip()
                    print('date: %s price: %s' % (date.replace('-', ''), price))
                    fw.write(date.replace('-', '') + ',' + price + '\n')
                    fw.flush()
    fw.close()
