#include <iostream>
#include <string>

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

int main(int argc, char** argv)
{
	COORD top_left;
	top_left.X = 0;
	top_left.Y = 0;

	HWND hwnd = GetConsoleWindow();
	ShowWindow(hwnd, SW_SHOWMAXIMIZED);

	HANDLE console_in_handle = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE console_out_handle = GetStdHandle(STD_OUTPUT_HANDLE);

	SetConsoleCP((UINT)437);

	std::string exe_path = "cmd.exe"; //powershell tasklist cmd

	HANDLE process_std_out_read = NULL, process_std_out_write = NULL;
	HANDLE process_std_in_read = NULL, process_std_in_write = NULL;
	HANDLE process_std_err_read = NULL, process_std_err_write = NULL;

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&process_std_out_read, &process_std_out_write, &sa, 0)) throw;
	if (!SetHandleInformation(process_std_out_read, HANDLE_FLAG_INHERIT, 0)) throw;

	if (!CreatePipe(&process_std_in_read, &process_std_in_write, &sa, 0)) throw;
	if (!SetHandleInformation(process_std_in_write, HANDLE_FLAG_INHERIT, 0)) throw;

	if (!CreatePipe(&process_std_err_read, &process_std_err_write, &sa, 0)) throw;
	if (!SetHandleInformation(process_std_err_read, HANDLE_FLAG_INHERIT, 0)) throw;

	STARTUPINFOA si;
	memset(&si, 0, sizeof(STARTUPINFOA));
	PROCESS_INFORMATION pi;
	memset(&pi, 0, sizeof(PROCESS_INFORMATION));
	si.cb = sizeof(si);
	si.hStdInput = process_std_in_read;
	si.hStdOutput = process_std_out_write;
	si.hStdError = process_std_err_write;
	si.dwFlags |= STARTF_USESTDHANDLES;

	if (!CreateProcessA(NULL, &exe_path[0], NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) throw;
	if (!pi.hProcess) throw;

	const int in_buffer_size = 128, out_buffer_size = 256;
	CHAR in_buffer[in_buffer_size], out_buffer[out_buffer_size];
	DWORD written = 0;
	DWORD in_buffer_length = 0;

	CloseHandle(process_std_out_write);
	CloseHandle(process_std_in_read);

	CONSOLE_SCREEN_BUFFER_INFO console_buffer_info;
	COORD cursor_pos;
	DWORD to_read, read;
	DWORD avaliable, remaining;
	memset(&out_buffer, 0, out_buffer_size);
	memset(&in_buffer, 0, in_buffer_size);

	while (true) {
		DWORD exit_code;
		if (!GetExitCodeProcess(pi.hProcess, &exit_code)) throw;
		if (exit_code != STILL_ACTIVE) break;

		DWORD num_events;
		if (!GetNumberOfConsoleInputEvents(console_in_handle, &num_events)) throw;
		if (num_events > 0) {
			INPUT_RECORD record[8];
			DWORD num_events_read;

			if (!ReadConsoleInputA(console_in_handle, (PINPUT_RECORD)&record, (DWORD)8, &num_events_read)) throw;

			for (int i = 0; i < num_events_read; i++) {
				if (record[i].EventType == 1) {
					if (record[i].Event.KeyEvent.bKeyDown) {
						CHAR key = record[i].Event.KeyEvent.uChar.AsciiChar;

						in_buffer[in_buffer_length] = key;
						in_buffer_length++;

						if (key == '\r') {
							in_buffer[in_buffer_length] = '\n';
							in_buffer_length++;
						}
					}
				}
			}
		}

		BOOL io_pending;
		if (!GetThreadIOPendingFlag(pi.hThread, &io_pending)) throw;

		if (io_pending) {
			if (!PeekNamedPipe(process_std_out_read, NULL, 0, NULL, &avaliable, &remaining)) throw;

			if (avaliable > 0) {
				if (avaliable < out_buffer_size) {
					to_read = avaliable;
				}
				else {
					to_read = out_buffer_size;
				}
				if (ReadFile(process_std_out_read, out_buffer, to_read, &read, NULL)) {
					CHAR* read_buffer = (CHAR*)_malloca(read + 1);

					if (!read_buffer) throw;

					memset(read_buffer, 0, read + 1);
					memcpy(read_buffer, &out_buffer, read);

					std::cout << read_buffer;
				}
				else
					throw;
			}

			if (!PeekNamedPipe(process_std_err_read, NULL, 0, NULL, &avaliable, &remaining)) throw;

			if (avaliable > 0) {
				if (avaliable < out_buffer_size) {
					to_read = avaliable;
				}
				else {
					to_read = out_buffer_size;
				}
				if (ReadFile(process_std_err_read, out_buffer, to_read, &read, NULL)) {
					CHAR* read_buffer = (CHAR*)_malloca(read + 1);

					if (!read_buffer) throw;

					memset(read_buffer, 0, read + 1);
					memcpy(read_buffer, &out_buffer, read);

					SetConsoleTextAttribute(console_out_handle, FOREGROUND_RED);
					std::cout << read_buffer;
					SetConsoleTextAttribute(console_out_handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
				}
				else
					throw;
			}
		}

		if (io_pending && avaliable == 0) {
			if (in_buffer_length > 0) {
				if (WriteFile(process_std_in_write, in_buffer, in_buffer_length, &written, NULL)) {
					memset(&in_buffer, 0, in_buffer_length);
					in_buffer_length = 0;
				}
				else
					throw;
			}
		}
	}
}