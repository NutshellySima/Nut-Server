#pragma once
#include "stdafx.h"

using namespace std;

template <typename T>
string compressContent(const T &Content)
{
	z_stream zs;
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;
	string Original;
	for (auto &I : Content)
		Original.push_back(I);
	zs.next_in = reinterpret_cast<unsigned char*>(Original.data());
	zs.avail_in = Original.size();
	auto res = deflateInit2(&zs, 9, Z_DEFLATED, 31, 9, Z_DEFAULT_STRATEGY);
	if (res != Z_OK)
	{
		cout << res << endl;
		cerr << "Deflate Init!\n";
		abort();
	}
	int ret;
	char outbuffer[32768];
	std::string outstring;
	// retrieve the compressed bytes blockwise
	do
	{
		zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
		zs.avail_out = sizeof(outbuffer);
		ret = deflate(&zs, Z_FINISH);
		if (outstring.size() < zs.total_out)
		{
			// append the block to the output string
			outstring.append(outbuffer,
							 zs.total_out - outstring.size());
		}
	} while (ret == Z_OK);
	deflateEnd(&zs);
	if (ret != Z_STREAM_END)
	{ // an error occurred that was not EOF
		std::ostringstream oss;
		oss << "Exception during zlib compression: (" << ret << ") " << zs.msg;
		throw(std::runtime_error(oss.str()));
	}
	string Message;
	for (auto I : outstring)
		Message.push_back(I);
	return Message;
}