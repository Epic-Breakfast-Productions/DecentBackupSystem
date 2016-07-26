 /*
	Decent Backup System Server

	Copyright 2016 Epic Breakfast Productions

	Author: Greg Stewart
*/

///////////////
#pragma region include_globals - Includes and global variables.
///////////////

//includes TODO:: check for redundant includes
#include <cstdlib>
#include <stdlib.h>

#include <iostream>//to do console stuff
#include <fstream>//to do file stuff
#include <stdio.h>//file stuff?
#include <sstream>//for string stream on the outputs

#include <time.h>//for time measurement
#include <algorithm>//for finding element in vector
#include <string>//for strings
#include <string.h>//for strings
#include <sys/stat.h>//for checking filepaths
#include <vector>//for lists of things
#include <dirent.h>//for looking at contents of directory

//sleep stuff/ other sys dependent stuff
#ifdef __linux__
#include <unistd.h>//sleep, rm dir
#endif
#ifdef _WIN32
#include <windows.h>//sleep
#endif

//usings
using namespace std;

/**************
**	Globals  **
**************/
const char delimeter = ';';
const string version = "0.0.1";//version number
string logFileLoc = "DBSS_LOG.txt";//the log file
string confFileLoc = "DBSS_CONF.txt";//the config file

string syncFolderPred = "DBSS_sync_";// the filename predicate for sync folders
string storFolderPred = "DBSS_stor_";// the filename predicate for storage folders

string syncFolderLoc = syncFolderPred + "dir";
string storFolderLoc = storFolderPred + "dir";

const string clientConfigFileName = "DBSS_CLIENT_CONF.txt";
const string clientSyncFileListName = "DBSS_STORED_FILES.txt";
const string clientSyncGetFileListName = "DBSS_TO_GET_LIST.txt";

int checkInterval = 60;//in seconds

/* Option Flags */
bool runFlag = false;//if to run the server or not
bool helpFlag = false;//if to display the help (stops the execution after displaying help)
bool listFlag = false;//
bool verbose = false;//if to output much more to console
bool setToStartOnStart = false;//if to set the program to run on startup
bool runTest = false;//if to run the test code

/* for sync folder config */
int numBackupsToKeepDef = -1;
int numBackupsToKeep = numBackupsToKeepDef;

/* Other */
int transferWait = 1;//the amount of time to wait to se if a file has finished transferring, in seconds

//sleep stuff/ other sys dependent stuff
#ifdef __linux__
const string foldSeparater = "/";
const string nlc = "\n";
#endif
#ifdef _WIN32
const string foldSeparater = "\\";
const string nlc = "\r\n";
#endif

/////////////////
#pragma endregion
/////////////////





///////////////
#pragma region workers - Functions to do miscelaneous tasks.
///////////////

/**
	Sleeps for a specified amount of milliseconds.

	@param sleepMs The number of milliseconds to sleep for.
 */
void mySleepMs(int sleepMs) {
#ifdef __linux__
	usleep(sleepMs * 1000);   // usleep takes sleep time in us (1 millionth of a second)
#endif
#ifdef _WIN32
	Sleep(sleepMs);
#endif
}

/**
	Sleeps for a specified amount of seconds.

	@param sleepS The number of seconds to sleep for.
 */
void mySleep(int sleepS){
	mySleepMs(sleepS * 1000);
}

/**
	Checks if the file path given is present.

	@param pathIn The path to examine if it is present
	@param dir If we are to determine of the path is a directory or not.
	@return If the path given is valid.
 */
bool checkFilePath(string pathIn, bool dir){
    //sendDebugMsg("Path Given: " + pathIn);
    bool worked = false;//if things worked
    struct stat pathStat;//buffer for the stat
    //check if valid
    if(worked = (lstat(pathIn.c_str(), &pathStat) == 0)){
        //sendDebugMsg("path is present");
        //check if a file or directory
        if((S_ISDIR(pathStat.st_mode)) && dir){
            //sendDebugMsg("path is dir");
            worked = true;
        }else if((pathStat.st_mode && S_IFREG) && !dir){
            //sendDebugMsg("path is file");
            //check if valid filetype
            worked = true;
        }else{
            //sendDebugMsg("path is not recognized");
            worked = false;
        }
    }else{//if valid path
        //sendDebugMsg("path is invalid");
        worked = false;
    }
    return worked;
}//checkFilePath(string, bool)

/**
	Gets the current timestamp for log use.

	@return The current timestamp.
 */
string getTimestamp(){
	time_t rawtime;
	struct tm * timeinfo;
	char buffer [80];
	time (&rawtime);
	timeinfo = localtime (&rawtime);
	strftime (buffer,80,"%F %I:%M:%S %p",timeinfo);
	return (string)buffer;
}//getTimestamp

/**
	Gets a certain number of tabs.
	@param numTabs The number of tabs you want (defaults to 0).
*/
string getTabs(int numTabs = 1) {
	string tabs = "";
	for (int i = 0; i < numTabs; i++) {
		tabs += "\t";
	}
	return tabs;
}

/**
	How one should output text in this program.

	@param types The type(s) of output you want ('c'=console, 'l'=log file, 'r'=returned)
	@param message The message you are trying to output.
	@param verbosity Param to allow output the message to console.
*/
string outputText(string types, string message, bool verbosity = true, int tabLevel = 0){
	stringstream output;
	output << getTimestamp() << " - " << getTabs(tabLevel) << message << endl;
	string outputText = output.str();

	if(types.find('c') != string::npos && verbosity){//console output
		cout << outputText;
	}
	if(types.find('l') != string::npos){
		ofstream confFile (logFileLoc.c_str(), ios::app);
		if(message != ""){
			confFile << outputText;
		}else{
			confFile << endl;
		}
		confFile.close();
	}
	if(types.find('r') != string::npos){
		return outputText;
	}
	return "";
}//outputText

/**
 * Gets the folder separater character in a url.
 * Returns '\0' on not finding a folder separater.
 * @param url The url to get the folder separating character from.
 * @return The folder separator character.
 */
char getSeparater(string url){
	size_t found = url.find("/");
	if(found != string::npos){//unix style
		return '/';
	}
	found = url.find("\\");
	if(found != string::npos){//windows style
		return '\\';
	}
	return '\0';
}

/**
	Creates a directory.

	@param directoryLocation The directory you wish to create.
*/
void createDirectory(string directoryLocation){
	if(checkFilePath(directoryLocation, true)){
		return;
	}
	string dir = "mkdir " + directoryLocation;
	system(dir.c_str());
	if(!checkFilePath(directoryLocation, true)){
		//create the parent directory
		string parDir = directoryLocation.substr(0,directoryLocation.find_last_of(getSeparater(directoryLocation)));//parent directory
		createDirectory(parDir);
		//run for the deepest dir again
		createDirectory(directoryLocation);
	}
}

void removeDirectory(string dirLocation) {
#ifdef __linux__
	rmdir(dirLocation.c_str());
#endif
#ifdef _WIN32
	RemoveDirectory(dirLocation.c_str());
#endif
}

/**
	Gets the last file or folder in a given file path.

	@param path The path we are concerned with.
*/
string getLastPartOfPath(string path) {
	return path.substr(path.find_last_of(foldSeparater) + 1);
}

/**
	Copies a file from a path into the given directory. Creates the destination directory if it is not present.

	TODO:: find out if this removes the file.

	@param fromPath The path of the file to move.
	@param toDir The directory to move the file into.
*/
string copyFile(string fromPath, string toDir, int tabLevel = 3) {

	string output = outputText("r", "Copying file \"" + fromPath + "\" to \"" + toDir + "\"...", false, tabLevel);


	if (!checkFilePath(toDir, true)) {
		createDirectory(toDir);
	}
	//create destination location
	string toPath = toDir + foldSeparater + getLastPartOfPath(fromPath);

	//TODO:: check if file already present where copying; remove it first if there?

	//open file streams
	ifstream  src(fromPath.c_str(), ios::binary);
	ofstream  dst(toPath.c_str(), ios::binary);
	//move data
	dst << src.rdbuf();
	//close the files
	src.close();
	dst.close();

	if(!checkFilePath(toPath, true)){
		output += outputText("r", "ERROR:: file NOT copied. Destination file not present after copy attempt.", false, tabLevel + 1);
	}

	return output + outputText("r", "Done.", false, tabLevel);
}

/**
	Gets the size of the file.

	@param file The file to get the size of.
	@return The size of the file.
*/
long getFileSize(string file) {
	struct stat filestatus;
	stat(file.c_str(), &filestatus);
	return filestatus.st_size;
}

/////////////////
#pragma endregion workers
/////////////////





///////////////
#pragma region folderOps - General operations for folders.
///////////////

/**
	Gets the items in a directory and puts them into a provided vector. Items are without the rest of the path given in the first patrameter.

	@param dirLoc The directory to get the contents of.
	@param itemList The pointer of the vector we are putting items into.
	@param wholePath Optional param to add the whole path to the files & folders.
*/
void getItemsInDir(string dirLoc, vector<string>* itemList, bool wholePath = false) {
	//cout << "Searching: " << dirLoc << endl;
	DIR *pDIR;
	struct dirent *entry;
	if (pDIR = opendir(dirLoc.c_str())) {
		while (entry = readdir(pDIR)) {
			if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
				//cout << "FOUND ITEM: " << entry->d_name << endl;
				if(wholePath){
					itemList->push_back(dirLoc + foldSeparater + entry->d_name);
				}else{
					itemList->push_back(entry->d_name);
				}
			}
		}
		closedir(pDIR);
	}else{
		cout << "ERROR:::::: Could not open directory (" << dirLoc << ")for inspection." << endl;
	}
	return;
}

/**
	Gets the items in a given directory, separating the sub directories and files into two lists.
	Wrappper for other getItemsInDir

	@param dirLoc Directory to get files and folders in.
	@param fileList The pointer of the vector we are putting the files into.
	@param dirList The pointer of the vector we are putting the folders into.
	@param wholePath Optional param to add the whole path to the files & folders.
*/
void getItemsInDir(string dirLoc, vector<string>* fileList, vector<string>* dirList, bool wholePath = false) {
	//cout << "Getting items in \"" << dirLoc << "\":";
	vector<string> itemList;
	getItemsInDir(dirLoc, &itemList, wholePath);

	//cout << " # items: " << itemList.size() << endl;

	for (vector<string>::iterator it = itemList.begin(); it != itemList.end(); ++it) {
		if (checkFilePath(dirLoc + foldSeparater + *it, true)) {//is directory
			dirList->push_back(*it);
		} else {
			fileList->push_back(*it);
		}
	}//foreach item in dir
}

/**
	Determines if the given directory is empty.

	@param dirLoc The location of the directory to test for emptiness.
*/
bool dirIsEmpty(string dirLoc) {
	vector<string> itemList;
	getItemsInDir(dirLoc, &itemList);
	return itemList.size() == 0;
}

/**
Determines if the givben directory is not empty.

@param dirLoc The location of the directory to test for emptiness.
*/
bool dirIsNotEmpty(string dirLoc) {
	return !dirIsEmpty(dirLoc);
}


/////////////////
#pragma endregion folderOps
/////////////////





///////////////
#pragma region SyncFolderOps - operations for sync folders.
///////////////

/**
Generates the folder config for a sync folder.

@param configLoc The location of the config file to create.
@param tabLevel The number of tabs put in front of output lines.
*/
string generateFolderConfig(string configLoc, int tabLevel = 2) {
	string output = outputText("r", "Generating folder configuration...", false, tabLevel);
	ofstream confFile(configLoc.c_str());
	if (!confFile.good()) {
		output += outputText("r", "ERROR:: Unable to create or open new sync config file.", false, tabLevel + 1);
		return output + outputText("r", "Done ---WITH ERRORS---", verbose, tabLevel);
	}
	//add items to configuration file
	confFile << "numBackupsToKeep" << delimeter << "5";
	confFile.close();
	return output + outputText("r", "Done.", false, tabLevel);;
}

/**
Ensures the sync folder's config is present. Creates it if it is not there.

@param configLoc The location of the config file to create.
@param tabLevel The number of tabs put in front of output lines.
*/
string ensureFolderConfig(string configLoc, int tabLevel = 1) {
	string output = outputText("r", "Ensuring folder config is present...", false, tabLevel);
	if(!checkFilePath(configLoc, false)){
		output += outputText("r", "Folder config not found.", false, tabLevel + 1);
		output += generateFolderConfig(configLoc, tabLevel + 1);
	} else {
		output += outputText("r", "Folder config is present!", false, tabLevel + 1);
	}
	return output + outputText("r", "Done.", false, tabLevel);
}

/**
Reads the folder config into the globals.

@param configLoc The location of the config file to read from.
@param backupsToKeep The pointer to the integer to keep track of the number of backups to keep.
@param thisDelimeter The delimeter to go between the key/value pairs in the config file.
@param tabLevel The number of tabs put in front of output lines.
*/
string readFolderConfig(string configLoc, int* backupsToKeep, string thisDelimeter = delimeter + "", int tabLevel = 1) {
	string output = outputText("r", "Reading configuration file...", false, tabLevel);

	ifstream confFile(configLoc.c_str()); // declare file stream: http://www.cplusplus.com/reference/iostream/ifstream/
	string variable, value;

	while (confFile.good()) {
		getline(confFile, variable, thisDelimeter.c_str()[0]);
		getline(confFile, value, '\n');
		//cout << "\"" << variable << "\", \"" << value << "\"" << endl;
		if (variable == "numBackupsToKeep") {
			output += outputText("r", "Got # of backups to keep: " + value, false, tabLevel + 1);
			*backupsToKeep = atoi(value.c_str());
		}
	}
	confFile.close();
	return output + outputText("r", "Done.", false, tabLevel);
}

/**
	Ensures that the storage directory is present.

	@param storageDirLoc The location the storage directory should be.
	@param tabLevel The number of tabs put in front of output lines.
*/
string ensureStorageDir(string storageDirLoc, int tabLevel = 1) {
	string output = "";

	if (!checkFilePath(storageDirLoc, true)) {
		output += outputText("r", "Storage folder not found. Creating...", false, tabLevel);
		createDirectory(storageDirLoc);
		if (!checkFilePath(storageDirLoc, true)) {
			output += outputText("r", "ERROR:: FAILED TO CREATE STORAGE DIRECTORY", tabLevel +1);
			output += outputText("r", "Done ---WITH ERROR---", tabLevel);
		} else {
			output += outputText("r", "Done.", tabLevel);
		}
	}
	return output;
}

/**
Crops the number of files in storage to the number set by the config.

@param storDir The storage directory in question.
@param numToKeep The number of files to keep.
@param tabLevel The number of tabs put in front of output lines.
*/
string cropNumInStor(string storDir, int numToKeep, int tabLevel = 1) {
	if (numToKeep == -1) {
		return "";
	}
	string output = outputText("r", "Cropping number of files in the storage folder...", false, tabLevel);

	//TODO:: crop number of items in folder to the number given
}

/**
	Refreshes the file list in the sync folder.

	@param fileListFile Pointer to the file stream that lists the files.
	@param storeDir The storage directory to get the list of files in.
	@param levels The levels to prepend to the entries into the fileList. (optional)
	@param tabLevel The number of tabs put in front of output lines. (optional)
 */
string refreshFileList(ofstream& fileListFile, string storeDir, string levels = foldSeparater, int tabLevel = 1) {
	string output = outputText("r", "Refreshing file list for files in \"" + storeDir +"\"...", false, tabLevel);

	if (!fileListFile.good()) {
		output += outputText("cl", "ERROR:: Unable to write to file list.", verbose, tabLevel + 1);
		return output + outputText("r", "Done ---WITH ERRORS---.", false, tabLevel);
	}

	vector<string> dirList;
	vector<string> fileList;
	getItemsInDir(storeDir, &fileList, &dirList);
	//get files in subfolder:
	for (vector<string>::iterator it = dirList.begin(); it != dirList.end(); ++it) {
		output += refreshFileList(fileListFile, storeDir + foldSeparater + *it, levels + *it + foldSeparater, tabLevel + 1);
	}
	//list files
	for (vector<string>::iterator it = fileList.begin(); it != fileList.end(); ++it) {
		fileListFile << levels << *it << endl;
	}
	return output + outputText("r", "Done.", false, tabLevel);
}

/**
	Refreshes the file list with the items in the storage directory.

	Wrapper for the recursive one.

	@param fileListLoc The location of the file list file.
	@param storeDir The location of the storage directory.
	@param tabLevel The number of tabs put in front of output lines.
 */
string refreshFileList(string fileListLoc, string storeDir, int tabLevel = 1) {
	string output = outputText("r", "Refreshing file list... (" + fileListLoc + ")", false, tabLevel);

	ofstream fileListFile(fileListLoc.c_str(), ofstream::trunc);
	//fileListFile << "hello?";
	output += refreshFileList(fileListFile, storeDir, foldSeparater, tabLevel + 1);
	fileListFile.close();
	return output + outputText("r", "Done.", false, tabLevel);
}

/**
	Ensures the
*/
string ensureGetList(string getListLoc, int tabLevel = 1) {
	string output = outputText("r", "Ensuring get list is present...", false, tabLevel);
	if(!checkFilePath(getListLoc, false)){
		output += outputText("r", "Get list not found. Creating...", false, tabLevel + 1);
		ofstream getListFile(getListLoc.c_str(), ofstream::trunc);
		//fileListFile << "hello?";
		getListFile.close();
		if(!checkFilePath(getListLoc, false)){
			output += outputText("r", "ERROR:: Unable to create get list.", false, tabLevel + 2);
		}else{
			output + outputText("r", "Done.", false, tabLevel + 1);
		}
	} else {
		output += outputText("r", "Get list is present!", false, tabLevel + 1);
	}
	return output + outputText("r", "Done.", false, tabLevel);
}

/**
	Gets a list of files to retrieve and put into the sync folder, and clear the get list as it goes.

	@param syncRetrieveListLoc The location of the list of files to get.
	@param toGetList The pointer to the vector of the files to get.
	@param tabLevel The number of tabs put in front of output lines.
*/
string getListOfFilesToGet(string syncRetrieveListLoc, vector<string>* toGetList, int tabLevel = 1) {
	string output = outputText("r", "Getting list of files to keep in sync folder...", false, tabLevel);
	output += ensureGetList(syncRetrieveListLoc, tabLevel + 1);
	ifstream getListFile(syncRetrieveListLoc.c_str());
	string fileToGet;

	while (getListFile.good()) {
		getline(getListFile, fileToGet, '\n');

		//TODO:: handle "/*" cases for everything in a folder

		//why?
		//fileToGet.erase(remove(fileToGet.begin(), fileToGet.end(), '\n'), fileToGet.end());
		//fileToGet.erase(remove(fileToGet.begin(), fileToGet.end(), '\r'), fileToGet.end());
		if (fileToGet != "") {
			//cout << "file to get: \"" << fileToGet << "\"" << endl;
			toGetList->push_back(fileToGet);
		}
	}
	getListFile.close();
	return output + outputText("r", "Done.", false, tabLevel);
}

/**
	Moves the contents of a sync folder into a storage folder.

	@param syncFolderLoc The location of the sync folder to move things from.
	@param storFolderLoc The location of the storage folder to move things to.
	@param ignoreList The pointer of the list of files to ignore.
	@param levels The levels to prepend to outputs. (keep as empty string for first call)
	@param tabLevel The number of tabs put in front of output lines.
*/
string moveSyncFolderContents(string syncFolderLoc, string storFolderLoc, vector<string>* ignoreList, string levels = "", int tabLevel = 1) {
	string output = outputText("r", "Moving files and folders in \"" + syncFolderLoc + "\" to \"" + storFolderLoc + "\"...", false, tabLevel);
	//ensure storage folder location
	ensureStorageDir(storFolderLoc, tabLevel + 1);

	//get directory and file list
	vector<string> dirList;
	vector<string> fileList;
	getItemsInDir(syncFolderLoc, &fileList, &dirList);

	//process directories recursively
	unsigned long numDir = dirList.size();
	output += outputText("r", "Processing sub folders (" + to_string(numDir) + ")...", false, tabLevel + 1);
	if(dirList.size() == 0){
		output += outputText("r", "--None--", false, tabLevel + 2);
	}else{
		for (vector<string>::iterator it = dirList.begin(); it != dirList.end(); ++it) {
			output += moveSyncFolderContents(syncFolderLoc + foldSeparater + *it, storFolderLoc + foldSeparater + *it, ignoreList, levels + foldSeparater + *it, tabLevel + 2);
		}
	}
	output += outputText("r", "Done.", false, tabLevel + 1);

	//process files
	unsigned long numFiles = fileList.size();
	output += outputText("r", "Transferring files (" + to_string(numFiles) + ")...", false, tabLevel + 1);
	if(fileList.size() == 0){
		output += outputText("r", "--None--", false, tabLevel + 2);
	}else{
		for (vector<string>::iterator it = fileList.begin(); it != fileList.end(); ++it) {
			if (find(ignoreList->begin(), ignoreList->end(), syncFolderLoc + foldSeparater + *it) != ignoreList->end()) {
				output += outputText("r", "Skipping (in ignore or get list) \"" + levels + foldSeparater + *it + "\".", false, tabLevel + 2);
			} else {
				//move file
				output += outputText("r", "Moving \"" + levels + foldSeparater + *it + "\"...", false, tabLevel + 2);
				output += copyFile(syncFolderLoc + foldSeparater + *it, storFolderLoc, tabLevel + 3);
				output += outputText("r", "Done.", false, tabLevel + 2);
				//remove file
				remove((syncFolderLoc + foldSeparater + *it).c_str());
			}
		}
	}
	output += outputText("r", "Done.", false, tabLevel + 1);

	//remove sync folder for this iteration
	if (dirIsEmpty(syncFolderLoc)) {
		removeDirectory(syncFolderLoc);
	}

	return output + outputText("r", "Done.", false, tabLevel);
}

string moveFilesToGet(string storFolderLoc, string syncFolderLoc, vector<string>* getList, string levels = "", int tabLevel = 1) {
	string output = outputText("r", "Moving files and folders to get in \"" + storFolderLoc + "\" to \"" + syncFolderLoc + "\"...", false, tabLevel);
	//ensure storage folder location
	ensureStorageDir(storFolderLoc, tabLevel + 1);

	vector<string> foldsSearched;
	vector<string> tempGetList = *getList;

	//get directory and file list
	vector<string> dirList;
	vector<string> fileList;
	getItemsInDir(storFolderLoc, &fileList, &dirList);

	//process directories recursively
	unsigned long numDir = dirList.size();
	output += outputText("r", "Processing sub folders (" + to_string(numDir) + ")...", false, tabLevel + 1);
	if(dirList.size() == 0){
		output += outputText("r", "--None--", false, tabLevel + 2);
	}else{
		for (vector<string>::iterator it = dirList.begin(); it != dirList.end(); ++it) {
			output += moveFilesToGet(storFolderLoc + foldSeparater + *it, syncFolderLoc + foldSeparater + *it, getList, levels + foldSeparater + *it, tabLevel + 2);
		}
	}
	output += outputText("r", "Done.", false, tabLevel + 1);

	//process files
	unsigned long numFiles = fileList.size();
	output += outputText("r", "Transferring files (" + to_string(numFiles) + ")...", false, tabLevel + 1);
	if(fileList.size() == 0){
		output += outputText("r", "--None--", false, tabLevel + 2);
	}else{
		for (vector<string>::iterator it = fileList.begin(); it != fileList.end(); ++it) {
			if (find(getList->begin(), getList->end(), levels + foldSeparater + *it) != getList->end()) {
				//if(!checkFilePath()){}
				//move file
				output += outputText("r", "Moving \"" + levels + foldSeparater + *it + "\"...", false, tabLevel + 2);
				output += copyFile(storFolderLoc + foldSeparater + *it, syncFolderLoc, tabLevel + 3);
				output += outputText("r", "Done.", false, tabLevel + 2);
				//remove file
				remove((storFolderLoc + foldSeparater + *it).c_str());
			} else {
				output += outputText("r", "Skipping (not in get list) \"" + levels + foldSeparater + *it + "\".", false, tabLevel + 2);
			}
		}
	}
	output += outputText("r", "Done.", false, tabLevel + 1);

	//remove sync folder for this iteration
	if (dirIsEmpty(syncFolderLoc)) {
		removeDirectory(syncFolderLoc);
	}

	return output + outputText("r", "Done.", false, tabLevel);
}

// TODO::
	// handle moving folders back to the sync folder
	// delete empty folders in storage
	// ignore list?
	// handle deleting files better?
	//status file?
	//handle replacing stored dir/files with new incoming ones
string processSyncDir(string syncDirLoc, string storeDirLoc, int tabLevel = 0, bool verbosity = verbose, string configFileName = clientConfigFileName, string syncFileListName = clientSyncFileListName, string getListFileName = clientSyncGetFileListName, int thisNumBackupsToKeep = numBackupsToKeepDef, string thisFoldSeparater = foldSeparater, string thisnlc = nlc) {
	string output = outputText("r", "Processing folder:\"" + syncDirLoc + "\"...", false, tabLevel);
	output += outputText("r", "Storage directory: \"" + storeDirLoc + "\"", false, tabLevel + 1);

	//config values
	int numToKeep = thisNumBackupsToKeep;


	//config and other list locations
	string syncConfigLoc = syncDirLoc + thisFoldSeparater + configFileName;
	string syncFileListLoc = syncDirLoc + thisFoldSeparater + syncFileListName;
	string syncRetrieveListLoc = syncDirLoc + thisFoldSeparater + getListFileName;

	//lists of files
	vector<string> getList;
	vector<string> ignoreList;
	//add the defaults to ignore list.
	ignoreList.push_back(syncConfigLoc);
	ignoreList.push_back(syncFileListLoc);
	ignoreList.push_back(syncRetrieveListLoc);

	//process config folder
	output += ensureFolderConfig(syncConfigLoc);
	output += readFolderConfig(syncConfigLoc, &numToKeep, delimeter + "", tabLevel + 1);



	//get list from getList file
	output += getListOfFilesToGet(syncRetrieveListLoc, &getList);
	output += outputText("r", "Getting the following files back:", false, tabLevel + 1);
	if(getList.size() == 0){
		output += outputText("r", "--None--", false, tabLevel + 2);
	}else{
		for (vector<string>::iterator it = getList.begin(); it != getList.end(); ++it) {
			output += outputText("r", *it, false, tabLevel + 2);
		}
	}
	output += outputText("r", "End List.", false, tabLevel + 1);

	//add items in get list to ignore list
	ignoreList.insert(ignoreList.end(), getList.begin(), getList.end());

	//ensure storage folder present
	output += ensureStorageDir(storeDirLoc, tabLevel + 1);

	//check if any errors have happened so far, exit if any did.
	if (output.find("ERROR::") != string::npos) {
		output += outputText("r", "ERROR:: Errors in run. View log to see what happened (search \"ERROR::\")", false, tabLevel + 1);
		return output + outputText("r", "Completed processing sync directory: " + syncDirLoc, verbosity, tabLevel);
	}

	//for each file in sync folder (not in ignore list), move it into the storage folder, removing it from the sync folder
	output += moveSyncFolderContents(syncDirLoc, storeDirLoc, &ignoreList, "", tabLevel + 1);

	//if not already moved to sync, move files in retrieveList to sync. remove them from storage
	output += moveFilesToGet(storeDirLoc, syncDirLoc, &getList, "", tabLevel + 1);

	//crop the number of files in storage
	output += cropNumInStor(storeDirLoc, thisNumBackupsToKeep);

	//refresh storage file list
	output += refreshFileList(syncFileListLoc, storeDirLoc);

	return output + outputText("r", "Completed processing sync directory: " + syncDirLoc, verbosity, tabLevel);
}

/**
	Searches the sync folder for folders to be searched.
*/
void searchSyncDir() {
	outputText("cl", "Begin processing of folders in sync folder directory...", verbose, 0);
	vector<string> fileList;
	vector<string> dirList;

	getItemsInDir(syncFolderLoc, &fileList, &dirList);

	unsigned long numDir = dirList.size();

	if(numDir > 0){
		outputText("cl", "# of folders to process: " + to_string(numDir), verbose, 1);
		for (vector<string>::iterator it = dirList.begin(); it != dirList.end(); ++it) {
			string curSyncFolder = syncFolderLoc + foldSeparater + *it;
			string curStorFolder = storFolderLoc + foldSeparater + *it;
			if (checkFilePath(curSyncFolder, true)) {
				//outputText("cl", "Processing sync folder \"" + curSyncFolder + "\"...", verbose, 2);
				//TODO:: do this with thread pooling
				outputText("cl", "Finished processing folder. Output:" + nlc + nlc + processSyncDir(curSyncFolder, curStorFolder) + "End of output." + nlc, verbose, 1);
				//cropNumInStor(curSyncFolder, searchInnerSyncDir(curSyncFolder));
			}
		}
	}else{
		outputText("cl", "---- No sync folders present in sync folder directory. ----", verbose, 1);
	}
	outputText("cl", "Done processing folders in sync folder directory.", verbose, 0);
}//searchSyncDir

 /////////////////
#pragma endregion SyncFolderOps
 /////////////////





///////////////
#pragma region MiscOperations - Other operations not in another category
///////////////

bool setRunOnStart() {
	outputText("c", "Setting to run the server on start.", verbose);
	//ofstream log (logFile.c_str(), ios::out | ios::app);
	//TODO:: this
}//setRunOnStart()

/////////////////
#pragma endregion MiscOperations
/////////////////





///////////////
#pragma region MainServerOps - Operations for running the main server.
///////////////

/**
	Runs the server. Sets up and reads configuration file, handles waiting before running through the sync folders again.
*/
void runServer() {
	bool okay = true;
	outputText("cl", nlc + nlc + nlc + nlc + nlc + "######## START SERVER ########", verbose);

	//test to see if sync and storage folders are present. create them if not.
	if (!checkFilePath(syncFolderLoc, true)) {
		createDirectory(syncFolderLoc);
		if (!checkFilePath(syncFolderLoc, true)) {
			outputText("cl", "******** FAILED TO CREATE/ OPEN SYNC DIRECTORY. EXITING. ********", verbose, 0);
			okay = false;
		}
		else {
			outputText("cl", "Created Sync Directory.", verbose, 0);
		}
	}

	if (!checkFilePath(storFolderLoc, true)) {
		createDirectory(storFolderLoc);
		if (!checkFilePath(storFolderLoc, true)) {
			outputText("cl", "******** FAILED TO CREATE/ OPEN STORAGE DIRECTORY. EXITING. ********", verbose, 0);
			okay = false;
		}
		else {
			outputText("cl", "Created Storage Directory.", verbose, 0);
		}
	}

	string waitIntStr;
	ostringstream convert;   // stream used for the conversion
	convert << checkInterval;
	waitIntStr = convert.str();
	if (okay) {
		while (okay) {
			searchSyncDir();
			outputText("cl", "Done folder processing. Waiting " + waitIntStr + " seconds..." + nlc + nlc + nlc, verbose, 0);
			mySleep(checkInterval);
			outputText("cl", "Done waiting.", verbose, 0);
			outputText("cl", "", verbose);
			//TODO:: check for errors?
		}
	} else {
		outputText("cl", "Something went wrong. Exiting execution of server.", verbose, 0);
	}
	outputText("cl", "######## END SERVER RUN ########", verbose);
}//runServer

/**
	Generates the default main configuration file at the current working directory of the executable (where it will look for it)
*/
void generateMainConfig() {
	outputText("c", "Generating Main Config File...", verbose);
	ofstream confFile(confFileLoc.c_str(), ios::app);
	if (!confFile.good()) {
		outputText("c", "ERROR:: Unable to create or open config file.", true);
		return;
	}
	confFile << "logFileLoc" << delimeter << logFileLoc.c_str() << endl <<
		"syncFolderPred" << delimeter << syncFolderPred.c_str() << endl <<
		"syncFolderLoc" << delimeter << syncFolderLoc.c_str() << endl <<
		"storFolderPred" << delimeter << storFolderPred.c_str() << endl <<
		"storFolderLoc" << delimeter << storFolderLoc.c_str() << endl <<
		"checkInterval" << delimeter << checkInterval;
	confFile.close();
	outputText("c", "Done.", verbose);
	return;
}//generateConfig()

/**
	Reads the main configuration file into global variables to work with.
*/
void readMainConfig() {
	ifstream confFile(confFileLoc.c_str()); // declare file stream: http://www.cplusplus.com/reference/iostream/ifstream/
	string variable, value;

	while (confFile.good()) {
		getline(confFile, variable, delimeter);
		getline(confFile, value, '\n');
		//cout << "\"" << variable << "\", \"" << value << "\"" << endl;
		if (variable == "logFileLoc") {
			logFileLoc = value;
		}
		else if (variable == "syncFolderPred") {
			syncFolderPred = value;
		}
		else if (variable == "syncFolderLoc") {
			syncFolderLoc = value;
		}
		else if (variable == "storFolderPred") {
			storFolderPred = value;
		}
		else if (variable == "storFolderLoc") {
			storFolderLoc = value;
		}
		else if (variable == "checkInterval") {
			checkInterval = atoi(value.c_str());
		}
	}//while reading config file

	confFile.close();
}//readConfig()


/////////////////
#pragma endregion
/////////////////


























/*
	Function for testing functions. "Remove for end product"
*/
void test(){
	cout << "test" << endl;
	createDirectory("test1");
	createDirectory("test2/test3");
}




/**
 * Standard main function. Use "-h" argument for help on how to run.
 */
int main(int argc, char *argv[]){
	if(argc < 2){//if got no arguments, output usage
		cout << "Usage:" << endl <<
			argv[0] << " <command> [command]..." << endl <<
			"List commands: -h" << endl;
		return 1;
	}

	/*
		Check for options (set flags)
	*/
	//cout << "getting inputs..." << endl;//debugging
	int curArg = 1;
	while(curArg < argc){
		//cout << "\t\"" << argv[curArg] << "\" ";//debugging
		if(string(argv[curArg]) == "-h"){
			//cout << "displaying help" << endl;//debugging
			helpFlag = true;
		}else if(string(argv[curArg]) == "-r"){
			//cout << "running server" << endl;//debugging
			runFlag = true;
		}else if(string(argv[curArg]) == "-l"){
			//cout << "listing folders" << endl;//debugging
			listFlag = true;
		}else if(string(argv[curArg]) == "-v"){
			//cout << "being verbose" << endl;//debugging
			verbose = true;
		}else if(string(argv[curArg]) == "-c"){
			//cout << "Changing config location." << endl;//debugging
			curArg++;
			confFileLoc = string(argv[curArg]);
			outputText("c", "Config file set to: \"" + confFileLoc + "\"", true);
		}else if(string(argv[curArg]) == "-s"){
			setToStartOnStart = true;
		}else if(string(argv[curArg]) == "-t"){
			runTest = true;
		}else{//debugging
			//cout << endl;
		}
		curArg++;
	}
	//cout << "Done." << endl;
	~curArg;

	if(runTest){
		test();
		return 0;
	}

	if(helpFlag){
		cout << endl <<
			"Decent Backup System Server" << endl <<
			"\tCopyright 2016 Epic Breakfast Productions" << endl <<
			"\tVersion: " << version << endl << endl <<
			"Commands:" << endl <<
			"\t-h        Display help." << endl <<
			"\t-r        Run the server." << endl <<
			"\t-v        Verbose output." << endl <<
			"\t-l        List files and folders used." << endl <<
			"\t-s        Set this to be run on startup." << endl <<
			"\t-c <loc>  Specify location of config file." << endl << endl;
		return 0;
	}

	/**
		Read config file for configuration stuff, creating it as necessary.
	*/
	if(!checkFilePath(confFileLoc, false) ){
		outputText("c", "Unable to open config file. Creating a new one...", verbose);
		generateMainConfig();
		if(!checkFilePath(confFileLoc, false) ){
			outputText("c", "ERROR:: Unable to create or open config file.", true);
			return 1;
		}
	}

	readMainConfig();

	if(listFlag){
		cout << "Files and folders:" << endl <<
			"\tConfig File:       " << confFileLoc << endl <<
			"\tLog File:          " << logFileLoc << endl <<
			"\tSync Directory:    " << syncFolderLoc << endl <<
			"\tStorage Directory: " << storFolderLoc << endl;
	}

	if(setToStartOnStart){
		setRunOnStart();
	}

	if(runFlag){
		runServer();
	}

	if(!helpFlag && !runFlag && !listFlag && !setToStartOnStart){
		outputText("c", "Not told to do anything. Use -h to see options.", verbose);
	}


	return 0;
}//main
