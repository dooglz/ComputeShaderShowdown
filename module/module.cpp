#pragma once
#include "utils.h"
#include <iostream>
#include "module.h"



#ifdef _WINDOWS
#define _AMD64_
#include <windows.h>
#include <Libloaderapi.h>
#include <Errhandlingapi.h>
#define moduleLib HMODULE
#define getLib(x) LoadLibrary(x.data())
#define freeLib(x) FreeLibrary(x)
#define getFunc(x,m) GetProcAddress(m,x)
#else
#define moduleLib void*
#define getModudule(x) nullptr
#define getFunc(x,m) nullptr
#define freeLib(x) NULL
#endif _WINDOWS


void* ExternalModule::_getProcAddress(const std::string& functionName)
{
	const auto proc = GetProcAddress((moduleLib)_module, functionName.data());
	//assert(proc);
	return proc;
}

//Not thread safe
ExternalModule::ExternalModule(const std::string& modulePath) : _module(getLib(modulePath)){

	if (_module != nullptr && _module)
	{
		std::cout << "module loaded: " <<modulePath << std::endl;
	}
	else {
		std::cerr << "Can't load module: " << modulePath << std::endl;
		char cbuf[256];
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,cbuf,(sizeof(cbuf) / sizeof(char)),NULL);
		std::cerr << cbuf << std::endl;
		BAIL;
	}

}

ExternalModule::~ExternalModule() {
	if (_module != NULL){
		freeLib((moduleLib)_module);
	}
}