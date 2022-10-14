#!/usr/bin/env python3

import re
import requests
from bs4 import BeautifulSoup

baseURL = 'https://www.sge.com.cn'
requestURL = 'https://www.sge.com.cn/sjzx/mrhqsj'

if __name__ == '__main__':

    endPage = 0
    beginPage = 0

    doc = requests.get(requestURL)
    if 200 != doc.status_code:
        print("request %s error" % requestURL)
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

    fw = open('/data/au.csv', 'w+')

    # 开始抓取
    for i in range(beginPage, endPage):
        url = requestURL + '?p=' + str(i)
        print("开始请求url: %s" % url)
        pageDoc = requests.get(url)
        pageSoup = BeautifulSoup(pageDoc.text, 'xml')

        pageItemDiv = pageSoup.find('div', class_='articleList border_ea mt30 mb30')
        pageItem = pageItemDiv.find_all('a', class_='title fs14  color333 clear')
        for j in pageItem:
            date = j.find('span', class_='fr').text
            uri = baseURL + j['href']
            print('开始请求url: %s' % uri)
            doc = requests.get(uri)
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
