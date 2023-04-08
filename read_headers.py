def gen_headers():
	for line in open("headers.txt"):
		x = line.split("|")[2:-1]
		if len(x) == 2:
			x[0] = x[0].replace(" ", "")
			x[1] = x[1].replace(" ", "")
			yield x

print("static const HeaderPair hpack_static_table[] = {\n  %s\n};\n" % ',\n  '.join('{{%d, "%s"}, {%d, "%s"}}' % (len(a), a, len(b), b) for a, b in gen_headers()))
