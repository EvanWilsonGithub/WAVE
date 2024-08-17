#include <iostream>
#include <fstream>
#include <windows.h>
#include <sys/stat.h>
#include <cstring>
#include <filesystem>
#include <string.h>
#include <vector>
#include <string>
#include <cstdint>
#include <map>
#include <chrono>
#pragma warning(disable : 4996)
using namespace std;

HFONT hFont = CreateFont(13, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET,
    OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
    DEFAULT_PITCH | FF_DONTCARE, TEXT("Tahoma"));

struct Index;
struct Section;
wstring ExePath() {
    TCHAR buffer[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, buffer, MAX_PATH);
    wstring::size_type pos = wstring(buffer).find_last_of(L"\\/");
    return wstring(buffer).substr(0, pos);
}
string filenames = "";

#pragma pack(push, 1)
struct Index
{
    uint32_t id;
    uint32_t offset;
    uint32_t size;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Section
{
    char sign[4];
    uint32_t size;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BankHeader
{
    uint32_t version;
    uint32_t id;
};
#pragma pack(pop)

enum class ObjectType : int8_t
{
    SoundEffectOrVoice = 2,
    EventAction = 3,
    Event = 4,
    RandomOrSequenceContainer = 5,
    SwitchContainer = 6,
    ActorMixer = 7,
    AudioBus = 8,
    BlendContainer = 9,
    MusicSegment = 10,
    MusicTrack = 11,
    MusicSwitchContainer = 12,
    MusicPlaylistContainer = 13,
    Attenuation = 14,
    DialogueEvent = 15,
    MotionBus = 16,
    MotionFx = 17,
    Effect = 18,
    Unknown = 19,
    AuxiliaryBus = 20
};

#pragma pack(push, 1)
struct Object
{
    ObjectType type;
    uint32_t size;
    uint32_t id;
};
#pragma pack(pop)

struct EventObject
{
    uint32_t action_count;
    vector<uint32_t> action_ids;
};

enum class EventActionScope : int8_t
{
    SwitchOrTrigger = 1,
    Global = 2,
    GameObject = 3,
    State = 4,
    All = 5,
    AllExcept = 6
};

enum class EventActionType : int8_t
{
    Stop = 1,
    Pause = 2,
    Resume = 3,
    Play = 4,
    Trigger = 5,
    Mute = 6,
    UnMute = 7,
    SetVoicePitch = 8,
    ResetVoicePitch = 9,
    SetVoiceVolume = 10,
    ResetVoiceVolume = 11,
    SetBusVolume = 12,
    ResetBusVolume = 13,
    SetVoiceLowPassFilter = 14,
    ResetVoiceLowPassFilter = 15,
    EnableState = 16,
    DisableState = 17,
    SetState = 18,
    SetGameParameter = 19,
    ResetGameParameter = 20,
    SetSwitch = 21,
    ToggleBypass = 22,
    ResetBypassEffect = 23,
    Break = 24,
    Seek = 25
};

enum class EventActionParameterType : int8_t
{
    Delay = 0x0E,
    Play = 0x0F,
    Probability = 0x10
};

struct EventActionObject
{
    EventActionScope scope;
    EventActionType action_type;
    uint32_t game_object_id;
    uint8_t parameter_count;
    vector<EventActionParameterType> parameters_types;
    vector<int8_t> parameters;
};

int Swap32(const uint32_t dword)
{
#ifdef __GNUC__
    return __builtin_bswap32(dword);
#elif _MSC_VER
    return _byteswap_ulong(dword);
#endif
}

template <typename T>
bool ReadContent(fstream& file, T& structure)
{
    return static_cast<bool>(file.read(reinterpret_cast<char*>(&structure), sizeof(structure)));
}

filesystem::path CreateOutputDirectory(filesystem::path bnk_filename)
{
    const auto directory_name = bnk_filename.filename().replace_extension("");
    auto directory = bnk_filename.replace_filename(directory_name);
    create_directory(directory);
    return directory;
}

bool Compare(char* char_string, const string& string)
{
    return strncmp(char_string, string.c_str(), string.length()) == 0;
}

bool HasArgument(char* arguments[], const int argument_count, const string& argument)
{
    for (auto i = 0U; i < static_cast<size_t>(argument_count); ++i)
    {
        if (Compare(arguments[i], argument))
        {
            return true;
        }
    }

    return false;
}

string extract(int argument_count, char* arguments[])
{
    cout << "Wwise *.BNK File Extractor\n";
    cout << "(c) RAWR 2015-2022 - https://rawr4firefall.com\n\n";

    // Has no argument(s)
    if (argument_count < 2)
    {
        cout << "Usage: bnkextr filename.bnk [/swap] [/nodir] [/obj]\n";
        cout << "\t/swap - swap byte order (use it for unpacking 'Army of Two')\n";
        cout << "\t/nodir - create no additional directory for the *.wem files\n";
        cout << "\t/obj - generate an objects.txt file with the extracted object data\n";
        return EXIT_SUCCESS;
    }
    auto bnk_filename = filesystem::path{ arguments[1]};
    auto swap_byte_order = HasArgument(arguments, argument_count, "/swap");
    auto no_directory = HasArgument(arguments, argument_count, "/nodir");
    auto dump_objects = HasArgument(arguments, argument_count, "/obj");

    auto bnk_file = fstream{ bnk_filename, ios::binary | ios::in };

    // Could not open BNK file
    if (!bnk_file.is_open())
    {
        cout << "Can't open input file: " << bnk_filename << "\n";
        return "fail";
    }

    auto data_offset = size_t{ 0U };
    auto files = vector<Index>{};
    auto content_section = Section{};
    auto content_index = Index{};
    auto bank_header = BankHeader{};
    auto objects = vector<Object>{};
    auto event_objects = map<uint32_t, EventObject>{};
    auto event_action_objects = map<uint32_t, EventActionObject>{};

    while (ReadContent(bnk_file, content_section))
    {
        const size_t section_pos = bnk_file.tellg();

        if (swap_byte_order)
        {
            content_section.size = Swap32(content_section.size);
        }

        if (Compare(content_section.sign, "BKHD"))
        {
            ReadContent(bnk_file, bank_header);
            bnk_file.seekg(content_section.size - sizeof(BankHeader), ios_base::cur);

            cout << "Wwise Bank Version: " << bank_header.version << "\n";
            cout << "Bank ID: " << bank_header.id << "\n";
        }
        else if (Compare(content_section.sign, "DIDX"))
        {
            // Read file indices
            for (auto i = 0U; i < content_section.size; i += sizeof(content_index))
            {
                ReadContent(bnk_file, content_index);
                files.push_back(content_index);
            }
        }
        else if (Compare(content_section.sign, "STID"))
        {
            // To be implemented
        }
        else if (Compare(content_section.sign, "DATA"))
        {
            data_offset = bnk_file.tellg();
        }
        else if (Compare(content_section.sign, "HIRC"))
        {
            auto object_count = uint32_t{ 0 };
            ReadContent(bnk_file, object_count);

            for (auto i = 0U; i < object_count; ++i)
            {
                auto object = Object{};
                ReadContent(bnk_file, object);

                if (object.type == ObjectType::Event)
                {
                    auto event = EventObject{};

                    if (bank_header.version >= 134)
                    {
                        auto count = uint8_t{ 0 };
                        ReadContent(bnk_file, count);
                        event.action_count = static_cast<uint32_t>(count);
                    }
                    else
                    {
                        ReadContent(bnk_file, event.action_count);
                    }

                    for (auto j = 0U; j < event.action_count; ++j)
                    {
                        auto action_id = uint32_t{ 0 };
                        ReadContent(bnk_file, action_id);
                        event.action_ids.push_back(action_id);
                    }

                    event_objects[object.id] = event;
                }
                else if (object.type == ObjectType::EventAction)
                {
                    auto event_action = EventActionObject{};

                    ReadContent(bnk_file, event_action.scope);
                    ReadContent(bnk_file, event_action.action_type);
                    ReadContent(bnk_file, event_action.game_object_id);

                    bnk_file.seekg(1, ios_base::cur);

                    ReadContent(bnk_file, event_action.parameter_count);

                    for (auto j = 0U; j < static_cast<size_t>(event_action.parameter_count); ++j)
                    {
                        auto parameter_type = EventActionParameterType{};
                        ReadContent(bnk_file, parameter_type);
                        event_action.parameters_types.push_back(parameter_type);
                    }

                    for (auto j = 0U; j < static_cast<size_t>(event_action.parameter_count); ++j)
                    {
                        auto parameter = int8_t{ 0 };
                        ReadContent(bnk_file, parameter);
                        event_action.parameters.push_back(parameter);
                    }

                    bnk_file.seekg(1, ios_base::cur);
                    bnk_file.seekg(object.size - 13 - event_action.parameter_count * 2, ios_base::cur);

                    event_action_objects[object.id] = event_action;
                }

                bnk_file.seekg(object.size - sizeof(uint32_t), ios_base::cur);
                objects.push_back(object);
            }
        }

        // Seek to the end of the section
        bnk_file.seekg(section_pos + content_section.size);
    }

    // Reset EOF
    bnk_file.clear();

    auto output_directory = bnk_filename.parent_path();

    if (!no_directory)
    {
        output_directory = CreateOutputDirectory(bnk_filename);
    }

    // Dump objects information
    if (dump_objects)
    {
        auto object_filename = output_directory;
        object_filename = object_filename.append("objects.txt");
        auto object_file = fstream{ object_filename, ios::out | ios::binary };

        if (!object_file.is_open())
        {
            cout << "Unable to write objects file '" << object_filename.string() << "'\n";
            return "failed";
        }

        for (auto& [type, size, id] : objects)
        {
            object_file << "Object ID: " << id << "\n";

            switch (type)
            {
            case ObjectType::Event:
                object_file << "\tType: Event\n";
                object_file << "\tNumber of Actions: " << event_objects[id].action_count << "\n";

                for (auto& action_id : event_objects[id].action_ids)
                {
                    object_file << "\tAction ID: " << action_id << "\n";
                }
                break;
            case ObjectType::EventAction:
                object_file << "\tType: EventAction\n";
                object_file << "\tAction Scope: " << static_cast<int>(event_action_objects[id].scope) << "\n";
                object_file << "\tAction Type: " << static_cast<int>(event_action_objects[id].action_type) << "\n";
                object_file << "\tGame Object ID: " << static_cast<int>(event_action_objects[id].game_object_id) << "\n";
                object_file << "\tNumber of Parameters: " << static_cast<int>(event_action_objects[id].parameter_count) << "\n";

                for (auto j = 0; j < event_action_objects[id].parameter_count; ++j)
                {
                    object_file << "\t\tParameter Type: " << static_cast<int>(event_action_objects[id].parameters_types[j]) << "\n";
                    object_file << "\t\tParameter: " << static_cast<int>(event_action_objects[id].parameters[j]) << "\n";
                }
                break;
            default:
                object_file << "\tType: " << static_cast<int>(type) << "\n";
            }
        }

        cout << "Objects file was written to: " << object_filename.string() << "\n";
    }

    // Extract WEM files
    if (data_offset == 0U || files.empty())
    {
        cout << "No WEM files discovered to be extracted\n";
        return EXIT_SUCCESS;
    }

    cout << "Found " << files.size() << " WEM files\n";
    cout << "Start extracting...\n";

    for (auto& [id, offset, size] : files)
    {
        auto wem_filename = output_directory;
        wem_filename = wem_filename.append(to_string(id)).replace_extension(".wem");
        auto wem_file = fstream{ wem_filename, ios::out | ios::binary };

        if (swap_byte_order)
        {
            size = Swap32(size);
            offset = Swap32(offset);
        }

        if (!wem_file.is_open())
        {
            cout << "Unable to write file '" << wem_filename.string() << "'\n";
            continue;
        }

        auto data = vector<char>(size, 0U);

        bnk_file.seekg(data_offset + offset);
        bnk_file.read(data.data(), size);
        wem_file.write(data.data(), size);
    }

    return (output_directory.string());
}


long GetFileSize(string filename)
{
	struct stat stat_buf;
	int rc = stat(filename.c_str(), &stat_buf);
	return rc == 0 ? stat_buf.st_size : -1;
}


char* get_file_from_input() {
	string str;
	cout << "Enter a filepath for your *.bnk or *.pck file: ";
	cin >> str;
	ifstream file;
	file.open(str);
	if (!file) {
		cout << "\nThis file could not be opened. It either does not exist, or you don't have the proper permissions to access it.\n";
		file.close();
		get_file_from_input();
	}
    char* char_array = new char[1024];
    strncpy(char_array, str.c_str(), 1024);
	return char_array;
}
#include <CommCtrl.h>
string filepath;
HWND window = nullptr;
HWND textbox = nullptr;
HWND button = nullptr;
HWND filePrompt = nullptr;
HWND filename = nullptr;
HWND generator = nullptr;
HWND progress = nullptr;

WNDPROC defWndProc = nullptr;

LRESULT OnWindowClose(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (MessageBox(window, L"Are you sure you want exit?", L"Exit WAVE", MB_ICONQUESTION | MB_YESNO) == IDYES)
        PostQuitMessage(0);
    return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

LRESULT OnButtonClick(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    SendMessage(window, WM_COMMAND, 0, 0);

    return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}
LRESULT ShowFileDialog(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    SendMessage(window, WM_COMMAND, 0, 0);
    OPENFILENAME ofn = { 0 };
    TCHAR szFile[260] = { 0 };
    // Initialize remaining fields of OPENFILENAME structure
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE)
    {
        HANDLE hf;
        hf = CreateFile(ofn.lpstrFile,
            GENERIC_READ,
            0,
            (LPSECURITY_ATTRIBUTES)NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            (HANDLE)NULL);
        CloseHandle(hf);
        LPSTR filepath = (char*)"";
        wstring str = szFile;
        filenames = string(str.begin(), str.end());
        DestroyWindow(filename);
        filename = CreateWindowEx(0, WC_STATIC, szFile, WS_CHILD | WS_VISIBLE, 120, 40, 400, 25, window, nullptr, nullptr, nullptr);
        SendMessage(filename, WM_SETFONT, (LPARAM)hFont, TRUE);
    }
    return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}
void generateSepThread() {
    char* arr[] = { (char*)"", (char*)(filenames.c_str()) };
    string dir = extract(2, arr);
    string executablePath;

    for (char x : ExePath()) executablePath += x;
    system((string("python " + executablePath + "/run.py ") + dir + " " + executablePath).c_str());
    Sleep(1000);
    DestroyWindow(progress);
    progress = CreateWindowEx(0, STATUSCLASSNAME, L"Done", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
    return;
}
LRESULT generate(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    DestroyWindow(progress);
    Sleep(1000);
    progress = CreateWindowEx(0, STATUSCLASSNAME, L"Extracting Files...", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, nullptr, nullptr, nullptr);
    generateSepThread();

    return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);

}
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_CLOSE && hwnd == window) return OnWindowClose(hwnd, message, wParam, lParam);
   if (message == WM_COMMAND && (HWND)lParam == filePrompt) return ShowFileDialog(hwnd, message, wParam, lParam);
    if (message == WM_COMMAND && (HWND)lParam == button && filenames != "") return generate(hwnd, message, wParam, lParam);

    return CallWindowProc(defWndProc, hwnd, message, wParam, lParam);
}

int WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow){
    // Elements
    window = CreateWindowEx(0, WC_DIALOG, L"WAVE - WWise Audio Viewer and Editor", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, nullptr, nullptr, nullptr, nullptr);
    textbox = CreateWindowEx(0, WC_STATIC, L"Please choose an option below.", WS_CHILD | WS_VISIBLE, 10, 10, 620, 25, window, nullptr, nullptr, nullptr);
    filePrompt = CreateWindowEx(0, WC_BUTTON, L"Select File", WS_CHILD | WS_VISIBLE, 10, 40, 100, 25, window, nullptr, nullptr, nullptr);
    filename = CreateWindowEx(0, WC_STATIC, L"", WS_CHILD | WS_VISIBLE, 120, 40, 400, 25, window, nullptr, nullptr, nullptr);
    button = CreateWindowEx(0, WC_BUTTON, L"Generate Files", WS_CHILD | WS_VISIBLE, 10, 70, 100, 25, window, nullptr, nullptr, nullptr);
    progress = CreateWindowEx(0, STATUSCLASSNAME, L"", WS_CHILD | WS_VISIBLE, 10, 70, 100, 25, window, nullptr, nullptr, nullptr);
    defWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
    ShowWindow(window, SW_SHOW);
    // Fonts
    SendMessage(window, WM_SETFONT, (LPARAM)hFont, TRUE);
    SendMessage(textbox, WM_SETFONT, (LPARAM)hFont, TRUE);
    SendMessage(filePrompt, WM_SETFONT, (LPARAM)hFont, TRUE);
    SendMessage(button, WM_SETFONT, (LPARAM)hFont, TRUE);
    MSG message = { 0 };
    while (GetMessage(&message, nullptr, 0, 0))
        DispatchMessage(&message);
    return (int)message.wParam;
}

// Test File:
// C:\Users\ltpla\Downloads\voice.bnk