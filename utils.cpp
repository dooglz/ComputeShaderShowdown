#pragma once
#include "utils.h"
#include <sstream>
#include <numeric>
#include <vector>
#include <algorithm>
#include <sstream>

#include <iostream>



std::chrono::time_point<chronoclock> startTimer() {
	return chronoclock::now();
}
long long endtimer(std::chrono::time_point<chronoclock> tp) {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(chronoclock::now() - tp).count();
}

std::string printSome(int32_t* data, size_t length) {
	std::vector<int> a;
	a.resize(std::min(length, (size_t)10));
	std::iota(a.begin(), a.end() - 2, 0);
	*(a.end() - 2) = length / (size_t)2;
	*(a.end() - 1) = length - (size_t)1;
	std::stringstream ss;
	for (const int& i : a) {
		ss << "[" << std::to_string(i) << "] " << std::to_string(data[i]) << ", ";
	}
	return ss.str().substr(0, ss.str().length() - 2);
}

std::wstring toWide(const std::string& str) {
	std::wstring str2(str.length(), L' ');
	std::copy(str.begin(), str.end(), str2.begin());
	return str2;
}