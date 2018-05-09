#pragma once

#ifdef AUTOUPDATER_EXPORTS
#define AUTOUPDATER_API __declspec(dllexport)
#else
#define AUTOUPDATER_API __declspec(dllimport)
#endif

#include <string>
#include <vector>
#include <iostream>

#include <exception>
#include <experimental/filesystem>

#define MAX_FILENAME 255
#define MAX_PATH 260
#define MAX_URL 2000
#define dir_delimter '/'
#define READ_SIZE 8192

// Updater Errors.
#define UPDATER_SUCCESS				(0)
#define UPDATER_ERROR				(-1)
#define UPDATER_UPDATE_AVAILABLE	(1)
#define UPDATER_NO_UPDATE			(UPDATER_SUCCESS)
#define UPDATER_EXCEPTION			(-2)
#define UPDATER_FILE_NOT_FOUND		(404)
#define UPDATER_FWRITE_ERROR		(2)
#define UPDATER_CURL_ERROR			(3)
#define UPDATER_INVALID_INPUT		(4)

// 0 Version Number Errors. - Handles version number type and downloadVersionNumber() function.
#define VN_SUCCESS					(UPDATER_SUCCESS)
#define VN_ERROR					(UPDATER_ERROR)
#define VN_FILE_NOT_FOUND			(4040)
#define VN_EXCEPTION				(-20)
#define VN_CURL_ERROR				(40)
#define VN_INVALID_VERSION			(30)
#define VN_EMPTY_STRING				(110)

// 1 Downloading Update Errors. - Handles downloadUpdate() function.
#define DU_SUCCESS					(UPDATER_SUCCESS)
#define DU_ERROR					(UPDATER_ERROR)
#define DU_FILE_NOT_FOUND			(4041)
#define DU_ERROR_WRITE_TO_FILE		(21)
#define DU_CURL_ERROR				(41)
#define DU_FWRITE_ERROR				(51)

// 2 Unzipping Errors. - Handles unZip() function
#define UZ_SUCCESS					(UPDATER_SUCCESS)
#define UZ_ERROR					(UPDATER_ERROR)
#define UZ_FILE_NOT_FOUND			(4042)
#define UZ_FWRITE_ERROR				(52)
#define UZ_GLOBAL_INFO_ERROR		(62)
#define UZ_FILE_INFO_ERROR			(72)
#define UZ_CANNOT_OPEN_DEST_FILE	(82)
#define UZ_READ_FILE_ERROR			(92)
#define UZ_CANNOT_READ_NEXT_FILE	(102)

// 3 Installing Update Errors. - Handles installUpdate() function
#define I_SUCCESS					(UPDATER_SUCCESS)
#define I_ERROR						(UPDATER_ERROR)
#define I_FAIL_TO_DELETE			(13)
#define I_FS_RENAME_ERROR			(23)
#define I_FS_DLL_ERROR				(53)
#define I_FS_COPY_ERROR				(33)
#define I_FS_REMOVE_ERROR			(43)

// 4 Cleanup Errors. - Handles cleanup() function
#define CU_SUCCESS					(UPDATER_SUCCESS)
#define CU_FS_REMOVE_ERROR			(14)
#define CU_CREATE_PROCESS_ERROR		(24)

namespace fs = std::experimental::filesystem;
using std::string;
using std::exception;

struct AUTOUPDATER_API Flag
{
public:

	Flag(fs::path* path, const string* message, int updater_error = UPDATER_ERROR)
		: m_filePath(path), m_message(message), m_error(updater_error), m_hasPath(true)
	{

	}
	Flag(string* path, const string* message, int updater_error = UPDATER_ERROR)
		: m_filePath((fs::path*)path), m_message(message), m_error(updater_error), m_hasPath(true)
	{
		
	}
	Flag(const string* message, int updater_error)
		: m_message(message), m_error(updater_error), m_hasPath(false)
	{
		
	}
	~Flag() 
	{

	}

	inline const fs::path *getFilePath() const { return m_filePath; }
	inline fs::path getFileName() { return m_filePath->filename(); }
	inline fs::path getFileExtension() { return m_filePath->extension(); }
	inline const string *getMessage() const { return m_message; }
	inline int getError() { return m_error; }
	inline bool hasPath() { return m_hasPath; }

	inline void setFilePath(fs::path *path) { m_filePath = path; }
	inline void setMessage(string *message) { m_message = message; }
	inline void setError(int error) { m_error = error; }

private:

	fs::path *m_filePath;
	const string *m_message;
	int m_error;
	bool m_hasPath = true;

};

struct AUTOUPDATER_API Version
{
public:
	Version(int a_major, int a_minor, char *a_revision)
		: major(a_major), minor(a_minor), m_error(VN_SUCCESS)
	{
		strncpy_s(revision, (char*)a_revision, sizeof(revision));
	}
	Version(string version) : m_error(VN_SUCCESS)
	{
		try
		{
			if (version.empty())
				throw VN_EMPTY_STRING;

			string::size_type pos = version.find('.');
			if (pos != string::npos)
			{
				major = stoi(version.substr(0, pos));
				string::size_type pos2 = version.find('.', pos + 1);

				if (pos2 != string::npos)
				{
					// Contains 2 periods.
					minor = stoi(version.substr(pos + 1, pos2));
					strncpy_s(revision, (char*)version.substr(pos2 + 1).c_str(), sizeof(revision));
				}
				else
				{
					// Contains 1 period.
					minor = stoi(version.substr(pos + 1));
					revision[0] = '\0';
				}
			}
			else
			{
				// Contains 0 periods.
				major = stoi(version);
				minor = -1;
			}
		}
		catch (errno_t error)
		{
			m_error = error;
		}
		catch (exception e)
		{
			m_error = VN_EXCEPTION;
			std::cout << "Exception Thrown: " << e.what() << std::endl;
		}
	}

	~Version()
	{
		
	}

	string getVersionString() 
	{ 
		try
		{
			if (revision[0] == '\0')
			{
				return std::to_string(major) + "." + std::to_string(minor);
			}
			if (minor == -1)
			{
				return std::to_string(major);
			}
			return std::to_string(major) + "." + std::to_string(minor) + "." + revision;
		}
		catch (exception e)
		{
			m_error = VN_EXCEPTION;
			string str = "Exception Thrown. Exception: ";
			str.append(e.what());
			return str;
		}
	}

	bool operator=(const Version &v)
	{
		if (major == v.major && minor == v.minor)
		{
			if (strcmp(revision, v.revision) == 0)
			{
				return true;
			}
		}
		return false;
	}
	bool operator>=(const Version &v)
	{
		if (this->operator=(v))
		{
			return true;
		}

		if (major >= v.major && minor >= v.minor)
		{
			if (strcmp(revision, v.revision) > 0)
			{
				return true;
			}			
		}
		return false;
	}


	// Getters and Setters 
	// ---------------------------------------------------------
	inline const int getMajor() const { return major; }
	inline const int getMinor() const { return minor; }
	inline const errno_t getError() const { return m_error; }
	inline string getRevision() { return revision; }

	inline void setMajor(const int a_major) { major = a_major; }
	inline void setMinor(const int a_minor) { minor = a_minor; }
	inline void setRevision(char *a_revision) { strncpy_s(revision, (char*)a_revision, sizeof(revision)); }
	// ----------------------------------------------------------------------------

private:
	int major;
	int minor;
	char revision[5]; // Memory for 4 characters and a null-terminator ('\0').

	errno_t m_error;
};

class AUTOUPDATER_API AutoUpdater
{
public:
	AutoUpdater(Version cur_version, const string version_url, const string download_url, const char* process_location = "");
	~AutoUpdater();

	int run();
	
	int downloadVersionNumber();
	bool checkForUpdate();
	int downloadUpdate();
	int unZipUpdate();
	int installUpdate();
	int cleanup();

private:
	static size_t _WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);
	static size_t _WriteData(void *ptr, size_t size, size_t nmemb, FILE *stream);
	void _SetDirs(const char* process_location = "");
	int _RenameAndCopy(const char* path);
	void _OutFlags();

protected:
	Version *m_version;
	Version *m_newVersion;

	std::vector<string> *m_pathsToDelete;
	std::vector<Flag*>	*m_flags;

	char m_versionURL[MAX_URL];
	char m_downloadURL[MAX_URL];

	char m_directory[MAX_PATH];
	char m_downloadDIR[MAX_PATH];
	char m_downloadNAME[MAX_FILENAME];
	char m_downloadFILE[MAX_PATH + MAX_FILENAME];
	char m_extractedDIR[MAX_PATH];
	char m_exeLOC[MAX_PATH + MAX_FILENAME];
};

