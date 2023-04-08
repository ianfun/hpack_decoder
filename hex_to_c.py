x = input().replace(' ', '')
print(','.join(
   map(lambda x: '0x' + x, (x[i:i+2] for i in range(0, len(x), 2)))
)
)
