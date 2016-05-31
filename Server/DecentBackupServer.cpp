 /*
	Decent Backup System Server

	Copyright 2016 Epic Breakfast Productions

	Author: Greg Stewart
*/

///////////////
#pragma region include_globals
///////////////

//includes TODO:: check for redundant includes
#include <cstdlib>
#include <stdlib.h>

#include <iostream>//to do console stuff
#include <fstream>//to do file stuff
#include <stdio.h>//file stuff?
#include <sstream>//for string stream on the outputs

#include <time.h>//for time measurement
#include <string>//for strings
#include <string.h>//for strings
#include <sys/stat.h>//for checking filepaths
#include <vector>//for lists of things
#include <dirent.h>//for looking at contents of directory

//#include <algorithm> //for checking file extentions

//sleep stuff/ other sys dependent stuff
#ifdef __linux__
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
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
const string clientSyncGetFileList = "DBSS_TO_GET_LIST.txt";

int checkInterval = 60;//in seconds

/* Option Flags */
bool runFlag = false;//if to run the server or not
bool helpFlag = false;//if to display the help (stops the execution after displaying help)
bool listFlag = false;//
bool verbose = false;//if to output much more to console
bool setToStartOnStart = false;//if to set the program to run on startup

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
	return NULL;
}//outputText

/**
	Creates a directory.

	@param directoryLocation The directory you wish to create.
*/
void createDirectory(string directoryLocation){
	string dir = "mkdir " + directoryLocation;
	system(dir.c_str());
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
void copyFile(string fromPath, string toDir) {
	if (!checkFilePath(toDir, true)) {
		createDirectory(toDir);
	}
	//create destination location
	string toPath = toDir + foldSeparater + getLastPartOfPath(fromPath);
	//open file streams
	ifstream  src(fromPath.c_str(), ios::binary);
	ofstream  dst(toPath.c_str(), ios::binary);
	//move data
	dst << src.rdbuf();
	//close the files
	src.close();
	dst.close();
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
	Gets the items in a directory and puts them into a provided vector. Items are without the rest of the path.

	@param dirLoc The directory to get the contents of.
	@param itemList The pointer of the vector we are putting items into.
*/
void getItemsInDir(string dirLoc, vector<string>* itemList) {
	DIR *pDIR;
	struct dirent *entry;
	if (pDIR = opendir(dirLoc.c_str())) {
		while (entry = readdir(pDIR)) {
			if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
				itemList->push_back(entry->d_name);
			}
		}
		closedir(pDIR);
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
	vector<string> itemList;
	getItemsInDir(dirLoc, &itemList);

	for (vector<string>::iterator it = itemList.begin(); it != itemList.end(); ++it) {
		if (checkFilePath(dirLoc + foldSeparater + *it, true)) {//is directory
			if (wholePath) {
				dirList->push_back(dirLoc + foldSeparater + *it);
			} else {
				dirList->push_back(*it);
			}
		} else {
			if (wholePath) {
				fileList->push_back(dirLoc + foldSeparater + *it);
			} else {
				fileList->push_back(*it);
			}
		}
	}//foreach item in dir
}

/**
	Determines if the givben directory is empty.

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
*/
void generateFolderConfig(string configLoc) {
	ofstream confFile(configLoc.c_str());
	if (!confFile.good()) {
		outputText("cl", "ERROR:: Unable to create or open new sync config file.", verbose, 3);
		return;
	}
	confFile << "numBackupsToKeep" << delimeter << "5";
	confFile.close();
	return;
}

/**
Ensures the sync folder's config is present. Creates it if it is not there.

@param configLoc The location of the config file to create.
*/
void ensureFolderConfig(string configLoc) {
	//TODO:: this
}

/**
Reads the folder config into the globals.

@param configLoc The location of the config file to create.
*/
void readFolderConfig(string configLoc) {
	ifstream confFile(configLoc.c_str()); // declare file stream: http://www.cplusplus.com/reference/iostream/ifstream/
	string variable, value;

	while (confFile.good()) {
		getline(confFile, variable, delimeter);
		getline(confFile, value, '\n');
		//cout << "\"" << variable << "\", \"" << value << "\"" << endl;
		if (variable == "numBackupsToKeep") {
			numBackupsToKeep = atoi(value.c_str());
		}
	}
	confFile.close();
}

/**
Crops the number of files in storage to the number set by the config.

@param storDir The storage directory in question.
@param numToKeep The number of files to keep.
*/
void cropNumInStor(string storDir, int numToKeep) {
	if (numToKeep == -1) {
		return;
	}
	//ensure directory is there
	//TODO:: this
}

/**
	Refreshes the file list in the sync folder.

	@param fileListFile Pointer to the file stream that lists the files.
	@param storeDir The storage directory to get the list of files in.
	@param levels The levels to prepend to the entries into the fileList. (optional)
 */
string refreshFileList(ofstream& fileListFile, string storeDir, string levels = "", int tabLevel = 2) {
	string output = outputText("r", "Refreshing file list...", false, tabLevel);
	//TODO:: make threadsafe
	if (!fileListFile.good()) {
		output += outputText("cl", "ERROR:: Unable to write to file list.", verbose, tablevel + 1);
		return output + outputText("r", "Done.", false, tablevel);
	}

	vector<string> dirList;
	vector<string> fileList;
	getItemsInDir(storeDir, &fileList, &dirList);

	for (vector<string>::iterator it = dirList.begin(); it != dirList.end(); ++it) {
		refreshFileList(fileListFile, storeDir + foldSeparater + *it, levels + *it + foldSeparater);
	}

	for (vector<string>::iterator it = fileList.begin(); it != fileList.end(); ++it) {
		fileListFile << levels << *it << endl;
	}
	return output + outputText("r", "Done.", false, tablevel);
}

/**
	Refreshes the file list with the items in the storage directory.

	@param fileListLoc The location of the file list file.
	@param storeDir The location of the storage directory.
 */
void refreshFileList(string fileListLoc, string storeDir) {
	//TODO:: make threadsafe
	ofstream fileListFile(fileListLoc.c_str());
	refreshFileList(fileListFile, storeDir);
	fileListFile.close();
}


/**
	Moves the files in the file list given to the sync folder.

	@param
*/
void getListOfFilesToGet(string syncRetrieveListLoc, vector<string>* toGetList) {
	ifstream getListFile(syncRetrieveListLoc.c_str());
	string fileToGet;

	while (getListFile.good()) {
		getline(getListFile, fileToGet, '\n');
		fileToGet.erase(remove(fileToGet.begin(), fileToGet.end(), '\n'), fileToGet.end());
		fileToGet.erase(remove(fileToGet.begin(), fileToGet.end(), '\r'), fileToGet.end());
		if (fileToGet != "") {
			//cout << "file to get: \"" << fileToGet << "\"" << endl;
			toGetList->push_back(fileToGet);
		}
	}
	getListFile.close();
}


void moveFilesToGet(string storageDir, string syncDir, vector<string>* toGetList) {
	//TODO:: make this threadsafe

	if (toGetList->size() == 0) {
		outputText("cl", "No files to move.", verbose, 4);
		return;
	}
	for (vector<string>::iterator it = toGetList->begin(); it != toGetList->end(); ++it) {
		//cout << "Moving \"" + storageDir + foldSeparater + *it + "\"" << endl;
		if (checkFilePath(syncDir + foldSeparater + *it, false)) {
			outputText("cl", "\"" + storageDir + foldSeparater + *it + "\" already present in sync folder.", verbose, 4);
			continue;
		}

		outputText("cl", "Moving \"" + storageDir + foldSeparater + *it + "\"...", verbose, 4);
		if (checkFilePath(storageDir + foldSeparater + *it, false)) {
			copyFile(storageDir + foldSeparater + *it, syncDir);
		}
		else {
			outputText("cl", "Invalid file location. Copy cancelled.", verbose, 5);
		}
	}
	outputText("cl", "Done.", verbose, 4);
}

string processSyncDir(string syncDirLoc, string storeDirLoc, int tabLevel = 1, bool verbosity = verbose, string configFileName = clientConfigFileName, string syncFileListName = clientSyncGetFileList, string getListFileName = clientSyncFileListName, int thisNumBackupsToKeep = numBackupsToKeepDef, string thisFoldSeparater = foldSeparater, string thisnlc = nlc) {

	string output = "";

	//config and other list locations
	string syncConfigLoc = syncDirLoc + thisFoldSeparater + configFileName;
	string syncFileListLoc = syncDirLoc + thisFoldSeparater + syncFileListName;
	string syncRetrieveListLoc = syncDirLoc + thisFoldSeparater + getListFileName;

	vector<string> getList;
	vector<string> ignoreList;
	ignoreList.push_back(syncConfigLoc);
	ignoreList.push_back(syncFileListLoc);
	ignoreList.push_back(syncRetrieveListLoc);

	//process config folder
	ensureFolderConfig(syncConfigLoc);
	//TODO: finish this step


	//get list from getList file
	output += getListOfFilesToGet(syncRetrieveListLoc, &getList);

	//add items in get list to ignore list

	//ensure storage folder present

	//for each file in sync folder (not in getList or ignore list), move it into the storage folder, removing it from the sync folder


	//if not already moved to sync, move files in retrieveList to sync. remove them from storage
	output += moveFilesToGet(storeDirLoc, syncDirLoc, &getList);

	//crop the number of files in storage
	output += cropNumInStor(storeDirLoc, thisNumBackupsToKeep);

	//refresh storage file list
	output += refreshFileList(syncFileListLoc, storeDirLoc);

	return output + outputText("r", "Completed processing sync directory: " + syncDirLoc, verbosity, tablevel);
}

/**
	Searches the sync folder for folders to be searched.
*/
void searchSyncDir() {
	vector<string> fileList;
	vector<string> dirList;

	getItemsInDir(syncFolderLoc, &fileList, &dirList);

	for (vector<string>::iterator it = dirList.begin(); it != dirList.end(); ++it) {
		string curSyncFolder = syncFolderLoc + foldSeparater + *it;
		string curStorFolder = storFolderLoc + foldSeparater + *it;
		if (checkFilePath(curSyncFolder, true)) {
			outputText("cl", "Processing sync folder \"" + curSyncFolder + "\"...", verbose, 2);
			//TODO:: do this with thread pooling
			outputText("cl", processSyncDir(curSyncFolder, curStorFolder), verbose, 2);
			//cropNumInStor(curSyncFolder, searchInnerSyncDir(curSyncFolder));
		}
	}
	outputText("cl", "Done.", verbose, 2);
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









//processSyncFolder(string storageDir, string curDir, )

int searchInnerSyncDir(string dir) {
	//folder config vars
	string syncConfigLoc = dir + foldSeparater + clientConfigFileName;
	string syncFileListLoc = dir + foldSeparater + clientSyncFileListName;
	string syncRetrieveListLoc = dir + foldSeparater + clientSyncGetFileList;

	string thisStorageFolder = storFolderLoc + foldSeparater + getLastPartOfPath(dir);

	vector<string> getList;
	vector<string> ignoreList;
	ignoreList.push_back(syncConfigLoc);
	ignoreList.push_back(syncFileListLoc);
	ignoreList.push_back(syncRetrieveListLoc);


	numBackupsToKeep = numBackupsToKeepDef;

	//test if config present, make it if its not
	if (!checkFilePath(syncConfigLoc, false)) {
		outputText("cl", "**** Unable to open sync config file. Creating a new one (\"" + syncConfigLoc + "\")...", verbose, 3);
		generateFolderConfig(syncConfigLoc);
		if (!checkFilePath(syncConfigLoc, false)) {
			outputText("cl", "**** ERROR:: Unable to create or open sync config file.", verbose, 3);
			return -1;
		}
		else {
			outputText("cl", "**** Created default sync config for \"" + dir + "\".", verbose, 3);
		}
	}
	readFolderConfig(syncConfigLoc);

	//test if the file list is present
	if (!checkFilePath(syncFileListLoc, false)) {
		outputText("cl", "**** Unable to open stored file list. Creating a new one (\"" + syncFileListLoc + "\")...", verbose, 3);
		ofstream confFile(syncFileListLoc.c_str());
		confFile.close();
		if (!checkFilePath(syncFileListLoc, false)) {
			outputText("cl", "**** ERROR:: Unable to create or open stored file list.", verbose, 3);
			return -1;
		}
		else {
			outputText("cl", "**** Created stored file list for \"" + dir + "\".", verbose, 3);
		}
	}


	refreshFileList(syncFileListLoc, thisStorageFolder);

	//test if the list to get is present
	if (!checkFilePath(syncRetrieveListLoc, false)) {
		outputText("cl", "**** Unable to open list of files to get. Creating a new one (\"" + syncRetrieveListLoc + "\")...", verbose, 3);
		ofstream confFile(syncRetrieveListLoc.c_str());
		confFile.close();
		if (!checkFilePath(syncRetrieveListLoc, false)) {
			outputText("cl", "**** ERROR:: Unable to create or open list of files to get.", verbose, 3);
			return -1;
		}
		else {
			outputText("cl", "**** Created list of files to get for \"" + dir + "\".", verbose, 3);
		}
	}
	getListOfFilesToGet(syncRetrieveListLoc, &getList);

	outputText("cl", "Searching \"" + dir + "\" for new files to store...", verbose, 3);

	DIR *pDIR;
	struct dirent *entry;
	if (pDIR = opendir(dir.c_str())) {
		while (entry = readdir(pDIR)) {
			if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
				string curFile = dir + foldSeparater + (string)entry->d_name;
				if (checkFilePath(curFile, false)
					&& !(find(ignoreList.begin(), ignoreList.end(), curFile) != ignoreList.end())
					&& !(find(getList.begin(), getList.end(), (string)entry->d_name) != getList.end())) {
					outputText("cl", "Found: \"" + curFile + "\". Dealing with it...", verbose, 4);
					//wait until fully transferred
					outputText("cl", "Waiting/Checking for full sync transfer...", verbose, 5);
					long initSize, tempSize;
					do {
						initSize = getFileSize(curFile);
						mySleep(transferWait);
						tempSize = getFileSize(curFile);
					} while (initSize != tempSize);
					outputText("cl", "Done.", verbose, 4);
					//transfer to storage
					outputText("cl", "Transferring to storage folder...", verbose, 5);
					copyFile(curFile, thisStorageFolder);
					outputText("cl", "Done.", verbose, 4);

					//remove original file
					outputText("cl", "Removing file from sync folder...", verbose, 5);
					if (remove(curFile.c_str()) != 0) {
						outputText("cl", "***** Method returned nonzero.", verbose, 6);
					}
					outputText("cl", "Done.", verbose, 5);

					outputText("cl", "Done.", verbose, 4);
				}
			}
		}
		closedir(pDIR);
	}
	outputText("cl", "Done.", verbose, 3);
	//move files they want to retrieve to sync
	outputText("cl", "Moving files in list to get...", verbose, 3);
	moveFilesToGet(thisStorageFolder, dir, &getList);
	outputText("cl", "Done.", verbose, 3);

	//refresh list of files in storage
	refreshFileList(syncFileListLoc, thisStorageFolder);

}//searchInnerSyncDir







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
			outputText("cl", "******** FAILED TO CREATE/ OPEN SYNC DIRECTORY. EXITING. ********", verbose, 1);
			okay = false;
		}
		else {
			outputText("cl", "Created Sync Directory.", verbose, 1);
		}
	}

	if (!checkFilePath(storFolderLoc, true)) {
		createDirectory(storFolderLoc);
		if (!checkFilePath(storFolderLoc, true)) {
			outputText("cl", "******** FAILED TO CREATE/ OPEN STORAGE DIRECTORY. EXITING. ********", verbose, 1);
			okay = false;
		}
		else {
			outputText("cl", "Created Storage Directory.", verbose, 1);
		}
	}

	string waitIntStr;
	ostringstream convert;   // stream used for the conversion
	convert << checkInterval;
	waitIntStr = convert.str();
	if (okay) {
		while (okay) {
			outputText("cl", "Begin check loop.", verbose, 1);

			searchSyncDir();

			outputText("cl", "End Check Loop. Waiting " + waitIntStr + "s...", verbose, 1);
			mySleep(checkInterval);
			outputText("cl", "End Waiting.", verbose, 1);
			outputText("cl", "", verbose);
		}
	} else {
		outputText("cl", "Something went wrong. Exiting execution of server.", verbose, 1);
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
		}else{//debugging
			//cout << endl;
		}
		curArg++;
	}
	//cout << "Done." << endl;
	~curArg;

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
