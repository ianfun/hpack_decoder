#include "huffman_table.inc"
#include <stdio.h>
#include <string>
static void hpack_huffman_decode(const unsigned char *buffer, size_t len, std::string& out)
{
	const unsigned char *ptr = buffer, *endptr = buffer + len;
	unsigned bitsOffset = 0, oldBitsOffset=0;
	const unsigned maxBits = len * 8;
	unsigned char c;
	if (!len) return;
	for (;;) {
		for (unsigned short *table = huffman_table;;) {
			ptr = buffer + (bitsOffset / 8);
			c = *ptr;
			unsigned offset = bitsOffset % 8;
			if (offset) {
				ptr++;
				c <<= offset;
				c |= *ptr >> (8 - offset);
			}
			unsigned short x = table[c];
			if (x <= 256) {
				oldBitsOffset += bits_consumed_table[x];
				if (oldBitsOffset > maxBits) return;
				bitsOffset = oldBitsOffset;
				out += (char)x;
				break;
			}
			table = (unsigned short*)((char*)huffman_table + x);
			bitsOffset += 8;
			if (bitsOffset > maxBits) return;
		}
	}
}
/*
void test1() {
	const unsigned char source[] = { 
		 0x9d,0x29,0xad,0x17,0x18,0x63,0xc7,0x8f,0x0b,0x97,0xc8,0xe9,0xae,0x82,0xae,0x43,0xd3
	};
	std::string out;
	decode(source, sizeof source, out);
	printf("out = %s\n", out.c_str());
}
void test2() {
	const unsigned char source[] = { 
		 0x94,0xe7,0x82,0x1d,0xd7,0xf2,0xe6,0xc7,0xb3,0x35,0xdf,0xdf,0xcd,0x5b,0x39,0x60,0xd5,0xaf,0x27,0x08,0x7f,0x36,0x72,0xc1,0xab,0x27,0x0f,0xb5,0x29,0x1f,0x95,0x87,0x31,0x60,0x65,0xc0,0x03,0xed,0x4e,0xe5,0xb1,0x06,0x3d,0x50,0x07
	};
	std::string out;
	decode(source, sizeof source, out);
	printf("out = %s\n", out.c_str());
}
void test3() {
	const unsigned char source[] = { 
		 0xd0,0x7a,0xbe,0x94,0x10,0x54,0xd4,0x44,0xa8,0x20,0x05,0x95,0x04,0x0b,0x81,0x66,0xe0,0x84,0xa6,0x2d,0x1b,0xff
	};
	std::string out;
	decode(source, sizeof source, out);
	printf("out = %s\n", out.c_str());
}
void test4() {
	const unsigned char source[] = { 
		 0xfe,0x3f,0x9f,0xfa,0xff,0xcf,0xff,0xcf,0xfd,0x7f,0xff,0xf,0xeb,0xfb,0xff,0xdf,0xff,0x3f
	};
	std::string out;
	decode(source, sizeof source, out);
	printf("out = %s\n", out.c_str());
}
int main() {
	// 7KB of memory is used for huffman decoding
	printf("huffman_table size: %lu\n", sizeof(huffman_table));
	printf("bits_consumed_table size: %lu\n", sizeof(bits_consumed_table));
	test1();
	test2();
	test3();
	test4();
}
*/
