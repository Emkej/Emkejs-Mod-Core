param(
    [Parameter(Mandatory = $true)][string]$DllPath,
    [string]$KenshiPath = ""
)

$ErrorActionPreference = 'Stop'

$code = @"
using System;
using System.Runtime.InteropServices;

public static class HubPhase6WallBGoneFallbackHarness
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void TestSetRegistryAttachEnabledRaw(int isEnabled);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void WallSetAttachFailureModeRaw(int mode);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void WallResetRuntimeStateRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void WallRunStartupAttachRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void WallOnOptionsWindowInitRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int WallUseHubUiRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int WallIsAttachRetryPendingRaw();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int WallHasAttachRetryAttemptedRaw();

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern bool SetDllDirectory(string lpPathName);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);

    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Ansi)]
    private static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

    [DllImport("kernel32", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr hModule);

    private const uint DONT_RESOLVE_DLL_REFERENCES = 0x00000001u;

    private const int ATTACH_FAILURE_NONE = 0;
    private const int ATTACH_FAILURE_STARTUP_ONLY = 1;
    private const int ATTACH_FAILURE_ALWAYS = 2;

    private static void Assert(bool condition, string message)
    {
        if (!condition)
        {
            throw new Exception(message);
        }
    }

    private static void AssertState(
        WallUseHubUiRaw useHubUi,
        WallIsAttachRetryPendingRaw isRetryPending,
        WallHasAttachRetryAttemptedRaw hasRetried,
        int expectedUseHubUi,
        int expectedRetryPending,
        int expectedRetried,
        string context)
    {
        int useHubUiValue = useHubUi();
        int retryPendingValue = isRetryPending();
        int retriedValue = hasRetried();

        Assert(useHubUiValue == expectedUseHubUi,
            context + " expected use_hub_ui=" + expectedUseHubUi + ", actual=" + useHubUiValue);
        Assert(retryPendingValue == expectedRetryPending,
            context + " expected retry_pending=" + expectedRetryPending + ", actual=" + retryPendingValue);
        Assert(retriedValue == expectedRetried,
            context + " expected retry_attempted=" + expectedRetried + ", actual=" + retriedValue);
    }

    private static T Bind<T>(IntPtr module, string exportName)
    {
        IntPtr proc = GetProcAddress(module, exportName);
        Assert(proc != IntPtr.Zero, "Missing export: " + exportName);
        return (T)(object)Marshal.GetDelegateForFunctionPointer(proc, typeof(T));
    }

    public static string Run(string dllPath, string kenshiPath)
    {
        IntPtr module = IntPtr.Zero;
        try
        {
            if (!string.IsNullOrEmpty(kenshiPath))
            {
                SetDllDirectory(kenshiPath);
            }

            module = LoadLibrary(dllPath);
            if (module == IntPtr.Zero)
            {
                module = LoadLibraryEx(dllPath, IntPtr.Zero, DONT_RESOLVE_DLL_REFERENCES);
            }
            Assert(module != IntPtr.Zero, "LoadLibrary failed for " + dllPath);

            TestSetRegistryAttachEnabledRaw setRegistryAttachEnabled = Bind<TestSetRegistryAttachEnabledRaw>(module, "EMC_ModHub_Test_SetRegistryAttachEnabled");
            WallSetAttachFailureModeRaw setAttachFailureMode = Bind<WallSetAttachFailureModeRaw>(module, "EMC_ModHub_Test_WallBGone_SetAttachFailureMode");
            WallResetRuntimeStateRaw resetRuntimeState = Bind<WallResetRuntimeStateRaw>(module, "EMC_ModHub_Test_WallBGone_ResetRuntimeState");
            WallRunStartupAttachRaw runStartupAttach = Bind<WallRunStartupAttachRaw>(module, "EMC_ModHub_Test_WallBGone_RunStartupAttach");
            WallOnOptionsWindowInitRaw onOptionsWindowInit = Bind<WallOnOptionsWindowInitRaw>(module, "EMC_ModHub_Test_WallBGone_OnOptionsWindowInit");
            WallUseHubUiRaw useHubUi = Bind<WallUseHubUiRaw>(module, "EMC_ModHub_Test_WallBGone_UseHubUi");
            WallIsAttachRetryPendingRaw isRetryPending = Bind<WallIsAttachRetryPendingRaw>(module, "EMC_ModHub_Test_WallBGone_IsAttachRetryPending");
            WallHasAttachRetryAttemptedRaw hasRetried = Bind<WallHasAttachRetryAttemptedRaw>(module, "EMC_ModHub_Test_WallBGone_HasAttachRetryAttempted");

            // Case 1: Startup attach success -> hub UI enabled, no retry.
            resetRuntimeState();
            setAttachFailureMode(ATTACH_FAILURE_NONE);
            setRegistryAttachEnabled(1);
            runStartupAttach();
            AssertState(useHubUi, isRetryPending, hasRetried, 1, 0, 0, "startup_success");

            // Case 2: Startup attach fails once -> retry pending; retry succeeds on options init.
            resetRuntimeState();
            setAttachFailureMode(ATTACH_FAILURE_STARTUP_ONLY);
            setRegistryAttachEnabled(1);
            runStartupAttach();
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 1, 0, "startup_fail_once");

            onOptionsWindowInit();
            AssertState(useHubUi, isRetryPending, hasRetried, 1, 0, 1, "retry_success");

            onOptionsWindowInit();
            AssertState(useHubUi, isRetryPending, hasRetried, 1, 0, 1, "retry_idempotent");

            // Case 3: Startup attach fail and retry fail -> deterministic fallback.
            resetRuntimeState();
            setAttachFailureMode(ATTACH_FAILURE_ALWAYS);
            setRegistryAttachEnabled(1);
            runStartupAttach();
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 1, 0, "startup_fail_always");

            onOptionsWindowInit();
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 0, 1, "retry_fail_final");

            onOptionsWindowInit();
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 0, 1, "retry_fail_idempotent");

            // Case 4: Attach succeeds but register_mod path fails -> fallback without retry.
            resetRuntimeState();
            setAttachFailureMode(ATTACH_FAILURE_NONE);
            setRegistryAttachEnabled(0);
            runStartupAttach();
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 0, 0, "register_mod_fail_fallback");

            onOptionsWindowInit();
            AssertState(useHubUi, isRetryPending, hasRetried, 0, 0, 0, "register_mod_fail_no_retry");

            setRegistryAttachEnabled(-1);
            setAttachFailureMode(ATTACH_FAILURE_NONE);

            return "PASS";
        }
        finally
        {
            if (module != IntPtr.Zero)
            {
                FreeLibrary(module);
            }
        }
    }
}
"@

Add-Type -TypeDefinition $code -Language CSharp
$result = [HubPhase6WallBGoneFallbackHarness]::Run($DllPath, $KenshiPath)
Write-Host $result
