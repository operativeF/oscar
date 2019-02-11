#!/usr/bin/python

from xmlrpclib import ServerProxy
import datetime

s = ServerProxy('http://localhost:8080')
print s.testFunc('abc')
print s.testFunc(1)
print s.testFunc(True)
print s.testFunc(0.5)

print s.testFunc(datetime.datetime.now())
print s.testFunc({'a':1, 'b':2, 'c':3})
