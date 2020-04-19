// ReencodeDate.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <ctime>
#include <vector>
#include <string>
#include <map>
#include <string.h>
#include <iostream>
#include <sstream>
#include <shlwapi.h>
#include <gdiplus.h>

using namespace std;
using namespace Gdiplus;

#define BOOST_FILESYSTEM_VERSION 3
#define BOOST_FILESYSTEM_NO_DEPRECATED 
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>

namespace fs = ::boost::filesystem;
namespace po = ::boost::program_options;

typedef enum {YEAR, MONTH, DAY, HOUR, MIN, SEC} FldType;

bool bTest{ false };
bool bRecursive{ false };
bool bUseFileLastWriteTime{ false };
string Dir{"."};
string InputFormat{ "" };
string OutputFormat{ "" };

class CGdiPlus
{
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
public:
	CGdiPlus() {
		// Initialize GDI+
		GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	};
	~CGdiPlus() {
		GdiplusShutdown(gdiplusToken);
	};
};

CGdiPlus g_GdiPlus;

struct FileData {
	string oldFullPath;
	string oldFileName;
	string Ext;
	string newFileName;
	FileData() {
		oldFullPath = "";
		oldFileName = "";
		Ext = "";
		newFileName = "";
	}
};

struct Field {
	string first;
	string second;
	int Len;
	FldType Type;
};

Field Placeholders[] = {
{ "%Y", "<Y>", 4, YEAR },
{ "%y", "<y>", 2, YEAR},
{ "%M", "<M>", 2, MONTH},
{ "%D", "<D>", 2, DAY},
{ "%d", "<D>", 2, DAY},
{ "%H", "<H>", 2, HOUR},
{ "%h", "<H>", 2, HOUR},
{ "%m", "<m>", 2, MIN},
{ "%S", "<S>", 2, SEC},
{ "%s", "<S>", 2, SEC},
};

bool ReadMetadata(const WCHAR * fileName, tm& t)
{
	UINT  size = 0;
	UINT  count = 0;
	bool bResult{ false };
	memset(&t, 0, sizeof(t));

	std::unique_ptr<Bitmap> bitmap(Bitmap::FromFile(fileName, FALSE));
	bitmap->GetPropertySize(&size, &count);

	class CAutoPropBuf {
		void* buf{ nullptr };
	public:
		CAutoPropBuf(unsigned size) { buf = malloc(size); }
		operator PropertyItem*() { return (PropertyItem*)buf; }
		~CAutoPropBuf() {if (buf) { free(buf); buf = nullptr; } }
	};
	CAutoPropBuf AutoPropBuf(size);

	PropertyItem* pPropBuffer = AutoPropBuf;
	bitmap->GetAllPropertyItems(size, count, pPropBuffer);

	for (UINT j = 0; j < count; ++j)
	{
		if (PropertyTagExifDTOrig /*0x9003*/ == pPropBuffer[j].id)
		{
			sscanf((const char *)(pPropBuffer[j].value),
				"%04d:%02d:%02d %02d:%02d:%02d",
				&t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec);
			--t.tm_mon; // tm_mon is 0..11
			bResult = true;
			break;
		}
	}
	return bResult;
}

bool ReadDateFromEXIF(string FileName, tm& t)
{
	const size_t cSize = FileName.length() + 1;
	wchar_t* wc = new wchar_t[cSize];
	mbstowcs(wc, FileName.c_str(), cSize);
	bool bResult = ReadMetadata(wc, t);
	delete[] wc;
	return bResult;
}

void DecodeDate(const string Src, const string& OrigFmt, tm& t)
{
	unsigned DateEl[12];
	map<unsigned, unsigned> DateMap;
	string ReadFormat{ "" };
	vector<int> fieldFormat{};
	char* ptr{nullptr};
	int fieldLen{0};

	memset(&t, 0, sizeof(t));

	string FmtString = OrigFmt;
	for (auto& p : Placeholders)
	{
		boost::replace_all(FmtString, p.first, string{"%"}+p.second);
	}
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("%");
	tokenizer tokens(FmtString, sep);

	auto processField = [&](unsigned fldSize, FldType fldType) {
		char Fmt[16];
		sprintf(Fmt, "%%0%uu", fldSize);
		ReadFormat += Fmt;
		fieldFormat.push_back(fldType);
		if (fieldLen > 3)
			ReadFormat += (ptr + 3);
	};
	for (tokenizer::iterator field = tokens.begin(); field != tokens.end(); ++field)
	{
		ptr = const_cast<char*>(field->c_str());
		fieldLen = field->length();
		bool bFound = false;
		for (auto& p : Placeholders) {
			if (!strncmp(ptr, p.second.c_str(), 3)) {
				processField(p.Len, p.Type);
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			ReadFormat += *field;
		}
	}

	int totalFields = sscanf(Src.c_str(), ReadFormat.c_str(), DateEl, DateEl + 1, DateEl + 2, DateEl + 3, DateEl + 4, DateEl + 5);
	for (unsigned i = 0; i < totalFields; i++) {
		DateMap[fieldFormat[i]] = DateEl[i];
	}
	if (DateMap[YEAR] < 100) {
		if (DateMap[YEAR] < 50)
			DateMap[YEAR] += 2000;
		else
		{
			DateMap[YEAR] += 1900;
		}
	}
	t.tm_year = DateMap[YEAR];
	t.tm_mon = DateMap[MONTH]-1;
	t.tm_mday = DateMap[DAY];
	t.tm_hour = DateMap[HOUR];
	t.tm_min = DateMap[MIN];
	t.tm_sec = DateMap[SEC];
}

string EncodeDate(const string& OrigFmt, const tm& t)
{
	string OutString = OrigFmt;

	map<unsigned, unsigned> DateMap;

	DateMap[YEAR]  = t.tm_year;
	DateMap[MONTH] = t.tm_mon + 1;
	DateMap[DAY]   = t.tm_mday;
	DateMap[HOUR]  = t.tm_hour;
	DateMap[MIN]   = t.tm_min;
	DateMap[SEC]   = t.tm_sec;

	for (auto& p: Placeholders) {
		char buf[5];
		sprintf(buf, "%0*u", p.Len, p.Len > 2 ? DateMap[p.Type] : DateMap[p.Type] % 100);
		boost::replace_all(OutString, p.first, buf);
	}
	return OutString;
}

template < typename IteratorT > void ListFiles (const fs::path& root, const string& pattern, vector<FileData>& ret)
{
	IteratorT it(root);
	IteratorT endit;
	while (it != endit)
	{
		if (fs::is_regular_file(*it) &&
			PathMatchSpec(it->path().filename().string().c_str(), pattern.c_str())) {
			FileData file;
			file.oldFullPath = it->path().string();
			file.oldFileName = it->path().filename().string();
			file.Ext = it->path().extension().string();
			ret.push_back(file);
		}
		++it;
	}
}

void ListAllFiles(const fs::path& root, const string& pattern, vector<FileData>& ret)
{
	if (!fs::exists(root)) {
		string errMsg = "Directory ";
		errMsg += root.string();
		errMsg += " does not exist";
		throw std::runtime_error(errMsg);
	} 
	if (!fs::is_directory(root)) {
		string errMsg = root.string();
		errMsg += " is not a directory";
		throw std::runtime_error(errMsg);
	}
	if (bRecursive)
		ListFiles<fs::recursive_directory_iterator>(root, pattern, ret);
	else
		ListFiles<fs::directory_iterator>(root, pattern, ret);
	if (!ret.size())
	{
		throw std::runtime_error("No files matching to input format pattern found.");
	}
}

void RenameFiles(vector<FileData>& files)
{
	for (const auto& file : files) {
		if (file.newFileName.length() && file.oldFileName != file.newFileName ) {
			auto newFullPathTemplate = file.oldFullPath;
			boost::replace_last(newFullPathTemplate, file.oldFileName, "???");
			auto newFileName = file.newFileName;
			auto newFullPath = newFullPathTemplate;
			boost::replace_last(newFullPath, "???", newFileName + file.Ext);
			boost::system::error_code ec;
			while (fs::exists(newFullPath)) {
				newFileName += ("_" + std::to_string(GetTickCount()));
				newFullPath = newFullPathTemplate;
				boost::replace_last(newFullPath, "???", newFileName + file.Ext);
			}
			std::cout << file.oldFileName << " --> " << newFileName << file.Ext << std::endl;

			if (!bTest) {
				fs::rename(file.oldFullPath, newFullPath, ec);
				if (ec) {
					std::cout << ec.message() << std::endl;
				}
			}
		}
	}
}

bool ContainsPlaceholders(const string& FmtString)
{
	for (auto& place : Placeholders) {
		if (boost::find_first(FmtString, place.first))
			return true;
	}
	return false;
}

void ParseCommandLine(int argc, char* argv[])
{
	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("directory,d", po::value<string>(), "directory")
		("input,i", po::value<string>(), "input file pattern")
		("output,o", po::value<string>(), "output file pattern")
		("recursive,r", "scan directory recursively")
		("test,t", "do not rename but show new file names")
		("filetime,f","use file last write time")
		;

	po::positional_options_description p;
	p.add("directory", -1);

	po::variables_map vm;
	//	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
	po::notify(vm);

	if (vm.size() == 0 || vm.count("help")) {
		stringstream ss; 
		ss << desc << "\n";
		throw runtime_error(ss.str());
	}
	if (vm.count("recursive")) {
		bRecursive = true;
	}
	if (vm.count("test")) {
		bTest = true;
	}
	if (vm.count("filetime")) {
		bUseFileLastWriteTime = true;
	}

	if (vm.count("directory")) {
		Dir = fs::system_complete(vm["directory"].as<string>()).string();
	}
	else {
		Dir = fs::system_complete(".").string();
	}

	if (!vm.count("output")) {
		throw runtime_error("Output file format is undefined");
	}
	else {
		OutputFormat = vm["output"].as<string>();
		if (!ContainsPlaceholders(OutputFormat)) {
			throw runtime_error("Output file format does not contain date/time placeholders");
		}
	}

	if (!vm.count("input")) {
		throw runtime_error("Input file format is undefined");
	}
	else {
		InputFormat = vm["input"].as<string>();
	}
}

int _tmain(int argc, char* argv[])
{
	vector < FileData > files;

	try {
		ParseCommandLine(argc, argv);

		string FileMask = InputFormat;
		bool bInputContainsPlaceholders = ContainsPlaceholders(InputFormat);
		if (bInputContainsPlaceholders) {
			for (auto& p : Placeholders) {
				boost::replace_all(FileMask, p.first, string(p.Len, '?'));
			}
		}
		ListAllFiles(Dir, FileMask, files);
		
		tm tZero = { 0 };
		for (auto& file : files) {
			tm t = { 0 };
			if (bInputContainsPlaceholders) {
				DecodeDate(file.oldFileName, InputFormat, t);
			}
			else if (file.Ext == ".jpg") {
				ReadDateFromEXIF(file.oldFullPath, t);
			}
			else if (bUseFileLastWriteTime) {
				boost::system::error_code ec;
				std::time_t tt = fs::last_write_time(file.oldFullPath, ec);
				if (ec) {
					std::cout << ec.message() << std::endl;
				}
				auto ptm = gmtime(&tt);
				memcpy(&t, ptm, sizeof(tm));
				if (t.tm_year > 100)
					t.tm_year = t.tm_year % 100 + 2000;
			}
			if (!memcmp(&t, &tZero, sizeof(tm))) {
				cout << file.oldFileName << ": no time information found" << "\n";
				continue;
			}
			file.newFileName = EncodeDate(OutputFormat, t);
		}
		
		RenameFiles(files);

	}
	catch (exception& x)
	{
		cout << x.what() << "\n";
	}
	return 0;
}

