#include <windows.h>
#include "Win32Bool.h"
#include "SerialIO.h"

namespace w32b = win32bool;

SerialIO::SerialIO()
	:handle_(INVALID_HANDLE_VALUE)
{
	::ZeroMemory(&readov_, sizeof(readov_));
	::ZeroMemory(&writeov_, sizeof(writeov_));
}

SerialIO::~SerialIO()
{
	CloseResource();
}

bool SerialIO::Open(const TCHAR* name, const TCHAR* param)
{
	bool success = w32b::TryWin32(w32b::CreateEvent(NULL, FALSE, FALSE, NULL, &readov_.hEvent), __FUNCTION__, __LINE__);

	if (success) {
		success = w32b::TryWin32(w32b::CreateEvent(NULL, FALSE, FALSE, NULL, &writeov_.hEvent), __FUNCTION__, __LINE__);
	}

	if (success) {
		success = w32b::TryWin32(w32b::CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL, &handle_), __FUNCTION__, __LINE__);
	}

	DCB dcb = { 0 };
	if (success) {
		success = w32b::TryWin32(::GetCommState(handle_, &dcb), __FUNCTION__, __LINE__);
	}

	if (success) {
		success = w32b::TryWin32(::BuildCommDCB(param, &dcb), __FUNCTION__, __LINE__);
	}

	if (success) {
		success = w32b::TryWin32(::SetCommState(handle_, &dcb), __FUNCTION__, __LINE__);
	}

	if (!success) {
		CloseResource();
	}
	return success;
}

bool SerialIO::Close()
{
	return  CloseResource();
}

enum SerialIO::code SerialIO::ReadChunk(char** lpBuffer, int* outlen, DWORD dwTimeoutMs)
{
	if (!IsInitialized()) {
		return SerialIO::FAIL;
	}
	bool success = true;
	enum code result = SerialIO::SUCCESS;
	DWORD firstread = 0;
	char c = 0;
	if ((result = Read(&c, 1, dwTimeoutMs, &firstread)) != SerialIO::SUCCESS) {
		success = false;
	}

	COMSTAT stat = { 0 };
	DWORD dwErrors = 0;
	if (success) {
		if (!(success = w32b::TryClearCommError(handle_, &dwErrors, &stat, __FUNCTION__, __LINE__))) {
			result = SerialIO::FAIL;
		}
	}

	char* buffer = NULL;
	if (success) {
		buffer = new char[stat.cbInQue + 1];
		buffer[0] = c;
	}

	DWORD seconderead = 0;
	if (success && stat.cbInQue > 0) {
		if ((result = Read(buffer + 1, stat.cbInQue, 0, &seconderead)) != SerialIO::SUCCESS) {
			success = false;
		}
	}

	*lpBuffer = success ? buffer : NULL;
	*outlen = success ? firstread + seconderead : 0;

	return result;
}

enum SerialIO::code SerialIO::Read(char* lpBuffer, DWORD nNumberOfBytesToRead, DWORD dwTimeoutMs, DWORD* readlen)
{
	if (!IsInitialized()) {
		return SerialIO::FAIL;
	}

	DWORD reason = 0;
	bool success = w32b::TryWin32AsyncIO(::ReadFile(handle_, lpBuffer, nNumberOfBytesToRead, NULL, &readov_), __FUNCTION__, __LINE__);
	if (success) {
		success = w32b::TryWin32(w32b::WaitForSingleObject(readov_.hEvent, dwTimeoutMs, &reason), __FUNCTION__, __LINE__);
	}
	if (success) {
		if (reason == WAIT_OBJECT_0) {
			success = w32b::TryWin32(::GetOverlappedResult(handle_, &readov_, readlen, TRUE), __FUNCTION__, __LINE__);
		}
		else if (reason == WAIT_TIMEOUT) {
			success = w32b::TryWin32(::CancelIo(handle_), __FUNCTION__, __LINE__);
		}
	}

	if (success && reason == WAIT_OBJECT_0) {
		return SerialIO::SUCCESS;
	}
	else if (success && reason == WAIT_TIMEOUT) {
		return SerialIO::TIME_OUT;
	}
	return SerialIO::FAIL;
}

enum SerialIO::code  SerialIO::Write(const char* lpBuffer, DWORD nNumberOfBytesToWrite, DWORD dwTimeoutMs, DWORD* written)
{
	if (!IsInitialized()) {
		return SerialIO::FAIL;
	}

	bool success = w32b::TryWin32AsyncIO(::WriteFile(handle_, lpBuffer, nNumberOfBytesToWrite, NULL, &writeov_), __FUNCTION__, __LINE__);
	DWORD reason = 0;
	if (success) {
		success = w32b::TryWin32(w32b::WaitForSingleObject(writeov_.hEvent, dwTimeoutMs, &reason), __FUNCTION__, __LINE__);
	}

	if (success) {
		if (reason == WAIT_OBJECT_0) {
			success = w32b::TryWin32(::GetOverlappedResult(handle_, &writeov_, written, TRUE), __FUNCTION__, __LINE__);
		}
		else if (reason == WAIT_TIMEOUT) {
			success = w32b::TryWin32(::CancelIo(handle_), __FUNCTION__, __LINE__);
		}
	}

	if (success && reason == WAIT_OBJECT_0) {
		return SerialIO::SUCCESS;
	}
	else if (success && reason == WAIT_TIMEOUT) {
		return SerialIO::TIME_OUT;
	}
	return SerialIO::FAIL;
}

bool SerialIO::IsInitialized()
{
	return readov_.hEvent != NULL && writeov_.hEvent != NULL && handle_ != INVALID_HANDLE_VALUE;
}

bool SerialIO::CloseResource()
{
	bool close_readov = true;
	if (!readov_.hEvent) {
		close_readov = w32b::TryWin32(::CloseHandle(readov_.hEvent), __FUNCTION__, __LINE__);
	}
	bool close_writeov = true;
	if (!writeov_.hEvent) {
		close_writeov = w32b::TryWin32(::CloseHandle(writeov_.hEvent), __FUNCTION__, __LINE__);
	}
	bool close_handle = true;
	if (handle_ != INVALID_HANDLE_VALUE) {
		close_handle = w32b::TryWin32(::CloseHandle(handle_), __FUNCTION__, __LINE__);
	}
	::ZeroMemory(&readov_, sizeof(readov_));
	::ZeroMemory(&writeov_, sizeof(writeov_));
	handle_ = INVALID_HANDLE_VALUE;

	return close_readov && close_writeov && close_handle;
}

