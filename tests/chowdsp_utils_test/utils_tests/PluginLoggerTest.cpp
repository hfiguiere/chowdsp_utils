#include <future>
#include <TimedUnitTest.h>

namespace
{
const String logFileSubDir = "chowdsp/utils_test";
const String logFileNameRoot = "chowdsp_utils_test_Log_";
} // namespace

class PluginLoggerTest : public TimedUnitTest
{
public:
    PluginLoggerTest() : TimedUnitTest ("Plugin Logger Test")
    {
    }

    void basicLogTest()
    {
        const String testLogString = "This string should be in the log file...";
        const String testNonLogString = "This string should not be in the log file...";

        File logFile;
        {
            chowdsp::PluginLogger logger { logFileSubDir, logFileNameRoot };
            Logger::writeToLog (testLogString);

            logFile = logger.getLogFile();
        }

        Logger::writeToLog (testNonLogString);

        auto logString = logFile.loadFileAsString();
        expect (logString.contains (testLogString), "Test log string was not found in the log file!");
        expect (! logString.contains (testNonLogString), "Test non-log string WAS found in the log file!");
    }

    void limitNumLogFilesTest()
    {
        constexpr int numLoggersAtOnce = 5;
        constexpr int numIters = 20;

        auto logsDirectory = FileLogger::getSystemLogFileFolder().getChildFile (logFileSubDir);
        auto getNumLogFiles = [&] {
            return logsDirectory.getNumberOfChildFiles (File::findFiles, "*");
        };

        for (int i = 0; i < numIters; ++i)
        {
            using LoggerPtr = std::unique_ptr<chowdsp::PluginLogger>;
            std::vector<std::future<LoggerPtr>> futures;
            for (int j = 0; j < numLoggersAtOnce; ++j)
                futures.push_back (std::async (std::launch::async, [] { return std::make_unique<chowdsp::PluginLogger> (logFileSubDir, logFileNameRoot); }));

            auto numLogFiles = getNumLogFiles();
            expectLessOrEqual (numLogFiles, 55, "Too many log files found in logs directory!");
        }

        logsDirectory.deleteRecursively();
    }

    void crashLogTest()
    {
        File logFile;
        {
            chowdsp::PluginLogger logger { logFileSubDir, logFileNameRoot };
            logFile = logger.getLogFile();
            chowdsp::PluginLogger::handleCrashWithSignal (44);
        }

        auto logString = logFile.loadFileAsString();
        expect (logString.contains ("Interrupt signal received!"), "Interrupt signal string not found in log!");
        expect (logString.contains ("Stack Trace:"), "Stack trace string not found in log!");
        expect (logString.contains ("Plugin crashing!!!"), "Plugin crashing string not found in log!");
        expect (! logString.contains ("Exiting gracefully"), "Exit string string WAS found in the log file!");

        // the first logger we create after a crash should report the crash...
        {
            auto prevNumTopLevelWindows = TopLevelWindow::getNumTopLevelWindows();
            chowdsp::PluginLogger logger { logFileSubDir, logFileNameRoot };

            auto newNumTopLevelWindows = TopLevelWindow::getNumTopLevelWindows();
            expectEquals (newNumTopLevelWindows, prevNumTopLevelWindows + 1, "AlertWindow not created!");

            // Linux on CI doesn't like trying to open the log file!
            for (int i = 0; i < newNumTopLevelWindows; ++i)
            {
                if (auto* alertWindow = dynamic_cast<AlertWindow*> (TopLevelWindow::getTopLevelWindow (i)))
                {
                    alertWindow->triggerButtonClick ("Show Log File");
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
                    break;
                }
            }
        }

        // the next logger should not!
        {
            auto prevNumTopLevelWindows = TopLevelWindow::getNumTopLevelWindows();
            chowdsp::PluginLogger logger { logFileSubDir, logFileNameRoot };

            auto newNumTopLevelWindows = TopLevelWindow::getNumTopLevelWindows();
            expectEquals (newNumTopLevelWindows, prevNumTopLevelWindows, "AlertWindow was incorrectly created!");
        }
    }

    void runTestTimed() override
    {
        beginTest ("Basic Log Test");
        basicLogTest();

        beginTest ("Limit Num Log Files Test");
        limitNumLogFilesTest();

#if ! JUCE_LINUX
        beginTest ("Crash Log Test");
        crashLogTest();
#endif

        // clean up after ourselves...
        auto logsDirectory = FileLogger::getSystemLogFileFolder().getChildFile (logFileSubDir);
        logsDirectory.deleteRecursively();
    }
};

static PluginLoggerTest pluginLoggerTest;
