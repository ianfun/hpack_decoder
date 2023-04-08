lines = []
g_i = 0
g_is = []
for s in open("hufman.txt").readlines():
	if s != '\n':
		if s.find('|') != -1:
			i = s.rfind(')')
			if i != -1:
				s = s[i+1::]
			i = s.rfind('[')
			if i != -1:
				s = s[:i]
				i = s.find(' ')
				if i != -1:
					s = s.split()[0].replace('|', '')
					lines.append(s)

class Tree:
	def __init__(self, c=None):
		if c is None:
			global g_i
			g_is.append(self)
			self.i = g_i
			g_i += 1
			self.childs = [None] * 256
		else:
			self.c = c
	def getTree(self, s):
		assert len(s)==8
		val = int(s, 2)
		x = self.childs[val]
		if x is None:
			x = Tree()
			self.childs[val] = x
		return x
root = Tree()
def build():
	for i, s in enumerate(lines):
		t = root
		while len(s) > 8:
			t = t.getTree(s[0:8])
			s = s[8:]
		tr = Tree(i)
		z =   int(s.ljust(8, '0'), 2)
		end = int(s.ljust(8, '1'), 2)
		while True:
			t.childs[z] = tr
			if z >= end:
				break
			z += 1
build()
"""
from sys import stdout
def test(code):
	binStr = ""
	for v in code:
		binStr += bin(v)[2:].rjust(8, '0')
	r = 0
	oldr = 0
	while r < len(binStr):
		t = root
		while r < len(binStr):
			x = binStr[r:r + 8].ljust(8, '1')
			j = int(x, 2)
			t = t.childs[j]
			if hasattr(t, 'c'):
				c = t.c
				l = len(lines[c])
				stdout.write(chr(c))
				oldr += l
				r = oldr
				break
			r += 8
	stdout.write('\n')
test([0xf1,0xe3,0xc2,0xe5,0xf2,0x3a,0x6b,0xa0,0xab,0x90,0xf4,0xff])
test([0x9d,0x29,0xad,0x17,0x18,0x63,0xc7,0x8f,0x0b,0x97,0xc8,0xe9,0xae,0x82,0xae,0x43,0xd3])
test([0xd0,0x7a,0xbe,0x94,0x10,0x54,0xd4,0x44,0xa8,0x20,0x05,0x95,0x04,0x0b,0x81,0x66,0xe0,0x84,0xa6,0x2d,0x1b,0xff
])
test([0xfe,0x3f,0x9f,0xfa,0xff,0xcf,0xff,0xcf,0xfd,0x7f,0xff,0xf,0xeb,0xfb,0xff,0xdf,0xff,0x3f])
exit(0)
"""
with open("huffman_table.inc", "w") as fd:
  f = fd.write
  f("static unsigned char bits_consumed_table[] = {\n%s\n};\n" % ','.join(map(str, map(len, lines))))
  f(
"static unsigned short huffman_table[] = {")
  def genEntry(x):
      if hasattr(x, 'c'):
         return str(x.c)
      return str(x.i * 256 * 2)
  f(
    ','.join(','.join(genEntry(x) for x in g_is[y].childs) for y in range(g_i))
   )
  f('};\n')
