/*
	Decent Backup System Server
	
	Copyright 2016 Epic Breakfast Productions
		
	Author: Greg Stewart
*/


//includes
#include <iostream>//to do console stuff
#include <fstream>//to do file stuff
#include <time.h>//for time measurement
#include <string>//for strings
#include <sys/stat.h>//for checking filepaths
#include <stdlib.h>

#include <stdio.h>
#include <cstdlib>
#include <string.h>
#include <dirent.h>
#include <algorithm> //for checking file extentions
#include <vector>

#include <sstream>

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
bool runFlag = false;
bool helpFlag = false;
bool listFlag = false;
bool verbose = false;
bool setToStartOnStart = false;

/* for sync folder config */
int numBackupsToKeepDef = -1;
int numBackupsToKeep = numBackupsToKeepDef;

/* Other */
int transferWait = 1;//seconds

//sleep stuff/ other sys dependent stuff
#ifdef __linux__
const string foldSeparater = "/";
const string nlc = "\n";
#endif
#ifdef _WIN32
const string foldSeparater = "\\";
const string nlc = "\r\n";
#endif

void mySleepMs(int sleepMs) {
#ifdef __linux__
	usleep(sleepMs * 1000);   // usleep takes sleep time in us (1 millionth of a second)
#endif
#ifdef _WIN32
	Sleep(sleepMs);
#endif
}

void mySleep(int sleepS){
	mySleepMs(sleepS * 1000);
}

/**
	Checks if the file path given is present.
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
    /*
    if(debugging & !worked){
        sendDebugMsg("checking file path \"" + pathIn + "\" failed.");
    }else if(debugging){
        sendDebugMsg("path given is valid");
    }
    */
    return worked;
}//checkFilePath(string)

bool isValidExtension(string extensionIn){
    if(extensionIn == "zip" || 
       extensionIn == "gz"
      ){
        return true;
    }else{
        return false;
    }
}//isValidExtension()

bool checkFileType(string pathIn){
    bool worked = true;
    //get extension from path and normalize it by making it uppercase
    string extension = pathIn.substr(pathIn.find_last_of(".") + 1);
    transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    //check if a valid filetype
    if(isValidExtension(extension)){//TODO: add more extensions
        worked = true;
    }else{
        worked = false;
    }
    return worked;
}//checkFileType(string)

string getTimestamp(){
	time_t rawtime;
	struct tm * timeinfo;
	char buffer [80];
	time (&rawtime);
	timeinfo = localtime (&rawtime);
	strftime (buffer,80,"%F %I:%M:%S %p",timeinfo);
	return (string)buffer;
}//getTimestamp

void outputText(char type, string message, bool verbosity){
	if(type == 'c' || type == ' '){//console output
		if(verbosity){
			cout << message << endl;
		}
	}
	if(type == 'l' || type == ' '){
		
		ofstream confFile (logFileLoc.c_str(), ios::app);
		if(message != ""){
			confFile << getTimestamp() << " - " << message << endl;
		}else{
			confFile << endl;
		}
		
		confFile.close();
	}
}//outputText

void outputText(char type, string message, bool verbosity, int tabLevel){
	string tabs = "";
	for(int i = 0; i < tabLevel; i++){
		tabs += "\t";
	}
	outputText(type, tabs + message, verbosity);
}

void createDirectory(string directoryLocation){
	string dir = "mkdir " + directoryLocation; 
	system(dir.c_str());
}

void generateFolderConfig(string configLoc){
	ofstream confFile (configLoc.c_str());
	if(!confFile.good()){
		outputText(' ', "ERROR:: Unable to create or open new sync config file.", verbose, 3);
		return;
	}
	confFile << "numBackupsToKeep" << delimeter << "5";
	confFile.close();
	return;
}

void readFolderConfig(string configLoc){
	ifstream confFile (configLoc.c_str()); // declare file stream: http://www.cplusplus.com/reference/iostream/ifstream/
	string variable, value;
	
	while ( confFile.good() ){
		getline ( confFile, variable, delimeter );
		getline ( confFile, value, '\n' );
		//cout << "\"" << variable << "\", \"" << value << "\"" << endl;
		if(variable == "numBackupsToKeep"){
			numBackupsToKeep = atoi(value.c_str());
		}		
	}
	confFile.close();
}

void cropNumInStor(string storDir, int numToKeep){
	if(numToKeep == -1){
		return;
	}
	//ensure directory is there
	//TODO:: this
}

string getLastPartOfPath(string path){
	return path.substr(path.find_last_of(foldSeparater) + 1);
}

void copyFile(string fromPath, string toDir){
	if(!checkFilePath(toDir, true)){
		createDirectory(toDir);
	}
	
	string toPath = toDir + foldSeparater + getLastPartOfPath(fromPath);
	
	ifstream  src(fromPath.c_str(), ios::binary);
    ofstream  dst(toPath.c_str(),   ios::binary);
	
    dst << src.rdbuf();
}

long getFileSize(string file){
	struct stat filestatus;
	stat( file.c_str(), &filestatus );
	return filestatus.st_size;
}

void getItemsInDir(string dirLoc, vector<string>* itemList){
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

void getItemsInDir(string dirLoc, vector<string>* fileList, vector<string>* dirList, bool wholePath = false){
	vector<string> itemList;
	getItemsInDir(dirLoc, &itemList);
	
	for (vector<string>::iterator it = itemList.begin(); it != itemList.end(); ++it) {
		if (checkFilePath(dirLoc + foldSeparater + *it, true)) {//is directory
			if(wholePath){
				dirList->push_back(dirLoc + foldSeparater + *it);
			}else{
				dirList->push_back(*it);
			}
		}else{
			if(wholePath){
				fileList->push_back(dirLoc + foldSeparater + *it);
			}else{
				fileList->push_back(*it);
			}
		}
	}
}

bool dirIsEmpty(string dirLoc){
	vector<string> itemList;
	getItemsInDir(dirLoc, &itemList);
	return itemList.size() == 0;
}

bool dirIsNotEmpty(string dirLoc){
	return !dirIsEmpty(dirLoc);
}

void refreshFileList(ofstream& fileListFile, string storeDir, string pred) {//TODO:: work with sub folders
	cout << "start refresh file list." << endl;
	if (!fileListFile.good()) {
		outputText(' ', "ERROR:: Unable to write to file list.", verbose, 3);
		return;
	}
	DIR *pDIR;
	struct dirent *entry;
	if (pDIR = opendir(storeDir.c_str())) {
		while (entry = readdir(pDIR)) {
			if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
				cout << entry->d_name << endl;
				if (checkFilePath(entry->d_name, false)) {
					cout << "\tis not dir" << endl;
					fileListFile << pred << entry->d_name << endl;
				}else {
					cout << "\tis dir" << endl;
					refreshFileList(fileListFile, storeDir + foldSeparater + entry->d_name, entry->d_name + foldSeparater);
				}
			}
		}
		closedir(pDIR);
	}
	return;
}

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
	if (toGetList->size() == 0) {
		outputText(' ', "No files to move.", verbose, 4);
		return;
	}
	for (vector<string>::iterator it = toGetList->begin(); it != toGetList->end(); ++it) {
		//cout << "Moving \"" + storageDir + foldSeparater + *it + "\"" << endl;
		if (checkFilePath(syncDir + foldSeparater + *it, false)) {
			outputText(' ', "\"" + storageDir + foldSeparater + *it + "\" already present in sync folder.", verbose, 4);
			continue;
		}

		outputText(' ', "Moving \"" + storageDir + foldSeparater + *it + "\"...", verbose, 4);
		if (checkFilePath(storageDir + foldSeparater + *it, false)) {
			copyFile(storageDir + foldSeparater + *it, syncDir);
		}else {
			outputText(' ', "Invalid file location. Copy cancelled.", verbose, 5);
		}
		outputText(' ', "Done.", verbose, 4);
	}
}

int searchInnerSyncDir(string dir){
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
	if(!checkFilePath(syncConfigLoc, false) ){
		outputText(' ', "**** Unable to open sync config file. Creating a new one (\""+ syncConfigLoc +"\")...", verbose, 3);
		generateFolderConfig(syncConfigLoc);
		if(!checkFilePath(syncConfigLoc, false) ){
			outputText(' ', "**** ERROR:: Unable to create or open sync config file.", verbose, 3);
			return -1;
		}else{
			outputText(' ', "**** Created default sync config for \"" + dir + "\".", verbose, 3);
		}
	}
	readFolderConfig(syncConfigLoc);

	//test if the file list is present
	if (!checkFilePath(syncFileListLoc, false)) {
		outputText(' ', "**** Unable to open stored file list. Creating a new one (\"" + syncFileListLoc + "\")...", verbose, 3);
		ofstream confFile(syncFileListLoc.c_str());
		confFile.close();
		if (!checkFilePath(syncFileListLoc, false)) {
			outputText(' ', "**** ERROR:: Unable to create or open stored file list.", verbose, 3);
			return -1;
		}
		else {
			outputText(' ', "**** Created stored file list for \"" + dir + "\".", verbose, 3);
		}
	}

	ofstream fileListFile(syncFileListLoc.c_str());
	refreshFileList(fileListFile, thisStorageFolder, "");
	fileListFile.close();

	//test if the list to get is present
	if (!checkFilePath(syncRetrieveListLoc, false)) {
		outputText(' ', "**** Unable to open list of files to get. Creating a new one (\"" + syncRetrieveListLoc + "\")...", verbose, 3);
		ofstream confFile(syncRetrieveListLoc.c_str());
		confFile.close();
		if (!checkFilePath(syncRetrieveListLoc, false)) {
			outputText(' ', "**** ERROR:: Unable to create or open list of files to get.", verbose, 3);
			return -1;
		}
		else {
			outputText(' ', "**** Created list of files to get for \"" + dir + "\".", verbose, 3);
		}
	}
	getListOfFilesToGet(syncRetrieveListLoc, &getList);

	outputText(' ', "Searching \"" + dir + "\" for new files to store...", verbose, 3);
	DIR *pDIR;
	struct dirent *entry;
	if( pDIR=opendir(dir.c_str()) ){
		while(entry = readdir(pDIR)){
			if( strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 ){
				string curFile = dir + foldSeparater + (string)entry->d_name;
				if(checkFilePath(curFile, false)
					&& !(find(ignoreList.begin(), ignoreList.end(), curFile) != ignoreList.end())
					&& !(find(getList.begin(), getList.end(), (string)entry->d_name) != getList.end())){
					outputText(' ', "Found: \"" + curFile + "\". Dealing with it..." , verbose, 4);
					//wait until fully transferred
					outputText(' ', "Waiting/Checking for full sync transfer...", verbose, 5);
					long initSize, tempSize;
					do {
						initSize = getFileSize(curFile);
						mySleep(transferWait);
						tempSize = getFileSize(curFile);
					} while (initSize != tempSize);
					outputText(' ', "Done." , verbose, 4);
					//transfer to storage
					outputText(' ', "Transferring to storage folder...", verbose, 5);
					copyFile(curFile, thisStorageFolder);
					outputText(' ', "Done." , verbose, 4);
					
					//remove original file
					outputText(' ', "Removing file from sync folder...", verbose, 5);
					if(remove(curFile.c_str()) != 0){
						outputText(' ', "***** Method returned nonzero." , verbose, 6);
					}
					outputText(' ', "Done." , verbose, 5);
					
					outputText(' ', "Done." , verbose, 4);
				}
			}
		}
		closedir(pDIR);
	}
	outputText(' ', "Done.", verbose, 3);
	//move files they want to retrieve to sync
	outputText(' ', "Moving files in list to get...", verbose, 3);
	moveFilesToGet(thisStorageFolder, dir, &getList);
	outputText(' ', "Done.", verbose, 3);

	//refresh list of files in storage
	ofstream fileListFileFinal(syncFileListLoc.c_str());
	refreshFileList(fileListFileFinal, thisStorageFolder, "");
	fileListFileFinal.close();
	
}//searchInnerSyncDir

void searchSyncDir(){
	DIR *pDIR;
	struct dirent *entry;
	if( pDIR=opendir(syncFolderLoc.c_str()) ){
		while(entry = readdir(pDIR)){
			if( strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 ){
				//cout << entry->d_name << "\n";
				
				string curSyncFolder = syncFolderLoc + foldSeparater + entry->d_name;
				if(checkFilePath(curSyncFolder, true)){
					outputText(' ', "Processing sync folder \"" + curSyncFolder + "\"..." , verbose, 2);
					cropNumInStor(storFolderLoc + foldSeparater + entry->d_name, searchInnerSyncDir(curSyncFolder));
				}
			}
			
		}
		closedir(pDIR);
	}
	outputText(' ', "Done." , verbose, 2);
}//searchSyncDir


int runServer(){
	bool okay = true;
	outputText(' ', nlc + nlc + nlc + nlc + nlc + "######## START SERVER ########", verbose);
	
	//test to see if sync and storage folders are present. create them if not.
	if(!checkFilePath(syncFolderLoc, true)){
		createDirectory(syncFolderLoc);
		if(!checkFilePath(syncFolderLoc, true)){
			outputText(' ', "******** FAILED TO CREATE/ OPEN SYNC DIRECTORY. EXITING. ********", verbose, 1);
			okay = false;
		}else{
			outputText(' ', "Created Sync Directory.", verbose, 1);
		}
	}
	
	if(!checkFilePath(storFolderLoc, true)){
		createDirectory(storFolderLoc);
		if(!checkFilePath(storFolderLoc, true)){
			outputText(' ', "******** FAILED TO CREATE/ OPEN STORAGE DIRECTORY. EXITING. ********", verbose, 1);
			okay = false;
		}else{
			outputText(' ', "Created Storage Directory.", verbose, 1);
		}
	}
	
	string waitIntStr;
	ostringstream convert;   // stream used for the conversion
	convert << checkInterval;
	waitIntStr = convert.str();
	if(okay){
		while(okay){
			outputText(' ', "Begin check loop.", verbose, 1);
			//for each syncing directory, see if there are files there (of .zip or .tar.gz) and move them to storage if their sizes aren't changing
			searchSyncDir();
			
			outputText(' ', "End Check Loop. Waiting " + waitIntStr + "s...", verbose, 1);
			mySleep(checkInterval);
			outputText(' ', "End Waiting.", verbose, 1);
			outputText(' ', "", verbose);
		}
	}else{
		outputText(' ', "Something went wrong. Exiting execution of server.", verbose, 1);
	}
	outputText(' ', "######## END SERVER RUN ########", verbose);
}//runServer

void generateMainConfig(){
	outputText('c', "Generating Main Config File...", verbose);
	ofstream confFile (confFileLoc.c_str(), ios::app);
	if(!confFile.good()){
		outputText('c', "ERROR:: Unable to create or open config file.", true);
		return;
	}
	confFile << "logFileLoc" << delimeter << logFileLoc.c_str() << endl <<
		"syncFolderPred" << delimeter << syncFolderPred.c_str() << endl <<
		"syncFolderLoc" << delimeter << syncFolderLoc.c_str() << endl <<
		"storFolderPred" << delimeter << storFolderPred.c_str() << endl <<
		"storFolderLoc" << delimeter << storFolderLoc.c_str() << endl << 
		"checkInterval" << delimeter << checkInterval;
	confFile.close();
	outputText('c', "Done.", verbose);
	return;
}//generateConfig()

void readMainConfig(){
	ifstream confFile (confFileLoc.c_str()); // declare file stream: http://www.cplusplus.com/reference/iostream/ifstream/
	string variable, value;
	
	while ( confFile.good() ){
		getline ( confFile, variable, delimeter );
		getline ( confFile, value, '\n' );
		//cout << "\"" << variable << "\", \"" << value << "\"" << endl;
		if(variable == "logFileLoc"){
			logFileLoc = value;
		}else if(variable == "syncFolderPred"){
			syncFolderPred = value;
		}else if(variable == "syncFolderLoc"){
			syncFolderLoc = value;
		}else if(variable == "storFolderPred"){
			storFolderPred = value;
		}else if(variable == "storFolderLoc"){
			storFolderLoc = value;
		}else if(variable == "checkInterval"){
			checkInterval = atoi(value.c_str());
		}
		
	}
	
	confFile.close();
}//readConfig()

bool setRunOnStart(){
	outputText('c', "Setting to run the server on start.", verbose);
	//ofstream log (logFile.c_str(), ios::out | ios::app);
	//TODO:: this
}//setRunOnStart()



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
			outputText('c', "Config file changed to: \"" + confFileLoc + "\"", true);
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
		outputText('c', "Unable to open config file. Creating a new one...", verbose);
		generateMainConfig();
		if(!checkFilePath(confFileLoc, false) ){
			outputText('c', "ERROR:: Unable to create or open config file.", true);
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
		outputText('c', "Not told to do anything. Use -h to see options.", verbose);
	}
	
	
	return 0;
}//main
