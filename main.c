#include <Windows.h>
#include <wchar.h>
#include <shlwapi.h>

HANDLE heap_ptr;
HANDLE _stdout;

struct numer_list
{
	size_t size;
	wchar_t** str;
};

struct Thread_data
{
	wchar_t* lpFolder;
	struct numer_list* remove_list;
};

struct numer_list* get_drives_list();
struct numer_list* get_remove_files_list(int argc, wchar_t** argv);

void* memcpy(void* dst, const void* src, size_t count);

void FindFilesRecursively(wchar_t* lpFolder,
						  const struct numer_list* const remove_list);

DWORD WINAPI Scan_remove_thread_f(LPVOID lpParam);

int WINAPI wWinMain(HINSTANCE hInstance,
					HINSTANCE hPrevInstance,
					PWSTR pCmdLine,
					int nCmdShow)
{
	/*Инициализация stdout*/
	_stdout = GetStdHandle(STD_OUTPUT_HANDLE);

	/*Нужно для получения аргументов командной строки в виде 16-битных строк*/
	int argc;
	wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	/*Инициализация heap*/
#define HEAP_SIZE 4096 * 4 /*4 страницы - достаточно*/
#define HEAP_MAX_SIZE 4096 * 4 /*достаточно*/
	heap_ptr = HeapCreate(0, HEAP_SIZE, HEAP_MAX_SIZE);
	
	struct numer_list* remove_list = get_remove_files_list(argc, argv);
	for(int i = 0; i < remove_list->size; i++)
	{
		wchar_t buffer_output[512];
		long unsigned int num_of_output = wsprintfW(buffer_output,
										 L"%d, %s\n",
										 i, remove_list->str[i]);
		WriteConsoleW(_stdout,
					  buffer_output,
					  num_of_output,
					  &num_of_output,
					  NULL);
	}

	struct numer_list* drives_list = get_drives_list();
	struct Thread_data th_data[drives_list->size];
	DWORD  th_id_list[drives_list->size];
	HANDLE th_list[drives_list->size];
	for(int i = 0; i < drives_list->size; i++)
	{
		//wchar_t buffer_output[512];
		//long unsigned int num_of_output = wsprintfW(buffer_output,
		//								 L"%d, %s\n",
		//								 i, drives_list->str[i]);
		//WriteConsoleW(_stdout,
		//			  buffer_output,
		//			  num_of_output,
		//			  &num_of_output,
		//			  NULL);

		th_data[i].lpFolder = drives_list->str[i];
		th_data[i].remove_list = remove_list;

		th_list[i] = CreateThread(
			NULL,				    // default security attributes
			0,					    // use default stack size
			Scan_remove_thread_f,   // thread function name
			&th_data[i],			// argument to thread function
			0,					    // use default creation flags
			&th_id_list[i]);   // returns the thread identifier
	}
	WaitForMultipleObjects(drives_list->size, th_list, TRUE, INFINITE);
	ExitProcess(EXIT_SUCCESS);
}

struct numer_list* get_remove_files_list(int argc, wchar_t** argv)
{
	/*
	 *	 Список файлов берется из аргументов командной строки
	 */
	struct numer_list* remove_list = HeapAlloc(heap_ptr,
											   HEAP_ZERO_MEMORY,
											   sizeof(struct numer_list));

	remove_list->size = argc - 1;
	remove_list->str = &argv[1];
	return remove_list;
}

struct numer_list* get_drives_list()
{
	wchar_t buf[1024] = {0};
	size_t buflen = GetLogicalDriveStringsW(1024, &buf[0]);
	/*Подсчет количества устройств*/
	size_t num_of_drives = 0;
	for(int i = 0; i < buflen; i++)
	{
		if (buf[i] == 0)
		{
			num_of_drives++;
		}
	}
	/* Выделение памяти под строки, копирование
	 *
	 * Выделить нужно: sizeof(struct numer_list) для drives_list
	 *				   sizeof(wchar_t*) * num_of_drives для строк
	 *				   sizeof(wchar_t) * buflen память строк
	*/

	size_t size_of_alloc = sizeof(struct numer_list) +
						   sizeof(wchar_t*) * num_of_drives +
						   sizeof(wchar_t) * buflen;
	void* alloc_memory = HeapAlloc(heap_ptr,
								   HEAP_ZERO_MEMORY,
								   size_of_alloc);

	struct numer_list* drives_list = alloc_memory;

	drives_list->size = num_of_drives;
	drives_list->str = alloc_memory + sizeof(struct numer_list);

	size_t index_str = 0;
	size_t start_line = 0;
	for(int i = 0; i < buflen; i++)
	{
		if (buf[i] == '\0')
		{
			size_t str_len = i - start_line + 1;
			drives_list->str[index_str] = alloc_memory +
										  sizeof(struct numer_list) +
										  sizeof(wchar_t*) * num_of_drives +
										  sizeof(wchar_t) * start_line;

			memcpy(drives_list->str[index_str],
				   &buf[start_line],
				   sizeof(wchar_t) * str_len);

			start_line = i + 1;
			index_str++;
		}
	}
	return drives_list;
}

DWORD WINAPI Scan_remove_thread_f(LPVOID lpParam)
{
	struct Thread_data* th_data = lpParam;
	FindFilesRecursively(th_data->lpFolder,
						 th_data->remove_list);
	return 0;
}

void FindFilesRecursively(wchar_t* lpFolder,
						  const struct numer_list* const remove_list)
{
	wchar_t szFullPattern[MAX_PATH];
	WIN32_FIND_DATAW FindFileData;
	HANDLE hFindFile;
	// first we are going to process any subdirectories
	PathCombineW(szFullPattern, lpFolder, L"*");
	hFindFile = FindFirstFileW(szFullPattern, &FindFileData);
	if(hFindFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			if(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if(FindFileData.cFileName[0] == '.')
				{
					
				}
				else
				{
					// found a subdirectory; recurse into it
					PathCombineW(szFullPattern, lpFolder, FindFileData.cFileName);
					FindFilesRecursively(szFullPattern,
										 remove_list);
				}
			}
		} while(FindNextFileW(hFindFile, &FindFileData));
		FindClose(hFindFile);
	}

	for(int i = 0; i < remove_list->size; i++)
	{
		PathCombineW(szFullPattern, lpFolder, remove_list->str[i]);
		/*Установка файловых аттрибутов для удаления файлов "только чтение"*/
		SetFileAttributesW(szFullPattern, FILE_ATTRIBUTE_NORMAL);
		if(DeleteFileW(szFullPattern))
		{
			wchar_t buffer_output[512];
			long unsigned int num_of_output = wsprintfW(buffer_output,
											 L"Удален: %s\n",
											 szFullPattern);
			WriteConsoleW(_stdout,
						  buffer_output,
						  num_of_output,
						  &num_of_output,
						  NULL);
		}
		else
		{
			if((GetLastError() == 0x7B) ||
			   (GetLastError() == 0x02) ||
			   (GetLastError() == 0x03))
			{
				/*Неверное имя файла, значит его нет. Это не ошибка*/
				return;
			}
			wchar_t buffer_output[512];
			long unsigned int num_of_output = wsprintfW(buffer_output,
							L"Ошибка номер %x при попытке удаления файла %s\n",
							GetLastError(), szFullPattern);
			WriteConsoleW(_stdout,
						  buffer_output,
						  num_of_output,
						  &num_of_output,
						  NULL);
		}
	}
}

void* memcpy(void* dst, const void* src, size_t count)
{
	void* ret = dst;
	
	char* csrc = (char*)src; 
	char* cdest = (char*)dst; 
	
	for (int i = 0; i < count; i++) 
		cdest[i] = csrc[i]; 
	
	return ret;
} 
