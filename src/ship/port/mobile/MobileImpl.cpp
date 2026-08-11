#if defined(__ANDROID__) || defined(__IOS__)
#include "ship/port/mobile/MobileImpl.h"
#include <SDL2/SDL.h>
#include "libultraship/bridge/consolevariablebridge.h"

#include <imgui_internal.h>

static float cameraYaw;
static float cameraPitch;

static bool isShowingVirtualKeyboard = true;
static bool isUsingTouchscreenControls = true;

void Ship::Mobile::ImGuiProcessEvent(bool wantsTextInput) {
    ImGuiInputTextState* state = ImGui::GetInputTextState(ImGui::GetActiveID());

    if (wantsTextInput) {
        if (!isShowingVirtualKeyboard) {
            state->ClearText();

            isShowingVirtualKeyboard = true;
            SDL_StartTextInput();
        }
    } else {
        if (isShowingVirtualKeyboard) {
            isShowingVirtualKeyboard = false;
            SDL_StopTextInput();
        }
    }
}

#endif
#ifdef __ANDROID__
#include <SDL_gamecontroller.h>
#include <jni.h>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <ucontext.h>
#include <unistd.h>

namespace {

constexpr size_t CRASH_REPORT_PATH_SIZE = 4096;
constexpr size_t CRASH_VERSION_SIZE = 128;

char sCrashReportPath[CRASH_REPORT_PATH_SIZE] = {};
char sCrashVersion[CRASH_VERSION_SIZE] = {};
volatile sig_atomic_t sHandlingCrash = 0;

void WriteAll(int fd, const char* text, size_t length) {
    while (length > 0) {
        ssize_t written = write(fd, text, length);
        if (written <= 0) {
            return;
        }
        text += written;
        length -= static_cast<size_t>(written);
    }
}

void WriteText(int fd, const char* text) {
    size_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    WriteAll(fd, text, length);
}

void WriteUnsigned(int fd, uint64_t value, unsigned base) {
    char digits[32];
    size_t count = 0;

    do {
        unsigned digit = value % base;
        digits[count++] = static_cast<char>(digit < 10 ? '0' + digit : 'a' + digit - 10);
        value /= base;
    } while (value != 0 && count < sizeof(digits));

    while (count > 0) {
        WriteAll(fd, &digits[--count], 1);
    }
}

void WriteField(int fd, const char* label, uint64_t value, bool hex = false) {
    WriteText(fd, label);
    if (hex) {
        WriteText(fd, "0x");
    }
    WriteUnsigned(fd, value, hex ? 16 : 10);
    WriteText(fd, "\n");
}

void WriteSignedField(int fd, const char* label, int64_t value) {
    WriteText(fd, label);
    if (value < 0) {
        WriteText(fd, "-");
        WriteUnsigned(fd, static_cast<uint64_t>(-(value + 1)) + 1, 10);
    } else {
        WriteUnsigned(fd, static_cast<uint64_t>(value), 10);
    }
    WriteText(fd, "\n");
}

const char* SignalName(int signal) {
    switch (signal) {
        case SIGABRT:
            return "SIGABRT";
        case SIGBUS:
            return "SIGBUS";
        case SIGFPE:
            return "SIGFPE";
        case SIGILL:
            return "SIGILL";
        case SIGSEGV:
            return "SIGSEGV";
        default:
            return "unknown";
    }
}

void CopyProcessMaps(int reportFd) {
    int mapsFd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (mapsFd < 0) {
        return;
    }

    WriteText(reportFd, "\nProcess memory map:\n");
    char buffer[2048];
    ssize_t bytesRead;
    while ((bytesRead = read(mapsFd, buffer, sizeof(buffer))) > 0) {
        WriteAll(reportFd, buffer, static_cast<size_t>(bytesRead));
    }
    close(mapsFd);
}

void HandleNativeCrash(int signal, siginfo_t* info, void* context) {
    if (sHandlingCrash != 0) {
        _exit(128 + signal);
    }
    sHandlingCrash = 1;

    int fd = open(sCrashReportPath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd >= 0) {
        WriteText(fd, "2 Ship 2 Harkinian Android crash report\n");
        WriteText(fd, "Report type: Native signal\n");
        WriteText(fd, "App version: ");
        WriteText(fd, sCrashVersion);
        WriteText(fd, "\nSignal: ");
        WriteText(fd, SignalName(signal));
        WriteText(fd, " (");
        WriteUnsigned(fd, static_cast<unsigned>(signal), 10);
        WriteText(fd, ")\n");

        if (info != nullptr) {
            WriteSignedField(fd, "Signal code: ", info->si_code);
            WriteField(fd, "Fault address: ", reinterpret_cast<uintptr_t>(info->si_addr), true);
        }

#if defined(__aarch64__)
        if (context != nullptr) {
            auto* machineContext = &static_cast<ucontext_t*>(context)->uc_mcontext;
            WriteField(fd, "Program counter: ", machineContext->pc, true);
            WriteField(fd, "Stack pointer: ", machineContext->sp, true);
            WriteField(fd, "Link register: ", machineContext->regs[30], true);
        }
#endif

        CopyProcessMaps(fd);
        fsync(fd);
        close(fd);
    }

    raise(signal);
}

void InstallSignalHandler(int signal) {
    struct sigaction action = {};
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = HandleNativeCrash;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigaction(signal, &action, nullptr);
}

void CopyJavaString(JNIEnv* env, jstring source, char* destination, size_t destinationSize) {
    if (source == nullptr || destinationSize == 0) {
        return;
    }

    const char* value = env->GetStringUTFChars(source, nullptr);
    if (value == nullptr) {
        return;
    }
    std::strncpy(destination, value, destinationSize - 1);
    destination[destinationSize - 1] = '\0';
    env->ReleaseStringUTFChars(source, value);
}

} // namespace

static std::atomic<bool> sGamepadBackPressed{false};

extern "C" void JNICALL Java_com_twoshipfork_mm_MainActivity_nativeInstallCrashReporter(
    JNIEnv* env, jobject, jstring reportPath, jstring versionName) {
    CopyJavaString(env, reportPath, sCrashReportPath, sizeof(sCrashReportPath));
    CopyJavaString(env, versionName, sCrashVersion, sizeof(sCrashVersion));

    InstallSignalHandler(SIGABRT);
    InstallSignalHandler(SIGBUS);
    InstallSignalHandler(SIGFPE);
    InstallSignalHandler(SIGILL);
    InstallSignalHandler(SIGSEGV);
}

extern "C" void JNICALL Java_com_twoshipfork_mm_MainActivity_nativeTestCrashReporter(JNIEnv*, jobject) {
    raise(SIGSEGV);
}

extern "C" void JNICALL Java_com_twoshipfork_mm_MainActivity_nativeGamepadBackPressed(JNIEnv* env, jobject obj) {
    sGamepadBackPressed = true;
}

bool Ship::Mobile::ConsumeGamepadBackPress() {
    return sGamepadBackPressed.exchange(false);
}

void Ship::Mobile::Init() {
    // None (add here Android initialization steps)
}

void Ship::Mobile::Exit() {
    SDL_Event quit_event;
    quit_event.type = SDL_QUIT;
    SDL_PushEvent(&quit_event);
    SDL_Quit();
    exit(0);
}

bool Ship::Mobile::IsUsingTouchscreenControls(){
    return isUsingTouchscreenControls;
}

void Ship::Mobile::EnableTouchArea(){
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject javaObject = (jobject)SDL_AndroidGetActivity();
    jclass javaClass = env->GetObjectClass(javaObject);
    jmethodID enabletoucharea = env->GetMethodID(javaClass, "EnableTouchArea", "()V");
    env->CallVoidMethod(javaObject, enabletoucharea);
}

void Ship::Mobile::DisableTouchArea(){
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject javaObject = (jobject)SDL_AndroidGetActivity();
    jclass javaClass = env->GetObjectClass(javaObject);
    jmethodID disabletoucharea = env->GetMethodID(javaClass, "DisableTouchArea", "()V");
    env->CallVoidMethod(javaObject, disabletoucharea);
}

float Ship::Mobile::GetCameraYaw(){
    return cameraYaw;
}

float Ship::Mobile::GetCameraPitch(){
    return cameraPitch;
}

static int virtual_joystick_id = -1;
static SDL_Joystick *virtual_joystick = nullptr;

extern "C" void JNICALL Java_com_twoshipfork_mm_MainActivity_attachController(JNIEnv* env, jobject obj) {
    virtual_joystick_id = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 18, 0);
    if (virtual_joystick_id == -1) {
            SDL_Log("Could not create overlay virtual controller");
        return;
    }

    virtual_joystick = SDL_JoystickOpen(virtual_joystick_id);
    if (virtual_joystick == nullptr)
        SDL_Log("Could not create virtual joystick");

    isUsingTouchscreenControls = true;
}

extern "C" void JNICALL Java_com_twoshipfork_mm_MainActivity_setCameraState(JNIEnv *env, jobject jobj, jint axis, jfloat value) {
    switch(axis){
        case 0:
            cameraYaw=value;
            break;
        case 1:
            cameraPitch=value;
            break;
    }
}

extern "C" void JNICALL Java_com_twoshipfork_mm_MainActivity_setButton(JNIEnv *env, jobject jobj, jint button, jboolean value) {
    if(button < 0){
        SDL_JoystickSetVirtualAxis(virtual_joystick,-button, value ? SDL_MAX_SINT16 : -SDL_MAX_SINT16); // This should be 0 when false, but I think there's a bug in SDL
    }else{
        SDL_JoystickSetVirtualButton(virtual_joystick, button, value);
    }
}

#include "ship/Context.h"

extern "C" void JNICALL Java_com_twoshipfork_mm_MainActivity_setAxis(JNIEnv *env, jobject jobj, jint axis, jshort value) {
    SDL_JoystickSetVirtualAxis(virtual_joystick, axis, value);
}

extern "C" void JNICALL Java_com_twoshipfork_mm_MainActivity_detachController(JNIEnv *env, jobject jobj) {
    SDL_JoystickClose(virtual_joystick);
    SDL_JoystickDetachVirtual(virtual_joystick_id);
    virtual_joystick = nullptr;
    virtual_joystick_id = -1;
    isUsingTouchscreenControls = false;
}

#endif
