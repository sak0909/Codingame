import sys
import math

#https://www.codingame.com/training/community/the-fastest

n=int(input())
m="24:60:60"
for i in range(n):m=min(m,input())
print(m)