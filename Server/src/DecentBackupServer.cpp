 /*
	Decent Backup System Server

	Copyright 2016 Epic Breakfast Productions

	Author: Greg Stewart

	Notes:
		Using C++ standard 2011 (c++11x or c++0x)

	TODO::
		- Make config ops use the new functions
*/

///////////////
#pragma region include_globals - Includes and global variables.
///////////////

//includes
#include <stdlib.h>//for many things
#include <vector>//for lists of things
#include <map>//for data pairs
#include <string.h>//for strings
#include <sstream>//for string stream on the outputs
#include <iostream>//to do console stuff
#include <fstream>//to do file stuff
#include <time.h>//for time measurement
#include <algorithm>//for finding element in vector
#include <sys/stat.h>//for checking filepaths
#include <dirent.h>//for looking at contents of directory
//sys dependent stuff
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
const string version = "A1.0.2";//version number
const char delimeter = ';';//the delimiter to use betwen key/values for config files
string logFileLoc = "DBSS_LOG.txt";//the log file
string confFileLoc = "DBSS_CONF.txt";//the config file

string syncFolderPred = "DBSS_sync_";// the filename predicate for sync folders
string storFolderPred = "DBSS_stor_";// the filename predicate for storage folders

string syncFolderLoc = syncFolderPred + "dir";
string storFolderLoc = storFolderPred + "dir";

//sync folder files
const string clientConfigFileName = "DBSS_CLIENT_CONF.txt";
const string clientSyncFileListName = "DBSS_STORED_FILES.txt";
const string clientSyncGetFileListName = "DBSS_TO_GET_LIST.txt";
const string clientSyncIgnoreFileListName = "DBSS_IGNORE_LIST.txt";

int checkInterval = 60;//how often to wait between checking sync foldders in seconds

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
int transferWait = 3;//the amount of time to wait to se if a file has finished transferring, in seconds

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
}//mySleepMs(int)

/**
	Sleeps for a specified amount of seconds.

	@param sleepS The number of seconds to sleep for.
 */
void mySleep(int sleepS){
	mySleepMs(sleepS * 1000);
}//mySleep(int)

/**
	Checks if the file path given is present.

	@param pathIn The path to examine if it is present
	@param dir If we are to determine of the path is a directory or not.
	@return If the path given is valid.
 */
bool checkFilePath(string pathIn, bool dir){
    bool worked = false;//if things worked
    struct stat pathStat;//buffer for the stat
    //check if valid
    if(worked = (lstat(pathIn.c_str(), &pathStat) == 0)){
        //check if a file or directory
        if((S_ISDIR(pathStat.st_mode)) && dir){
            worked = true;
        }else if((pathStat.st_mode && S_IFREG) && !dir){//check if valid filetype
            worked = true;
        }else{
            worked = false;
        }
    }else{//if valid path
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
}//getTimestamp()

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
}//getTabs(int)

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
}//outputText(string, string, bool, int)

/**
	Gets the folder separater character in a url.
	Returns '\0' on not finding a folder separater.

	@param url The url to get the folder separating character from.
	@return The folder separator character.
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
}//getSeparater(string)

/**
	Creates a directory.

	TODO:: check for possible infinite loops? will do so if cannot ever create any of the parent directories

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
}//createDirectory(string)

void removeDirectory(string dirLocation) {
#ifdef __linux__
	rmdir(dirLocation.c_str());
#endif
#ifdef _WIN32
	RemoveDirectory(dirLocation.c_str());
#endif
}//removeDirectory(string)

/**
	Gets the last file or folder in a given file path.

	@param path The path we are concerned with.
*/
string getLastPartOfPath(string path) {
	return path.substr(path.find_last_of(foldSeparater) + 1);
}//getLastPartOfPath(string)

/**
	Gets the size of the file.

	@param file The file to get the size of.
	@return The size of the file.
*/
long getFileSize(string file) {
	struct stat filestatus;
	stat(file.c_str(), &filestatus);
	return filestatus.st_size;
}//getFileSize(string)

/**
	Copies a file from a path into the given directory. Creates the destination directory if it is not present.

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

	// check/wait for file to be done transferring
	output += outputText("r", "Checking that file is not being transferred by another process...", false, tabLevel + 1);
	long initSize, tempSize;
	do {
		initSize = getFileSize(fromPath);
		mySleep(transferWait);
		tempSize = getFileSize(fromPath);
		if(initSize != tempSize){
			output += outputText("r", "Still transferring.", false, tabLevel + 2);
		}
	} while (initSize != tempSize);
	output += outputText("r", "Done.", false, tabLevel + 1);

	//open file streams
	ifstream  src(fromPath.c_str(), ios::binary);
	ofstream  dst(toPath.c_str(), ios::binary);
	//move data
	dst << src.rdbuf();
	//close the files
	src.close();
	dst.close();

	if(!checkFilePath(toPath, false)){
		output += outputText("r", "ERROR:: file NOT copied. Destination file not present after copy attempt.", false, tabLevel + 1);
	}
	return output + outputText("r", "Done.", false, tabLevel);
}//copyFile(string, string, int)

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
}//getItemsInDir(string, vector<string>*, bool)

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
}//getItemsInDir(string, vector<string>*, vector<string>*, bool)

/**
	Gets the directories in a given directory.

	@param dirLoc The location of the directory we are getting the items for.
	@param dirList The vector to push all the subdirectories into.
	@param wholePath Optional param to add the whole path to the files & folders.
 */
void getDirsInDir(string dirLoc, vector<string>* dirList, bool wholePath = false) {
	//cout << "Getting items in \"" << dirLoc << "\":";
	vector<string> itemList;
	getItemsInDir(dirLoc, &itemList, wholePath);
	for (vector<string>::iterator it = itemList.begin(); it != itemList.end(); ++it) {
		if (checkFilePath(dirLoc + foldSeparater + *it, true)) {//is directory
			dirList->push_back(*it);
		}
	}//foreach item in dir
}//getDirsInDir(string, vector<string>*, bool)

/**
	Determines if the given directory is empty.

	@param dirLoc The location of the directory to test for emptiness.
*/
bool dirIsEmpty(string dirLoc) {
	vector<string> itemList;
	getItemsInDir(dirLoc, &itemList);
	return itemList.size() == 0;
}//dirIsEmpty(string)

/**
	Determines if the givben directory is not empty.

	@param dirLoc The location of the directory to test for emptiness.
*/
bool dirIsNotEmpty(string dirLoc) {
	return !dirIsEmpty(dirLoc);
}//dirIsNotEmpty(string)

/**
	Generalized function for getting a list of files from a file.

	Accepts a '#' as a comment, and ignores the line.

	@param fileListFile The file to read the list from.
	@param fileList The vector to put the locations into.
 */
void readFileList(string fileListLoc, vector<string>* fileList){
	ifstream fileListFile(fileListLoc.c_str());
	string curLine;
	while (fileListFile.good()) {
		getline(fileListFile, curLine, '\n');
		if (curLine != "" && curLine.at(0) != '#') {
			fileList->push_back(curLine);
		}
	}
	fileListFile.close();
}//readFileList(string, vector<string>*)

/**
Reads data pairs from a file, and puts them into a provided 2D vector.

Accepts a '#' as a comment, and ignores the line.

@param dataPairLoc The location of the file with the data pairs.
@param dataPairs The map we are placing the pairs into.
@param delim Optional param for setting the delimeter used in the file.
 */
void readDataPairs(string dataPairLoc, map<string, string>* dataPairs, string delim = delimeter){
	ifstream fileListFile(dataPairLoc.c_str());
	string curLine, key, data;
	while (fileListFile.good()) {
		getline(fileListFile, key, delim.c_str()[0]);
		if (key != "" && key.at(0) != '#') {
			getline(fileListFile, data, nlc.c_str()[0]);
			dataPairs->insert(make_pair(key, data));
		}
	}
	fileListFile.close();
}//readDataPairs(string, map<string, string>*, string)

/**
Writes out of key/value pairs to the given file location.

@param dataPairLoc The location of the file to write to.
@param dataPairs The data pairs to write out.
@param comments Comments to write out. Key is the line to put the comment (value) at.
@param delim Optional. The delimeter to use between the key/value pairs.
 */
void writeDataPairs(string dataPairLoc, map<string, string>* dataPairs, map<int, string>* comments, string delim = delimeter){
	unsigned long long int lineCount = 1;
	ofstream outFile(dataPairLoc.c_str());
	if (!outFile.good()) {
		return;
	}
	//add items to configuration file
	map<string, string>::iterator it = dataPairs->begin();
	while(it != dataPairs->end()){
		while(comments->find(lineCount) != comments->end()){
			outFile << "#" << comments->find(lineCount) << endl;
			lineCount++;
		}
		lineCount++;
		outFile << it->first << delim << it->second << endl;
	}
	outFile.close();
}//writeDataPairs(string, map<string, string>*, string)

/**
Writes out of key/value pairs to the given file location.

@param dataPairLoc The location of the file to write to.
@param dataPairs The data pairs to write out.
@param delim Optional. The delimeter to use between the key/value pairs.
 */
void writeDataPairs(string dataPairLoc, map<string, string>* dataPairs, string delim = delimeter){
	map<int, string> temp;
	writeDataPairs(dataPairLoc, dataPairs, &temp, delim);
}//writeDataPairs(string, map<string, string>*, string)

/**
	Removes any empty sub directories in the given directory.
	Recursively checks all sub folders

	@param parentDir The parent directory of the subfolders you want to remove.
 */
void removeEmptySubfolders(string parentDir){
	vector<string> subDirs;
	getDirsInDir(parentDir, &subDirs);
	string curDir;
	for (vector<string>::iterator it = subDirs.begin(); it != subDirs.end(); ++it) {
		curDir = parentDir + foldSeparater + *it;
		removeEmptySubfolders(curDir);
		if(dirIsEmpty(curDir)){
			removeDirectory(curDir);
		}
	}
}//removeEmptySubfolders(string)


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
}//generateFolderConfig(string, int)

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
}//ensureFolderConfig(string, int)

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
}//readFolderConfig(string, int*, string, int)

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
}//ensureStorageDir(string, int)

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
}//cropNumInStor(string, int, int)

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
}//refreshFileList(ofstream&, string, string, int)

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
}//refreshFileList(string, string, int)

/**
	Ensures the list of files to get is present.

	@param getListLoc The location of the list of files to get.
	@param tabLevel The number of tabs put in front of output lines.
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
}//ensureGetList(string, int)

/**
	Gets a list of files to retrieve and put into the sync folder, and clear the get list as it goes.

	@param syncRetrieveListLoc The location of the list of files to get.
	@param toGetList The pointer to the vector of the files to get.
	@param tabLevel The number of tabs put in front of output lines.
*/
string getListOfFilesToGet(string syncRetrieveListLoc, vector<string>* toGetList, int tabLevel = 1) {
	string output = outputText("r", "Getting list of files to keep in sync folder...", false, tabLevel);
	output += ensureGetList(syncRetrieveListLoc, tabLevel + 1);
	readFileList(syncRetrieveListLoc, toGetList);
	return output + outputText("r", "Done.", false, tabLevel);
}//getListOfFilesToGet(string, vector<string>*, int)

/**
	Ensures the list of files to ignore is present.

	@param ignoreListLoc The location of the ignore list file.
	@param tabLevel The number of tabs put in front of output lines.
 */
string ensureIgnoreList(string ignoreListLoc, int tabLevel = 1) {
	string output = outputText("r", "Ensuring ignore list is present...", false, tabLevel);
	if(!checkFilePath(ignoreListLoc, false)){
		output += outputText("r", "Get list not found. Creating...", false, tabLevel + 1);
		ofstream ignoreListFile(ignoreListLoc.c_str(), ofstream::trunc);
		ignoreListFile << "# default; accounts for typical folder sync services" << endl
			<< "/.sync" << endl << "# add yours after this line:" << endl;
		ignoreListFile.close();
		if(!checkFilePath(ignoreListLoc, false)){
			output += outputText("r", "ERROR:: Unable to create get list.", false, tabLevel + 2);
		}else{
			output + outputText("r", "Done.", false, tabLevel + 1);
		}
	} else {
		output += outputText("r", "Get list is present!", false, tabLevel + 1);
	}
	return output + outputText("r", "Done.", false, tabLevel);
}//ensureIgnoreList(string, int)

/**
	Gets the list of files to ignore from the file.

	@param syncIgnoreListLoc The location of the ignore list file.
	@param toIgnoreList The vector we are putting th items to ignore into.
	@param tabLevel The number of tabs put in front of output lines.
 */
string getListOfFilesToIgnore(string syncIgnoreListLoc, vector<string>* toIgnoreList, int tabLevel = 1) {
	string output = outputText("r", "Getting list of files to ignore in sync folder...", false, tabLevel);
	output += ensureGetList(syncIgnoreListLoc, tabLevel + 1);
	readFileList(syncIgnoreListLoc, toIgnoreList);
	return output + outputText("r", "Done.", false, tabLevel);
}//getListOfFilesToIgnore(string, vector<string>*, int)

/**
	Moves the contents of a folder into another storage folder.

	@param fromFolderLoc The location of the folder to move things from.
	@param toFolderLoc The location of the folder to move things to.
	@param ignoreList The pointer of the list of files to ignore.
	@param levels The levels to prepend to outputs. (keep as empty string for first call)
	@param tabLevel The number of tabs put in front of output lines.
*/
string moveFolderContents(string fromFolderLoc, string toFolderLoc, vector<string>* ignoreList, string levels = "", int tabLevel = 1) {
	string output = outputText("r", "Moving files and folders in \"" + syncFolderLoc + levels + "\" to \"" + storFolderLoc + levels + "\"...", false, tabLevel);

	//get directory and file list
	vector<string> dirList;
	vector<string> fileList;
	getItemsInDir(fromFolderLoc + levels, &fileList, &dirList);

	//process directories recursively
	unsigned long numDir = dirList.size();
	output += outputText("r", "Processing sub folders (" + to_string(numDir) + ")...", false, tabLevel + 1);
	if(dirList.size() == 0){
		output += outputText("r", "--None--", false, tabLevel + 2);
	}else{
		for (vector<string>::iterator it = dirList.begin(); it != dirList.end(); ++it) {
			if (find(ignoreList->begin(), ignoreList->end(), levels + foldSeparater + *it) != ignoreList->end()) {
				output += outputText("r", "Skipping (in ignore or get list) \"" + levels + foldSeparater + *it + "\".", false, tabLevel + 2);
			}else{
				output += moveFolderContents(fromFolderLoc, toFolderLoc, ignoreList, levels + foldSeparater + *it, tabLevel + 2);
			}
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
			if (find(ignoreList->begin(), ignoreList->end(), levels + foldSeparater + *it) != ignoreList->end()) {
				output += outputText("r", "Skipping (in ignore or get list) \"" + levels + foldSeparater + *it + "\".", false, tabLevel + 2);
			} else {
				//move file
				output += outputText("r", "Moving \"" +  levels + foldSeparater + *it + "\"...", false, tabLevel + 2);
				output += copyFile(fromFolderLoc + levels + foldSeparater + *it, toFolderLoc + levels, tabLevel + 3);
				output += outputText("r", "Done.", false, tabLevel + 2);
				//remove file
				remove((fromFolderLoc + levels + foldSeparater + *it).c_str());
			}
		}
	}
	output += outputText("r", "Done.", false, tabLevel + 1);

	return output + outputText("r", "Done.", false, tabLevel);
}//moveFolderContents(string, string, vector<string>*, string, int)

/**
	Moves files back from the storage folder.

	@param storFolderLoc The location of the storage folder.
	@param syncFolderLoc The location of the syncing folder.
	@param getList The vector list of files to get.
	@param tabLevel The number of tabs put in front of output lines.
 */
string moveFilesToGet(string storFolderLoc, string syncFolderLoc, vector<string>* getList, int tabLevel = 1) {
	string output = outputText("r", "Moving files and folders to get in \"" + storFolderLoc + "\" to \"" + syncFolderLoc + "\"...", false, tabLevel);
	if(getList->size() == 0){
		output += outputText("r", "--No files to move--", false, tabLevel + 2);
	}else{
		//iterate through each in get list, moving it if need be.
		for (vector<string>::iterator it = getList->begin(); it != getList->end(); ++it) {
			//setup file locations for operation
			string storFileLoc = storFolderLoc + *it;
			string syncFileLoc = syncFolderLoc + *it;
			string syncFileDirLoc = syncFileLoc.substr(0, syncFileLoc.size() - getLastPartOfPath(*it).size());

			//see if it is in storage
			if(checkFilePath(storFileLoc, true)){//folder
				vector<string> emptyVect;
				output += outputText("r", "Moving all files and folders in \"" + storFileLoc + "\"...", false, tabLevel + 1);
				output += moveFolderContents(storFileLoc, syncFileLoc, &emptyVect, "", tabLevel + 2);
				output += outputText("r", "Done.", false, tabLevel + 1);
			}else if(checkFilePath(storFileLoc, false)){//file
				//move the file to storage.
				output += outputText("r", "Moving \"" + storFileLoc + "\"...", false, tabLevel + 1);
				output += copyFile(storFileLoc, syncFileDirLoc, tabLevel + 2);
				remove(storFileLoc.c_str());
				output += outputText("r", "Done.", false, tabLevel + 1);
			}else if(checkFilePath(syncFileLoc, false)){//check if not already in sync
				output += outputText("r", "Already moved: \"" + *it + "\"", false, tabLevel + 1);
			}else{
				output += outputText("r", "WARNING:::: Appears that \"" + *it + "\" is in neither the storage or sync directories.", false, tabLevel + 1);
			}
		}//foreach of the files to get
	}//if there are files to get
	return output + outputText("r", "Done.", false, tabLevel);
}//moveFilesToGet(string, string, vector<string>*, int)

// TODO::
	// status file?
/**
	Processes an entire sync folder to move things accordingly.
	TODO:: doc
 */
string processSyncDir(string syncDirLoc, string storeDirLoc, int tabLevel = 0, bool verbosity = verbose, string configFileName = clientConfigFileName, string syncFileListName = clientSyncFileListName, string getListFileName = clientSyncGetFileListName, string ignoreListFileName = clientSyncIgnoreFileListName, int thisNumBackupsToKeep = numBackupsToKeepDef, string thisFoldSeparater = foldSeparater, string thisnlc = nlc) {
	string output = outputText("r", "Processing folder:\"" + syncDirLoc + "\"...", false, tabLevel);
	output += outputText("r", "Storage directory: \"" + storeDirLoc + "\"", false, tabLevel + 1);

	//config values
	int numToKeep = thisNumBackupsToKeep;

	//config and other list locations
	string syncConfigLoc = syncDirLoc + thisFoldSeparater + configFileName;
	string syncFileListLoc = syncDirLoc + thisFoldSeparater + syncFileListName;
	string syncRetrieveListLoc = syncDirLoc + thisFoldSeparater + getListFileName;
	string syncIgnoreListLoc = syncDirLoc + thisFoldSeparater + ignoreListFileName;

	//lists of files
	vector<string> getList;
	vector<string> ignoreList;
	//add the defaults to ignore list.
	ignoreList.push_back(thisFoldSeparater + configFileName);
	ignoreList.push_back(thisFoldSeparater + syncFileListName);
	ignoreList.push_back(thisFoldSeparater + getListFileName);
	ignoreList.push_back(thisFoldSeparater + ignoreListFileName);
	//process config folder
	output += ensureFolderConfig(syncConfigLoc);
	output += readFolderConfig(syncConfigLoc, &numToKeep, delimeter + "", tabLevel + 1);

	//process ignore folder
	output += ensureIgnoreList(syncIgnoreListLoc);
	output += getListOfFilesToIgnore(syncIgnoreListLoc, &ignoreList);

	//get list from getList file
	output += getListOfFilesToGet(syncRetrieveListLoc, &getList);
	output += outputText("r", "Getting the following files back:", false, tabLevel + 1);
	if(getList.size() == 0){
		output += outputText("r", "--None--", false, tabLevel + 2);
	}else{
		for (vector<string>::iterator it = getList.begin(); it != getList.end(); ++it) {
			output += outputText("r", "\"" + *it + "\"", false, tabLevel + 2);
		}
	}
	output += outputText("r", "End List.", false, tabLevel + 1);

	//add items in get list to ignore list
	ignoreList.insert(ignoreList.end(), getList.begin(), getList.end());
	output += outputText("r", "Ignoring the following files in the sync folder:", false, tabLevel + 1);
	if(ignoreList.size() == 0){
		output += outputText("r", "--None--", false, tabLevel + 2);
	}else{
		for (vector<string>::iterator it = ignoreList.begin(); it != ignoreList.end(); ++it) {
			output += outputText("r", "\"" + *it + "\"", false, tabLevel + 2);
		}
	}
	output += outputText("r", "End List.", false, tabLevel + 1);

	//ensure storage folder present
	output += ensureStorageDir(storeDirLoc, tabLevel + 1);

	//check if any errors have happened so far, exit if any did.
	if (output.find("ERROR::") != string::npos) {
		output += outputText("r", "ERROR:: Errors in run. View log to see what happened (search \"ERROR::\")", false, tabLevel + 1);
		return output + outputText("r", "Completed processing sync directory: " + syncDirLoc, verbosity, tabLevel);
	}

	//for each file in sync folder (not in ignore list), move it into the storage folder, removing it from the sync folder
	output += moveFolderContents(syncDirLoc, storeDirLoc, &ignoreList, "", tabLevel + 1);

	//if not already moved to sync, move files in retrieveList to sync. remove them from storage
	output += moveFilesToGet(storeDirLoc, syncDirLoc, &getList, tabLevel + 1);

	//crop the number of files in storage
	output += cropNumInStor(storeDirLoc, thisNumBackupsToKeep);

	//refresh storage file list
	output += refreshFileList(syncFileListLoc, storeDirLoc);

	//delete empty folders
	output += outputText("r", "Removing empty sub folders in storage...", verbosity, tabLevel + 1);
	removeEmptySubfolders(storeDirLoc);
	output += outputText("r", "Done", verbosity, tabLevel + 1);

	return output + outputText("r", "Completed processing sync directory: " + syncDirLoc, verbosity, tabLevel);
}// processSyncDir(string, string, int, bool, string, string, string, string, int, string, string)

/**
	Searches the sync folder for folders to be searched.

	Uses processSyncDir() on each folder found in the sync parent dir.
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
				//TODO:: do this with thread pooling
				outputText("cl", "Finished processing folder. Output:" + nlc + nlc + processSyncDir(curSyncFolder, curStorFolder) + "End of output." + nlc, verbose, 1);
			}
		}
	}else{
		outputText("cl", "---- No sync folders present in sync folder directory. ----", verbose, 1);
	}
	outputText("cl", "Done processing folders in sync folder directory.", verbose, 0);
}//searchSyncDir()

 /////////////////
#pragma endregion SyncFolderOps
 /////////////////





///////////////
#pragma region MiscOperations - Other operations not in another category
///////////////

bool setRunOnStart() {
	outputText("c", "Setting to run the server on start.", verbose);
	//ofstream log (logFile.c_str(), ios::out | ios::app);
	//TODO:: this for both windows and linux
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
}//runServer()

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
#pragma endregion MainServerOps
/////////////////





/*
	Function for testing functions. "Remove for end product"
*/
void test(){
	cout << "test" << endl;
	createDirectory("test1");
	createDirectory("test2/test3");
}//test()

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
}//main()

#ifdef _WIN32
///////////////
#pragma region Service - Sets up windows services functions.
///////////////

/////////////////
#pragma endregion
/////////////////
#endif





#ifdef __linux__
///////////////
#pragma region Daemon - Sets up linux daemon functions.
///////////////

/////////////////
#pragma endregion
/////////////////
#endif
