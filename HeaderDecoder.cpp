#include <string>
#include <vector>
#include <cstdio>
#define STATIC_TABLE_SIZE 61
typedef uint8_t *PBYTE;
typedef const uint8_t *PCBYTE;
typedef uint8_t BYTE;
#include "huffman_table.inc"
static void hpack_huffman_decode(PCBYTE buffer, size_t len, std::string& out)
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
struct StringRef {
	size_t len;
	const char *str;
};
struct HeaderPair
{
	StringRef header, value;
};
struct DynamicHeaderPair {
	uint32_t headerLen, valueLen;
	char headerStart[0];
	char *getValueStart() {
		return headerStart + headerLen;
	}
};
#include "headers.inc"

struct HapckHeaderDecoderBase {
	DynamicHeaderPair *createDynamicHeaderPair(StringRef H, StringRef V) {
		size_t len = sizeof(DynamicHeaderPair) +  H.len + V.len;
		DynamicHeaderPair *p = (DynamicHeaderPair*)::malloc(len);
		memcpy(p->headerStart, H.str, p->headerLen = H.len);
		memcpy(p->getValueStart(), V.str, p->valueLen = V.len);
		return p;
	}
	StringRef decodeString(PBYTE &src, std::string &buffer) {
		bool isHuffman = *src & 128;
		auto length = decode_int<7>(src);
		if (isHuffman) {
			buffer.clear();
		  hpack_huffman_decode(src, length, buffer);
		  src += length;
		  return {buffer.length(), buffer.data()};
		}
		StringRef res = {length, (char*)src};
		src += length;
		return res;
	}
	std::string _name_cache, _value_cache;
	unsigned _SETTINGS_HEADER_TABLE_SIZE = 4096;
	unsigned dynamic_table_size = 4096;
	unsigned current_dynamic_table_size = 0;
	void evict() {
		while (current_dynamic_table_size > dynamic_table_size) {
			DynamicHeaderPair *entry = dynamic_table.front();
			current_dynamic_table_size -= 32 + entry->headerLen + entry->valueLen;
			printf("  evict: %.*s: %.*s\n", (int)entry->headerLen, entry->headerStart, (int)entry->valueLen, entry->getValueStart());
			dynamic_table.erase(dynamic_table.begin());
			::free(entry);
		}
	}
template <unsigned N>
static inline uint8_t make_fill_n() {
	return (uint8_t)~(((uint8_t)0xff >> N) << N);
}
template <unsigned N>
static uint32_t decode_int(PBYTE &buffer) {
	const uint8_t fill = make_fill_n<N>();
	uint32_t result = *buffer++ & fill;
	if (result != fill)
		return result;
	unsigned shift = 0;
	for (;;) {
		uint8_t data = *buffer++;
		result += (uint32_t)(data & (uint8_t)127) << shift;
		shift += 7;
		if (!(data & 128))
		  break;
	}
	return result;
}
	std::vector<DynamicHeaderPair*> dynamic_table;
	using on_header_callback_ty = void (*)(HapckHeaderDecoderBase *self, const HeaderPair &pair);
	on_header_callback_ty on_header;
	HapckHeaderDecoderBase(on_header_callback_ty on_header): on_header{on_header} {
	}
	~HapckHeaderDecoderBase() {
		for (const auto it: dynamic_table)
			free(it);
	}
	int decodeHeader(PBYTE data, size_t len) {
		PBYTE end = data + len;
	while (data < end) {
		uint8_t first = *data;
		if (first & 128) {
			auto Idx = decode_int<7>(data);
			puts("  == Indexed - Add ==");
			if (!Idx) return -1;
			--Idx;
			if (Idx < STATIC_TABLE_SIZE) {
				this->on_header(this, hpack_static_table[Idx]);
			} else {
				Idx -= STATIC_TABLE_SIZE;
				if (Idx > dynamic_table.size())
					return -1;
				DynamicHeaderPair *entry = dynamic_table[dynamic_table.size() - 1 - Idx];
				this->on_header(this, {
					{entry->headerLen, entry->headerStart},{entry->valueLen, entry->getValueStart()}
				});
			}
		} else if (first & 64) {
			StringRef H;
			auto Idx = decode_int<6>(data);
			puts("  == Literal indexed ==");
			if (Idx) {
				Idx--;
				if (Idx < STATIC_TABLE_SIZE)
				  H = hpack_static_table[Idx].header;
				else {
					Idx -= STATIC_TABLE_SIZE;
				    if (Idx > dynamic_table.size())
					    return -1;
				    DynamicHeaderPair *entry = dynamic_table[dynamic_table.size() - 1 - Idx];
				    H = StringRef{entry->headerLen, entry->headerStart};
				}
			} else {
				H = decodeString(data, _name_cache);
			}
			StringRef V = decodeString(data, _value_cache);
			this->on_header(this, {H, V});
			dynamic_table.push_back(
			  createDynamicHeaderPair(H, V));
			current_dynamic_table_size += 32 + H.len + V.len;
			evict();
		} else if (first & 32) {
			puts("  == Dynamic Header size update ==");
			auto newSize = decode_int<5>(data);
			if (newSize > _SETTINGS_HEADER_TABLE_SIZE) 
			  return -1;
			printf("    new size: %u\n", newSize);
			dynamic_table_size = newSize;
			evict();
		} else if (first & 16) {
			StringRef H;
			auto Idx = decode_int<4>(data);
			puts("  == Literal never indexed ==");
			if (Idx) {
				Idx--;
				if (Idx < STATIC_TABLE_SIZE)
				  H = hpack_static_table[Idx].header;
				else {
					Idx -= STATIC_TABLE_SIZE;
				    if (Idx > dynamic_table.size())
					    return -1;
				    DynamicHeaderPair *entry = dynamic_table[dynamic_table.size() - 1 - Idx];
				    H = StringRef{entry->headerLen, entry->headerStart};
				}
			} else {
				H = decodeString(data, _name_cache);
			}
			StringRef V = decodeString(data, _value_cache);
			this->on_header(this, {H, V});
		} else {
			StringRef H;
			auto Idx = decode_int<4>(data);
			puts("  == Literal not indexed ==");
			if (Idx) {
				Idx--;
				if (Idx < STATIC_TABLE_SIZE)
				  H = hpack_static_table[Idx].header;
				else {
					Idx -= STATIC_TABLE_SIZE;
				    if (Idx > dynamic_table.size())
					    return -1;
				    DynamicHeaderPair *entry = dynamic_table[dynamic_table.size() - 1 - Idx];
				    H = StringRef{entry->headerLen, entry->headerStart};
				}
			} else {
				H = decodeString(data, _name_cache);
			}
			StringRef V = decodeString(data, _value_cache);
			this->on_header(this, {H, V});
		}
	}
		return 0;
	}
};
struct MyCustomHpackDecoder: public HapckHeaderDecoderBase 
{
	MyCustomHpackDecoder(): HapckHeaderDecoderBase(&MyCustomHpackDecoder::on_header) {}
	static void on_header(HapckHeaderDecoderBase *self, const HeaderPair &pair) {
		HapckHeaderDecoderBase *subThis = static_cast<MyCustomHpackDecoder*>(self);
		(void)subThis;
		printf("    %.*s: %.*s\n", (int)pair.header.len, pair.header.str, (int)pair.value.len, pair.value.str);
	}
};
static void req_without_huffman() {
	puts("# Request Examples without Huffman Coding");
	MyCustomHpackDecoder d;
	BYTE buffer1[] = {0x82,0x86,0x84,0x41,0x0f,0x77,0x77,0x77,0x2e,0x65,0x78,0x61,0x6d,0x70,0x6c,0x65,0x2e,0x63,0x6f,0x6d};
	BYTE buffer2[] = {0x82,0x86,0x84,0xbe,0x58,0x08,0x6e,0x6f,0x2d,0x63,0x61,0x63,0x68,0x65};
	BYTE buffer3[] = {0x82,0x87,0x85,0xbf,0x40,0x0a,0x63,0x75,0x73,0x74,0x6f,0x6d,0x2d,0x6b,0x65,0x79,0x0c,0x63,0x75,0x73,0x74,0x6f,0x6d,0x2d,0x76,0x61,0x6c,0x75,0x65};
	puts("## First Request");
	d.decodeHeader(buffer1, sizeof buffer1);
	puts("## Second Request");
	d.decodeHeader(buffer2, sizeof buffer2);
	puts("## Third Request");
	d.decodeHeader(buffer3, sizeof buffer3);
}
static void req_with_huffman() {
	puts("# Request Examples with Huffman Coding");
	MyCustomHpackDecoder d;
	BYTE buffer1[] = {0x82,0x86,0x84,0x41,0x8c,0xf1,0xe3,0xc2,0xe5,0xf2,0x3a,0x6b,0xa0,0xab,0x90,0xf4,0xff};
	BYTE buffer2[] = {0x82,0x86,0x84,0xbe,0x58,0x86,0xa8,0xeb,0x10,0x64,0x9c,0xbf};
	BYTE buffer3[] = {0x82,0x87,0x85,0xbf,0x40,0x88,0x25,0xa8,0x49,0xe9,0x5b,0xa9,0x7d,0x7f,0x89,0x25,0xa8,0x49,0xe9,0x5b,0xb8,0xe8,0xb4,0xbf};
	puts("## First Request");
	d.decodeHeader(buffer1, sizeof buffer1);
	puts("## Second Request");
	d.decodeHeader(buffer2, sizeof buffer2);
	puts("## Third Request");
	d.decodeHeader(buffer3, sizeof buffer3);
}
static void res_without_huffman() {
	puts("# Response Examples without Huffman Coding");
	MyCustomHpackDecoder d;
	d.dynamic_table_size = 256;
	BYTE buffer1[] = {0x48,0x03,0x33,0x30,0x32,0x58,0x07,0x70,0x72,0x69,0x76,0x61,0x74,0x65,0x61,0x1d,0x4d,0x6f,0x6e,0x2c,0x20,0x32,0x31,0x20,0x4f,0x63,0x74,0x20,0x32,0x30,0x31,0x33,0x20,0x32,0x30,0x3a,0x31,0x33,0x3a,0x32,0x31,0x20,0x47,0x4d,0x54,0x6e,0x17,0x68,0x74,0x74,0x70,0x73,0x3a,0x2f,0x2f,0x77,0x77,0x77,0x2e,0x65,0x78,0x61,0x6d,0x70,0x6c,0x65,0x2e,0x63,0x6f,0x6d};
	BYTE buffer2[] = {0x48,0x03,0x33,0x30,0x37,0xc1,0xc0,0xbf};
	BYTE buffer3[] = {0x88,0xc1,0x61,0x1d,0x4d,0x6f,0x6e,0x2c,0x20,0x32,0x31,0x20,0x4f,0x63,0x74,0x20,0x32,0x30,0x31,0x33,0x20,0x32,0x30,0x3a,0x31,0x33,0x3a,0x32,0x32,0x20,0x47,0x4d,0x54,0xc0,0x5a,0x04,0x67,0x7a,0x69,0x70,0x77,0x38,0x66,0x6f,0x6f,0x3d,0x41,0x53,0x44,0x4a,0x4b,0x48,0x51,0x4b,0x42,0x5a,0x58,0x4f,0x51,0x57,0x45,0x4f,0x50,0x49,0x55,0x41,0x58,0x51,0x57,0x45,0x4f,0x49,0x55,0x3b,0x20,0x6d,0x61,0x78,0x2d,0x61,0x67,0x65,0x3d,0x33,0x36,0x30,0x30,0x3b,0x20,0x76,0x65,0x72,0x73,0x69,0x6f,0x6e,0x3d,0x31};
	puts("## First Response");
	d.decodeHeader(buffer1, sizeof buffer1);
	puts("## Second Response");
	d.decodeHeader(buffer2, sizeof buffer2);
	puts("## Third Response");
	d.decodeHeader(buffer3, sizeof buffer3);
}
static void res_with_huffman() {
	puts("# Response Examples with Huffman Coding");
	MyCustomHpackDecoder d;
	d.dynamic_table_size = 256;
	BYTE buffer1[] = {0x48,0x82,0x64,0x02,0x58,0x85,0xae,0xc3,0x77,0x1a,0x4b,0x61,0x96,0xd0,0x7a,0xbe,0x94,0x10,0x54,0xd4,0x44,0xa8,0x20,0x05,0x95,0x04,0x0b,0x81,0x66,0xe0,0x82,0xa6,0x2d,0x1b,0xff,0x6e,0x91,0x9d,0x29,0xad,0x17,0x18,0x63,0xc7,0x8f,0x0b,0x97,0xc8,0xe9,0xae,0x82,0xae,0x43,0xd3};
	BYTE buffer2[] = {0x48,0x83,0x64,0x0e,0xff,0xc1,0xc0,0xbf};
	BYTE buffer3[] = {0x88,0xc1,0x61,0x96,0xd0,0x7a,0xbe,0x94,0x10,0x54,0xd4,0x44,0xa8,0x20,0x05,0x95,0x04,0x0b,0x81,0x66,0xe0,0x84,0xa6,0x2d,0x1b,0xff,0xc0,0x5a,0x83,0x9b,0xd9,0xab,0x77,0xad,0x94,0xe7,0x82,0x1d,0xd7,0xf2,0xe6,0xc7,0xb3,0x35,0xdf,0xdf,0xcd,0x5b,0x39,0x60,0xd5,0xaf,0x27,0x08,0x7f,0x36,0x72,0xc1,0xab,0x27,0x0f,0xb5,0x29,0x1f,0x95,0x87,0x31,0x60,0x65,0xc0,0x03,0xed,0x4e,0xe5,0xb1,0x06,0x3d,0x50,0x07};
	puts("## First Response");
	d.decodeHeader(buffer1, sizeof buffer1);
	puts("## Second Response");
	d.decodeHeader(buffer2, sizeof buffer2);
	puts("## Third Response");
	d.decodeHeader(buffer3, sizeof buffer3);
}
int main() {
	const char *gap = "=========================================\n";

	puts(gap);
	req_without_huffman();
	puts(gap);
	req_with_huffman();
	puts(gap);
	res_without_huffman();
	puts(gap);
	res_with_huffman();
	puts(gap);
}
