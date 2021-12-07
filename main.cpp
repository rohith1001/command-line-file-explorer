#include <bits/stdc++.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
using namespace std;

#define KEY_ESCAPE 0x001b
#define KEY_ENTER 0x000a
#define KEY_UP 0x0105
#define KEY_DOWN 0x0106
#define KEY_LEFT 0x0107
#define KEY_RIGHT 0x0108
#define KEY_h 104
#define KEY_k 107
#define KEY_l 108
#define KEY_BACK_SPACE 127
#define KEY_COLON 58

#define FOREBLU "\x1B[34m"  // Blue
#define RESETTEXT "\x1B[0m" // Set all colors back to normal.
#define BLU(x) FOREBLU << x << RESETTEXT

#define cursorUpward(x) printf("\033[%dA", (x))
#define cursorDownward(x) printf("\033[%dB", (x))

char cwd[PATH_MAX];
char home[PATH_MAX];
struct dirent *dir;

struct stat fileInfo;
int gX, gY;
const int batch_size = 3;
struct termios myTerm, oterm;
stack<string> goBackStack;
stack<string> goFrontStack;

// void handleUpCase(int &x, int n, int batch_number);
// void handleDownCase(int &x, int n, int batch_number);
bool validateArrowPress(int &x, int n, int batch_number);
void setToCanonical();
void setToNonCanonical();
void collectAllFilesInDir(string directory);
string readInCommand();
void ignoreCharInCommand();
bool checkIfDir(string directory);

void getCurrentDirectory(char (&myCwd)[PATH_MAX]) {
    if (getcwd(myCwd, sizeof(myCwd)) == NULL) {
        cout << "Error in getting current directory path...Exiting" << endl;
        return;
    }
}

int getch() {
    int c = 0;
    tcgetattr(0, &oterm);
    memcpy(&myTerm, &oterm, sizeof(myTerm));
    myTerm.c_lflag &= ~(ICANON | ECHO);
    myTerm.c_cc[VMIN] = 1;
    myTerm.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &myTerm);
    c = getchar();
    tcsetattr(0, TCSANOW, &oterm);
    return c;
}

int kbhit(void) {
    int c = 0;
    tcgetattr(0, &oterm);
    memcpy(&myTerm, &oterm, sizeof(myTerm));
    myTerm.c_lflag &= ~(ICANON | ECHO);
    myTerm.c_cc[VMIN] = 0;
    myTerm.c_cc[VTIME] = 1;
    tcsetattr(0, TCSANOW, &myTerm);
    c = getchar();
    tcsetattr(0, TCSANOW, &oterm);
    if (c != -1)
        ungetc(c, stdin);
    return ((c != -1) ? 1 : 0);
}

int kbesc(void) {
    int c;

    if (!kbhit()) {
        return KEY_ESCAPE;
    }
    c = getch();
    if (c == '[') {
        switch (getch()) {
        case 'A':
            c = KEY_UP;
            break;
        case 'B':
            c = KEY_DOWN;
            break;
        case 'C':
            c = KEY_RIGHT;
            break;
        case 'D':
            c = KEY_LEFT;
            break;
        default:
            c = 0;
            break;
        }
    } else {
        c = 0;
    }
    if (c == 0)
        while (kbhit())
            getch();
    return c;
}

static int kbget(void) {
    int c;
    c = getch();
    return (c == KEY_ESCAPE) ? kbesc() : c;
}

void clearScreen() { cout << "\033[2J"; }

void moveCursor(int x, int y) { cout << "\033[" << x << ";" << y << "H"; }

string getPermissionsInfo(struct stat &fileInfo) {
    string permissions = "";
    mode_t perm = fileInfo.st_mode;
    permissions += S_ISDIR(perm) ? 'd' : (S_ISREG(perm) ? '-' : ' ');
    permissions += (perm & S_IRUSR) ? 'r' : '-';
    permissions += (perm & S_IWUSR) ? 'w' : '-';
    permissions += (perm & S_IXUSR) ? 'x' : '-';
    permissions += (perm & S_IRGRP) ? 'r' : '-';
    permissions += (perm & S_IWGRP) ? 'w' : '-';
    permissions += (perm & S_IXGRP) ? 'x' : '-';
    permissions += (perm & S_IROTH) ? 'r' : '-';
    permissions += (perm & S_IWOTH) ? 'w' : '-';
    permissions += (perm & S_IXOTH) ? 'x' : '-';
    return permissions;
}

void getUserAndGroup(string &userName, string &groupName) {
    struct passwd *pw = getpwuid(fileInfo.st_uid);
    struct group *gr = getgrgid(fileInfo.st_gid);
    string s1(pw->pw_name); // for user
    string s2(gr->gr_name); // for group
    userName = s1;
    groupName = s2;
}

void simplyPrintInBatches(vector<string> &fileRecords, int from, int to) {
    for (int i = from; i < to; i++) {
        if (fileRecords[i].at(0) == 'd') {
            cout << BLU("(") << BLU(i + 1) << BLU(") ") << BLU(fileRecords[i]);
        } else {
            cout << "(" << i + 1 << ") " << fileRecords[i];
        }
    }
}

void setToCanonical() { tcsetattr(0, TCSANOW, &oterm); }

void setToNonCanonical() {
    myTerm.c_lflag &= ~(ICANON | ECHO);
    myTerm.c_cc[VMIN] = 1;  // minimum characters for non-canonical read
    myTerm.c_cc[VTIME] = 0; // timeout in deciseconds for non-canonical read
    tcsetattr(0, TCSANOW, &myTerm);
}

void displayFileRecordsInBatches(vector<string> &fileRecords, int &batch_number) {
    clearScreen();
    moveCursor(1, 1);
    int n = fileRecords.size();
    int from = (batch_number - 1) * batch_size;
    int to = (batch_number * batch_size);
    if (to > n) {
        to = n;
    }
    simplyPrintInBatches(fileRecords, from, to);
}

string convetLmtToString(time_t mt) {
    time_t t = mt;
    struct tm lt;
    localtime_r(&t, &lt);
    char timbuf[80];
    strftime(timbuf, sizeof(timbuf), "%c", &lt);
    string s(timbuf);
    return s;
}

void getCursor(int *p, int *q) {
    tcgetattr(0, &oterm);
    memcpy(&myTerm, &oterm, sizeof(myTerm));
    myTerm.c_lflag &= ~(ICANON | ECHO);
    myTerm.c_cc[VMIN] = 0;
    myTerm.c_cc[VTIME] = 1;
    tcsetattr(0, TCSANOW, &myTerm);
    cout << "\033[6n";
    scanf("\033[%d;%dR", p, q);
    tcsetattr(0, TCSANOW, &oterm);
}

// void accept_input(vector<string> &fileRecords, int &batch_number, int &x) {
//     int n = fileRecords.size();
//     int n_batches = ceil(n / batch_size);
//     while (true) {
//         char ch;
//         ch = cin.get();
//         if (ch == '\033') {
//             char d, e;
//             d = cin.get();
//             e = cin.get();
//             if (e == 'A') {
//                 handleUpCase(x, n, batch_number);
//             } else if (e == 'B') {
//                 handleDownCase(x, n, batch_number);
//             }
//         } else if (ch == 'k') {
//             batch_number++;
//             batch_number = (batch_number > n_batches) ? 1 : batch_number;
//             break;
//         } else if (ch == 'l') {
//             batch_number--;
//             batch_number = (batch_number < 1) ? n_batches : 1;
//             break;
//         } else {
//             break;
//         }
//     }
// }

int checkIfDirectory(vector<string> &fileRecords, int x, int n, int batch_number) {
    int index = ((batch_number - 1) * batch_size) + x - 1;
    if (fileRecords[index].at(0) == 'd') {
        return index;
    }
    return -1;
}

int checkIfRegular(vector<string> &fileRecords, int x, int n, int batch_number) {
    int index = ((batch_number - 1) * batch_size) + x - 1;
    if (fileRecords[index].at(0) == '-') {
        return index;
    }
    return -1;
}

void changeDirectory(string directoryName) {
    char myDir[directoryName.size() + 1];
    strcpy(myDir, directoryName.c_str());
    chdir(myDir);
    getCurrentDirectory(cwd); // update the current working directory
}

void newNormalMode() { collectAllFilesInDir(cwd); }

void openFileInDefaultApp(string file) {
    char fileName[PATH_MAX];
    strcpy(fileName, file.c_str());
    pid_t pid = fork();
    if (pid == 0) {
        execl("/usr/bin/xdg-open", "xdg-open", fileName, NULL);
        exit(1);
    }
}

bool checkIfDirIsCurrentDir(string dirName) {
    if (dirName == ".") {
        return true;
    }
    return false;
}

bool checkIfDirIsHomeParent(string dirName) {
    ofstream myfile;
    char myDir[PATH_MAX];
    getCurrentDirectory(myDir);
    string currDir(myDir);
    myfile.open("mylog.txt");
    myfile << "Curr dir is: " << currDir << endl;
    myfile << "dirName is: " << dirName << endl;
    if (string(home) == myDir && (dirName == "..")) {
        myfile << "returning true" << endl;
        myfile.close();
        return true;
    }
    myfile << "returning false" << endl;
    myfile.close();
    return false;
}

bool enter_functionality(vector<string> &fileRecords, vector<string> &fileNamesVector, const int x, const int n,
                         const int batch_number) {
    int index = checkIfDirectory(fileRecords, x, n, batch_number);
    if (index != -1 && !checkIfDirIsHomeParent(fileNamesVector[index]) &&
        !checkIfDirIsCurrentDir(fileNamesVector[index])) {
        goBackStack.push(cwd);
        changeDirectory(fileNamesVector[index]);
        newNormalMode();
        return true;
    } else {
        index = checkIfRegular(fileRecords, x, n, batch_number);
        if (index != -1) {
            openFileInDefaultApp(fileNamesVector[index]);
            return false;
        }
    }
    return false;
}

bool backSpace_functionality() {
    char parent_dir[] = "..";
    if (!checkIfDirIsHomeParent(parent_dir)) {
        goBackStack.push(cwd);
        changeDirectory(parent_dir);
        newNormalMode();
        return true;
    }
    return false;
}

bool rightKey_functionality() {
    if (!goFrontStack.empty()) {
        string goFrontDir = goFrontStack.top();
        goFrontStack.pop();
        goBackStack.push(cwd);
        if (!checkIfDirIsHomeParent(goFrontDir) && !checkIfDirIsCurrentDir(goFrontDir)) {
            changeDirectory(goFrontDir);
            newNormalMode();
            return true;
        }
    }
    return false;
}

bool leftKey_functionality() {
    // the old working directory is on the stack
    // current cwd is stored in variable cwd
    if (!goBackStack.empty()) {
        string goBackDir = goBackStack.top();
        goBackStack.pop();
        goFrontStack.push(cwd);
        if (!checkIfDirIsHomeParent(goBackDir) && !checkIfDirIsCurrentDir(goBackDir)) {
            changeDirectory(goBackDir);
            newNormalMode();
            return true;
        }
    }
    return false;
}

bool isDirPresent(const std::string &s) {
    struct stat fileInformation;
    return ((stat(s.c_str(), &fileInformation) == 0) && S_ISDIR(fileInformation.st_mode));
}

void goto_functionality() {
    string goto_dir;
    goto_dir = readInCommand();
    string absgoto_dir = string(home) + '/' + goto_dir;
    if (isDirPresent(absgoto_dir)) {
        goBackStack.push(cwd);
        changeDirectory(absgoto_dir);
        newNormalMode();
    }
}

void createFile_functionality() {
    string file_name, destination_path;
    file_name = readInCommand();
    destination_path = readInCommand();
    string absdesPathWithName = string(home) + '/' + destination_path + '/';
    if (isDirPresent(absdesPathWithName)) {
        absdesPathWithName += file_name;
        ofstream newFile(absdesPathWithName);
        newFile.close();
    }
    newNormalMode();
}

void createDir_functionality() {
    string dir_name, destination_path;
    dir_name = readInCommand();
    destination_path = readInCommand();
    string absdesPathWithName = string(home) + '/' + destination_path + '/';
    if (isDirPresent(absdesPathWithName)) {
        absdesPathWithName += dir_name;
        char dirArr[absdesPathWithName.size() + 1];
        strcpy(dirArr, absdesPathWithName.c_str());
        int status = mkdir(dirArr, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        // status error checking not implemented
    }
    newNormalMode();
}

// void ignoreCharInCommand() {
//     char ch;
//     while (ch != KEY_ENTER) {
//         ch = kbget();
//     }
// }

void readInCommandStrings(vector<string> &vecStr) {
    string str = "";
    int ch1;
    while (true) {
        setToNonCanonical();
        ch1 = cin.get();
        setToCanonical();
        if ((int)ch1 == 27) { // for escape
            vecStr.clear();
        } else if ((int)ch1 == 32) { // for space
            cout << " ";
            vecStr.push_back(str);
            str = "";
            ch1 = '\0';
        } else if (ch1 == 10) { // for enter
            vecStr.push_back(str);
            return;
        } else if ((int)ch1 == 127) {
            cout << "\b \b";
            if (str.size() >= 1) {
                str.resize(str.size() - 1);
            }
        }
        cout << (char)ch1;
        str += (char)ch1;
    }
    return;
}

string readInCommand() {
    string str = "";
    int ch1;
    while (true) {
        setToNonCanonical();
        ch1 = cin.get();
        setToCanonical();
        if ((int)ch1 == 27) { // for escape
            str = "";
            return str;
        } else if ((int)ch1 == 32) { // for space
            cout << " ";
            return str;
        } else if (ch1 == 10) {
            return str;
        } else if ((int)ch1 == 127) {
            cout << "\b \b";
            if (str.size() >= 1) {
                str.resize(str.size() - 1);
            }
        }
        cout << (char)ch1;
        str += (char)ch1;
    }
    return str;
}

void copyRegFile(string source, string destination) {
    cout << "Inside copyRegFile" << endl;
    cout << "Source is: " << source << endl;
    cout << "Destination is " << destination << endl;
    // char block[1024];
    // int inFileStatus, outFileStatus;
    // int status;
    // inFileStatus = fopen(source.c_str(), O_RDONLY);
    // cout << "Error in inFile is " << strerror(errno);
    // cout << inFileStatus << endl;
    // outFileStatus = fopen(destination.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    // cout << "Error in outFile is " << strerror(errno);
    // cout << outFileStatus << endl;
    // while ((status = read(inFileStatus, block, sizeof(block))) > 0) {
    //     write(outFileStatus, block, status);
    // }
    ifstream sourceFile(source.c_str(), ios::binary);
    ofstream destFile(destination.c_str(), ios::binary);

    destFile << sourceFile.rdbuf();

    sourceFile.close();
    destFile.close();

    return;
}

bool copy_functionality() {
    vector<string> stringParameters;
    readInCommandStrings(stringParameters);
    // cout << "hello123 mic testing" << endl;
    // cout << "size is " << stringParameters.size() << endl;
    if (stringParameters.size() == 0) {
        return false;
    }
    if (stringParameters.size() > 0) {
        int n = stringParameters.size();
        string destination = string(home) + '/' + stringParameters[n - 1] + '/';
        for (int i = 0; i < n - 1; i++) {
            string source = string(home) + '/' + stringParameters[i];
            destination += stringParameters[i];
            if (checkIfDir(source)) {
                cout << "\33[2K";
                cout << "Recursive copy not implemented" << endl;
                return false;
            } else {
                // cout << "source is: " << source << endl;
                // cout << "destination is: " << destination << endl;
                copyRegFile(source, destination);
            }
        }
        return true;
    }
    return false;
}

bool checkIfDir(string directory) {
    struct stat fileStat;
    char dir_arr[directory.size() + 1];
    strcpy(dir_arr, directory.c_str());
    if (stat(dir_arr, &fileStat) != 0) {
        cout << "Error accessing file stat...Exiting" << endl;
        return false;
    }
    mode_t perm = fileStat.st_mode;
    return S_ISDIR(perm);
}

void search_functionality(string fileName, string directory) {
    struct stat fileStat;
    int batch_number = 1;
    char dir_arr[directory.size() + 1];
    strcpy(dir_arr, directory.c_str());
    DIR *d;
    d = opendir(dir_arr);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (string(dir->d_name) == fileName) {
                cout << "\33[2K";
                moveCursor(batch_size + 2, 1);
                cout << "\33[2K";
                cout << "True";
                moveCursor(batch_size + 1, 1);
                return;
            }
            if (checkIfDir(dir->d_name)) {
                search_functionality(fileName, dir->d_name);
            }
        }
    }
    cout << "\33[2K";
    moveCursor(batch_size + 2, 1);
    cout << "\33[2K";
    cout << "False";
    moveCursor(batch_size + 1, 1);
    return;
}

void readSearchParameters(string &searchFile) { searchFile = readInCommand(); }

void rename_functionality() {
    string oldName = readInCommand();
    string newName = readInCommand();
    string abs_oldName = string(home) + '/' + oldName;
    string abs_newName = string(home) + '/' + newName;
    cout << "abs_oldName: " << abs_oldName << endl;
    cout << "abs_newName: " << abs_newName << endl;
    int value = rename(abs_oldName.c_str(), abs_newName.c_str());
    if (!value) {
        newNormalMode();
    }
}

bool command_mode() {
    moveCursor(batch_size + 2, 1);
    cout << ":";
    string myCommand;
    myCommand = readInCommand();
    if (myCommand == "") {
        return false;
    } else if (myCommand == "goto") {
        goto_functionality();
        return true;
    } else if (myCommand == "create_file") {
        createFile_functionality();
        return true;
    } else if (myCommand == "create_dir") {
        createDir_functionality();
        return true;
    } else if (myCommand == "copy") {
        return copy_functionality();
    } else if (myCommand == "search") {
        string searchFile;
        readSearchParameters(searchFile);
        search_functionality(searchFile, home);
        return false;
    } else if (myCommand == "rename") {
        rename_functionality();
        return true;
    }
    return false;
}

void accept_input(vector<string> &fileRecords, vector<string> &fileNamesVector, int &batch_number, int &x) {
    int n = fileRecords.size();
    int n_batches = ceil(n / batch_size);
    bool valid = false;
    bool returnedFromCommand = false;
    while (true) {
        int c;
        c = kbget();
        if (c == KEY_UP) {
            cursorUpward(1);
        } else if (c == KEY_DOWN) {
            cursorDownward(1);
        } else if (c == KEY_LEFT) {
            if (leftKey_functionality()) {
                break;
            }
        } else if (c == KEY_RIGHT) {
            if (rightKey_functionality()) {
                break;
            }
        } else if (c == KEY_k) { // for pressing k
            batch_number++;
            batch_number = (batch_number > n_batches) ? 1 : batch_number;
            break;
        } else if (c == KEY_l) { // for pressing l
            batch_number--;
            batch_number = (batch_number < 1) ? n_batches : batch_number;
            break;
        } else if (c == KEY_ENTER) {
            if (validateArrowPress(x, n, batch_number) &&
                enter_functionality(fileRecords, fileNamesVector, x, n, batch_number)) {
                break;
            }
        } else if (c == KEY_BACK_SPACE) {
            if (backSpace_functionality()) {
                break;
            }
        } else if (c == KEY_COLON) {
            if (command_mode()) {
                break;
            } else {
                cout << "\33[2K";
                moveCursor(1, 1);
                c = KEY_UP;
            }
        }
    }
}

void normal_mode(vector<string> &fileRecords, vector<string> &fileNamesVector, int &batch_number) {
    int x = 1;
    int prevBatch_num = batch_number;
    string prevPathStr(cwd);
    displayFileRecordsInBatches(fileRecords, batch_number);
    moveCursor(1, 1);
    accept_input(fileRecords, fileNamesVector, batch_number, x);
    string currentPathStr(cwd);
    if (prevBatch_num != batch_number && prevPathStr == currentPathStr) {
        // above if condition, just to avoid infinite recursion
        // recursion is confusing for me...:P
        normal_mode(fileRecords, fileNamesVector, batch_number);
    }
}

void collectAllFilesInDir(string directory) {
    vector<string> fileRecords;
    vector<string> fileNamesVector;
    int batch_number = 1;
    char dir_arr[directory.size() + 1];
    strcpy(dir_arr, directory.c_str());
    DIR *d;
    d = opendir(dir_arr);
    if (d) {
        clearScreen();
        moveCursor(1, 1);
        while ((dir = readdir(d)) != NULL) {
            fileNamesVector.push_back(dir->d_name);
        }
        sort(fileNamesVector.begin(), fileNamesVector.end());
        for (auto f : fileNamesVector) {
            char fileName[f.size() + 1];
            strcpy(fileName, f.c_str());
            string userName = "", groupName = "";
            string fileRecordString = "";
            if (stat(fileName, &fileInfo) != 0) {
                cout << "Error accessing file stat...Exiting" << endl;
                return;
            }
            fileRecordString += getPermissionsInfo(fileInfo) + "\t";
            fileRecordString += to_string(fileInfo.st_size) + "\t";
            fileRecordString += convetLmtToString(fileInfo.st_mtime) + "\t";
            fileRecordString += f + "\n";
            fileRecords.push_back(fileRecordString);
            fileRecordString = "";
        }
    }
    closedir(d);
    normal_mode(fileRecords, fileNamesVector, batch_number);
}

void getTermAttr() {
    tcgetattr(0, &myTerm);
    tcgetattr(0, &oterm);
}

void moveCursorUp() { cout << "\033[1A"; }

void moveCursorDown() { cout << "\033[1B"; }

bool validateArrowPress(int &x, int n, int batch_number) {
    getCursor(&gX, &gY);
    x = gX;
    int from = (batch_number - 1) * batch_size;
    int to = (batch_number * batch_size);
    if (to > n) {
        to = n;
    }
    int curr_batch_size = to - from;
    if (x <= curr_batch_size && x >= 1) {
        return true;
    }
    return false;
}

int main() {
    getCurrentDirectory(cwd);
    strcpy(home, cwd);
    collectAllFilesInDir(home);
    return 0;
}
