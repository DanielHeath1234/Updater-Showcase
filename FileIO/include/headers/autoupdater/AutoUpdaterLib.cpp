#include "AutoUpdaterLib.h"
#include "zlib\unzip.h"

#include <curl/curl.h>
#include <direct.h>
#include <windows.h>
#include <algorithm>
#include <iomanip>

using std::string;

AutoUpdater::AutoUpdater(Version cur_version, const string version_url, const string download_url, const char* process_location)
	: m_version(&cur_version)
{
	// Copies const string into char array for use in CURL.
	strncpy_s(m_versionURL, version_url.c_str(), sizeof(m_versionURL));
	strncpy_s(m_downloadURL, download_url.c_str(), sizeof(m_downloadURL));

	// Set directories based off of the main process's location.
	_SetDirs(process_location);

	// Error checks on initilisation version.
	if (m_version->getError() != VN_SUCCESS)
		m_flags.push_back(new Flag("Version Number Error.", m_version->getError()));

	// Runs the updater upon construction, checks for errors and outputs flags.
	int error = run();
	if (error != UPDATER_SUCCESS)
	{
		char err = std::to_string(error).back();
		switch (err)
		{
		case '2':
			m_flags.push_back(new Flag("Unzip Error", error));
			break;

		default:
			m_flags.push_back(new Flag("AutoUpdater Error", error));
			break;
		}
	}

	_OutFlags();
}

AutoUpdater::~AutoUpdater()
{

}

int AutoUpdater::run()
{
	// Keep .0 on the end of float when outputting.
	std::cout << std::fixed << std::setprecision(1);
	errno_t value = UPDATER_SUCCESS;

	// Downloads version number.
	value = downloadVersionNumber();
	if (value != VN_SUCCESS)
		return value;

	// Checks for update.
	if (!checkForUpdate())
		return UPDATER_NO_UPDATE;

	char input;
	std::cout << "Would you like to update? (y,n)" << std::endl;
	std::cin >> input;

	switch (input)
	{
	case 'y':
		system("cls");

		// Download the update.
		std::cout << "Downloading update please wait..." << std::endl << std::endl;
		value = downloadUpdate();
		if (value != DU_SUCCESS)
			return value;

		// Unzip the update.
		std::cout << std::endl << "Unzipping update please wait..." << std::endl << std::endl;
		value = unZipUpdate();
		if (value != UZ_SUCCESS)
			return value;

		// Install the update.
		std::cout << std::endl << "Installing update please wait..." << std::endl << std::endl;
		value = installUpdate();
		if (value != I_SUCCESS)
			return value;

		// Cleanup.
		/*std::cout << std::endl << "Cleaning up..." << std::endl << std::endl;
		value = cleanup();
		if (value != CU_SUCCESS)
			return value;*/

			// Update was successful.
		std::cout << std::endl << "Update Successful." << std::endl << std::endl;
		return UPDATER_SUCCESS;
		break;

	case 'n':
		return UPDATER_NO_UPDATE;
		break;

	default:
		return UPDATER_INVALID_INPUT;
		break;
	}

	return UPDATER_ERROR;
}

int AutoUpdater::downloadVersionNumber()
{
	errno_t err = 0;
	CURL *curl;
	CURLcode res;
	string readBuffer;

	curl = curl_easy_init();
	if (curl)
	{
		// Download raw version number from file.
		curl_easy_setopt(curl, CURLOPT_URL, m_versionURL);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			m_flags.push_back(new Flag(curl_easy_strerror(res), VN_CURL_ERROR));
			return VN_CURL_ERROR;
		}

		curl_easy_cleanup(curl);

		// Changes new-line with null-terminator.
		std::replace(readBuffer.begin(), readBuffer.end(), '\n', '\0');

		// Attempt to initalise downloaded version string as type Version.
		m_newVersion = new Version(readBuffer);
		if (m_newVersion->getError() != VN_SUCCESS)
			return VN_ERROR;

		if (m_newVersion->getMajor() == 404 && m_newVersion->getMinor() == -1)
			return VN_FILE_NOT_FOUND;

		return VN_SUCCESS;
	}
	return VN_CURL_ERROR;
}

bool AutoUpdater::checkForUpdate()
{
	// Checks if versions are equal.
	if (m_version->operator>=(*m_newVersion))
	{
		// The versions are equal. No Update Available. Don't bother prompting.
		return false;
	}

	// An update is available.
	std::cout << "An Update is Available." << std::endl
		<< "Newest Version: " << m_newVersion->getVersionString() << std::endl
		<< "Current Version: " << m_version->getVersionString() << std::endl << std::endl;
	return true;
}

int AutoUpdater::downloadUpdate()
{
	CURL *curl;
	FILE *fp;
	errno_t err;
	CURLcode res;

	curl = curl_easy_init();
	if (curl)
	{
		// Checks if download directory exists and if not, creates it.
		if (!fs::exists(m_downloadDIR))
		{
			std::cout << "Download path does not exist. Creating directory now." << std::endl << "Path: " << m_downloadDIR << std::endl;
			fs::create_directory(m_downloadDIR);
		}

		// Opens file stream and sets up curl.
		curl_easy_setopt(curl, CURLOPT_URL, m_downloadURL);
		err = fopen_s(&fp, m_downloadFILE, "wb"); // wb - Create file for writing in binary mode.
		if (err != DU_SUCCESS)
		{
			if (err = DU_ERROR_WRITE_TO_FILE)
				return DU_ERROR_WRITE_TO_FILE;

			return DU_ERROR;
		}

		// Debug output.
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

		// Follow Redirection.
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "deflate");

		// Write to file.
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _WriteData);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

		// cURL error return, cURL cleanup and file close.
		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			m_flags.push_back(new Flag(curl_easy_strerror(res), DU_CURL_ERROR));
			return DU_CURL_ERROR;
		}

		curl_easy_cleanup(curl);
		fclose(fp);
		std::cout << std::endl << "Download Successful." << std::endl;
		return DU_SUCCESS;
	}
	return DU_CURL_ERROR;
}

int AutoUpdater::unZipUpdate()
{
	// Open the zip file
	unzFile fOpen = unzOpen(m_downloadFILE);
	unzFile *zipfile = &fOpen;
	if (zipfile == NULL)
	{
		//printf("%s", ": not found\n");
		return UZ_FILE_NOT_FOUND;
	}

	// Get info about the zip file
	unz_global_info *global_info = new unz_global_info;
	if (unzGetGlobalInfo(zipfile, global_info) != UNZ_OK)
	{
		//printf("could not read file global info\n");
		unzClose(*zipfile);
		return UZ_GLOBAL_INFO_ERROR;
	}

	// Buffer to hold data read from the zip file.
	char *read_buffer[READ_SIZE];

	// Loop to extract all files
	uLong i;
	for (i = 0; i < (*global_info).number_entry; ++i)
	{
		// Get info about current file.
		unz_file_info file_info;
		char filename[MAX_FILENAME];

		if (unzGetCurrentFileInfo(
			*zipfile,
			&file_info,
			filename,
			MAX_FILENAME,
			NULL, 0, NULL, 0) != UNZ_OK)
		{
			unzClose(*zipfile);
			return UZ_FILE_INFO_ERROR;
		}

		char dirAndName[MAX_PATH] = "\0";
		strncat_s(dirAndName, m_downloadDIR, sizeof(dirAndName));
		strncat_s(dirAndName, filename, sizeof(dirAndName));
		if (i == 0)
			strncpy_s(m_extractedDIR, dirAndName, sizeof(m_extractedDIR));

		// Check if this entry is a directory or file.
		const size_t filename_length = strlen(filename);
		if (filename[filename_length - 1] == dir_delimter)
		{
			// Entry is a directory, so create it.
			printf("dir:%s\n", filename);
			fs::create_directory(dirAndName);
		}
		else
		{
			// Entry is a file, so extract it.
			printf("file:%s\n", filename);
			if (unzOpenCurrentFile(*zipfile) != UNZ_OK)
			{
				unzClose(zipfile);
				return UZ_FILE_INFO_ERROR;
			}

			// Open a file to write out the data.
			FILE *out;
			errno_t err = 0;
			err = fopen_s(&out, dirAndName, "wb");
			if (out == NULL)
			{
				unzCloseCurrentFile(*zipfile);
				unzClose(*zipfile);
				return UZ_CANNOT_OPEN_DEST_FILE;
			}

			int error = UNZ_OK;
			do
			{
				error = unzReadCurrentFile(*zipfile, read_buffer, READ_SIZE);
				if (error < 0)
				{
					unzCloseCurrentFile(*zipfile);
					unzClose(*zipfile);
					return UZ_READ_FILE_ERROR;
				}

				// Write data to file.
				if (error > 0)
				{
					if (fwrite(read_buffer, error, 1, out) != 1)
					{
						return UZ_FWRITE_ERROR;
					}
				}
			} while (error > 0);

			fclose(out);
		}

		unzCloseCurrentFile(*zipfile);

		// Go the the next entry listed in the zip file.
		if ((i + 1) < (*global_info).number_entry)
		{
			int err = unzGoToNextFile(*zipfile);
			if (err != UNZ_OK)
			{
				if (err == UNZ_END_OF_LIST_OF_FILE)
				{
					std::cout << std::endl << "UnZip Successful." << std::endl;
					unzClose(*zipfile);
					delete global_info;
					return UZ_SUCCESS;
				}
				unzClose(*zipfile);
				delete global_info;
				return UZ_CANNOT_READ_NEXT_FILE;
			}
		}
	}

	delete global_info;
	unzClose(*zipfile);

	return UZ_SUCCESS;
}

int AutoUpdater::installUpdate()
{
	std::error_code ec;

	// Rename process.
	if (_RenameAndCopy(m_exeLOC) != I_SUCCESS)
		return I_FS_RENAME_ERROR;

	// Install update. (don't forget .exe)
	fs::path update = m_extractedDIR;
	string dir(m_directory);
	std::size_t found = dir.find_last_of("/\\");
	dir = dir.substr(0, found);
	fs::path install = dir;

	for (auto& p : fs::recursive_directory_iterator(update))
	{
		string path(p.path().string());
		path.erase(0, strnlen_s(m_extractedDIR, sizeof(m_extractedDIR)));

		string installPath(install.string());
		installPath += "\\";
		installPath += path;

		if (fs::is_directory(p.path())) // Directory
		{
			if (!fs::exists(p.path(), ec)) // Directory doesn't exist. Create it.
			{
				std::cout << "Creating Directory: " << path << std::endl;
				fs::create_directory(installPath, ec);
			}
			else
			{
				std::cout << "Directory Exists: " << path << std::endl;
				continue;
			}
		}
		else // File
		{
			// Do not overwrite AutoUpdater source. Avoid overwriting with old code.
			if (p.path().filename() == "AutoUpdater.cpp" || p.path().filename() == "AutoUpdater.h" || p.path().filename() == "Source.cpp")
			{
				continue;
			}
			if (fs::exists(p.path(), ec)) // If file already exists. Overwrite it.
			{
				if (p.path().extension() == ".dll") // Checks if file is a dll (if in use, cannot be updated)
				{
					// Attempts update if there is a difference between update and install
					//  as well as checks for successful overwrite.
					uintmax_t updateFileSize = fs::file_size(p.path());
					uintmax_t installFileSize = fs::file_size(installPath);
					if (updateFileSize != installFileSize) // Checks for size difference in files. 
					{
						std::cout << "Attempting to overwrite dll file " << path << std::endl;
						fs::copy(p.path(), installPath, fs::copy_options::overwrite_existing, ec);
						if (ec.value() != 0)
						{
							// Failure to overwrite dll.
							std::cout << "Failed to overwrite file " << path << std::endl;
							m_flags.push_back(new Flag(&p.path().string(), ec.message(), I_FS_DLL_ERROR));
							continue;
						}

						std::cout << "Overwrite successful on file " << path << std::endl;
					}
					else
					{
						std::cout << "Skipping overwrite. File sizes are equal. " << path << std::endl;
					}
				}
				else // File isn't a dll.
				{
					std::cout << "Overwriting File: " << path << std::endl;
					fs::copy(p.path(), installPath, fs::copy_options::overwrite_existing, ec);
				}
			}
			else
			{
				std::cout << "Creating File: " << path << std::endl;
				fs::copy(p.path(), installPath, fs::copy_options::none, ec);
			}
		}

		if (ec.value() != 0)
		{
			m_flags.push_back(new Flag(&p.path().string(), (ec).message(), I_FS_COPY_ERROR));
			return I_FS_COPY_ERROR;
		}
	}

	// Delete update's temp download directory.
	fs::remove_all(m_downloadDIR, ec);
	if (ec.value() != 0)
	{
		m_flags.push_back(new Flag(ec.message(), I_FS_REMOVE_ERROR));
		return I_FS_REMOVE_ERROR;
	}

	std::cout << std::endl << "Install Successful." << std::endl;
	return I_SUCCESS;
}

int AutoUpdater::cleanup()
{
	// Open new process.
	/*int value = (int)ShellExecute(NULL, NULL, (LPWSTR)m_exeLOC, NULL, NULL, SW_SHOW);
	if (value < 32) // ShellExecute() success return is > 32.
	{
		m_flags.push_back(new Flag(std::to_string(value), CU_CREATE_PROCESS_ERROR));
		return CU_CREATE_PROCESS_ERROR;
	}

	// Delete files that weren't in update?
	// Avoid deleting .git

	// Delete renamed .bak files.
	/*if (!m_pathsToDelete.empty())
	{
		std::error_code ec;
		for (auto iter = m_pathsToDelete.begin(); iter != m_pathsToDelete.end(); iter++)
		{
			fs::remove((*iter), ec);
			if (ec.value() != 0)
			{
				m_flags.push_back(new Flag((*iter), ec.message(), CU_FS_REMOVE_ERROR));
			}
		}
	}*/

	std::cout << std::endl << "Cleanup Successful." << std::endl;
	return CU_SUCCESS;
}

size_t AutoUpdater::_WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	((string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

size_t AutoUpdater::_WriteData(void * ptr, size_t size, size_t nmemb, FILE * stream)
{
	size_t written = fwrite(ptr, size, nmemb, stream);
	if (written != nmemb)
		return DU_FWRITE_ERROR;

	return written * size;
}

void AutoUpdater::_SetDirs(const char* process_location)
{
	try
	{
		// Get current process's path and set m_exeLOC to it.
		(process_location == "") ?
			GetModuleFileName(NULL, m_exeLOC, sizeof(m_exeLOC)) :
			strncpy_s(m_exeLOC, process_location, sizeof(m_exeLOC));

		// Remove process name and extension from m_exeLOC and
		// set m_directory to folder containing process.
		string dir(m_exeLOC);
		std::size_t found = dir.find_last_of("/\\");
		string path = dir.substr(0, found);
		strncpy_s(m_directory, path.c_str(), sizeof(m_directory)); // Solution Directory.

		// Set m_downloadDIR to temp folder within directory.
		path += "\\temp\\";
		strncpy_s(m_downloadDIR, path.c_str(), sizeof(m_downloadDIR));

		// Sets m_downloadNAME to name and + .zip (extension).
		string file = dir.substr(found + 1);
		found = file.find_last_of(".");
		string dlName;
		dlName += file.substr(0, found);
		dlName += ".zip";
		strncpy_s(m_downloadNAME, dlName.c_str(), sizeof(m_downloadNAME));

		// Sets m_downloadFILE to download directory appending download name.
		strncpy_s(m_downloadFILE, m_downloadDIR, sizeof(m_downloadFILE));
		strncat_s(m_downloadFILE, m_downloadNAME, sizeof(m_downloadFILE));
	}
	catch (std::runtime_error e)
	{
		m_flags.push_back(new Flag(e.what(), UPDATER_DIRECTORY_EXCEPTION));
		std::cout << "Exception Thrown while setting directories: " << e.what() << std::endl;
	}
}

int AutoUpdater::_RenameAndCopy(const char* path)
{
	// Chicken and egg.
	std::error_code ec;
	fs::path process(path);
	fs::path processR = process;
	processR += ".bak";
	fs::rename(process, processR, ec);
	fs::copy(processR, process, ec);
	m_pathsToDelete.push_back(processR.string());
	if (ec.value() != 0)
	{
		m_flags.push_back(new Flag(ec.message(), I_FS_REMOVE_ERROR));
		return I_FS_RENAME_ERROR;
	}
	return I_SUCCESS;
}

void AutoUpdater::_OutFlags()
{
	if (m_flags.empty())
		return;

	for (auto iter = m_flags.begin(); iter != m_flags.end(); iter++)
	{
		if ((*iter)->hasPath())
		{
			std::cout << "ERROR FLAGGED: " << std::endl
				<< "File Path: " << (*iter)->getFilePath() << std::endl
				<< "Error Message: " << (*iter)->getMessage() << std::endl
				<< "Error Code: " << (*iter)->getError() << std::endl;
		}
		else
		{
			std::cout << "ERROR FLAGGED: " << std::endl
				<< "Error Message: " << (*iter)->getMessage() << std::endl
				<< "Error Code: " << (*iter)->getError() << std::endl;
		}
	}
	// TODO: System pause is windows specific.
	system("pause");
}

